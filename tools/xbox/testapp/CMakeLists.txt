# Xbox Dev Mode test app (UWP CoreApplication + D3D12). Build with tools/xbox/build_testapp.sh, which uses
# cmake/toolchains/uwp-x64-llvm-mingw.cmake.
cmake_minimum_required(VERSION 3.20)
project(ArkeyanRecompXboxTest LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/uwp-x64-llvm-mingw.cmake")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# All imports (D3D12, DXGI, XAudio2 2.9, WinRT, FromApp file APIs) resolve through the windowsapp umbrella
# library that the llvm-mingw uwp wrapper links.
# winrt_extra.h and uwp_compat.cpp are shared with the game's UWP host.
set(ARKEYAN_UWP_HOST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../runtime/host/uwp)
add_executable(ArkeyanRecompTest WIN32 main.cpp ${ARKEYAN_UWP_HOST_DIR}/uwp_compat.cpp)
target_include_directories(ArkeyanRecompTest PRIVATE ${ARKEYAN_UWP_HOST_DIR})
target_compile_options(ArkeyanRecompTest PRIVATE -Wall -Wextra -Wno-unused-parameter -march=btver2)
target_link_options(ArkeyanRecompTest PRIVATE -municode)
