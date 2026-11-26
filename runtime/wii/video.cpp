#include "wii/video.h"
#include "wii/gx_efb.h"
#include "wii/gx_fifo.h"
#include "wii/gx_raster.h"
#include "wii/memory_map.h"
#include "wii/remote.h"
#include "gpu/gpu.h"
#include "host/env.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace wii::video
{
    namespace
    {
        SDL_Window* g_window = nullptr;
        bool g_ready = false;
        uint8_t* g_base = nullptr; // the guest's memory, which the copies write
        gx::Efb& TheEfb()
        {
            static gx::Efb& efb = *new gx::Efb{}; // used by the guest's threads until the process ends
            return efb;
        }

        // The last display copies, by destination: their sizes, which the video interface takes from its own
        // registers on the console.
        constexpr size_t Remembered = 4;
        gx::XfbCopy g_copies[Remembered];
        size_t g_nextCopy = 0;

        gx::Rasterizer& TheRasterizer()
        {
            static gx::Rasterizer& rasterizer = *new gx::Rasterizer;
            return rasterizer;
        }

        gpu::Texture* g_image = nullptr;
        uint32_t g_imageWidth = 0, g_imageHeight = 0;
        std::vector<uint8_t> g_rgba;
        uint32_t g_shown = 0;

        // SKY_WII_FRAMES=<folder>: frames 1-3 and every 60th written there as PPM pictures, for the tests.
        void Dump(uint32_t width, uint32_t height)
        {
            static const char* folder = skyenv::Get("SKY_WII_FRAMES");
            if (folder == nullptr || !(g_shown <= 3 || g_shown % 60 == 0))
                return;
            const std::string path = std::string(folder) + "/frame" + std::to_string(g_shown) + ".ppm";
            if (FILE* file = fopen(path.c_str(), "wb"))
            {
                fprintf(file, "P6\n%u %u\n255\n", width, height);
                for (size_t i = 0; i < size_t(width) * height; i++)
                    fwrite(&g_rgba[4 * i], 1, 3, file);
                fclose(file);
            }
        }

        SDL_Gamepad* g_gamepad = nullptr;

        // Remote 1's buttons from the keyboard and the first controller: the remote held upright, as the menus ask.
        uint32_t HostButtons()
        {
            using namespace remote::button;
            struct Key
            {
                SDL_Scancode key;
                SDL_GamepadButton pad;
                uint32_t bit;
            };
            static constexpr Key Keys[] = {
                { SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH, A },
                { SDL_SCANCODE_BACKSPACE, SDL_GAMEPAD_BUTTON_EAST, B },
                { SDL_SCANCODE_1, SDL_GAMEPAD_BUTTON_WEST, One },
                { SDL_SCANCODE_2, SDL_GAMEPAD_BUTTON_NORTH, Two },
                { SDL_SCANCODE_EQUALS, SDL_GAMEPAD_BUTTON_START, Plus },
                { SDL_SCANCODE_MINUS, SDL_GAMEPAD_BUTTON_BACK, Minus },
                { SDL_SCANCODE_H, SDL_GAMEPAD_BUTTON_GUIDE, Home },
                { SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP, Up },
                { SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN, Down },
                { SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT, Left },
                { SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Right },
            };
            const bool* keys = SDL_GetKeyboardState(nullptr);
            uint32_t buttons = 0;
            for (const Key& key : Keys)
                if (keys[key.key] || (g_gamepad != nullptr && SDL_GetGamepadButton(g_gamepad, key.pad)))
                    buttons |= key.bit;
            return buttons;
        }

        void OnCopy(uint32_t bp52)
        {
            if ((bp52 & gx::copy::ToXfb) == 0)
                return; // a copy to a texture: the renderer's, later
            gx::XfbCopy done;
            if (gx::CopyToXfb(TheEfb(), g_base, gx::Processor().BpRegisters(), done))
            {
                g_copies[g_nextCopy] = done;
                g_nextCopy = (g_nextCopy + 1) % Remembered;
            }
        }
    } // namespace

    bool Start(const char* title, uint8_t* base)
    {
        g_base = base;
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        {
            fprintf(stderr, "[video] SDL: %s\n", SDL_GetError());
            return false;
        }
        const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        g_window = SDL_CreateWindow(title, 1280, 960, flags | gpu::WindowFlags());
        if (g_window == nullptr)
        {
            // SDL's offscreen driver (the tests') has no Metal view: the renderer then draws to an offscreen target,
            // and the window takes no graphics context of SDL's own.
            SDL_PropertiesID properties = SDL_CreateProperties();
            SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
            SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 1280);
            SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 960);
            SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, Sint64(flags));
            SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, true);
            g_window = SDL_CreateWindowWithProperties(properties);
            SDL_DestroyProperties(properties);
        }
        g_ready = g_window != nullptr && gpu::Init(g_window);
        if (!g_ready)
        {
            fprintf(stderr, "[video] no window or renderer: %s\n", SDL_GetError());
            return false;
        }
        gx::CommandProcessor& gp = gx::Processor();
        gp.onCopy = OnCopy;
        gp.onBp = [](uint8_t index, uint32_t value) { TheRasterizer().Bp(index, value); };
        gp.onDraw = [](const gx::DrawCall& draw) { TheRasterizer().Draw(gx::Processor(), g_base, draw, TheEfb()); };
        fprintf(stderr, "[video] %s\n", gpu::BackendName());
        return true;
    }

    bool Pump()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                return false;
            if (event.type == SDL_EVENT_GAMEPAD_ADDED && g_gamepad == nullptr)
                g_gamepad = SDL_OpenGamepad(event.gdevice.which);
            if (event.type == SDL_EVENT_GAMEPAD_REMOVED && g_gamepad != nullptr &&
                SDL_GetGamepadID(g_gamepad) == event.gdevice.which)
            {
                SDL_CloseGamepad(g_gamepad);
                g_gamepad = nullptr;
            }
        }
        remote::First().SetHostButtons(HostButtons());
        return true;
    }

    void Show(const uint8_t* base, uint32_t address)
    {
        if (!g_ready)
            return;
        const uint32_t physical = address & 0x1FFFFFFF;
        gx::XfbCopy shown{ physical, 640, 528, 1280 }; // a frame buffer no copy made: the SDK's usual size
        for (const gx::XfbCopy& copy : g_copies)
            if (copy.address == physical && copy.width != 0)
                shown = copy;
        const uint32_t cached = 0x80000000u | physical;
        if (!InMemory(cached, shown.stride * shown.height))
            return;
        if (g_image == nullptr || g_imageWidth != shown.width || g_imageHeight != shown.height)
        {
            if (g_image != nullptr)
                gpu::DestroyTexture(g_image);
            gpu::TextureDesc desc;
            desc.width = shown.width;
            desc.height = shown.height;
            desc.format = gpu::Format::R8G8B8A8_UNORM;
            g_image = gpu::CreateTexture(desc);
            g_imageWidth = shown.width;
            g_imageHeight = shown.height;
            g_rgba.resize(size_t(shown.width) * shown.height * 4);
        }
        gx::XfbToRgba(base + cached, shown.width, shown.height, shown.stride, g_rgba.data());
        g_shown++;
        Dump(shown.width, shown.height);
        gpu::UploadTexture(g_image, g_rgba.data(), shown.width * 4);
        gpu::Present(g_image);
    }
}
