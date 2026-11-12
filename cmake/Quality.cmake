# Warnings, sanitizers and tests for our own targets only (runtime, games, engines, host tools).
# Never apply this to ArkeyanRecompLib or any target built from the generated PPC code.
#
# In the root CMakeLists.txt, after the targets exist:
#     include(cmake/Quality.cmake)
#     sky_quality(ArkeyanRuntime)                      # each game executable and each runtime library
#     sky_add_tests(ArkeyanSettingsTest ArkeyanPortalTest ...)   # tests that build by default and run without arguments
#
# Options: -DSKY_WERROR=ON (CI), -DSKY_SANITIZE=address|thread|undefined (debug runs, never in a kit).

option(SKY_WERROR "Treat warnings as errors in our targets" OFF)
set(SKY_SANITIZE "" CACHE STRING "Sanitizer for our targets: address, thread, undefined or empty")

# enable_testing() does nothing inside a function: it runs here, in the includer's directory.
enable_testing()

function(sky_quality target)
    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE /W4 /permissive- /wd4100 /wd4201)
        if(SKY_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra
            -Wno-unused-parameter          # hook signatures are fixed by the recompiler (ctx, base)
            -Wshadow
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wnull-dereference
            -Wimplicit-fallthrough
            -Wformat=2
            -Wmisleading-indentation
            -Wundef)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE -Wunreachable-code -Wthread-safety)
        endif()
        if(SKY_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(SKY_SANITIZE)
            target_compile_options(${target} PRIVATE -fsanitize=${SKY_SANITIZE} -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=${SKY_SANITIZE})
        endif()
    endif()
endfunction()

function(sky_add_tests)
    foreach(test IN LISTS ARGN)
        if(TARGET ${test})
            sky_quality(${test})
            add_test(NAME ${test} COMMAND ${test})
            set_tests_properties(${test} PROPERTIES TIMEOUT 120)
        endif()
    endforeach()
endfunction()
