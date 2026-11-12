if(WIN32)
    set(TEST_PLATFORM ${CMAKE_SOURCE_DIR}/runtime/platform/platform_win32.cpp)
else()
    set(TEST_PLATFORM ${CMAKE_SOURCE_DIR}/runtime/platform/platform_posix.cpp)
endif()
add_executable(TTLanguageTest language_test.cpp ../language.cpp ../voice_disc.cpp
    ${CMAKE_SOURCE_DIR}/runtime/memory.cpp ${TEST_PLATFORM})
target_include_directories(TTLanguageTest PRIVATE include .. ${CMAKE_SOURCE_DIR}/runtime)
target_include_directories(TTLanguageTest SYSTEM PRIVATE ${ARKEYAN_XENON_UTILS})
target_link_libraries(TTLanguageTest PRIVATE ArkeyanXenonUtils ArkeyanArtReader)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(TTLanguageTest PRIVATE "SHELL:-iquote ${ARKEYAN_XENON_UTILS}")
endif()
sky_add_tests(TTLanguageTest)

add_executable(TTVoiceDiscTest voice_disc_test.cpp ../voice_disc.cpp)
target_include_directories(TTVoiceDiscTest PRIVATE .. ${CMAKE_SOURCE_DIR}/runtime)
target_link_libraries(TTVoiceDiscTest PRIVATE ArkeyanArtReader)

add_executable(TTInputTest input_test.cpp ../adapter.cpp ../info.cpp
    ${CMAKE_SOURCE_DIR}/runtime/host/input.cpp ${CMAKE_SOURCE_DIR}/runtime/host/pad_map.cpp)
target_include_directories(TTInputTest PRIVATE .. ${CMAKE_SOURCE_DIR}/runtime)
sky_add_tests(TTVoiceDiscTest TTInputTest)
set_tests_properties(TTInputTest PROPERTIES ENVIRONMENT "SKY_PRESS_FILE=")
