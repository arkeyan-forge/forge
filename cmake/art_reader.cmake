# The figure and collection library and the readers of the disc's pictures: their sources (ARKEYAN_COLLECTION_SOURCES,
# ARKEYAN_ART_SOURCES, used by the runtime's targets) and the ArkeyanArtReader library Arkeyan-Builder links (the game's icon is made
# from the disc's logo). Included by runtime/CMakeLists.txt, and by the root project in its tools-only configuration
# (tools/make_kit.sh builds Arkeyan-Builder that way, without the runtime).
set(ARKEYAN_RUNTIME_DIR ${CMAKE_SOURCE_DIR}/runtime)
# The figure and collection library (host/figures without the portraits, host/collection), shared with Skylauncher
# (docs/runtime/host/collection_api.md). The catalogue (resources/collection/catalogue.json) is built in.
set(ARKEYAN_CATALOGUE ${CMAKE_SOURCE_DIR}/resources/collection/catalogue.json)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/embedded_catalogue.cpp
    COMMAND ${CMAKE_COMMAND} -DNAME=g_embeddedCatalogue -DINPUT=${ARKEYAN_CATALOGUE}
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/embedded_catalogue.cpp -P ${CMAKE_SOURCE_DIR}/cmake/embed_file.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/cmake/embed_file.cmake ${ARKEYAN_CATALOGUE}
    COMMENT "Embedding the toy catalogue")
set(ARKEYAN_COLLECTION_SOURCES
    ${ARKEYAN_RUNTIME_DIR}/host/figures/figure_catalog.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/figures/figure_crypto.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/figures/figure_data.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/figures/figure_tag.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/catalogue.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/collection.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/datadir.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/game_data.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/icons.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/json.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/portable_pack.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/progress.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/save_backup.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/search.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/collection/upgrades.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/embedded_catalogue.cpp)
# Each game's pictures from the player's disc: logo, background, portraits (docs/runtime/host/game_art.md). Where each
# game keeps them is its manifest, games/<id>/art.json, every one built in (a new game adds a file, no code).
file(GLOB ARKEYAN_ART_MANIFESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/games/*/art.json)
list(SORT ARKEYAN_ART_MANIFESTS)
string(REPLACE ";" "|" ARKEYAN_ART_MANIFEST_LIST "${ARKEYAN_ART_MANIFESTS}")
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/embedded_art_manifests.cpp
    COMMAND ${CMAKE_COMMAND} "-DINPUTS=${ARKEYAN_ART_MANIFEST_LIST}" -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/art_manifests.json
        -P ${CMAKE_SOURCE_DIR}/cmake/join_json.cmake
    COMMAND ${CMAKE_COMMAND} -DNAME=g_embeddedArtManifests -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/art_manifests.json
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/embedded_art_manifests.cpp -P ${CMAKE_SOURCE_DIR}/cmake/embed_file.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/cmake/join_json.cmake ${CMAKE_SOURCE_DIR}/cmake/embed_file.cmake ${ARKEYAN_ART_MANIFESTS}
    COMMENT "Embedding the art manifests" VERBATIM)
set(ARKEYAN_ART_SOURCES
    ${ARKEYAN_RUNTIME_DIR}/host/art/alchemy_archive.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/art/disc_languages.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/art/game_art.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/art/game_text.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/art/lux_images.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/art/stfs.cpp
    ${ARKEYAN_RUNTIME_DIR}/host/art/xenos_texture.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/embedded_art_manifests.cpp)

# The pictures' readers as a library, for Arkeyan-Builder (the game's icon is made from the disc's logo: tools/arkeyan-builder).
add_library(ArkeyanArtReader STATIC ${ARKEYAN_ART_SOURCES} ${ARKEYAN_COLLECTION_SOURCES})
target_include_directories(ArkeyanArtReader PUBLIC ${ARKEYAN_RUNTIME_DIR})
target_include_directories(ArkeyanArtReader SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/stb) # a dashboard backdrop is a JPEG
target_link_libraries(ArkeyanArtReader PUBLIC SDL3::SDL3-static)
