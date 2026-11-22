cmake_minimum_required(VERSION 3.20)
project(ArkeyanRuntime VERSION 0.1.0 LANGUAGES CXX C)
if(WIN32)
    enable_language(RC)
    set(ARKEYAN_KIT_VERSION "0.0.0" CACHE STRING "The Windows kit executables' version (tools/make_kit.sh)")
    # A Windows version resource takes four numbers: a beta or a test kit (0.9.9b3-4-gabc1234) keeps its first three.
    string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" kit_version_numbers "${ARKEYAN_KIT_VERSION}")
    if(NOT kit_version_numbers)
        set(kit_version_numbers 0.0.0)
    endif()
    string(REPLACE "." "," ARKEYAN_KIT_VERSION_PARTS "${kit_version_numbers}.0")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build folders and commands from before the rename (docs/naming.md): an old -DSSA_<X> becomes ARKEYAN_<X>, so a
# reconfigured build-giants stays Giants instead of falling back to the default game.
get_cmake_property(old_cache CACHE_VARIABLES)
foreach(old IN LISTS old_cache)
    if(old MATCHES "^SSA_(.+)$" AND NOT old STREQUAL "SSA_ISO")
        set(new ARKEYAN_${CMAKE_MATCH_1})
        if(NOT DEFINED CACHE{${new}})
            get_property(type CACHE ${old} PROPERTY TYPE)
            if(type STREQUAL "UNINITIALIZED")
                set(type STRING)
            endif()
            set(${new} "$CACHE{${old}}" CACHE ${type} "")
            message(STATUS "${old} is now ${new}")
        endif()
        unset(${old} CACHE)
    endif()
endforeach()

# Warnings, sanitizers and ctest registration for our own targets (never ArkeyanRecompLib or thirdparty/).
include(cmake/Quality.cmake)
# The checks (tools/ci/run.sh): only the host libraries and unit tests, without private/<game>/ppc (no game data).
option(SKY_TESTS_ONLY "Skip the translated game code: only the host unit tests and tools (tools/ci/run.sh)" OFF)
if(SKY_TESTS_ONLY)
    set(ARKEYAN_FFMPEG OFF CACHE BOOL "" FORCE) # the unit tests decode no audio
endif()

# The game this build recompiles (docs/architecture/layout.md): ssa (Spyro's Adventure, the default), giants, swapforce,
# trapteam, superchargers or imaginators.
# It selects games/<game>/ (addresses.h: the guest addresses the hooks use; adapter.cpp: runtime/game.h) and the
# engine's hooks, engines/lux/ or engines/alchemy/ (docs/architecture/layout.md).
set(ARKEYAN_GAME ssa CACHE STRING "Game to build: ssa, giants, swapforce, trapteam, superchargers or imaginators")
set_property(CACHE ARKEYAN_GAME PROPERTY STRINGS ssa giants swapforce trapteam superchargers imaginators)
if(NOT ARKEYAN_GAME MATCHES "^(ssa|giants|swapforce|trapteam|superchargers|imaginators)$")
    message(FATAL_ERROR "ARKEYAN_GAME=${ARKEYAN_GAME}: use ssa, giants, swapforce, trapteam, superchargers or imaginators")
endif()
set(ARKEYAN_GAME_DIR ${CMAKE_SOURCE_DIR}/games/${ARKEYAN_GAME})
# The engine the game runs on: its hooks are compiled in with the runtime.
if(ARKEYAN_GAME MATCHES "^(giants|swapforce|trapteam|superchargers|imaginators)$")
    set(ARKEYAN_ENGINE alchemy)
else()
    set(ARKEYAN_ENGINE lux)
endif()
set(ARKEYAN_ENGINE_DIR ${CMAKE_SOURCE_DIR}/engines/${ARKEYAN_ENGINE})
# The catalogue id of the game this tree builds (its data folder, saves, launcher row), ARKEYAN_GAME_ID, is defined for
# the files that read it only (runtime/CMakeLists.txt).
# The game's own version (games/<id>/game.txt, raised with each fix), shown in its window title: defined for
# runtime/main.cpp only (runtime/CMakeLists.txt), so a new version does not change the game code's compile command.
file(STRINGS ${ARKEYAN_GAME_DIR}/game.txt ARKEYAN_GAME_VERSION REGEX "^version=")
string(REPLACE "version=" "" ARKEYAN_GAME_VERSION "${ARKEYAN_GAME_VERSION}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ARKEYAN_GAME_DIR}/game.txt)

# Our changes to submodules (patches/), applied to a fresh checkout.
include(cmake/patches.cmake)

# XenonRecomp decodes with our own PowerPC decoder (tools/xenonrecomp/ppc_decoder.cpp); binutils' is not built.
set(ARKEYAN_PPC_DECODER_DIR ${CMAKE_SOURCE_DIR}/tools/xenonrecomp)
add_subdirectory(thirdparty/XenonRecomp)
if(WIN32 AND NOT ARKEYAN_UWP)
    # Our kit build of the upstream translator: keep its attribution and identify the kit version.
    configure_file(tools/arkeyan-builder/xenonrecomp.rc.in xenonrecomp.rc @ONLY)
    target_sources(XenonRecomp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/xenonrecomp.rc)
    target_include_directories(XenonRecomp PRIVATE ${CMAKE_SOURCE_DIR}/runtime/platform/win32)
endif()
# XenonUtils for the runtime: the same sources without disasm.cpp and the decoder, which only the translator needs.
set(ARKEYAN_XENON_UTILS ${CMAKE_SOURCE_DIR}/thirdparty/XenonRecomp/XenonUtils)
set(ARKEYAN_XENON_THIRDPARTY ${CMAKE_SOURCE_DIR}/thirdparty/XenonRecomp/thirdparty)
add_library(ArkeyanXenonUtils STATIC
    ${ARKEYAN_XENON_UTILS}/xex.cpp
    ${ARKEYAN_XENON_UTILS}/image.cpp
    ${ARKEYAN_XENON_UTILS}/xdbf_wrapper.cpp
    ${ARKEYAN_XENON_UTILS}/xex_patcher.cpp
    ${ARKEYAN_XENON_UTILS}/memory_mapped_file.cpp
    ${ARKEYAN_XENON_THIRDPARTY}/libmspack/libmspack/mspack/lzxd.c
    ${ARKEYAN_XENON_THIRDPARTY}/tiny-AES-c/aes.c)
target_compile_definitions(ArkeyanXenonUtils PRIVATE NOMINMAX)
# SYSTEM: XenonUtils' own warnings (xbox.h) stay out of the targets that include it.
target_include_directories(ArkeyanXenonUtils SYSTEM
    PUBLIC ${ARKEYAN_XENON_UTILS} ${ARKEYAN_XENON_THIRDPARTY}/simde
    PRIVATE ${ARKEYAN_XENON_THIRDPARTY}/libmspack/libmspack/mspack ${ARKEYAN_XENON_THIRDPARTY}/tiny-AES-c
        ${ARKEYAN_XENON_THIRDPARTY}/TinySHA1)
if(MINGW OR CMAKE_CXX_COMPILER MATCHES "/zig$")
    # fmt's format.h calls malloc/free without including <cstdlib>, which libc++ does not pull in on mingw (nor
    # zig's, the Linux kit's).
    target_compile_options(fmt PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstdlib>")
endif()

if(ARKEYAN_UWP)
    # Code that only builds against the desktop API family of the Windows headers: SDL's headers refuse
    # WINAPI_FAMILY_APP, and XenonUtils has desktop-only code the runtime never calls. Imports still come from the UWP
    # umbrella library only (tools/xbox/check_uwp_imports.sh checks the executable).
    set(ARKEYAN_UWP_DESKTOP_FAMILY -UWINAPI_FAMILY -DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP)
    target_compile_options(XenonUtils PRIVATE ${ARKEYAN_UWP_DESKTOP_FAMILY})
    target_compile_options(ArkeyanXenonUtils PRIVATE ${ARKEYAN_UWP_DESKTOP_FAMILY})
    # SDL 3 does not support UWP. The UWP host implements the few SDL calls the shared runtime code makes, against
    # SDL's headers, and stands in for SDL3::SDL3-static.
    add_library(SDL3-static STATIC runtime/host/uwp/sdl_subset.cpp)
    target_include_directories(SDL3-static PUBLIC thirdparty/SDL/include PRIVATE runtime thirdparty/stb)
    target_compile_options(SDL3-static PRIVATE ${ARKEYAN_UWP_DESKTOP_FAMILY})
    add_library(SDL3::SDL3-static ALIAS SDL3-static)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    if(SKY_TESTS_ONLY)
        # The unit tests open no window: SDL without X11 or Wayland, so the checks need none of their packages.
        set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
        set(SDL_X11 OFF CACHE BOOL "" FORCE)
        set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
    endif()
    add_subdirectory(thirdparty/SDL EXCLUDE_FROM_ALL)
    # ccache cannot cache a compile that reads a precompiled header (the kits' KIT_CCACHE=1); the library is
    # byte-identical without it.
    set_target_properties(SDL3-static PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
endif()
# #72 step 2: the runtime reads the code layout from a record LoadXex checks (runtime/guest_layout.h), not from ppc/'s
# constants. Translated code is unchanged. OFF builds the runtime exactly as before.
option(ARKEYAN_LAYOUT_RECORD "Runtime: the code layout from a checked record instead of compiled-in bounds" OFF)
# macOS test configuration: the Vulkan backend over MoltenVK instead of Metal, to run the Khronos validation layer
# (docs/runtime/gpu/plume_backends.md). Not for players.
option(ARKEYAN_MACOS_VULKAN "macOS: Vulkan (MoltenVK) instead of Metal, for the validation layers" OFF)
option(ARKEYAN_VULKAN_VALIDATION "Enable the Vulkan validation layer in plume even in a Release build" OFF)
include(thirdparty/plume.cmake)

# zstd unpacks the translated shader cache (it comes with XenosRecomp).
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/XenosRecomp/thirdparty/zstd/build/cmake EXCLUDE_FROM_ALL)
if(NOT ARKEYAN_UWP)
    add_subdirectory(tools/xexdump)
    add_subdirectory(tools/abi)
    # The translated samples run on the build machine; their code needs clang's builtins.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CROSSCOMPILING)
        add_subdirectory(tools/xenonrecomp)
    endif()
    add_subdirectory(tools/arkeyan-builder)
    sky_quality(xexdump)
    sky_quality(Arkeyan-Builder)
endif()
# tools/prepare_game.sh builds the host tools (XenonRecomp, xexdump) with ARKEYAN_TOOLS_ONLY, before ppc/ exists; so does
# tools/make_kit.sh (Arkeyan-Builder).
option(ARKEYAN_TOOLS_ONLY "Configure only the host tools (XenonRecomp, xexdump); ppc/ is not needed" OFF)
if(ARKEYAN_TOOLS_ONLY)
    include(cmake/art_reader.cmake) # Arkeyan-Builder links ArkeyanArtReader
    sky_quality(ArkeyanArtReader)
    sky_quality(ArkeyanHostFiles)
    return()
endif()
# The translated game code; tools/make_kit.sh points it at a game-free stub to build the player kit's runtime.
set(ARKEYAN_PPC_DIR ${CMAKE_SOURCE_DIR}/private/${ARKEYAN_GAME}/ppc CACHE PATH "XenonRecomp output (ppc_*.cpp/h, shader_cache.cpp)")
option(ARKEYAN_NETPLAY_TRACE "Trace host-sourced translated guest loads" OFF)
if(NOT SKY_TESTS_ONLY)
    add_subdirectory(ArkeyanRecompLib)
    # Unread by the game code, but part of the player's compile command (the kit's compile.args), whose freshness key
    # would otherwise compile every install's game code again.
    get_target_property(ARKEYAN_PPC_IMPORTED ArkeyanRecompLib IMPORTED)
    if(NOT ARKEYAN_PPC_IMPORTED)
        target_compile_definitions(ArkeyanRecompLib PRIVATE ARKEYAN_GAME_ID="${ARKEYAN_GAME}")
        if(ARKEYAN_NETPLAY_TRACE)
            target_compile_options(ArkeyanRecompLib PRIVATE "SHELL:-include \"${CMAKE_SOURCE_DIR}/runtime/netplay/trace_macros.h\"")
        endif()
    elseif(ARKEYAN_NETPLAY_TRACE)
        message(FATAL_ERROR "ARKEYAN_NETPLAY_TRACE requires compiling translated code, without ARKEYAN_PPC_LIBRARY")
    endif()
endif()
# The runtime is built into build/ArkeyanRuntime/: the executable, the bundle and the test paths keep their names.
add_subdirectory(runtime ArkeyanRuntime)

add_library(ArkeyanNetplay STATIC runtime/netplay/clock.cpp runtime/netplay/delivery.cpp runtime/netplay/events.cpp runtime/netplay/inputs.cpp
    runtime/netplay/state_hash.cpp runtime/netplay/scheduler.cpp runtime/netplay/trace.cpp)
target_include_directories(ArkeyanNetplay PUBLIC ${CMAKE_SOURCE_DIR}/runtime)
sky_quality(ArkeyanNetplay)
target_sources(ArkeyanRuntime PRIVATE runtime/netplay/runtime.cpp)
target_link_libraries(ArkeyanRuntime PRIVATE ArkeyanNetplay)
if(ARKEYAN_NETPLAY_TRACE)
    target_compile_definitions(ArkeyanRuntime PRIVATE ARKEYAN_NETPLAY_TRACE=1)
    target_compile_definitions(ArkeyanNetplay PRIVATE ARKEYAN_NETPLAY_TRACE=1)
endif()
foreach(target ArkeyanAchievementsKernelTest ArkeyanGuestThreadTest)
    if(TARGET ${target})
        target_link_libraries(${target} PRIVATE ArkeyanNetplay)
    endif()
endforeach()
add_executable(ArkeyanNetplayTest runtime/netplay/netplay_test.cpp)
add_executable(ArkeyanSchedulerTest runtime/netplay/scheduler_test.cpp)
add_executable(ArkeyanTraceTest runtime/netplay/trace_test.cpp)
target_link_libraries(ArkeyanTraceTest PRIVATE ArkeyanNetplay)
set_target_properties(ArkeyanTraceTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ArkeyanRuntime)
target_link_libraries(ArkeyanSchedulerTest PRIVATE ArkeyanNetplay)
set_target_properties(ArkeyanSchedulerTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ArkeyanRuntime)
target_link_libraries(ArkeyanNetplayTest PRIVATE ArkeyanNetplay)
set_target_properties(ArkeyanNetplayTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ArkeyanRuntime)

# Our own targets only. ArkeyanRecompLib (generated code), thirdparty/, imgui and ArkeyanXenonUtils are left alone.
sky_quality(ArkeyanRuntime)
foreach(target ArkeyanRecompGpu ArkeyanRecompGpuTest ArkeyanRuntimeLauncherUi ArkeyanArtReader ArkeyanHostFiles ArkeyanTexturePackTest ArkeyanRuntimeLauncherTest ArkeyanGameDataTest ArkeyanFileListTest ArkeyanPortablePackTest ArkeyanVfsTest
        ArkeyanProgressTest ArkeyanAchievementsKernelTest ArkeyanGuestThreadTest)
    if(TARGET ${target})
        sky_quality(${target})
    endif()
endforeach()
# The tests that build by default and need no argument (the others run from tools/test_all.sh with a disc or a scratch folder).
sky_add_tests(ArkeyanTraceTest ArkeyanSchedulerTest ArkeyanNetplayTest ArkeyanSettingsTest ArkeyanUwpSettingsTest ArkeyanGameOptionsTest ArkeyanAudioDeviceTest ArkeyanPortalTest ArkeyanPortalAdpcmTest ArkeyanPortalHidReportTest ArkeyanPortalUsbTest ArkeyanPortalSpeakerTest ArkeyanSpeakersTest ArkeyanPortalAsyncTest ArkeyanFigureTest ArkeyanArtTest ArkeyanCollectionTest ArkeyanPictureLookupTest ArkeyanDatadirTest
    ArkeyanSavesTest ArkeyanGuestFormatTest ArkeyanVersionResourceTest ArkeyanRenderScaleTest ArkeyanPackSamplingTest ArkeyanUpscaleTest ArkeyanFrameGenTest ArkeyanSwapChainRecoveryTest ArkeyanGpuBackendTest ArkeyanFrameLimiterTest ArkeyanAddressProtectTest ArkeyanStallWatchTest ArkeyanFrameRateFallbackTest ArkeyanYieldBackOffTest ArkeyanPresentModeTest ArkeyanPresentRectTest ArkeyanRangeHashTest ArkeyanIndexConvertTest ArkeyanClockTest ArkeyanCrashSymbolsTest ArkeyanHeapViewTest ArkeyanWriteWatchTest ArkeyanReadFullyTest ArkeyanTiledDepthTest ArkeyanAutoScaleTest ArkeyanFirstFrameTest
    ArkeyanThunkSlotTest ArkeyanGuestLayoutTest ArkeyanVertexPairsTest ArkeyanFrameAnchorTest ArkeyanMrtAliasTest ArkeyanTargetWritesTest ArkeyanFloatScanTest ArkeyanStepPacerTest ArkeyanRingCursorTest ArkeyanRingRegistersTest ArkeyanUltrawideTest ArkeyanCommandsTest ArkeyanMouseTest ArkeyanWindowSizeTest ArkeyanToyboxLookTest ArkeyanToyboxInPlayTest ArkeyanToyboxPutOnTest ArkeyanToyboxStripTest ArkeyanToyboxKeyMaskTest ArkeyanToyboxWheelKeysTest ArkeyanToyboxPadBindingsTest
    ArkeyanToyboxSwapTest ArkeyanToyboxGridTest ArkeyanToyboxTrapTest ArkeyanToyboxVersionGameTest ArkeyanGlyphModelsTest ArkeyanProfilesTest ArkeyanPromptTextTest ArkeyanToyboxLinkTest ArkeyanPortalScriptTest ArkeyanZoneNameTest ArkeyanLevelRedirectTest ArkeyanLiveBlocksTest ArkeyanNightmareGateTest
    ArkeyanBinkFrameTest ArkeyanBinkDisplayTest ArkeyanPresenceTest ArkeyanChestsTest ArkeyanSdlHintsTest)
if(SKY_TESTS_ONLY)
    set_target_properties(ArkeyanRuntime ArkeyanRecompGpuTest PROPERTIES EXCLUDE_FROM_ALL ON)
    # Adapter tests use synthetic guest calls and do not require translated game code.
    if(EXISTS "${ARKEYAN_GAME_DIR}/tests/CMakeLists.txt")
        add_subdirectory(${ARKEYAN_GAME_DIR}/tests game-tests)
    endif()
endif()
