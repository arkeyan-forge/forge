# Windows x64 cross-compilation with llvm-mingw (clang, lld, libc++, UCRT): https://github.com/mstorsjo/llvm-mingw
# Tested with release 20260922 on macOS. The generated game code needs clang (__builtin_assume,
# __builtin_rotateleft*), so GCC-based mingw-w64 cannot build it.
#
#   cmake -S . -B build-win64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-w64-mingw32-clang.cmake \
#         -DLLVM_MINGW_ROOT=<extracted llvm-mingw release> -DCMAKE_BUILD_TYPE=Release
#
# Without LLVM_MINGW_ROOT the x86_64-w64-mingw32-* tools are taken from PATH.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(LLVM_MINGW_ROOT "${LLVM_MINGW_ROOT}" CACHE PATH "llvm-mingw installation directory")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_MINGW_ROOT)

set(ARKEYAN_MINGW_TRIPLE x86_64-w64-mingw32)
if(LLVM_MINGW_ROOT)
    set(ARKEYAN_MINGW_TOOL_PREFIX "${LLVM_MINGW_ROOT}/bin/${ARKEYAN_MINGW_TRIPLE}")
    set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW_ROOT}/${ARKEYAN_MINGW_TRIPLE}")
else()
    set(ARKEYAN_MINGW_TOOL_PREFIX "${ARKEYAN_MINGW_TRIPLE}")
endif()

if(CMAKE_HOST_WIN32)
    set(ARKEYAN_MINGW_EXE .exe) # a Windows-hosted llvm-mingw (CI)
endif()
set(CMAKE_C_COMPILER "${ARKEYAN_MINGW_TOOL_PREFIX}-clang${ARKEYAN_MINGW_EXE}")
set(CMAKE_CXX_COMPILER "${ARKEYAN_MINGW_TOOL_PREFIX}-clang++${ARKEYAN_MINGW_EXE}")
set(CMAKE_RC_COMPILER "${ARKEYAN_MINGW_TOOL_PREFIX}-windres${ARKEYAN_MINGW_EXE}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# CPU baseline: AMD Jaguar (-march=btver2: SSE4.2, AVX, F16C, BMI1; no AVX2/FMA/BMI2), the Xbox One CPU, so the
# same code also runs there (Xbox Series X|S is Zen 2). simde maps the guest's vector code onto SSE/AVX.
# -mno-sse4a: btver2 includes AMD's SSE4a (extrq/insertq), which the compiler picks for byte extracts in the translated
# vector code (Swap Force: 96 of them) and which every Intel CPU (and Rosetta) rejects as an illegal instruction.
# -mno-incremental-linker-compatible: no build time in the COFF object header, so the same source gives the same bytes.
set(CMAKE_C_FLAGS_INIT "-march=btver2 -mno-sse4a -mno-incremental-linker-compatible")
set(CMAKE_CXX_FLAGS_INIT "-march=btver2 -mno-sse4a -mno-incremental-linker-compatible")

# Self-contained executable: libc++, libunwind and winpthreads are linked in; only system DLLs and the UCRT remain.
# --no-insert-timestamp: no link time in the PE header (reproducible executables).
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -Wl,--no-insert-timestamp")
