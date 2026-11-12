# CMake toolchain: Windows 10/11 UWP (AppContainer) x64 with llvm-mingw (clang, lld, libc++, UCRT).
# Used for the Xbox Dev Mode build (tools/xbox/README.md). Fetch the pinned toolchain with
# tools/xbox/setup_tools.sh, or point LLVM_MINGW_ROOT (CMake variable or environment) at an llvm-mingw
# "ucrt" release.
#
# The *-w64-mingw32uwp-* wrappers define WINAPI_FAMILY=WINAPI_FAMILY_APP, UNICODE and _UCRT, and link the
# windowsapp (UWP API umbrella) and ucrtapp import libraries. The C++ runtime (libc++, libunwind,
# winpthreads) is linked statically so the package needs no extra DLLs.
#
# CPU baseline: Xbox One is AMD Jaguar (AVX, F16C, BMI1; no AVX2/FMA/BMI2), Xbox Series X|S is Zen 2.
# One package for both consoles must not use instructions above -march=btver2.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
# Selects the UWP host (runtime/host/uwp) instead of SDL.
set(ARKEYAN_UWP ON)

if(NOT LLVM_MINGW_ROOT)
    if(DEFINED ENV{LLVM_MINGW_ROOT})
        set(LLVM_MINGW_ROOT "$ENV{LLVM_MINGW_ROOT}")
    else()
        set(LLVM_MINGW_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../private/xbox/toolchain/llvm-mingw")
    endif()
endif()
get_filename_component(LLVM_MINGW_ROOT "${LLVM_MINGW_ROOT}" ABSOLUTE)
# Forward to try_compile() projects, which re-read this file.
set(ENV{LLVM_MINGW_ROOT} "${LLVM_MINGW_ROOT}")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_MINGW_ROOT)

set(_uwp_prefix "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32uwp-")
if(CMAKE_HOST_WIN32)
    set(_uwp_exe ".exe")
else()
    set(_uwp_exe "")
endif()
set(CMAKE_C_COMPILER "${_uwp_prefix}clang${_uwp_exe}")
set(CMAKE_CXX_COMPILER "${_uwp_prefix}clang++${_uwp_exe}")
set(CMAKE_RC_COMPILER "${_uwp_prefix}windres${_uwp_exe}")
set(CMAKE_AR "${LLVM_MINGW_ROOT}/bin/llvm-ar${_uwp_exe}" CACHE FILEPATH "")
set(CMAKE_RANLIB "${LLVM_MINGW_ROOT}/bin/llvm-ranlib${_uwp_exe}" CACHE FILEPATH "")
set(UWP_DLLTOOL "${LLVM_MINGW_ROOT}/bin/llvm-dlltool${_uwp_exe}" CACHE FILEPATH "llvm-dlltool used to make import libraries")

set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW_ROOT}/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# CMake's Windows-GNU defaults (-lkernel32 -luser32 ...) would come before the umbrella libraries and bind
# imports to desktop DLL names; the uwp wrapper links windowsapp/ucrtapp itself.
set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "UWP: umbrella libraries come from the llvm-mingw uwp wrapper")
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "UWP: umbrella libraries come from the llvm-mingw uwp wrapper")

# ARKEYAN_UWP_MARCH: btver2 (default: both consoles) or x86-64-v3 for an Xbox Series X|S package (AVX2, FMA, BMI2;
# host/uwp/cpu_check.cpp makes it exit cleanly on an Xbox One). -ffp-contract=off: the guest's floating-point results stay
# those of separate multiplies and adds (the Xenon fuses only when the code says fmadd), now that FMA exists.
if(NOT ARKEYAN_UWP_MARCH)
    if(DEFINED ENV{ARKEYAN_UWP_MARCH})
        set(ARKEYAN_UWP_MARCH "$ENV{ARKEYAN_UWP_MARCH}")
    else()
        set(ARKEYAN_UWP_MARCH btver2)
    endif()
endif()
set(ENV{ARKEYAN_UWP_MARCH} "${ARKEYAN_UWP_MARCH}")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARKEYAN_UWP_MARCH)
set(CMAKE_C_FLAGS_INIT "-march=${ARKEYAN_UWP_MARCH} -ffp-contract=off")
set(CMAKE_CXX_FLAGS_INIT "-march=${ARKEYAN_UWP_MARCH} -ffp-contract=off")

# --appcontainer sets IMAGE_DLLCHARACTERISTICS_APPCONTAINER, which UWP executables must carry.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -Wl,--appcontainer")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static -Wl,--appcontainer")
