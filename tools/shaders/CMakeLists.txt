cmake_minimum_required(VERSION 3.20)
project(ShaderTranslationChecks LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(XENOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty/XenosRecomp")
add_subdirectory("${XENOS_ROOT}" xenos)
add_executable(ShaderTranslationTest translation_test.cpp
    "${XENOS_ROOT}/XenosRecomp/shader_recompiler.cpp"
    "${XENOS_ROOT}/XenosRecomp/dxc_compiler.cpp")
target_include_directories(ShaderTranslationTest PRIVATE "${XENOS_ROOT}/XenosRecomp"
    "${XENOS_ROOT}/thirdparty/smol-v/source")
target_precompile_headers(ShaderTranslationTest PRIVATE "${XENOS_ROOT}/XenosRecomp/pch.h")
target_link_libraries(ShaderTranslationTest PRIVATE Microsoft::DirectXShaderCompiler fmt::fmt xxHash::xxhash libzstd_static)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(ShaderTranslationTest PRIVATE -fms-extensions -Wno-switch -Wno-null-arithmetic)
endif()
enable_testing()
add_test(NAME ShaderTranslationTest COMMAND ShaderTranslationTest "${XENOS_ROOT}/XenosRecomp/shader_common.h")
