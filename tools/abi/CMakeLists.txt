set(XENON_ROOT ${CMAKE_SOURCE_DIR}/thirdparty/XenonRecomp)
add_executable(ArkeyanPublicHeaders emit.cpp ${XENON_ROOT}/XenonRecomp/recompiler_config.cpp)
target_include_directories(ArkeyanPublicHeaders PRIVATE ${XENON_ROOT}/XenonRecomp ${XENON_ROOT}/XenonAnalyse)
target_precompile_headers(ArkeyanPublicHeaders PRIVATE ${XENON_ROOT}/XenonRecomp/pch.h)
target_link_libraries(ArkeyanPublicHeaders PRIVATE XenonUtils fmt::fmt tomlplusplus::tomlplusplus xxHash::xxhash)
# This target compiles the dependency's parser and precompiled header with its existing warning policy.

if(NOT CMAKE_CROSSCOMPILING AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(ArkeyanPublicHeadersFixture translation_fixture.cpp ${XENON_ROOT}/XenonRecomp/recompiler.cpp
        ${XENON_ROOT}/XenonRecomp/recompiler_config.cpp)
    target_include_directories(ArkeyanPublicHeadersFixture PRIVATE ${XENON_ROOT}/XenonRecomp)
    target_precompile_headers(ArkeyanPublicHeadersFixture PRIVATE ${XENON_ROOT}/XenonRecomp/pch.h)
    target_link_libraries(ArkeyanPublicHeadersFixture PRIVATE LibXenonAnalyse XenonUtils fmt::fmt
        tomlplusplus::tomlplusplus xxHash::xxhash)
    target_compile_options(ArkeyanPublicHeadersFixture PRIVATE -Wno-switch -Wno-unused-variable -Wno-null-arithmetic)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME ArkeyanPublicHeadersTest COMMAND ${Python3_EXECUTABLE} -I
            ${CMAKE_CURRENT_SOURCE_DIR}/headers.py test --emitter $<TARGET_FILE:ArkeyanPublicHeaders>
            --fixture $<TARGET_FILE:ArkeyanPublicHeadersFixture> --cxx ${CMAKE_CXX_COMPILER})
        set_tests_properties(ArkeyanPublicHeadersTest PROPERTIES TIMEOUT 120)
    endif()
endif()
