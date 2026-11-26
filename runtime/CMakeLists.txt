# Host OS services (memory, threads, faults, files): POSIX or Win32.
if(WIN32)
    set(ARKEYAN_PLATFORM_SOURCES platform/platform_win32.cpp)
    # Windows 10 (CreateFile2) in every target that links it, before any header (fmt forces <cstdlib> first on mingw).
    set_source_files_properties(platform/platform_win32.cpp PROPERTIES COMPILE_DEFINITIONS _WIN32_WINNT=0x0A00)
else()
    set(ARKEYAN_PLATFORM_SOURCES platform/platform_posix.cpp)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/art_reader.cmake)

# Keep target names and bundle identifiers stable; player-visible names come from game.txt.
file(STRINGS "${ARKEYAN_GAME_DIR}/game.txt" ARKEYAN_RUNTIME_NAME REGEX "^runtime=")
string(REGEX REPLACE "^runtime=" "" ARKEYAN_RUNTIME_NAME "${ARKEYAN_RUNTIME_NAME}")
if(ARKEYAN_GAME STREQUAL "giants")
    set(ARKEYAN_BUNDLE_ID SGRecomp)
elseif(ARKEYAN_GAME STREQUAL "swapforce")
    set(ARKEYAN_BUNDLE_ID SSFRecomp)
elseif(ARKEYAN_GAME STREQUAL "trapteam")
    set(ARKEYAN_BUNDLE_ID TTRecomp)
elseif(ARKEYAN_GAME STREQUAL "superchargers")
    set(ARKEYAN_BUNDLE_ID SSCRecomp)
elseif(ARKEYAN_GAME STREQUAL "imaginators")
    set(ARKEYAN_BUNDLE_ID SIMRecomp)
else()
    set(ARKEYAN_BUNDLE_ID SSARecomp)
endif()
# Optional disc-derived icon family, generated locally by Arkeyan-Builder --icon-only. Never checked in.
set(ARKEYAN_GAME_ICON_DIR "${CMAKE_SOURCE_DIR}/private/${ARKEYAN_GAME}/icon" CACHE PATH "Local game_icon.png/.ico/.icns folder")

add_executable(ArkeyanRuntime
    apu/xma_context.cpp
    d3d/bink_display.cpp
    d3d/capture.cpp
    d3d/command_buffers.cpp
    d3d/device.cpp
    d3d/render.cpp
    d3d/resolve_writeback.cpp
    d3d/resource_lock.cpp
    d3d/ring.cpp
    d3d/ultrawide.cpp
    d3d/upscale.cpp
    d3d/shaders.cpp
    d3d/xenos.cpp
    d3d/null_gpu.cpp
    game.cpp
    main.cpp
    memory.cpp
    write_watch.cpp
    loader.cpp
    platform/jmpbuf.cpp
    platform/timebase.cpp
    ${ARKEYAN_PLATFORM_SOURCES}
    thread.cpp
    indirect_call_guard.cpp
    vfs.cpp
    disc/rvz.cpp
    disc/sha1.cpp
    disc/wii_disc.cpp
    kernel/audio.cpp
    host/audio.cpp
    host/display.cpp
    host/dds.cpp
    host/glyphs.cpp
    host/glyph_models.cpp
    host/glyph_assets.cpp
    host/texture_pack.cpp
    host/game_options.cpp
    ${ARKEYAN_COLLECTION_SOURCES}
    ${ARKEYAN_ART_SOURCES}
    host/input.cpp
    host/pad_map.cpp host/toybox/pad_bindings.cpp
    host/mouse.cpp
    host/mouse_input.cpp
    host/sdl_hints.cpp
    host/launcher.cpp
    host/launcher_games.cpp
    host/portable.cpp
    host/xex_title.cpp
    host/collection/achievements.cpp
    host/collection/presence.cpp
    host/collection/chests.cpp
    host/log.cpp
    host/gamemenu.cpp
    host/gamemenu_debug.cpp
    host/machine_state.cpp
    host/commands.cpp
    host/report.cpp
    host/portal.cpp
    host/portal_adpcm.cpp
    host/portal_async.cpp
    host/portal_emulated.cpp
    host/portal_identity.cpp
    host/portal_mixed.cpp
    host/portal_prompt.cpp
    host/portal_speaker.cpp
    host/portal_hid.cpp
    host/portal_usb.cpp
    host/windows_devices.cpp
    host/profiles.cpp
    host/settings.cpp
    host/window_size.cpp
    host/toybox.cpp
    host/toybox/add_list.cpp
    host/toybox/draw.cpp
    host/toybox/grid.cpp
    host/toybox/menu.cpp
    host/toybox/model.cpp
    host/toybox/trap_villain.cpp
    host/toybox/version_game.cpp
    host/toybox_link.cpp
    host/toybox_remote.cpp
    host/toybox_socket.cpp
    kernel/files.cpp
    kernel/imports_giants.cpp
    kernel/imports_swapforce.cpp
    kernel/imports_trapteam.cpp
    kernel/input.cpp
    kernel/memory.cpp
    kernel/module.cpp
    kernel/net.cpp
    kernel/objects.cpp
    kernel/overlapped.cpp
    kernel/guest_format.cpp
    kernel/printf.cpp
    kernel/portal.cpp
    kernel/rtl_time.cpp
    kernel/strings.cpp
    kernel/stubs.cpp
    kernel/sync.cpp
    kernel/system.cpp
    kernel/threads.cpp
    kernel/video.cpp
    kernel/xam.cpp
    kernel/xam_achievements.cpp
    kernel/xam_content.cpp
    kernel/xam_user.cpp
    kernel/xconfig.cpp
    kernel/xma.cpp
    kernel/xmp.cpp)
set_target_properties(ArkeyanRuntime PROPERTIES OUTPUT_NAME "${ARKEYAN_RUNTIME_NAME}")
if(NOT ARKEYAN_KIT_BUILD)
    if(WIN32)
        string(REPLACE "." "," ARKEYAN_GAME_VERSION_PARTS "${ARKEYAN_GAME_VERSION}.0")
        set(ARKEYAN_ICON_RESOURCE "")
        if(EXISTS "${ARKEYAN_GAME_ICON_DIR}/game_icon.ico")
            set(ARKEYAN_ICON_RESOURCE "1 ICON \"${ARKEYAN_GAME_ICON_DIR}/game_icon.ico\"")
        endif()
        configure_file(${CMAKE_SOURCE_DIR}/tools/arkeyan-builder/game.rc.in game.rc @ONLY)
        target_sources(ArkeyanRuntime PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/game.rc)
    endif()
    if(EXISTS "${ARKEYAN_GAME_ICON_DIR}/game_icon.png")
        if(APPLE)
            target_sources(ArkeyanRuntime PRIVATE "${ARKEYAN_GAME_ICON_DIR}/game_icon.png")
            set_source_files_properties("${ARKEYAN_GAME_ICON_DIR}/game_icon.png" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        else()
            add_custom_command(TARGET ArkeyanRuntime POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ARKEYAN_GAME_ICON_DIR}/game_icon.png" "$<TARGET_FILE_DIR:ArkeyanRuntime>/game_icon.png"
                VERBATIM)
        endif()
    else()
        message(STATUS "No local disc icon family: generate it with Arkeyan-Builder --icon-only; platform defaults apply")
    endif()
endif()
target_compile_definitions(ArkeyanRuntime PRIVATE ARKEYAN_VERSION="${PROJECT_VERSION}")
# The commit the runtime was configured from, for the log's first lines (host/log.cpp).
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE ARKEYAN_BUILD OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git diff --quiet HEAD -- runtime ArkeyanRecompLib WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE ARKEYAN_BUILD_DIRTY ERROR_QUIET)
if(ARKEYAN_BUILD AND ARKEYAN_BUILD_DIRTY)
    string(APPEND ARKEYAN_BUILD "-dirty")
endif()
set_source_files_properties(host/log.cpp PROPERTIES COMPILE_DEFINITIONS "ARKEYAN_BUILD=\"${ARKEYAN_BUILD}\"")

add_subdirectory(gpu)

# The game's zlib inflate runs as the host's (inflate.cpp): loads are mostly inflating. The system zlib when there is
# one (macOS, Linux), else miniz from the tree (Windows).
find_package(ZLIB QUIET)
set(ARKEYAN_MINIZ_DIR ${CMAKE_SOURCE_DIR}/thirdparty/XenosRecomp/thirdparty/smol-v/testing/external/miniz)
# miniz (public domain, from the tree; inflate.cpp's without zlib): a library of its own,
# so our warning flags stay off it, and its directory is SYSTEM for the targets that include it.
add_library(ArkeyanMiniz STATIC ${ARKEYAN_MINIZ_DIR}/miniz.c)
target_compile_definitions(ArkeyanMiniz PRIVATE MINIZ_NO_STDIO MINIZ_NO_TIME MINIZ_NO_ARCHIVE_APIS MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
target_include_directories(ArkeyanMiniz SYSTEM INTERFACE ${ARKEYAN_MINIZ_DIR})
if(ZLIB_FOUND)
    target_sources(ArkeyanRuntime PRIVATE inflate.cpp)
    target_link_libraries(ArkeyanRuntime PRIVATE ZLIB::ZLIB)
elseif(EXISTS ${ARKEYAN_MINIZ_DIR}/miniz.c)
    target_sources(ArkeyanRuntime PRIVATE inflate.cpp)
    target_link_libraries(ArkeyanRuntime PRIVATE ArkeyanMiniz)
    set_source_files_properties(inflate.cpp PROPERTIES COMPILE_DEFINITIONS ARKEYAN_INFLATE_MINIZ)
else()
    message(STATUS "No zlib and no miniz: levels are inflated by the game's own translated zlib (slower loads)")
endif()

# XMA audio (Wwise music and sound effects) is decoded with FFmpeg's libavcodec: the minimal static LGPL build of
# tools/ffmpeg/build.sh (built into the build tree by default, or ARKEYAN_FFMPEG_ROOT), else a system one through
# pkg-config. GPL/non-free FFmpeg builds are rejected at configure time.
include(${CMAKE_SOURCE_DIR}/cmake/ffmpeg.cmake)
arkeyan_setup_ffmpeg(ArkeyanRuntime)

# The engine's hooks (engines/<engine>/) and the game's adapter (games/<id>/) are compiled into the runtime: the hooks
# override the game's translated functions, the adapter answers game.h.
file(GLOB ARKEYAN_ENGINE_SOURCES CONFIGURE_DEPENDS ${ARKEYAN_ENGINE_DIR}/*.cpp)
file(GLOB ARKEYAN_GAME_SOURCES CONFIGURE_DEPENDS ${ARKEYAN_GAME_DIR}/*.cpp)
list(FILTER ARKEYAN_GAME_SOURCES EXCLUDE REGEX "_test\\.cpp$")
target_sources(ArkeyanRuntime PRIVATE ${ARKEYAN_ENGINE_SOURCES} ${ARKEYAN_GAME_SOURCES})
# A disc edition of other code (docs/tools/edition_finder.md): its own addresses.h, made from its edition report by
# tools/editions/from_report.py, found before the game's.
set(ARKEYAN_EDITION "" CACHE STRING "Media ID of a disc edition whose games/<id>/editions/<ID>/addresses.h to build with")
set(ARKEYAN_ADDRESSES ${ARKEYAN_GAME_DIR}/addresses.h)
if(ARKEYAN_EDITION)
    set(ARKEYAN_ADDRESSES ${ARKEYAN_GAME_DIR}/editions/${ARKEYAN_EDITION}/addresses.h)
    if(NOT EXISTS ${ARKEYAN_ADDRESSES})
        message(FATAL_ERROR "no ${ARKEYAN_ADDRESSES}")
    endif()
endif()
# The other runtime files reach the game through game_hooks.h only: its addresses.h is forwarded from a folder of the
# build tree, so that their compile commands are the same for every game, and tools/make_kit.sh, which builds the
# games one after the other in one tree, compiles them once.
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/game/addresses.h CONTENT "#include \"${ARKEYAN_ADDRESSES}\"\n")
target_include_directories(ArkeyanRuntime PRIVATE . ${CMAKE_CURRENT_BINARY_DIR}/game
    ${CMAKE_SOURCE_DIR}/thirdparty/XenosRecomp/thirdparty/xxHash ${CMAKE_SOURCE_DIR}/thirdparty/stb)
set(ARKEYAN_GAME_INCLUDES ${ARKEYAN_GAME_DIR} ${ARKEYAN_ENGINE_DIR})
if(ARKEYAN_EDITION)
    list(PREPEND ARKEYAN_GAME_INCLUDES ${ARKEYAN_GAME_DIR}/editions/${ARKEYAN_EDITION})
endif()
set_property(SOURCE ${ARKEYAN_ENGINE_SOURCES} ${ARKEYAN_GAME_SOURCES} APPEND PROPERTY INCLUDE_DIRECTORIES ${ARKEYAN_GAME_INCLUDES})
# Button glyphs are drawn with the embedded Bungee (embedded_fonts.cpp, below) and stb_truetype (stb_impl.cpp).
target_sources(ArkeyanRuntime PRIVATE host/stb_impl.cpp)
target_include_directories(ArkeyanRuntime SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/stb) # stb's warnings are not ours
if(ARKEYAN_UWP)
    # Xbox present test (tools/xbox/presenttest): a small separate package, built on request.
    add_executable(ArkeyanPresentTest WIN32 EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/xbox/presenttest/main.cpp host/uwp/uwp_compat.cpp)
    target_include_directories(ArkeyanPresentTest PRIVATE host/uwp)
    target_link_libraries(ArkeyanPresentTest PRIVATE plume)
    target_link_options(ArkeyanPresentTest PRIVATE -municode)
endif()
target_link_libraries(ArkeyanRuntime PRIVATE ArkeyanRecompLib ArkeyanRecompGpu ArkeyanXenonUtils fmt::fmt SDL3::SDL3-static libzstd_static)
# ppc_context.h and ppc_config.h are XenonRecomp's output: system headers here, so their warnings stay out of ours. System
# directories come after every -I one, so XenonUtils (which has its own ppc_context.h) is one too, after the game's.
target_include_directories(ArkeyanRuntime SYSTEM PRIVATE ${ARKEYAN_PPC_DIR} ${ARKEYAN_XENON_UTILS})
# The checks (SKY_TESTS_ONLY, no game data): the runtime and the tests that use the guest's context are not built, but
# clang-tidy parses their sources from the compile database; tidy/ppc_config.h stands in for the generated one.
function(arkeyan_tidy_standin target)
    if(SKY_TESTS_ONLY AND NOT EXISTS ${ARKEYAN_PPC_DIR}/ppc_config.h)
        target_include_directories(${target} SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tidy)
    endif()
endfunction()
arkeyan_tidy_standin(ArkeyanRuntime)
# ARKEYAN_LAYOUT_RECORD: layout/ppc_config.h, before ppc/, gives runtime sources the four bounds from the active layout;
# guest_layout_record.cpp builds the record from the generated constants.
function(arkeyan_layout_record target)
    if(ARKEYAN_LAYOUT_RECORD)
        target_sources(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/guest_layout.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/guest_layout_record.cpp)
        target_compile_definitions(${target} PRIVATE ARKEYAN_LAYOUT_RECORD)
        target_include_directories(${target} SYSTEM BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/layout)
    endif()
endfunction()
arkeyan_layout_record(ArkeyanRuntime)
# Linux's C library has its own byteswap.h, found before a SYSTEM directory. The executable's functions go in its
# dynamic symbol table so that a fault report names the recompiled function (dladdr), as on macOS.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(ArkeyanRuntime PRIVATE "SHELL:-iquote ${ARKEYAN_XENON_UTILS}")
    set_target_properties(ArkeyanRuntime PROPERTIES ENABLE_EXPORTS ON)
endif()

# The Toy Box overlay: Dear ImGui (MIT) rasterized on the CPU by host/imgui_software.cpp (every platform, the Xbox
# too), with Outfit and Bungee built into the executable.
set(IMGUI_ROOT ${CMAKE_SOURCE_DIR}/thirdparty/imgui)
add_library(imgui STATIC ${IMGUI_ROOT}/imgui.cpp ${IMGUI_ROOT}/imgui_draw.cpp ${IMGUI_ROOT}/imgui_tables.cpp
    ${IMGUI_ROOT}/imgui_widgets.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_ROOT})
# Outfit's variable font defaults to its Thin weight, which stb_truetype renders as is: the Toy Box uses static
# SemiBold and Bold instances (fontTools varLib.instancer wght=600/700 --update-name-table).
set(ARKEYAN_OUTFIT_BOLD ${CMAKE_SOURCE_DIR}/resources/toybox/fonts/Outfit-Bold.ttf)
set(ARKEYAN_OUTFIT_SEMIBOLD ${CMAKE_SOURCE_DIR}/resources/toybox/fonts/Outfit-SemiBold.ttf)
set(ARKEYAN_BUNGEE ${CMAKE_SOURCE_DIR}/resources/launcher/fonts/Bungee-Regular.ttf)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/embedded_fonts.cpp
    COMMAND ${CMAKE_COMMAND} -DOUTFIT_BOLD=${ARKEYAN_OUTFIT_BOLD} -DOUTFIT_SEMIBOLD=${ARKEYAN_OUTFIT_SEMIBOLD} -DBUNGEE=${ARKEYAN_BUNGEE}
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/embedded_fonts.cpp -P ${CMAKE_SOURCE_DIR}/cmake/embed_fonts.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/cmake/embed_fonts.cmake ${ARKEYAN_OUTFIT_BOLD} ${ARKEYAN_OUTFIT_SEMIBOLD} ${ARKEYAN_BUNGEE}
    COMMENT "Embedding the Toy Box fonts")
target_sources(ArkeyanRuntime PRIVATE host/imgui_software.cpp ${CMAKE_CURRENT_BINARY_DIR}/embedded_fonts.cpp)
target_link_libraries(ArkeyanRuntime PRIVATE imgui)

# CPU-only diagnostic overlay check; synthetic measurements, no GPU device or game is started.
add_executable(ArkeyanDebugScreenTest host/gamemenu_debug_test.cpp host/gamemenu_debug.cpp host/imgui_software.cpp
    gpu/frame_stats.cpp ${CMAKE_CURRENT_BINARY_DIR}/embedded_fonts.cpp)
target_include_directories(ArkeyanDebugScreenTest PRIVATE . ${CMAKE_SOURCE_DIR}/thirdparty/plume)
target_compile_definitions(ArkeyanDebugScreenTest PRIVATE ARKEYAN_VERSION="${PROJECT_VERSION}")
target_link_libraries(ArkeyanDebugScreenTest PRIVATE imgui SDL3::SDL3-static)
if(WIN32)
    target_compile_definitions(ArkeyanDebugScreenTest PRIVATE _WIN32_WINNT=0x0A00)
endif()

# The CPU ImGui renderer at a framebuffer scale (the Toy Box on high-density screens): ./ArkeyanImguiSoftwareTest
add_executable(ArkeyanImguiSoftwareTest host/imgui_software_test.cpp host/imgui_software.cpp)
target_include_directories(ArkeyanImguiSoftwareTest PRIVATE .)
target_link_libraries(ArkeyanImguiSoftwareTest PRIVATE imgui)

# Launcher checks without UI: ArkeyanRuntimeLauncherTest <game disc .iso> <scratch dir> [<damaged copy>...]
add_executable(ArkeyanRuntimeLauncherTest EXCLUDE_FROM_ALL host/launcher_test.cpp host/launcher.cpp host/launcher_games.cpp host/portable.cpp host/xex_title.cpp ${ARKEYAN_GAME_DIR}/info.cpp)
target_include_directories(ArkeyanRuntimeLauncherTest PRIVATE .)
target_link_libraries(ArkeyanRuntimeLauncherTest PRIVATE ArkeyanHostFiles ArkeyanXenonUtils fmt::fmt SDL3::SDL3-static)

# The F3 menu's Commands page on a scratch launcher.ini: keys, pad buttons, swaps, reset (no game data): ./ArkeyanCommandsTest
# The launcher.ini reader and writer come with ArkeyanRuntimeLauncherTest's sources (its test file aside).
get_target_property(ARKEYAN_COMMANDS_TEST_SOURCES ArkeyanRuntimeLauncherTest SOURCES)
list(REMOVE_ITEM ARKEYAN_COMMANDS_TEST_SOURCES host/launcher_test.cpp)
add_executable(ArkeyanCommandsTest host/commands_test.cpp host/commands.cpp host/settings.cpp host/window_size.cpp host/pad_map.cpp host/toybox/pad_bindings.cpp host/mouse.cpp
    ${ARKEYAN_COMMANDS_TEST_SOURCES})
target_include_directories(ArkeyanCommandsTest PRIVATE .)
target_link_libraries(ArkeyanCommandsTest PRIVATE ArkeyanHostFiles ArkeyanXenonUtils fmt::fmt SDL3::SDL3-static)

# DDS reader and writer, content hash and texture pack index (no game data needed): ./ArkeyanTexturePackTest
add_executable(ArkeyanTexturePackTest EXCLUDE_FROM_ALL host/texture_pack_test.cpp host/dds.cpp host/texture_pack.cpp)
target_include_directories(ArkeyanTexturePackTest PRIVATE . ${CMAKE_SOURCE_DIR}/thirdparty/XenosRecomp/thirdparty/xxHash)
target_link_libraries(ArkeyanTexturePackTest PRIVATE ArkeyanArtReader)

# The guest's printf (flags, width, precision, the Microsoft conversions; no game data needed): ./ArkeyanGuestFormatTest
add_executable(ArkeyanGuestFormatTest kernel/guest_format_test.cpp kernel/guest_format.cpp)
target_include_directories(ArkeyanGuestFormatTest PRIVATE .)
target_link_libraries(ArkeyanGuestFormatTest PRIVATE ArkeyanXenonUtils)

# Physical pad shortcuts, deterministic timing and remap targets (no game data).
add_executable(ArkeyanToyboxPadBindingsTest host/toybox/pad_bindings_test.cpp host/toybox/pad_bindings.cpp host/pad_map.cpp)
target_include_directories(ArkeyanToyboxPadBindingsTest PRIVATE .)

# The in-game interface's toast motion and F3 menu layout (host/toybox/look.h): ./ArkeyanToyboxLookTest
add_executable(ArkeyanToyboxLookTest host/toybox/look_test.cpp)
target_include_directories(ArkeyanToyboxLookTest PRIVATE .)

# When an ask for a seat may mark its figure out of health (host/toybox/in_play.h): ./ArkeyanToyboxInPlayTest
add_executable(ArkeyanToyboxInPlayTest host/toybox/in_play_test.cpp)
target_include_directories(ArkeyanToyboxInPlayTest PRIVATE .)

# The copies of real toys in the Toy Box: the side the question focuses, one question at a time (host/toybox/real_copy.h): ./ArkeyanToyboxRealCopyTest
add_executable(ArkeyanToyboxRealCopyTest host/toybox/real_copy_test.cpp)
target_include_directories(ArkeyanToyboxRealCopyTest PRIVATE .)

# Which refusal a seat gives a card, for the toast and the seats a drag greys out (host/toybox/put_on.h): ./ArkeyanToyboxPutOnTest
add_executable(ArkeyanToyboxPutOnTest host/toybox/put_on_test.cpp)
target_include_directories(ArkeyanToyboxPutOnTest PRIVATE .)
# What the game shows of an online session, banner and chip (host/toybox/online_banner.h): ./ArkeyanToyboxOnlineBannerTest
add_executable(ArkeyanToyboxOnlineBannerTest host/toybox/online_banner_test.cpp)
target_include_directories(ArkeyanToyboxOnlineBannerTest PRIVATE .)
target_link_libraries(ArkeyanToyboxOnlineBannerTest PRIVATE ArkeyanNetplay)

# The section strip's chips wrapping onto rows (host/toybox/strip.h): ./ArkeyanToyboxStripTest
add_executable(ArkeyanToyboxStripTest host/toybox/strip_test.cpp)
target_include_directories(ArkeyanToyboxStripTest PRIVATE .)

# The keys the game's bindings see around the Toy Box and the F3 menu (host/toybox/key_mask.h): ./ArkeyanToyboxKeyMaskTest
add_executable(ArkeyanToyboxKeyMaskTest host/toybox/key_mask_test.cpp)
target_include_directories(ArkeyanToyboxKeyMaskTest PRIVATE .)
target_link_libraries(ArkeyanToyboxKeyMaskTest PRIVATE SDL3::SDL3-static)

# The stick keys aiming the quick swap wheel (host/toybox/wheel_keys.h): ./ArkeyanToyboxWheelKeysTest
add_executable(ArkeyanToyboxWheelKeysTest host/toybox/wheel_keys_test.cpp)
target_include_directories(ArkeyanToyboxWheelKeysTest PRIVATE .)
target_link_libraries(ArkeyanToyboxWheelKeysTest PRIVATE SDL3::SDL3-static)

# The Toy Box grids' scroll: the least that shows the cursor, a section's header at the top (host/toybox/grid.h): ./ArkeyanToyboxGridTest
add_executable(ArkeyanToyboxGridTest host/toybox/grid_test.cpp host/toybox/grid.cpp)
target_include_directories(ArkeyanToyboxGridTest PRIVATE .)

# Atomic virtual figure and swapper replacements (no device or figure files): ./ArkeyanToyboxSwapTest
add_executable(ArkeyanToyboxSwapTest host/toybox/swap_test.cpp host/portal_prompt.cpp)
target_include_directories(ArkeyanToyboxSwapTest PRIVATE .)

# A trap's Villain row (host/toybox/trap_villain.h): the choices of each element, the write, the evolved switch, in a
# scratch collection (no game data): ./ArkeyanToyboxTrapTest
add_executable(ArkeyanToyboxTrapTest host/toybox/trap_villain_test.cpp host/toybox/trap_villain.cpp)
target_include_directories(ArkeyanToyboxTrapTest PRIVATE .)
target_link_libraries(ArkeyanToyboxTrapTest PRIVATE ArkeyanArtReader)

# The game Add a figure's versions panel names for each version (host/toybox/version_game.h): ./ArkeyanToyboxVersionGameTest
add_executable(ArkeyanToyboxVersionGameTest host/toybox/version_game_test.cpp host/toybox/version_game.cpp)
target_include_directories(ArkeyanToyboxVersionGameTest PRIVATE .)
target_link_libraries(ArkeyanToyboxVersionGameTest PRIVATE ArkeyanArtReader)

# Add a figure for a seat: the toys of its families, then a row that shows every toy (host/toybox/add_list.h): ./ArkeyanToyboxAddListTest
add_executable(ArkeyanToyboxAddListTest host/toybox/add_list_test.cpp host/toybox/add_list.cpp)
target_include_directories(ArkeyanToyboxAddListTest PRIVATE .)
target_link_libraries(ArkeyanToyboxAddListTest PRIVATE ArkeyanArtReader)

# The remembered "resolution scale auto" step-down's file line: ./ArkeyanAutoScaleTest
add_executable(ArkeyanAutoScaleTest gpu/auto_scale_test.cpp)
target_include_directories(ArkeyanAutoScaleTest PRIVATE .)

# The depth write-back's tile-by-tile offsets against xenos::Tiled2D: ./ArkeyanTiledDepthTest
add_executable(ArkeyanTiledDepthTest d3d/tiled_depth_test.cpp d3d/xenos.cpp)
target_include_directories(ArkeyanTiledDepthTest PRIVATE .)
target_link_libraries(ArkeyanTiledDepthTest PRIVATE plume ArkeyanXenonUtils SDL3::SDL3-static)

# The colour write-back's packing (a resolve the CPU reads, put in guest memory as the 360 stores it): ./ArkeyanResolveWritebackTest
add_executable(ArkeyanResolveWritebackTest d3d/resolve_writeback_test.cpp d3d/resolve_writeback.cpp d3d/xenos.cpp)
target_include_directories(ArkeyanResolveWritebackTest PRIVATE .)
target_link_libraries(ArkeyanResolveWritebackTest PRIVATE plume ArkeyanXenonUtils SDL3::SDL3-static)

# A read-only resource lock's cached-window address mapped to the allocation's window: ./ArkeyanResourceLockTest
add_executable(ArkeyanResourceLockTest d3d/resource_lock_test.cpp)
target_include_directories(ArkeyanResourceLockTest PRIVATE .)

# The write watch (faults on watched guest pages, the stamps, the cool-down): ./ArkeyanWriteWatchTest
add_executable(ArkeyanWriteWatchTest write_watch_test.cpp write_watch.cpp)
target_include_directories(ArkeyanWriteWatchTest PRIVATE .)
target_link_libraries(ArkeyanWriteWatchTest PRIVATE ArkeyanHostFiles)

# A file read that a passing failure does not cut short (read_fully.h, issue #26): ./ArkeyanReadFullyTest
add_executable(ArkeyanReadFullyTest read_fully_test.cpp)
target_include_directories(ArkeyanReadFullyTest PRIVATE .)

# Which textures the bindless heaps bind through their own view (cube maps, arrays, swizzles): ./ArkeyanHeapViewTest
add_executable(ArkeyanHeapViewTest gpu/heap_view_test.cpp)
target_include_directories(ArkeyanHeapViewTest PRIVATE .)

# The perf log's clock counts nanoseconds: ./ArkeyanClockTest
add_executable(ArkeyanClockTest platform/clock_test.cpp)
target_include_directories(ArkeyanClockTest PRIVATE .)

add_executable(ArkeyanJmpBufTest platform/jmpbuf_test.cpp platform/jmpbuf.cpp)
target_include_directories(ArkeyanJmpBufTest PRIVATE .)
sky_add_tests(ArkeyanJmpBufTest)

# Read-backs kept by request, for online play's depth write-back: ./ArkeyanReadbackQueueTest
add_executable(ArkeyanReadbackQueueTest gpu/readback_queue_test.cpp)
target_include_directories(ArkeyanReadbackQueueTest PRIVATE .)
sky_add_tests(ArkeyanReadbackQueueTest)

# A fault report names the recompiled function it stopped in (dladdr finds sub_XXXXXXXX): ./ArkeyanCrashSymbolsTest
if(NOT WIN32)
    add_executable(ArkeyanCrashSymbolsTest platform/crash_symbols_test.cpp)
    set_target_properties(ArkeyanCrashSymbolsTest PROPERTIES ENABLE_EXPORTS ON)
endif()

# The buffer cache's range hash (chunks shared by overlapping ranges): ./ArkeyanRangeHashTest
add_executable(ArkeyanRangeHashTest gpu/range_hash_test.cpp)
target_include_directories(ArkeyanRangeHashTest PRIVATE . ${CMAKE_SOURCE_DIR}/thirdparty/XenosRecomp/thirdparty/xxHash)

# Guest indices in host order, 16-bit and 32-bit: ./ArkeyanIndexConvertTest
add_executable(ArkeyanIndexConvertTest gpu/index_convert_test.cpp)
target_include_directories(ArkeyanIndexConvertTest PRIVATE .)

# 16-bit vertex component pairs back in order after the 32-bit swap: ./ArkeyanVertexPairsTest
add_executable(ArkeyanVertexPairsTest d3d/vertex_pairs_test.cpp)
target_include_directories(ArkeyanVertexPairsTest PRIVATE .)

# Where a HUD draw goes in the wide frame (a corner piece to the window's corner): ./ArkeyanFrameAnchorTest
add_executable(ArkeyanFrameAnchorTest d3d/frame_anchor_test.cpp)
target_include_directories(ArkeyanFrameAnchorTest PRIVATE .)

# Render target slots of one draw that share an EDRAM key but not a surface: ./ArkeyanMrtAliasTest
add_executable(ArkeyanMrtAliasTest d3d/mrt_alias_test.cpp)
target_include_directories(ArkeyanMrtAliasTest PRIVATE .)

# The write masks of the render targets a pixel shader does not write: ./ArkeyanTargetWritesTest
add_executable(ArkeyanTargetWritesTest d3d/target_writes_test.cpp)
target_include_directories(ArkeyanTargetWritesTest PRIVATE .)

# The frame dump's float scan (NaN, infinities, negatives, denormals): ./ArkeyanFloatScanTest
add_executable(ArkeyanFloatScanTest d3d/float_scan_test.cpp)
target_include_directories(ArkeyanFloatScanTest PRIVATE .)

# Empty and malformed movie frames must be black and terminal: ./ArkeyanBinkFrameTest
add_executable(ArkeyanBinkFrameTest d3d/bink_frame_test.cpp)
target_include_directories(ArkeyanBinkFrameTest PRIVATE .)

# The actual Bink hooks with synthetic guest calls, no game code or GPU: ./ArkeyanBinkDisplayTest
add_executable(ArkeyanBinkDisplayTest d3d/bink_display_test.cpp d3d/bink_display.cpp memory.cpp)
target_include_directories(ArkeyanBinkDisplayTest PRIVATE d3d/tests/include .)
target_include_directories(ArkeyanBinkDisplayTest SYSTEM PRIVATE
    ${ARKEYAN_XENON_UTILS} ${ARKEYAN_XENON_THIRDPARTY}/simde)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(ArkeyanBinkDisplayTest PRIVATE "SHELL:-iquote ${ARKEYAN_XENON_UTILS}")
endif()

# The layout record (#72): parsed envelopes, the checks, table entries and ranges equal to the constant path, thunk slots
# in gaps, the lookup cost both ways. Invented layouts, no game code: ./ArkeyanGuestLayoutTest
add_executable(ArkeyanGuestLayoutTest guest_layout_test.cpp guest_layout.cpp)
target_include_directories(ArkeyanGuestLayoutTest PRIVATE .)
target_include_directories(ArkeyanGuestLayoutTest SYSTEM PRIVATE ${ARKEYAN_XENON_UTILS} ${ARKEYAN_XENON_THIRDPARTY}/simde)

# Where host thunks go in the code's padding: ./ArkeyanThunkSlotTest
add_executable(ArkeyanThunkSlotTest thunk_slot_test.cpp)
target_include_directories(ArkeyanThunkSlotTest PRIVATE .)

# When a guest yield backs off, and the scope that keeps a working job from it, with a fake clock: ./ArkeyanYieldBackOffTest
add_executable(ArkeyanYieldBackOffTest kernel/yield_backoff_test.cpp)
target_include_directories(ArkeyanYieldBackOffTest PRIVATE .)

# When the stall report fires (a game swapping on without drawing), with a fake clock: ./ArkeyanStallWatchTest
add_executable(ArkeyanStallWatchTest d3d/stall_watch_test.cpp)
target_include_directories(ArkeyanStallWatchTest PRIVATE .)

# What MmQueryAddressProtect answers for physical memory (its cache attributes): ./ArkeyanAddressProtectTest
add_executable(ArkeyanAddressProtectTest kernel/address_protect_test.cpp)
target_include_directories(ArkeyanAddressProtectTest PRIVATE .)

# The frame rate limiter's schedule and wait, with a fake clock: ./ArkeyanFrameLimiterTest
add_executable(ArkeyanFrameLimiterTest kernel/frame_limiter_test.cpp)
target_include_directories(ArkeyanFrameLimiterTest PRIVATE .)

# The spoken-line skip: which presses close a line, the cinematic played fast (engines/lux/dialogue_skip.h; no game
# data): ./ArkeyanDialogueSkipTest
add_executable(ArkeyanDialogueSkipTest ${CMAKE_SOURCE_DIR}/engines/lux/tests/dialogue_skip_test.cpp)
target_include_directories(ArkeyanDialogueSkipTest PRIVATE ${CMAKE_SOURCE_DIR}/engines/lux)

# The texts that name the Start button, shown as its picture (engines/alchemy/prompt_text.h; no game data): ./ArkeyanPromptTextTest
add_executable(ArkeyanPromptTextTest ${CMAKE_SOURCE_DIR}/engines/alchemy/tests/prompt_text_test.cpp)
target_include_directories(ArkeyanPromptTextTest PRIVATE ${CMAKE_SOURCE_DIR}/engines/alchemy)

# SKY_LEVEL's file redirect, the first level or SKY_LEVEL_FROM's (kernel/level_redirect.h; no game data): ./ArkeyanLevelRedirectTest
add_executable(ArkeyanLevelRedirectTest kernel/level_redirect_test.cpp)
target_include_directories(ArkeyanLevelRedirectTest PRIVATE .)

# The Toy Box script's portal items, place= and remove= (host/portal_script.h; no game data): ./ArkeyanPortalScriptTest
add_executable(ArkeyanPortalScriptTest host/portal_script_test.cpp)
target_include_directories(ArkeyanPortalScriptTest PRIVATE .)

# SKY_LEVEL's zone name for the zone-request games, a map or a full zone path (engines/alchemy/zone_name.h): ./ArkeyanZoneNameTest
add_executable(ArkeyanZoneNameTest ${CMAKE_SOURCE_DIR}/engines/alchemy/tests/zone_name_test.cpp)
target_include_directories(ArkeyanZoneNameTest PRIVATE ${CMAKE_SOURCE_DIR}/engines/alchemy)

# The difficulty menu's Nightmare answer for a lock kept in a script variable, the save left alone
# (engines/alchemy/nightmare_gate.h; no game data): ./ArkeyanNightmareGateTest
add_executable(ArkeyanNightmareGateTest ${CMAKE_SOURCE_DIR}/engines/alchemy/tests/nightmare_gate_test.cpp)
target_include_directories(ArkeyanNightmareGateTest PRIVATE ${CMAKE_SOURCE_DIR}/engines/alchemy)

# Which TLSF blocks belong to their pool as it is now, the stale free of SuperChargers' race start dropped
# (engines/alchemy/live_blocks.h; no game data): ./ArkeyanLiveBlocksTest
add_executable(ArkeyanLiveBlocksTest ${CMAKE_SOURCE_DIR}/engines/alchemy/tests/live_blocks_test.cpp)
target_include_directories(ArkeyanLiveBlocksTest PRIVATE ${CMAKE_SOURCE_DIR}/engines/alchemy)

# The models that show a button over a glyph texture (host/glyphs.h, Model: the letters skipped, the texture matrix): ./ArkeyanGlyphModelsTest
add_executable(ArkeyanGlyphModelsTest host/glyph_models_test.cpp host/glyph_models.cpp host/glyph_assets.cpp
    host/art/alchemy_archive.cpp host/art/lux_images.cpp host/art/xenos_texture.cpp)
target_include_directories(ArkeyanGlyphModelsTest PRIVATE .)
target_include_directories(ArkeyanGlyphModelsTest PRIVATE ${CMAKE_SOURCE_DIR}/engines/alchemy)
target_include_directories(ArkeyanGlyphModelsTest SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/XenosRecomp/thirdparty/xxHash)

# The players' profiles (host/profiles.h): the environment, the names the fonts draw, the gamer picture tile (a PNG made
# in memory; no game data): ./ArkeyanProfilesTest
add_executable(ArkeyanProfilesTest host/profiles_test.cpp host/profiles.cpp host/stb_impl.cpp)
target_include_directories(ArkeyanProfilesTest PRIVATE .)
target_include_directories(ArkeyanProfilesTest SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/stb)

# The controllers as the title reads them (host/input.h; no SDL, no game data): ./ArkeyanInputTest
add_executable(ArkeyanInputTest host/input_test.cpp host/input.cpp)
target_include_directories(ArkeyanInputTest PRIVATE .)

# The mouse's names, bindings and capture (host/mouse.h; no SDL, no game data): ./ArkeyanMouseTest
add_executable(ArkeyanMouseTest host/mouse_test.cpp host/mouse.cpp)
target_include_directories(ArkeyanMouseTest PRIVATE .)

# The game window's size (host/window_size.h; no SDL, no game data): ./ArkeyanWindowSizeTest
add_executable(ArkeyanWindowSizeTest host/window_size_test.cpp host/window_size.cpp)
target_include_directories(ArkeyanWindowSizeTest PRIVATE .)

# Settings parser test (launcher.ini, environment, command line; no game data needed): ./ArkeyanSettingsTest
add_executable(ArkeyanSettingsTest host/settings_test.cpp host/settings.cpp host/window_size.cpp host/pad_map.cpp host/toybox/pad_bindings.cpp host/mouse.cpp)
target_include_directories(ArkeyanSettingsTest PRIVATE .)
target_link_libraries(ArkeyanSettingsTest PRIVATE fmt::fmt SDL3::SDL3-static)
# The console's settings file (host/uwp/settings_file.cpp): defaults, the game's own section. ./ArkeyanUwpSettingsTest
add_executable(ArkeyanUwpSettingsTest host/uwp/settings_file_test.cpp host/uwp/settings_file.cpp host/settings.cpp host/window_size.cpp host/pad_map.cpp host/toybox/pad_bindings.cpp host/mouse.cpp)
target_include_directories(ArkeyanUwpSettingsTest PRIVATE .)
target_link_libraries(ArkeyanUwpSettingsTest PRIVATE fmt::fmt SDL3::SDL3-static)
# The game's options both hosts apply (host/game_options.cpp): FPS cap, languages, Nightmare. ./ArkeyanGameOptionsTest
add_executable(ArkeyanGameOptionsTest host/game_options_test.cpp host/game_options.cpp)
target_include_directories(ArkeyanGameOptionsTest PRIVATE .)
target_link_libraries(ArkeyanGameOptionsTest PRIVATE fmt::fmt SDL3::SDL3-static)

# Fonts are data, not game content: Outfit and Bungee (SIL Open Font License, licence files next to them). On macOS
# they live in the app bundle's Resources/fonts (SDL_GetBasePath returns that directory); elsewhere they are copied
# next to the executable.
set(ARKEYAN_FONT_FILES
    ${CMAKE_SOURCE_DIR}/resources/launcher/fonts/Outfit-Variable.ttf
    ${CMAKE_SOURCE_DIR}/resources/launcher/fonts/Bungee-Regular.ttf
    ${CMAKE_SOURCE_DIR}/resources/launcher/fonts/OFL-Outfit.txt
    ${CMAKE_SOURCE_DIR}/resources/launcher/fonts/OFL-Bungee.txt)
if(NOT ARKEYAN_UWP)
    if(APPLE)
        set_source_files_properties(${ARKEYAN_FONT_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/fonts)
        target_sources(ArkeyanRuntime PRIVATE ${ARKEYAN_FONT_FILES})
    else()
        add_custom_command(TARGET ArkeyanRuntime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources/launcher/fonts $<TARGET_FILE_DIR:ArkeyanRuntime>/fonts)
    endif()
endif()

if(APPLE)
    # Double-clickable game bundle, ad-hoc signed. A sibling link points to its executable
    # so command-line runs keep working.
    set(ARKEYAN_MACOS_MINIMUM ${CMAKE_OSX_DEPLOYMENT_TARGET})
    if(NOT ARKEYAN_MACOS_MINIMUM)
        # Without a deployment target the compiler targets the build machine's macOS major release.
        execute_process(COMMAND sw_vers -productVersion OUTPUT_VARIABLE ARKEYAN_MACOS_MINIMUM OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX REPLACE "^([0-9]+).*" "\\1.0" ARKEYAN_MACOS_MINIMUM "${ARKEYAN_MACOS_MINIMUM}")
    endif()

    set(ARKEYAN_ICON ${CMAKE_SOURCE_DIR}/resources/macos/ArkeyanRuntime.icns)
    if(EXISTS "${ARKEYAN_GAME_ICON_DIR}/game_icon.icns" AND NOT ARKEYAN_KIT_BUILD)
        set(ARKEYAN_ICON "${ARKEYAN_GAME_ICON_DIR}/game_icon.icns")
    endif()
    target_sources(ArkeyanRuntime PRIVATE ${ARKEYAN_ICON})
    set_source_files_properties(${ARKEYAN_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    # Ship the third-party notices in the bundle; listing it as a bundle resource keeps them in sync with the target.
    target_sources(ArkeyanRuntime PRIVATE ${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    get_filename_component(ARKEYAN_ICON_FILE "${ARKEYAN_ICON}" NAME)
    # macOS keys the Dock's icon on the identifier and the bundle version: a player's game carries its own version (an update
    # gets its icon again), and a local build (no kit) another identifier, so a test build's neutral icon never stands in
    # for the player's game.
    set(ARKEYAN_BUNDLE_ID_SUFFIX "")
    if(NOT ARKEYAN_KIT_BUILD)
        set(ARKEYAN_BUNDLE_ID_SUFFIX ".dev")
    endif()
    set_target_properties(ArkeyanRuntime PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/resources/macos/Info.plist.in
        MACOSX_BUNDLE_BUNDLE_NAME "${ARKEYAN_RUNTIME_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER org.ssarecomp.${ARKEYAN_BUNDLE_ID}${ARKEYAN_BUNDLE_ID_SUFFIX}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${ARKEYAN_GAME_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${ARKEYAN_GAME_VERSION}
        MACOSX_BUNDLE_ICON_FILE "${ARKEYAN_ICON_FILE}")
    target_link_libraries(ArkeyanRuntime PRIVATE "-framework CoreGraphics" "-framework IOKit" "-framework CoreFoundation")
    target_sources(ArkeyanRuntime PRIVATE host/portal_usb_mac.cpp) # Xbox portals through IOKit (no libusb on a Mac)
    add_custom_command(TARGET ArkeyanRuntime POST_BUILD
        COMMAND codesign --force --sign - --timestamp=none $<TARGET_BUNDLE_DIR:ArkeyanRuntime>
        COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_BINARY_DIR}/${ARKEYAN_RUNTIME_NAME}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink "${ARKEYAN_RUNTIME_NAME}.app/Contents/MacOS/${ARKEYAN_RUNTIME_NAME}" "${CMAKE_CURRENT_BINARY_DIR}/${ARKEYAN_RUNTIME_NAME}"
        VERBATIM)
endif()

# Remove only this game's old output after a successful rebuild; the target and identifier stay stable.
if(NOT ARKEYAN_RUNTIME_NAME STREQUAL ARKEYAN_BUNDLE_ID)
    if(APPLE)
        add_custom_command(TARGET ArkeyanRuntime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_CURRENT_BINARY_DIR}/${ARKEYAN_BUNDLE_ID}.app"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_BINARY_DIR}/${ARKEYAN_BUNDLE_ID}"
            VERBATIM)
    else()
        add_custom_command(TARGET ArkeyanRuntime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_BINARY_DIR}/${ARKEYAN_BUNDLE_ID}${CMAKE_EXECUTABLE_SUFFIX}"
            VERBATIM)
    endif()
endif()

add_executable(ArkeyanRenderScaleTest d3d/render_scale_test.cpp)

# A texture pack replacement's mip range and the debug tint: ./ArkeyanPackSamplingTest
add_executable(ArkeyanPackSamplingTest d3d/pack_sampling_test.cpp)
target_include_directories(ArkeyanPackSamplingTest PRIVATE .)

# The wide scene and frame sizes for a window's shape: ./ArkeyanUltrawideTest
add_executable(ArkeyanUltrawideTest d3d/ultrawide_test.cpp d3d/ultrawide.cpp)
target_include_directories(ArkeyanUltrawideTest PRIVATE . ${ARKEYAN_GAME_DIR})

# Upscaling: the jitter sequence, the camera vote (d3d/upscale_math.h), the settings and scales (gpu/upscale_mode.h),
# the SPIR-V reflection of FSR's backend (gpu/fsr/spirv_reflect.h): ./ArkeyanUpscaleTest
add_executable(ArkeyanUpscaleTest d3d/upscale_test.cpp gpu/fsr/spirv_reflect.cpp)
target_include_directories(ArkeyanUpscaleTest PRIVATE .)

# Frame generation's rules: when a frame is made, the pacing (gpu/fsr/framegen_rules.h): ./ArkeyanFrameGenTest
add_executable(ArkeyanFrameGenTest gpu/fsr/framegen_test.cpp)
target_include_directories(ArkeyanFrameGenTest PRIVATE .)

# What a failed swap chain acquire or present leads to (gpu/swapchain_recovery.h): ./ArkeyanSwapChainRecoveryTest
add_executable(ArkeyanSwapChainRecoveryTest gpu/swapchain_recovery_test.cpp)
target_include_directories(ArkeyanSwapChainRecoveryTest PRIVATE .)

# Desktop Windows' graphics API: the choice and the fallback (gpu/gpu_backend.h): ./ArkeyanGpuBackendTest
add_executable(ArkeyanGpuBackendTest gpu/gpu_backend_test.cpp)
target_include_directories(ArkeyanGpuBackendTest PRIVATE .)

# The vsync mode for a refresh rate and the display cadence: ./ArkeyanPresentModeTest
add_executable(ArkeyanPresentModeTest gpu/present_mode_test.cpp)
target_include_directories(ArkeyanPresentModeTest PRIVATE .)

# Where the present pass draws the frame, letterboxed or filled: ./ArkeyanPresentRectTest
add_executable(ArkeyanPresentRectTest gpu/present_rect_test.cpp)
target_include_directories(ArkeyanPresentRectTest PRIVATE .)

# The automatic 60 -> 30 policy on recorded and made-up frame sequences: ./ArkeyanFrameRateFallbackTest
add_executable(ArkeyanFrameRateFallbackTest host/settings_fallback_test.cpp)
target_include_directories(ArkeyanFrameRateFallbackTest PRIVATE .)
# Direct3D 12's first frames: when none reaches the screen, Vulkan (gpu/first_frame.h): ./ArkeyanFirstFrameTest
add_executable(ArkeyanFirstFrameTest gpu/first_frame_test.cpp)
target_include_directories(ArkeyanFirstFrameTest PRIVATE .)
# SDL video selection and Linux native driver retries, without SDL or game data: ./ArkeyanVideoDriverTest
add_executable(ArkeyanVideoDriverTest host/video_driver_test.cpp)
target_include_directories(ArkeyanVideoDriverTest PRIVATE .)
# The SDL hints set at startup (Steam's on-screen keyboard off, the portals ignored by the gamepad code): ./ArkeyanSdlHintsTest
add_executable(ArkeyanSdlHintsTest host/sdl_hints_test.cpp host/sdl_hints.cpp)
target_include_directories(ArkeyanSdlHintsTest PRIVATE .)
target_link_libraries(ArkeyanSdlHintsTest PRIVATE SDL3::SDL3-static)

# The pacer of per-frame simulation steps, with a fake clock: ./ArkeyanStepPacerTest
add_executable(ArkeyanStepPacerTest d3d/step_pacer_test.cpp)
target_include_directories(ArkeyanStepPacerTest PRIVATE .)

# The ring scan's register file: a run of writes as the same writes one at a time: ./ArkeyanRingRegistersTest
add_executable(ArkeyanRingRegistersTest d3d/ring_registers_test.cpp)
target_include_directories(ArkeyanRingRegistersTest PRIVATE .)

# Where the PM4 ring scan reads from across command buffer detours (issue #39): ./ArkeyanRingCursorTest
add_executable(ArkeyanRingCursorTest d3d/ring_cursor_test.cpp)
target_include_directories(ArkeyanRingCursorTest PRIVATE .)

# The pointer in Spyro's Adventure's menus (games/ssa/menu_pointer.h; no game data): ./ArkeyanSsaMenuTest
add_executable(ArkeyanSsaMenuTest ${CMAKE_SOURCE_DIR}/games/ssa/menu_pointer_test.cpp ${CMAKE_SOURCE_DIR}/games/ssa/menu_pointer.cpp)

# The pointer in the Alchemy games' menus (engines/alchemy/menu_pointer.h; no game data): ./ArkeyanAlchemyMenuTest
add_executable(ArkeyanAlchemyMenuTest ${CMAKE_SOURCE_DIR}/engines/alchemy/tests/menu_pointer_test.cpp ${CMAKE_SOURCE_DIR}/engines/alchemy/menu_pointer.cpp)
target_include_directories(ArkeyanAlchemyMenuTest PRIVATE ${CMAKE_SOURCE_DIR}/engines/alchemy)

# The Juice menus as the pointer reads them (engines/alchemy/juice_menu.h; no game data): ./ArkeyanJuiceMenuTest
add_executable(ArkeyanJuiceMenuTest ${CMAKE_SOURCE_DIR}/engines/alchemy/tests/juice_menu_test.cpp ${CMAKE_SOURCE_DIR}/engines/alchemy/juice_menu.cpp)
target_include_directories(ArkeyanJuiceMenuTest PRIVATE ${CMAKE_SOURCE_DIR}/engines/alchemy)

# Protocol test of the emulated Skylanders portal (no game data needed): ./ArkeyanPortalTest
add_executable(ArkeyanPortalTest host/portal_emulated.cpp host/portal_mixed.cpp host/portal_identity.cpp host/portal_prompt.cpp ${CMAKE_SOURCE_DIR}/games/ssa/ssa_portal_prompt.cpp ${CMAKE_SOURCE_DIR}/games/giants/giants_portal_prompt.cpp ${CMAKE_SOURCE_DIR}/games/swapforce/swapforce_portal_prompt.cpp ${CMAKE_SOURCE_DIR}/games/superchargers/superchargers_portal_prompt.cpp host/portal_test.cpp host/figures/figure_data.cpp)
target_include_directories(ArkeyanPortalTest PRIVATE . ${CMAKE_SOURCE_DIR}/games/ssa ${CMAKE_SOURCE_DIR}/games/giants ${CMAKE_SOURCE_DIR}/games/swapforce ${CMAKE_SOURCE_DIR}/games/superchargers)
target_link_libraries(ArkeyanPortalTest PRIVATE SDL3::SDL3-static)

# The portal speaker's ADPCM decoder against a test-only encoder (no game data needed): ./ArkeyanPortalAdpcmTest
add_executable(ArkeyanPortalAdpcmTest host/portal_adpcm.cpp host/portal_adpcm_test.cpp)
target_include_directories(ArkeyanPortalAdpcmTest PRIVATE .)

# The macOS HID portal's control SET_REPORT and SDL's macOS device path (no device needed): ./ArkeyanPortalHidReportTest
add_executable(ArkeyanPortalHidReportTest host/portal_hid_report_test.cpp)
target_include_directories(ArkeyanPortalHidReportTest PRIVATE .)

# The USB (Xbox) portals' descriptor parsing, Xbox 360 framing, error names and udev rule; on macOS also the IOKit
# transport's device list (no device needed, nothing opened): ./ArkeyanPortalUsbTest
add_executable(ArkeyanPortalUsbTest host/portal_usb_test.cpp)
target_include_directories(ArkeyanPortalUsbTest PRIVATE .)
if(APPLE)
    target_sources(ArkeyanPortalUsbTest PRIVATE host/portal_usb_mac.cpp)
    target_link_libraries(ArkeyanPortalUsbTest PRIVATE "-framework IOKit" "-framework CoreFoundation")
endif()

# The USB backend over a fake Xbox 360 portal (no device needed): ./ArkeyanPortalUsbBackendTest
add_executable(ArkeyanPortalUsbBackendTest host/portal_usb.cpp host/portal_usb_backend_test.cpp)
target_include_directories(ArkeyanPortalUsbBackendTest PRIVATE .)
target_link_libraries(ArkeyanPortalUsbBackendTest PRIVATE SDL3::SDL3-static)
if(APPLE)
    target_sources(ArkeyanPortalUsbBackendTest PRIVATE host/portal_usb_mac.cpp)
    target_link_libraries(ArkeyanPortalUsbBackendTest PRIVATE "-framework IOKit" "-framework CoreFoundation")
elseif(WIN32 AND NOT ARKEYAN_UWP)
    target_sources(ArkeyanPortalUsbBackendTest PRIVATE host/portal_usb_win.cpp host/portal_gip_win.cpp host/windows_devices.cpp)
    target_compile_definitions(ArkeyanPortalUsbBackendTest PRIVATE _WIN32_WINNT=0x0A00)
    target_link_libraries(ArkeyanPortalUsbBackendTest PRIVATE cfgmgr32 setupapi winusb runtimeobject ole32)
endif()

# The Traptanium speaker's route per portal model and the computer's stream, on the game's framing (no game data needed): ./ArkeyanPortalSpeakerTest
add_executable(ArkeyanPortalSpeakerTest host/portal_speaker.cpp host/portal_adpcm.cpp host/portal_identity.cpp host/figures/figure_data.cpp host/portal_speaker_test.cpp)
target_include_directories(ArkeyanPortalSpeakerTest PRIVATE .)

# The output device: the default unless it is a portal's, on fake device names (no audio device): ./ArkeyanAudioDeviceTest
add_executable(ArkeyanAudioDeviceTest host/audio_device_test.cpp)
target_include_directories(ArkeyanAudioDeviceTest PRIVATE .)

# The speakers setting: its words, the auto rule, the 5.1 channel order, the stereo fold unchanged (no game data, no audio device): ./ArkeyanSpeakersTest
add_executable(ArkeyanSpeakersTest host/speakers_test.cpp)
target_include_directories(ArkeyanSpeakersTest PRIVATE .)

# The Toy Box link: its messages and the game's end on the loopback, with a fake window (no game data needed): ./ArkeyanToyboxLinkTest
add_executable(ArkeyanToyboxLinkTest host/toybox_link_test.cpp host/toybox_link.cpp host/toybox_remote.cpp host/toybox_socket.cpp
    host/collection/presence.cpp)
target_include_directories(ArkeyanToyboxLinkTest PRIVATE .)
target_link_libraries(ArkeyanToyboxLinkTest PRIVATE ArkeyanArtReader)
if(WIN32)
    target_link_libraries(ArkeyanToyboxLinkTest PRIVATE ws2_32)
endif()

# Real portal I/O off the title's threads, with a fake device that stalls (no game data needed): ./ArkeyanPortalAsyncTest
add_executable(ArkeyanPortalAsyncTest host/portal_async.cpp host/portal_async_test.cpp)
target_include_directories(ArkeyanPortalAsyncTest PRIVATE .)
target_link_libraries(ArkeyanPortalAsyncTest PRIVATE SDL3::SDL3-static)
if(WIN32)
    target_link_libraries(ArkeyanPortalAsyncTest PRIVATE winmm) # timeBeginPeriod
endif()

# Figure library test (no game data needed): ./ArkeyanFigureTest [--iso <disc image>] [<figure dump>...]
add_executable(ArkeyanFigureTest host/figures/figure_test.cpp host/portal_emulated.cpp host/portal_identity.cpp d3d/xenos.cpp)
target_include_directories(ArkeyanFigureTest PRIVATE .)
target_link_libraries(ArkeyanFigureTest PRIVATE ArkeyanHostFiles plume ArkeyanXenonUtils fmt::fmt SDL3::SDL3-static)

# Disc pictures: the texture, archive and image readers on synthetic data (no game data needed); with SSA_ISO and/or
# GIANTS_ISO set, each game's pictures read from its disc into a scratch SKYLANDERS_DATA: ./ArkeyanArtTest
add_executable(ArkeyanArtTest host/art/art_test.cpp host/stb_impl.cpp)
target_include_directories(ArkeyanArtTest PRIVATE .)
target_include_directories(ArkeyanArtTest SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/stb)
target_link_libraries(ArkeyanArtTest PRIVATE ArkeyanHostFiles)
if(NOT ARKEYAN_UWP)
    target_sources(ArkeyanArtTest PRIVATE host/accents_test.cpp host/launcher.cpp host/launcher_games.cpp host/xex_title.cpp
        host/portal_emulated.cpp host/texture_pack.cpp host/dds.cpp host/log.cpp host/settings.cpp host/window_size.cpp host/pad_map.cpp host/toybox/pad_bindings.cpp host/mouse.cpp host/portable.cpp
        ${CMAKE_SOURCE_DIR}/tools/arkeyan-builder/process.cpp ${ARKEYAN_GAME_DIR}/info.cpp)
    target_compile_definitions(ArkeyanArtTest PRIVATE ARKEYAN_VERSION="test" SKY_ACCENTS_TEST)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Keep existing launcher/log warnings visible without hiding path-test failures.
        target_compile_options(ArkeyanArtTest PRIVATE -Wno-error=unused-const-variable -Wno-error=missing-field-initializers)
    endif()
    target_include_directories(ArkeyanArtTest PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/XenosRecomp/thirdparty/xxHash)
    target_include_directories(ArkeyanArtTest SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/stb)
    target_link_libraries(ArkeyanArtTest PRIVATE ArkeyanXenonUtils fmt::fmt SDL3::SDL3-static)
    if(WIN32)
        target_sources(ArkeyanArtTest PRIVATE platform/win32/SSARecomp.rc)
    endif()
endif()

# Collection library test (no game data, never the player's folders: a scratch collection): ./ArkeyanCollectionTest
add_executable(ArkeyanCollectionTest host/collection/collection_test.cpp)
target_include_directories(ArkeyanCollectionTest PRIVATE .)
target_link_libraries(ArkeyanCollectionTest PRIVATE ArkeyanArtReader)

# The picture lookups' memory of the portraits folders (synthetic pictures in a scratch folder): ./ArkeyanPictureLookupTest
add_executable(ArkeyanPictureLookupTest host/collection/picture_lookup_test.cpp)
target_include_directories(ArkeyanPictureLookupTest PRIVATE .)
target_link_libraries(ArkeyanPictureLookupTest PRIVATE ArkeyanArtReader)

# Data folder helper and its migration (no game data, scratch folders only): ./ArkeyanDatadirTest
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_executable(ArkeyanDatadirTest host/collection/datadir_test.cpp)
    target_include_directories(ArkeyanDatadirTest PRIVATE .)
    target_link_libraries(ArkeyanDatadirTest PRIVATE ArkeyanArtReader)
endif()

# "Copy the game data" and the folder reader (synthetic files, scratch folders only): ./ArkeyanGameDataTest <scratch dir>
add_executable(ArkeyanGameDataTest host/collection/game_data_test.cpp)
target_include_directories(ArkeyanGameDataTest PRIVATE .)
target_link_libraries(ArkeyanGameDataTest PRIVATE ArkeyanHostFiles)

# The file list of the kit and of an installed game, and its checker (synthetic files, scratch folders only): ./ArkeyanFileListTest <scratch dir>
add_executable(ArkeyanFileListTest host/collection/file_list_test.cpp host/collection/file_list.cpp)
target_include_directories(ArkeyanFileListTest PRIVATE .)
target_link_libraries(ArkeyanFileListTest PRIVATE ArkeyanArtReader)

# The disc reader on damaged and crafted images (synthetic files, scratch folders only): ./ArkeyanVfsTest <scratch dir>
add_executable(ArkeyanVfsTest vfs_test.cpp)
target_include_directories(ArkeyanVfsTest PRIVATE .)
target_link_libraries(ArkeyanVfsTest PRIVATE ArkeyanHostFiles)

# The Wii disc reader on synthetic RVZ images, damaged and crafted ones too: ./ArkeyanRvzTest [scratch dir]; with
# --disc <image.rvz>, a real disc's files are listed and its hashes checked.
# The Wii host's pieces (docs/runtime/wii/): its address space, the executable's loader, the boot information, the
# guest's messages.
add_library(ArkeyanWii STATIC wii/ax.cpp wii/ax_mix.cpp wii/boot_info.cpp wii/executable.cpp wii/gx_fifo.cpp
    wii/gx_texture.cpp wii/gx_vertex.cpp wii/ios.cpp wii/memory_map.cpp wii/nand.cpp wii/module.cpp wii/processor.cpp
    wii/remote.cpp wii/settings.cpp)
target_include_directories(ArkeyanWii PUBLIC .)
target_link_libraries(ArkeyanWii PUBLIC ArkeyanHostFiles)
# Synthetic executables in a mapped address space, the NAND in a scratch folder: ./ArkeyanWiiTest [scratch dir]
add_executable(ArkeyanWiiTest wii/wii_test.cpp)
target_link_libraries(ArkeyanWiiTest PRIVATE ArkeyanWii)
add_executable(ArkeyanAxMixTest wii/ax_mix_test.cpp)
target_link_libraries(ArkeyanAxMixTest PRIVATE ArkeyanWii)
# GX's FIFO: the write-gather pipe's lines, the command processor on hand-made commands: ./ArkeyanGxFifoTest
add_executable(ArkeyanGxFifoTest wii/gx_fifo_test.cpp)
target_link_libraries(ArkeyanGxFifoTest PRIVATE ArkeyanWii)
# GX's vertices decoded and its primitives as triangle lists: ./ArkeyanGxVertexTest
add_executable(ArkeyanGxVertexTest wii/gx_vertex_test.cpp)
target_link_libraries(ArkeyanGxVertexTest PRIVATE ArkeyanWii)

# The GX texture formats decoded from pictures the test encodes: ./ArkeyanGxTextureTest
add_executable(ArkeyanGxTextureTest wii/gx_texture_test.cpp)
target_link_libraries(ArkeyanGxTextureTest PRIVATE ArkeyanWii)
# Remote 1's samples and the test script's presses: ./ArkeyanWiiRemoteTest
add_executable(ArkeyanWiiRemoteTest wii/remote_test.cpp)
target_link_libraries(ArkeyanWiiRemoteTest PRIVATE ArkeyanWii)

add_executable(ArkeyanRvzTest disc/rvz_test.cpp)
target_include_directories(ArkeyanRvzTest PRIVATE .)
target_link_libraries(ArkeyanRvzTest PRIVATE ArkeyanHostFiles)

# The portable game's pack and the game's reader of it (synthetic files, scratch folders only): ./ArkeyanPortablePackTest <scratch dir>
add_executable(ArkeyanPortablePackTest host/collection/portable_pack_test.cpp)
target_include_directories(ArkeyanPortablePackTest PRIVATE .)
target_link_libraries(ArkeyanPortablePackTest PRIVATE ArkeyanHostFiles)

# Chests added from the launcher: its request and status files (synthetic kinds, a scratch folder).
add_executable(ArkeyanChestsTest host/collection/chests_test.cpp host/collection/chests.cpp)
target_include_directories(ArkeyanChestsTest PRIVATE .)
target_link_libraries(ArkeyanChestsTest PRIVATE ArkeyanArtReader)

# The launcher's presence snapshot and the worker with the link disabled (synthetic states only).
add_executable(ArkeyanPresenceTest game.cpp host/collection/presence_test.cpp host/collection/presence.cpp
    host/toybox_remote.cpp host/toybox_socket.cpp host/toybox_link.cpp)
target_include_directories(ArkeyanPresenceTest PRIVATE .)
target_link_libraries(ArkeyanPresenceTest PRIVATE ArkeyanArtReader)
if(WIN32)
    target_link_libraries(ArkeyanPresenceTest PRIVATE ws2_32)
endif()

# Save backups, atomic writes, restoring and the running game's marker (no game data, scratch folders only): ./ArkeyanSavesTest
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_executable(ArkeyanSavesTest host/collection/save_backup_test.cpp)
    target_include_directories(ArkeyanSavesTest PRIVATE .)
    target_link_libraries(ArkeyanSavesTest PRIVATE ArkeyanArtReader)
endif()

# Achievements (the XDBF reader), unlocks and play time (synthetic data, scratch folders; the discs given are only read):
# ./ArkeyanProgressTest [<disc>.iso...]. And the achievement calls of a title on a fake guest context, against a disc:
# ./ArkeyanAchievementsKernelTest <disc>.iso
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_executable(ArkeyanProgressTest host/collection/progress_test.cpp host/collection/achievements.cpp)
    target_include_directories(ArkeyanProgressTest PRIVATE .)
    target_link_libraries(ArkeyanProgressTest PRIVATE ArkeyanHostFiles ArkeyanXenonUtils fmt::fmt SDL3::SDL3-static)

    add_executable(ArkeyanAchievementsKernelTest EXCLUDE_FROM_ALL kernel/xam_achievements_test.cpp kernel/xam_achievements.cpp kernel/objects.cpp
        kernel/overlapped.cpp memory.cpp host/collection/achievements.cpp)
    target_include_directories(ArkeyanAchievementsKernelTest PRIVATE .)
    target_include_directories(ArkeyanAchievementsKernelTest SYSTEM PRIVATE ${ARKEYAN_PPC_DIR} ${ARKEYAN_XENON_UTILS}) # as ArkeyanRuntime's
    arkeyan_tidy_standin(ArkeyanAchievementsKernelTest)
    target_link_libraries(ArkeyanAchievementsKernelTest PRIVATE ArkeyanHostFiles ArkeyanXenonUtils fmt::fmt SDL3::SDL3-static)

    # Host calls into the guest, thread memory over 2000 create/exit cycles, the dispatcher table; the game's code range,
    # no game code: ./ArkeyanGuestThreadTest
    add_executable(ArkeyanGuestThreadTest EXCLUDE_FROM_ALL thread_test.cpp thread.cpp indirect_call_guard.cpp kernel/objects.cpp
        memory.cpp ${ARKEYAN_PLATFORM_SOURCES})
    target_include_directories(ArkeyanGuestThreadTest PRIVATE .)
    target_include_directories(ArkeyanGuestThreadTest SYSTEM PRIVATE ${ARKEYAN_PPC_DIR} ${ARKEYAN_XENON_UTILS}) # as ArkeyanRuntime's
    arkeyan_tidy_standin(ArkeyanGuestThreadTest)
    target_link_libraries(ArkeyanGuestThreadTest PRIVATE ArkeyanXenonUtils)
    arkeyan_layout_record(ArkeyanGuestThreadTest)
endif()

# On Windows/packaged builds, copy third-party notices into the output directory.
if(WIN32)
    target_sources(ArkeyanRuntime PRIVATE platform/win32/SSARecomp.rc)
    target_compile_definitions(ArkeyanRuntime PRIVATE _WIN32_WINNT=0x0A00) # Windows 10
    if(NOT ARKEYAN_UWP)
        target_link_libraries(ArkeyanRuntime PRIVATE winmm)
    endif()
    target_link_libraries(ArkeyanRuntime PRIVATE ws2_32) # the Toy Box link (host/toybox_socket.cpp)
    if(NOT ARKEYAN_UWP)
        target_link_libraries(ArkeyanRuntime PRIVATE cfgmgr32) # each portal's driver, read only (host/portal_usb.cpp)
        # The Xbox portals: WinUSB devices, and the Xbox One portal through Windows.Gaming.Input.Custom
        target_sources(ArkeyanRuntime PRIVATE host/portal_usb_win.cpp host/portal_gip_win.cpp)
        target_link_libraries(ArkeyanRuntime PRIVATE winusb runtimeobject ole32)
    endif()
    add_custom_command(TARGET ArkeyanRuntime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
            $<TARGET_FILE_DIR:ArkeyanRuntime>/THIRD_PARTY_NOTICES.md
        VERBATIM)
endif()

if(ARKEYAN_UWP)
    # Xbox Dev Mode (UWP app, packaged by tools/xbox/package.sh): host/uwp replaces main.cpp and the SDL host
    # (window and events, audio, launcher, real portals).
    get_target_property(ARKEYAN_SOURCES ArkeyanRuntime SOURCES)
    list(REMOVE_ITEM ARKEYAN_SOURCES main.cpp host/sdl_hints.cpp host/mouse_input.cpp host/audio.cpp host/launcher.cpp host/portable.cpp host/xex_title.cpp host/log.cpp host/portal_hid.cpp
        host/portal_usb.cpp host/windows_devices.cpp host/gamemenu.cpp host/report.cpp)
    set_property(TARGET ArkeyanRuntime PROPERTY SOURCES ${ARKEYAN_SOURCES} host/audio_xaudio2.cpp host/uwp/main.cpp
        host/uwp/portal_backends.cpp host/uwp/settings_uwp.cpp host/uwp/settings_file.cpp host/uwp/uwp_compat.cpp host/uwp/cpu_check.cpp host/uwp/gamemenu_uwp.cpp)
    # The CPU check runs before any other code, so it stays on the baseline whatever ARKEYAN_UWP_MARCH (toolchain file).
    set_source_files_properties(host/uwp/cpu_check.cpp PROPERTIES COMPILE_OPTIONS "-march=btver2")
    if(NOT ARKEYAN_UWP_MARCH STREQUAL "btver2")
        target_compile_definitions(ArkeyanRuntime PRIVATE ARKEYAN_REQUIRE_X86_64_V3)
    endif()
    set_target_properties(ArkeyanRuntime PROPERTIES WIN32_EXECUTABLE ON)
    target_link_options(ArkeyanRuntime PRIVATE -municode)
    # Shared files that include SDL's headers (see ARKEYAN_UWP_DESKTOP_FAMILY in the root CMakeLists.txt).
    set_source_files_properties(host/display.cpp host/portal.cpp host/portal_emulated.cpp kernel/xam_content.cpp
        host/settings.cpp host/uwp/settings_uwp.cpp host/uwp/settings_file.cpp host/game_options.cpp host/gamemenu_debug.cpp host/toybox.cpp host/toybox/draw.cpp host/toybox/menu.cpp host/toybox/model.cpp host/toybox/trap_villain.cpp host/uwp/gamemenu_uwp.cpp
        host/art/game_art.cpp host/art/game_text.cpp
        host/collection/catalogue.cpp host/collection/collection.cpp host/collection/icons.cpp
        PROPERTIES COMPILE_OPTIONS "${ARKEYAN_UWP_DESKTOP_FAMILY}")
    set_source_files_properties(gpu/gpu.cpp DIRECTORY gpu PROPERTIES COMPILE_OPTIONS "${ARKEYAN_UWP_DESKTOP_FAMILY}")
    set_target_properties(ArkeyanPortalTest ArkeyanPortalAdpcmTest ArkeyanPortalSpeakerTest ArkeyanSpeakersTest ArkeyanFigureTest ArkeyanArtTest ArkeyanCollectionTest ArkeyanSettingsTest ArkeyanUwpSettingsTest ArkeyanGameOptionsTest ArkeyanCommandsTest ArkeyanDebugScreenTest ArkeyanRecompGpuTest PROPERTIES EXCLUDE_FROM_ALL ON)
endif()

# The game's version in its window title (games/<id>/game.txt, read by the top CMakeLists.txt): main.cpp only, so a new
# version leaves the other files' compile commands (and the kit's compile.args) unchanged.
set_source_files_properties(main.cpp host/uwp/main.cpp PROPERTIES COMPILE_DEFINITIONS ARKEYAN_GAME_VERSION="${ARKEYAN_GAME_VERSION}")
# The game's id, for the same reason: only the files that read it (and toybox/model.h's includers) differ between games.
set_property(SOURCE main.cpp host/log.cpp host/portal.cpp host/portal_mixed.cpp host/gamemenu.cpp host/gamemenu_debug.cpp
    host/toybox.cpp host/toybox/draw.cpp host/toybox/menu.cpp host/toybox/model.cpp kernel/files.cpp
    kernel/xam_achievements.cpp kernel/xam_achievements_test.cpp kernel/xam_content.cpp APPEND PROPERTY COMPILE_DEFINITIONS
    ARKEYAN_GAME_ID="${ARKEYAN_GAME}")

add_library(ArkeyanNetplayTransport STATIC netplay/chacha20.cpp netplay/protocol.cpp netplay/session.cpp netplay/socket.cpp netplay/portmap.cpp)
target_include_directories(ArkeyanNetplayTransport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ArkeyanNetplayTransport PUBLIC ArkeyanNetplay)
if(WIN32)
    target_link_libraries(ArkeyanNetplayTransport PUBLIC ws2_32 iphlpapi)
endif()
sky_quality(ArkeyanNetplayTransport)
target_link_libraries(ArkeyanRuntime PRIVATE ArkeyanNetplayTransport)
add_executable(ArkeyanNetplayProtocolTest netplay/protocol_test.cpp)
add_executable(ArkeyanNetplaySessionTest netplay/session_test.cpp)
target_link_libraries(ArkeyanNetplayProtocolTest PRIVATE ArkeyanNetplayTransport)
target_link_libraries(ArkeyanNetplaySessionTest PRIVATE ArkeyanNetplayTransport)
add_executable(ArkeyanPortMapTest netplay/portmap_test.cpp)
target_link_libraries(ArkeyanPortMapTest PRIVATE ArkeyanNetplayTransport)
sky_add_tests(ArkeyanNetplayProtocolTest ArkeyanNetplaySessionTest ArkeyanPortMapTest)

add_executable(ArkeyanNetplaySetupTest netplay/session_setup_test.cpp host/game_options.cpp host/settings.cpp host/window_size.cpp
    host/pad_map.cpp host/toybox/pad_bindings.cpp host/mouse.cpp host/profiles.cpp host/stb_impl.cpp)
target_include_directories(ArkeyanNetplaySetupTest PRIVATE .)
target_include_directories(ArkeyanNetplaySetupTest SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/stb)
target_link_libraries(ArkeyanNetplaySetupTest PRIVATE ArkeyanNetplayTransport fmt::fmt SDL3::SDL3-static)
sky_add_tests(ArkeyanNetplaySetupTest)
set_tests_properties(ArkeyanNetplaySetupTest PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(ArkeyanNetplayStartupTest netplay/session_startup_test.cpp)
target_link_libraries(ArkeyanNetplayStartupTest PRIVATE ArkeyanNetplay)
sky_add_tests(ArkeyanNetplayStartupTest)
set_tests_properties(ArkeyanNetplayStartupTest PROPERTIES
    ENVIRONMENT "SKY_NETPLAY_HOST=1234;SKY_NETPLAY_DETERMINISTIC=0;SKY_NETPLAY_DELIVERY=frame;SKY_NETPLAY_SCHEDULE=cooperative")
