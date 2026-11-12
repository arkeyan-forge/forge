# Game code translated by XenonRecomp into ppc/ (run it first, see README).
# ARKEYAN_PPC_LIBRARY=<libArkeyanRecompLib.a built from the same ppc/> reuses that build instead of compiling it again
# (for extra build trees).
set(ARKEYAN_PPC_LIBRARY "" CACHE FILEPATH "Prebuilt ArkeyanRecompLib to link instead of compiling ppc/")

if(ARKEYAN_PPC_LIBRARY)
    add_library(ArkeyanRecompLib STATIC IMPORTED GLOBAL)
    set_target_properties(ArkeyanRecompLib PROPERTIES IMPORTED_LOCATION ${ARKEYAN_PPC_LIBRARY})
    target_include_directories(ArkeyanRecompLib INTERFACE
        ${ARKEYAN_PPC_DIR}
        ${CMAKE_SOURCE_DIR}/thirdparty/XenonRecomp/thirdparty/simde)
    return()
endif()

file(GLOB ARKEYAN_PPC_SOURCES CONFIGURE_DEPENDS ${ARKEYAN_PPC_DIR}/ppc_recomp.*.cpp)

add_library(ArkeyanRecompLib STATIC ${ARKEYAN_PPC_SOURCES} ${ARKEYAN_PPC_DIR}/ppc_func_mapping.cpp)
target_include_directories(ArkeyanRecompLib PUBLIC
    ${ARKEYAN_PPC_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty/XenonRecomp/thirdparty/simde)
target_compile_options(ArkeyanRecompLib PRIVATE -fno-strict-aliasing -Wno-unused-label -Wno-unused-variable)
target_compile_options(ArkeyanRecompLib PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/indirect_call_guard.h)
