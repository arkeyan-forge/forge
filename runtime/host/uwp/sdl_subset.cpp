// SDL 3 has no UWP support, but the shared runtime code calls SDL for a few utilities: the preference folder, small
// file reads and writes, window properties, timing and RGBA PNG files (gpu/, host/display.cpp, host/portal*.cpp,
// kernel/xam_content.cpp, host/glyphs.cpp, host/toybox.cpp, host/art/game_art.cpp, host/collection/, host/machine_state.cpp,
// host/gamemenu_debug.cpp). This file implements exactly those calls for the UWP host, against SDL's own headers;
// anything else fails to link. The files that include SDL are compiled for the desktop API family, since SDL's
// headers refuse WINAPI_FAMILY_APP (see runtime/CMakeLists.txt); the imports stay within the UWP surface
// (tools/xbox/check_uwp_imports.sh).
#include "host/uwp/uwp.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

struct SDL_Window
{
    SDL_PropertiesID properties;
    int width;
    int height;
};

// A file, or a read-only block of memory (SDL_IOFromConstMem: the Toy Box's built-in pictures).
struct SDL_IOStream
{
    FILE* file;
    const uint8_t* memory = nullptr;
    size_t size = 0;
    size_t position = 0;
};

namespace
{
    std::string g_preferencePath;
    IUnknown* g_coreWindow;
    uint32_t g_windowWidth;
    uint32_t g_windowHeight;
    SDL_Window g_window;

    using Property = std::variant<void*, std::string, Sint64, bool>;
    std::mutex g_propertiesMutex;
    std::map<SDL_PropertiesID, std::map<std::string, Property>> g_properties;
    SDL_PropertiesID g_nextProperties = 1;

    const auto g_start = std::chrono::steady_clock::now();

    fs::path Utf8Path(const char* path) { return fs::path(reinterpret_cast<const char8_t*>(path)); }

    bool SetProperty(SDL_PropertiesID id, const char* name, Property value)
    {
        std::lock_guard lock(g_propertiesMutex);
        auto set = g_properties.find(id);
        if (set == g_properties.end())
            return false;
        set->second[name] = std::move(value);
        return true;
    }

    // SDL's wildcards: '*' any run of characters, '?' one character.
    bool Match(const char* pattern, const char* name, bool ignoreCase)
    {
        auto same = [&](char a, char b) {
            return ignoreCase ? std::tolower(uint8_t(a)) == std::tolower(uint8_t(b)) : a == b;
        };
        if (*pattern == '\0')
            return *name == '\0';
        if (*pattern == '*')
            return Match(pattern + 1, name, ignoreCase) || (*name != '\0' && Match(pattern, name + 1, ignoreCase));
        return *name != '\0' && (*pattern == '?' || same(*pattern, *name)) && Match(pattern + 1, name + 1, ignoreCase);
    }
}

void UwpSetPreferencePath(std::string path) { g_preferencePath = std::move(path); }

void UwpSetCoreWindow(IUnknown* window, uint32_t width, uint32_t height)
{
    g_coreWindow = window;
    g_windowWidth = width;
    g_windowHeight = height;
}

// --- Filesystem and files ------------------------------------------------------------------------------------------

// The package's LocalState folder for every organisation and application name: the package is the application.
char* SDL_GetPrefPath(const char*, const char*)
{
    if (g_preferencePath.empty())
        return nullptr;
    return strdup(g_preferencePath.c_str());
}

void SDL_free(void* memory) { free(memory); }

// The entries of the directory itself (no recursion), in one allocation like SDL's.
char** SDL_GlobDirectory(const char* path, const char* pattern, SDL_GlobFlags flags, int* count)
{
    std::vector<std::string> names;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(Utf8Path(path), error))
    {
        std::u8string name = entry.path().filename().u8string();
        std::string utf8(name.begin(), name.end());
        if (pattern == nullptr || Match(pattern, utf8.c_str(), flags & SDL_GLOB_CASEINSENSITIVE))
            names.push_back(std::move(utf8));
    }
    if (error)
        return nullptr;

    size_t size = (names.size() + 1) * sizeof(char*);
    for (const std::string& name : names)
        size += name.size() + 1;
    auto list = static_cast<char**>(malloc(size));
    char* text = reinterpret_cast<char*>(list + names.size() + 1);
    for (size_t i = 0; i < names.size(); i++)
    {
        list[i] = text;
        memcpy(text, names[i].c_str(), names[i].size() + 1);
        text += names[i].size() + 1;
    }
    list[names.size()] = nullptr;
    if (count != nullptr)
        *count = int(names.size());
    return list;
}

bool SDL_GetPathInfo(const char* path, SDL_PathInfo* info)
{
    std::error_code error;
    fs::file_status status = fs::status(Utf8Path(path), error);
    if (error || !fs::exists(status))
        return false;
    if (info != nullptr)
    {
        *info = {};
        info->type = fs::is_regular_file(status) ? SDL_PATHTYPE_FILE
                     : fs::is_directory(status)  ? SDL_PATHTYPE_DIRECTORY
                                                 : SDL_PATHTYPE_OTHER;
        if (info->type == SDL_PATHTYPE_FILE)
            info->size = fs::file_size(Utf8Path(path), error);
    }
    return true;
}

bool SDL_CreateDirectory(const char* path)
{
    std::error_code error;
    fs::create_directories(Utf8Path(path), error);
    return fs::is_directory(Utf8Path(path), error);
}

void* SDL_LoadFile(const char* file, size_t* size)
{
    std::ifstream stream(Utf8Path(file), std::ios::binary | std::ios::ate);
    if (!stream)
        return nullptr;
    size_t length = size_t(stream.tellg());
    auto data = static_cast<char*>(malloc(length + 1)); // SDL adds a terminating zero
    stream.seekg(0);
    if (!stream.read(data, std::streamsize(length)))
    {
        free(data);
        return nullptr;
    }
    data[length] = '\0';
    if (size != nullptr)
        *size = length;
    return data;
}

bool SDL_RenamePath(const char* oldPath, const char* newPath)
{
    std::error_code error;
    fs::rename(Utf8Path(oldPath), Utf8Path(newPath), error);
    return !error;
}

bool SDL_RemovePath(const char* path)
{
    std::error_code error;
    return fs::remove(Utf8Path(path), error);
}

bool SDL_SaveFile(const char* file, const void* data, size_t size)
{
    std::ofstream stream(Utf8Path(file), std::ios::binary | std::ios::trunc);
    return stream.write(static_cast<const char*>(data), std::streamsize(size)) && stream.flush();
}

SDL_IOStream* SDL_IOFromFile(const char* file, const char* mode)
{
    std::wstring wideMode(mode, mode + strlen(mode));
    FILE* handle = _wfopen(Utf8Path(file).c_str(), wideMode.c_str());
    return handle != nullptr ? new SDL_IOStream{ handle } : nullptr;
}

SDL_IOStream* SDL_IOFromConstMem(const void* memory, size_t size)
{
    if (memory == nullptr)
        return nullptr;
    return new SDL_IOStream{ nullptr, static_cast<const uint8_t*>(memory), size, 0 };
}

size_t SDL_WriteIO(SDL_IOStream* stream, const void* data, size_t size)
{
    return stream->file != nullptr ? fwrite(data, 1, size, stream->file) : 0;
}

size_t SDL_ReadIO(SDL_IOStream* stream, void* data, size_t size)
{
    if (stream->file != nullptr)
        return fread(data, 1, size, stream->file);
    const size_t count = std::min(size, stream->size - stream->position);
    memcpy(data, stream->memory + stream->position, count);
    stream->position += count;
    return count;
}

bool SDL_CloseIO(SDL_IOStream* stream)
{
    if (stream == nullptr)
        return false;
    bool closed = stream->file == nullptr || fclose(stream->file) == 0;
    delete stream;
    return closed;
}

// The entries of the directory itself, as SDL gives them: the folder with a trailing separator, then the name.
bool SDL_EnumerateDirectory(const char* path, SDL_EnumerateDirectoryCallback callback, void* userdata)
{
    std::error_code error;
    fs::directory_iterator entries(Utf8Path(path), error);
    if (error)
        return false;
    std::string folder = path;
    if (!folder.empty() && folder.back() != '/' && folder.back() != '\\')
        folder += '\\';
    for (const fs::directory_entry& entry : entries)
    {
        const std::u8string name = entry.path().filename().u8string();
        const SDL_EnumerationResult result =
            callback(userdata, folder.c_str(), reinterpret_cast<const char*>(name.c_str()));
        if (result == SDL_ENUM_SUCCESS)
            return true;
        if (result == SDL_ENUM_FAILURE)
            return false;
    }
    return true;
}

// --- Surfaces (RGBA32 only: the Toy Box's portraits and their PNG cache) -----------------------------------------

namespace
{
    SDL_Surface* NewSurface(int width, int height, void* pixels, int pitch)
    {
        auto surface = new SDL_Surface{};
        surface->format = SDL_PIXELFORMAT_RGBA32;
        surface->w = width;
        surface->h = height;
        surface->refcount = 1;
        if (pixels != nullptr)
        {
            surface->flags = SDL_SURFACE_PREALLOCATED;
            surface->pixels = pixels;
            surface->pitch = pitch;
        }
        else
        {
            surface->pitch = width * 4;
            surface->pixels = calloc(size_t(height), size_t(surface->pitch));
        }
        return surface;
    }
}

SDL_Surface* SDL_CreateSurfaceFrom(int width, int height, SDL_PixelFormat format, void* pixels, int pitch)
{
    if (format != SDL_PIXELFORMAT_RGBA32 || width <= 0 || height <= 0 || pixels == nullptr)
        return nullptr;
    return NewSurface(width, height, pixels, pitch);
}

void SDL_DestroySurface(SDL_Surface* surface)
{
    if (surface == nullptr)
        return;
    if (!(surface->flags & SDL_SURFACE_PREALLOCATED))
        free(surface->pixels);
    delete surface;
}

SDL_Surface* SDL_ConvertSurface(SDL_Surface* surface, SDL_PixelFormat format)
{
    if (surface == nullptr || surface->format != SDL_PIXELFORMAT_RGBA32 || format != SDL_PIXELFORMAT_RGBA32)
        return nullptr;
    SDL_Surface* copy = NewSurface(surface->w, surface->h, nullptr, 0);
    for (int y = 0; y < surface->h; y++)
        memcpy(static_cast<uint8_t*>(copy->pixels) + y * copy->pitch,
            static_cast<const uint8_t*>(surface->pixels) + y * surface->pitch, size_t(copy->pitch));
    return copy;
}

SDL_Surface* SDL_LoadPNG(const char* file)
{
    size_t size = 0;
    void* data = SDL_LoadFile(file, &size);
    if (data == nullptr)
        return nullptr;
    int width = 0, height = 0;
    stbi_uc* pixels = stbi_load_from_memory(static_cast<const stbi_uc*>(data), int(size), &width, &height, nullptr, 4);
    free(data);
    if (pixels == nullptr)
        return nullptr;
    SDL_Surface* surface = NewSurface(width, height, nullptr, 0);
    memcpy(surface->pixels, pixels, size_t(width) * size_t(height) * 4);
    stbi_image_free(pixels);
    return surface;
}

SDL_Surface* SDL_LoadPNG_IO(SDL_IOStream* stream, bool closeio)
{
    if (stream == nullptr)
        return nullptr;
    std::vector<uint8_t> data;
    uint8_t chunk[4096];
    for (size_t read; (read = SDL_ReadIO(stream, chunk, sizeof(chunk))) != 0;)
        data.insert(data.end(), chunk, chunk + read);
    if (closeio)
        SDL_CloseIO(stream);
    int width = 0, height = 0;
    stbi_uc* pixels = stbi_load_from_memory(data.data(), int(data.size()), &width, &height, nullptr, 4);
    if (pixels == nullptr)
        return nullptr;
    SDL_Surface* surface = NewSurface(width, height, nullptr, 0);
    memcpy(surface->pixels, pixels, size_t(width) * size_t(height) * 4);
    stbi_image_free(pixels);
    return surface;
}

bool SDL_SavePNG(SDL_Surface* surface, const char* file)
{
    if (surface == nullptr || surface->format != SDL_PIXELFORMAT_RGBA32)
        return false;
    std::vector<uint8_t> png;
    auto append = [](void* context, void* data, int size) {
        auto* out = static_cast<std::vector<uint8_t>*>(context);
        out->insert(out->end(), static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
    };
    return stbi_write_png_to_func(append, &png, surface->w, surface->h, 4, surface->pixels, surface->pitch) != 0 &&
           SDL_SaveFile(file, png.data(), png.size());
}

const char* SDL_GetError() { return ""; }

// --- Events --------------------------------------------------------------------------------------------------------

// No event queue: the host reads the view's events itself (host/uwp/main.cpp). The F3 menu's "quit to the launcher"
// and the glyph test script's events go nowhere; there is no launcher on the console.
bool SDL_PushEvent(SDL_Event*) { return false; }

// The console's text is never copied anywhere.
bool SDL_SetClipboardText(const char*) { return false; }

// The UWP host has no keyboard hotkeys: watchers are never called.
bool SDL_AddEventWatch(SDL_EventFilter, void*) { return true; }

// No keyboard: the Toy Box's nickname field only keeps the name it had, and SKY_TOYBOX_SCRIPT finds no key.
bool SDL_StartTextInput(SDL_Window*) { return true; }

bool SDL_StopTextInput(SDL_Window*) { return true; }

SDL_Keycode SDL_GetKeyFromName(const char*) { return SDLK_UNKNOWN; }

// No keyboard events reach the shared code on the console: every key reads as released (the Toy Box's wheel aims
// with the arrows only on desktops).
const bool* SDL_GetKeyboardState(int* count)
{
    static const bool keys[SDL_SCANCODE_COUNT] = {};
    if (count != nullptr)
        *count = SDL_SCANCODE_COUNT;
    return keys;
}

// --- Properties ----------------------------------------------------------------------------------------------------

SDL_PropertiesID SDL_CreateProperties()
{
    std::lock_guard lock(g_propertiesMutex);
    SDL_PropertiesID id = g_nextProperties++;
    g_properties[id];
    return id;
}

void SDL_DestroyProperties(SDL_PropertiesID id)
{
    std::lock_guard lock(g_propertiesMutex);
    g_properties.erase(id);
}

bool SDL_SetPointerProperty(SDL_PropertiesID id, const char* name, void* value) { return SetProperty(id, name, value); }

bool SDL_SetStringProperty(SDL_PropertiesID id, const char* name, const char* value)
{
    return SetProperty(id, name, std::string(value != nullptr ? value : ""));
}

bool SDL_SetNumberProperty(SDL_PropertiesID id, const char* name, Sint64 value) { return SetProperty(id, name, value); }

bool SDL_SetBooleanProperty(SDL_PropertiesID id, const char* name, bool value) { return SetProperty(id, name, value); }

void* SDL_GetPointerProperty(SDL_PropertiesID id, const char* name, void* defaultValue)
{
    std::lock_guard lock(g_propertiesMutex);
    auto set = g_properties.find(id);
    if (set == g_properties.end())
        return defaultValue;
    auto property = set->second.find(name);
    if (property == set->second.end() || !std::holds_alternative<void*>(property->second))
        return defaultValue;
    return std::get<void*>(property->second);
}

// --- Window --------------------------------------------------------------------------------------------------------

// The one window there is: the view's CoreWindow, at the size the host chose for the swap chain.
SDL_Window* SDL_CreateWindow(const char*, int, int, SDL_WindowFlags)
{
    if (g_coreWindow == nullptr)
        return nullptr;
    if (g_window.properties == 0)
    {
        g_window = { SDL_CreateProperties(), int(g_windowWidth), int(g_windowHeight) };
        SDL_SetPointerProperty(g_window.properties, UwpCoreWindowProperty, g_coreWindow);
    }
    return &g_window;
}

SDL_Window* SDL_CreateWindowWithProperties(SDL_PropertiesID) { return nullptr; }

SDL_PropertiesID SDL_GetWindowProperties(SDL_Window* window) { return window != nullptr ? window->properties : 0; }

SDL_WindowFlags SDL_GetWindowFlags(SDL_Window*) { return SDL_WINDOW_FULLSCREEN; }

bool SDL_GetWindowSizeInPixels(SDL_Window* window, int* width, int* height)
{
    if (window == nullptr)
        return false;
    if (width != nullptr)
        *width = window->width;
    if (height != nullptr)
        *height = window->height;
    return true;
}

bool SDL_GetWindowSize(SDL_Window* window, int* width, int* height)
{
    return SDL_GetWindowSizeInPixels(window, width, height);
}

void SDL_Metal_DestroyView(SDL_MetalView) {}

// One display, the TV, at the swap chain's size: the console presents at 60 Hz.
SDL_DisplayID SDL_GetDisplayForWindow(SDL_Window*) { return 1; }

const SDL_DisplayMode* SDL_GetCurrentDisplayMode(SDL_DisplayID)
{
    static SDL_DisplayMode mode{};
    mode.displayID = 1;
    mode.format = SDL_PIXELFORMAT_XRGB8888;
    mode.w = int(g_windowWidth);
    mode.h = int(g_windowHeight);
    mode.pixel_density = 1.0f;
    mode.refresh_rate = 60.0f;
    return &mode;
}

float SDL_GetWindowDisplayScale(SDL_Window*) { return 1.0f; }

const char* SDL_GetPlatform() { return "Xbox (UWP)"; }

int SDL_GetNumLogicalCPUCores() { return int(std::max(1u, std::thread::hardware_concurrency())); }

// --- Button glyphs -------------------------------------------------------------------------------------------------

// The UWP host calls glyphs::Init (the [input] glyphs setting, xboxone by default) but never glyphs::OnEvent: there is
// no keyboard, and the pad is Windows.Gaming.Input's (an Xbox One / Series pad to SDL). These only satisfy the link.
SDL_Scancode SDL_GetScancodeFromName(const char*) { return SDL_SCANCODE_UNKNOWN; }

SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode, SDL_Keymod, bool) { return SDLK_UNKNOWN; }

const char* SDL_GetKeyName(SDL_Keycode) { return ""; }

const char* SDL_GetScancodeName(SDL_Scancode) { return ""; }

SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode, SDL_Keymod*) { return SDL_SCANCODE_UNKNOWN; }

// No SDL gamepads either (the debug screen's list is empty), and no virtual joystick (SKY_GLYPH_SCRIPT is a desktop
// test aid).
SDL_JoystickID* SDL_GetGamepads(int* count)
{
    if (count != nullptr)
        *count = 0;
    return static_cast<SDL_JoystickID*>(calloc(1, sizeof(SDL_JoystickID)));
}

SDL_GamepadType SDL_GetGamepadTypeForID(SDL_JoystickID) { return SDL_GAMEPAD_TYPE_XBOXONE; }

const char* SDL_GetGamepadStringForType(SDL_GamepadType) { return "xboxone"; }

SDL_JoystickID SDL_AttachVirtualJoystick(const SDL_VirtualJoystickDesc*) { return 0; }

bool SDL_DetachVirtualJoystick(SDL_JoystickID) { return false; }

SDL_Joystick* SDL_OpenJoystick(SDL_JoystickID) { return nullptr; }

void SDL_CloseJoystick(SDL_Joystick*) {}

bool SDL_SetJoystickVirtualButton(SDL_Joystick*, int, bool) { return false; }

bool SDL_SetJoystickVirtualAxis(SDL_Joystick*, int, Sint16) { return false; }

SDL_JoystickID SDL_GetGamepadID(SDL_Gamepad*) { return 0; }

SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad*) { return SDL_GAMEPAD_TYPE_XBOXONE; }

// --- Time ----------------------------------------------------------------------------------------------------------

Uint64 SDL_GetTicksNS()
{
    return Uint64(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - g_start).count());
}

Uint64 SDL_GetTicks() { return SDL_GetTicksNS() / 1000000; }

// Nanoseconds since 1970 (UTC), as SDL_Time.
bool SDL_GetCurrentTime(SDL_Time* ticks)
{
    if (ticks == nullptr)
        return false;
    *ticks = SDL_Time(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
    return true;
}

void SDL_Delay(Uint32 milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

void SDL_DelayPrecise(Uint64 nanoseconds) { std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds)); }
