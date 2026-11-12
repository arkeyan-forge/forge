# XMA audio: FFmpeg's libavcodec decodes WMA Pro frames (apu/xma_decoder_ffmpeg.cpp).
#
# The default build uses the minimal static FFmpeg from tools/ffmpeg/build.sh (LGPL-2.1-or-later, only the WMA Pro
# decoder, no --enable-gpl / --enable-nonfree), built into the build tree on first configure. ARKEYAN_FFMPEG_ROOT points
# at a prefix built beforehand (Windows, Xbox, distribution); without ARKEYAN_FFMPEG_AUTOBUILD the build falls back to a
# system FFmpeg through pkg-config. Whatever is found is checked for a GPL or non-free licence: these binaries are
# distributed statically linked, so such an FFmpeg would relicense the whole executable.
#
# ARKEYAN_FFMPEG=OFF builds without music and sound effects (apu/xma_decoder_null.cpp) and skips all of this.

option(ARKEYAN_FFMPEG "Decode XMA audio with FFmpeg's libavcodec" ON)
set(ARKEYAN_FFMPEG_ROOT "" CACHE PATH
    "Install prefix of tools/ffmpeg/build.sh; empty: build the minimal LGPL FFmpeg in the build tree")
option(ARKEYAN_FFMPEG_AUTOBUILD "Build the minimal LGPL FFmpeg with tools/ffmpeg/build.sh when ARKEYAN_FFMPEG_ROOT is empty" ON)
set(ARKEYAN_FFMPEG_JOBS "" CACHE STRING "Parallel jobs for the automatic FFmpeg build (empty: build.sh's default)")

function(arkeyan_ffmpeg_reject reason)
    message(FATAL_ERROR
        "XMA audio: the FFmpeg found is not LGPL and cannot be linked into ArkeyanRuntime.\n"
        "${reason}\n"
        "These binaries are distributed statically linked; a GPL or non-free FFmpeg would make the whole executable\n"
        "GPL or non-redistributable. Build the minimal LGPL FFmpeg with\n"
        "  tools/ffmpeg/build.sh host <prefix>            (macOS/Linux)\n"
        "  tools/ffmpeg/build.sh windows-x64 <prefix>    (Windows)\n"
        "  tools/ffmpeg/build.sh uwp-x64 <prefix>        (Xbox Dev Mode)\n"
        "and configure with -DARKEYAN_FFMPEG_ROOT=<prefix>. -DARKEYAN_FFMPEG=OFF builds without music and sound effects.")
endfunction()

# Runs avcodec_license()/avcodec_configuration() (host builds) or reads the strings FFmpeg compiled into libavcodec
# (cross builds cannot run the probe). The GPL/non-free check is the same either way.
function(arkeyan_ffmpeg_check_license label include_dirs library_dirs library_names library_files)
    set(link_items)
    foreach(directory ${library_dirs})
        list(APPEND link_items "-L${directory}")
    endforeach()
    foreach(name ${library_names})
        list(APPEND link_items "-l${name}")
    endforeach()

    if(NOT CMAKE_CROSSCOMPILING AND link_items)
        try_run(probe_run probe_compile ${CMAKE_BINARY_DIR}/ssa-ffmpeg-license-probe
            ${CMAKE_SOURCE_DIR}/cmake/ffmpeg_license_probe.c
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${include_dirs}" "-DLINK_LIBRARIES=${link_items}"
            RUN_OUTPUT_VARIABLE probe_output)
        if(probe_compile AND probe_run EQUAL 0)
            string(REGEX MATCH "license=([^\n]*)" license_match "${probe_output}")
            set(license "${CMAKE_MATCH_1}")
            string(REGEX MATCH "configuration=([^\n]*)" configuration_match "${probe_output}")
            set(configuration "${CMAKE_MATCH_1}")
            # Do not cache the probe: a reconfigure with another FFmpeg must run the new one.
            unset(probe_run CACHE)
            unset(probe_compile CACHE)
            if(license STREQUAL "")
                message(WARNING "XMA audio: could not read the licence of ${label}")
            else()
                message(STATUS "XMA audio: FFmpeg licence: ${license}")
                if(license MATCHES "^GPL" OR license MATCHES "[Nn]onfree")
                    arkeyan_ffmpeg_reject("${label} reports \"${license}\" (configuration: ${configuration})")
                endif()
                if(configuration MATCHES "--enable-gpl" OR configuration MATCHES "--enable-nonfree")
                    arkeyan_ffmpeg_reject("${label} was built with --enable-gpl or --enable-nonfree:\n${configuration}")
                endif()
                return()
            endif()
        endif()
    endif()

    set(saw_license FALSE)
    foreach(file ${library_files})
        if(EXISTS "${file}")
            file(STRINGS "${file}" strings REGEX "libavcodec license:|--enable-gpl|--enable-nonfree")
            foreach(line ${strings})
                if(line MATCHES "libavcodec license:")
                    set(saw_license TRUE)
                    message(STATUS "XMA audio: FFmpeg licence: ${line}")
                endif()
                if(line MATCHES "license: GPL" OR line MATCHES "license: nonfree" OR line MATCHES "--enable-gpl"
                    OR line MATCHES "--enable-nonfree")
                    arkeyan_ffmpeg_reject("${file} contains: ${line}")
                endif()
            endforeach()
        endif()
    endforeach()
    if(NOT saw_license)
        message(WARNING
            "XMA audio: could not verify that ${label} is LGPL; make sure it was not built with --enable-gpl or "
            "--enable-nonfree")
    endif()
endfunction()

function(arkeyan_setup_ffmpeg target)
    if(NOT ARKEYAN_FFMPEG)
        message(STATUS "XMA audio: disabled (ARKEYAN_FFMPEG=OFF)")
        target_sources(${target} PRIVATE ${CMAKE_SOURCE_DIR}/runtime/apu/xma_decoder_null.cpp)
        return()
    endif()

    if(ARKEYAN_UWP)
        set(ffmpeg_target uwp-x64)
    elseif(CMAKE_CROSSCOMPILING AND MINGW)
        set(ffmpeg_target windows-x64)
    else()
        set(ffmpeg_target host)
    endif()

    set(prefix "")
    set(mode "")
    set(label "")
    set(include_dirs "")
    set(library_dirs "")
    set(library_names "")
    set(library_files "")

    if(ARKEYAN_FFMPEG_ROOT)
        get_filename_component(prefix "${ARKEYAN_FFMPEG_ROOT}" ABSOLUTE)
        set(mode static)
        set(label "ARKEYAN_FFMPEG_ROOT=${prefix}")
    elseif(ARKEYAN_FFMPEG_AUTOBUILD AND NOT CMAKE_HOST_WIN32)
        set(prefix "${CMAKE_BINARY_DIR}/ffmpeg")
        set(mode static)
        set(label "tools/ffmpeg/build.sh ${ffmpeg_target} in ${prefix}")
    endif()

    if(mode STREQUAL "static")
        set(libavcodec "${prefix}/lib/libavcodec.a")
        set(libavutil "${prefix}/lib/libavutil.a")
        if(NOT EXISTS "${libavcodec}" OR NOT EXISTS "${libavutil}")
            if(ARKEYAN_FFMPEG_ROOT)
                message(FATAL_ERROR
                    "${libavcodec} or ${libavutil} not found in ARKEYAN_FFMPEG_ROOT=${prefix}.\n"
                    "Build the prefix with tools/ffmpeg/build.sh ${ffmpeg_target} ${prefix}, or configure with "
                    "-DARKEYAN_FFMPEG=OFF.")
            endif()
            message(STATUS "XMA audio: building the minimal LGPL FFmpeg ${ffmpeg_target} in ${prefix} (one time)")
            set(env_command)
            if(ARKEYAN_FFMPEG_JOBS)
                set(env_command ${CMAKE_COMMAND} -E env "JOBS=${ARKEYAN_FFMPEG_JOBS}")
            endif()
            execute_process(
                COMMAND ${env_command} ${CMAKE_SOURCE_DIR}/tools/ffmpeg/build.sh ${ffmpeg_target} ${prefix}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE build_result
                OUTPUT_VARIABLE build_output
                ERROR_VARIABLE build_error)
            if(NOT build_result EQUAL 0 OR NOT EXISTS "${libavcodec}" OR NOT EXISTS "${libavutil}")
                message(WARNING
                    "XMA audio: could not build the minimal FFmpeg ${ffmpeg_target}; falling back to a system FFmpeg.\n"
                    "${build_output}\n${build_error}")
                set(mode "")
                set(label "")
            endif()
        endif()
        if(mode STREQUAL "static")
            set(include_dirs "${prefix}/include")
            set(library_dirs "${prefix}/lib")
            set(library_names avcodec avutil)
            set(library_files "${libavcodec}" "${libavutil}")
        endif()
    endif()

    if(NOT mode STREQUAL "static")
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            # Homebrew's prefix is not in every pkg-config's default search path.
            if(APPLE AND EXISTS /opt/homebrew/lib/pkgconfig)
                set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/homebrew/lib/pkgconfig")
            endif()
            pkg_check_modules(ARKEYAN_LIBAVCODEC QUIET IMPORTED_TARGET libavcodec libavutil)
        endif()
        if(NOT ARKEYAN_LIBAVCODEC_FOUND)
            message(FATAL_ERROR
                "FFmpeg (libavcodec, libavutil) not found; it decodes the game's XMA audio.\n"
                "Build the minimal static LGPL one with tools/ffmpeg/build.sh ${ffmpeg_target} <prefix> and pass "
                "-DARKEYAN_FFMPEG_ROOT=<prefix>, or configure with -DARKEYAN_FFMPEG=OFF to build without music and sound "
                "effects.")
        endif()
        set(mode pkgconfig)
        set(label "system FFmpeg ${ARKEYAN_LIBAVCODEC_libavcodec_VERSION} through pkg-config")
        set(include_dirs "${ARKEYAN_LIBAVCODEC_INCLUDE_DIRS}")
        set(library_dirs "${ARKEYAN_LIBAVCODEC_LIBRARY_DIRS}")
        set(library_names "${ARKEYAN_LIBAVCODEC_LIBRARIES}")
        foreach(name ${library_names})
            foreach(directory ${library_dirs})
                file(GLOB candidates
                    "${directory}/${CMAKE_STATIC_LIBRARY_PREFIX}${name}${CMAKE_STATIC_LIBRARY_SUFFIX}"
                    "${directory}/${CMAKE_SHARED_LIBRARY_PREFIX}${name}${CMAKE_SHARED_LIBRARY_SUFFIX}*")
                list(APPEND library_files ${candidates})
            endforeach()
        endforeach()
        message(STATUS "XMA audio: FFmpeg libavcodec ${ARKEYAN_LIBAVCODEC_libavcodec_VERSION} through pkg-config")
    endif()

    arkeyan_ffmpeg_check_license("${label}" "${include_dirs}" "${library_dirs}" "${library_names}" "${library_files}")

    if(mode STREQUAL "static")
        target_include_directories(${target} PRIVATE ${include_dirs})
        target_link_libraries(${target} PRIVATE ${library_files})
    else()
        target_link_libraries(${target} PRIVATE PkgConfig::ARKEYAN_LIBAVCODEC)
    endif()
    target_sources(${target} PRIVATE ${CMAKE_SOURCE_DIR}/runtime/apu/xma_decoder_ffmpeg.cpp)
    if(WIN32)
        target_link_libraries(${target} PRIVATE bcrypt) # libavutil's random seed (in the UWP API set too)
    endif()
endfunction()
