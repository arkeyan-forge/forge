# XenonRecomp's output checked on the host: invented PowerPC samples are translated at build time by the patched
# translator's own sources (ArkeyanXenonRecompSampleGen), and ArkeyanXenonRecompTest runs the translated C++.
set(XENON_ROOT ${CMAKE_SOURCE_DIR}/thirdparty/XenonRecomp)
add_executable(ArkeyanXenonRecompSampleGen sample_gen.cpp ${XENON_ROOT}/XenonRecomp/recompiler.cpp
    ${XENON_ROOT}/XenonRecomp/recompiler_config.cpp)
target_include_directories(ArkeyanXenonRecompSampleGen PRIVATE ${XENON_ROOT}/XenonRecomp)
target_precompile_headers(ArkeyanXenonRecompSampleGen PRIVATE ${XENON_ROOT}/XenonRecomp/pch.h) # its sources rely on it
target_link_libraries(ArkeyanXenonRecompSampleGen PRIVATE LibXenonAnalyse XenonUtils fmt::fmt tomlplusplus::tomlplusplus
    xxHash::xxhash)
target_compile_options(ArkeyanXenonRecompSampleGen PRIVATE -Wno-switch -Wno-unused-variable -Wno-null-arithmetic)

set(SAMPLE ${CMAKE_CURRENT_BINARY_DIR}/xenonrecomp_sample.h)
add_custom_command(OUTPUT ${SAMPLE} COMMAND ArkeyanXenonRecompSampleGen ${SAMPLE} DEPENDS ArkeyanXenonRecompSampleGen
    COMMENT "Translating the invented XenonRecomp samples")
add_executable(ArkeyanXenonRecompTest xenonrecomp_test.cpp ${SAMPLE})
# SYSTEM: the generated code and ppc_context.h are not held to the project's warnings.
target_include_directories(ArkeyanXenonRecompTest SYSTEM PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${XENON_ROOT}/XenonUtils
    ${XENON_ROOT}/thirdparty/simde)
target_compile_options(ArkeyanXenonRecompTest PRIVATE -fno-strict-aliasing)
sky_add_tests(ArkeyanXenonRecompTest)

# Our PowerPC decoder (ppc_decoder.cpp, also built into XenonRecomp) on words built from the instruction-set documents.
add_executable(ArkeyanPpcDecoderTest ppc_decoder_test.cpp ppc_decoder.cpp)
target_include_directories(ArkeyanPpcDecoderTest SYSTEM PRIVATE ${XENON_ROOT}/thirdparty/disasm) # ppc-inst.h
sky_add_tests(ArkeyanPpcDecoderTest)

# The proof that our decoder decodes as the old one did, local only (decoder_compare.sh): ARKEYAN_DECODER_REFERENCE is a
# copy in private/ (git-ignored) of binutils' PowerPC disassembler (GPL), which nothing else builds.
set(ARKEYAN_DECODER_REFERENCE "" CACHE PATH "decoder_compare.sh only: the old disassembler's sources")
if(ARKEYAN_DECODER_REFERENCE)
    add_library(ArkeyanDecoderReference STATIC ${ARKEYAN_DECODER_REFERENCE}/ppc-dis.c ${ARKEYAN_DECODER_REFERENCE}/disasm.c)
    target_include_directories(ArkeyanDecoderReference PUBLIC ${ARKEYAN_DECODER_REFERENCE})
    target_compile_options(ArkeyanDecoderReference PRIVATE -w)
    add_executable(ArkeyanDecoderCompare decoder_compare.cpp decoder_compare_reference.cpp)
    target_link_libraries(ArkeyanDecoderCompare PRIVATE ArkeyanDecoderReference XenonUtils)
endif()
