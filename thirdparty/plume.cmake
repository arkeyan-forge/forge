# plume (github.com/renderbag/plume, MIT), the render hardware interface under runtime/gpu. Built here instead of with
# its own CMakeLists so each platform compiles only its backends (Metal on Apple, D3D12 + Vulkan on Windows, Vulkan
# through SDL3 elsewhere) and so it does not set CMAKE_OSX_DEPLOYMENT_TARGET for the whole project.
set(PLUME_DIR ${CMAKE_CURRENT_LIST_DIR}/plume)
set(PLUME_VULKAN_INCLUDES
    ${PLUME_DIR}/contrib/volk
    ${PLUME_DIR}/contrib/Vulkan-Headers/include
    ${PLUME_DIR}/contrib/VulkanMemoryAllocator/include)

if(APPLE AND ARKEYAN_MACOS_VULKAN)
    # Test configuration: Vulkan over MoltenVK, for the validation layers (docs/runtime/gpu/plume_backends.md).
    # ARKEYAN_VULKAN_VALIDATION=ON also turns the validation layer on in a Release build (plume enables it only without NDEBUG).
    enable_language(OBJC OBJCXX)
    add_library(plume STATIC ${PLUME_DIR}/plume_vulkan.cpp ${PLUME_DIR}/plume_apple.mm)
    target_include_directories(plume PUBLIC ${PLUME_VULKAN_INCLUDES})
    target_compile_definitions(plume PUBLIC PLUME_SDL3_VULKAN_ENABLED)
    target_link_libraries(plume PUBLIC "-framework AppKit" "-framework Foundation" "-framework QuartzCore")
    if(ARKEYAN_VULKAN_VALIDATION)
        target_compile_definitions(plume PRIVATE VULKAN_VALIDATION_LAYER_ENABLED=)
    endif()
    target_link_libraries(plume PUBLIC SDL3::Headers)
elseif(APPLE)
    enable_language(OBJC OBJCXX)
    add_library(plume STATIC ${PLUME_DIR}/plume_metal.cpp ${PLUME_DIR}/plume_apple.mm)
    target_include_directories(plume PRIVATE ${PLUME_DIR}/contrib/metal-cpp)
    target_compile_definitions(plume PRIVATE PLUME_APPLE_RETINA_ENABLED)
    target_link_libraries(plume PUBLIC "-framework AppKit" "-framework Foundation" "-framework IOKit" "-framework Metal"
        "-framework QuartzCore")
elseif(ARKEYAN_UWP)
    # Xbox Dev Mode: D3D12 only, presenting to the app's CoreWindow (patches/plume.patch).
    add_library(plume STATIC ${PLUME_DIR}/plume_d3d12.cpp ${PLUME_DIR}/contrib/D3D12MemoryAllocator/src/D3D12MemAlloc.cpp)
    target_include_directories(plume PUBLIC ${PLUME_DIR}/contrib/D3D12MemoryAllocator/include
        PRIVATE ${CMAKE_SOURCE_DIR}/cmake/mingw)
    target_compile_definitions(plume PUBLIC NOMINMAX PLUME_D3D12_COREWINDOW)
    # uuid: GUID constants (IID_IUnknown), which desktop builds get from CMake's default Windows libraries.
    target_link_libraries(plume PUBLIC d3d12 dxgi uuid)
elseif(WIN32)
    add_library(plume STATIC ${PLUME_DIR}/plume_d3d12.cpp ${PLUME_DIR}/plume_vulkan.cpp
        ${PLUME_DIR}/contrib/D3D12MemoryAllocator/src/D3D12MemAlloc.cpp)
    target_include_directories(plume PUBLIC ${PLUME_VULKAN_INCLUDES} ${PLUME_DIR}/contrib/D3D12MemoryAllocator/include)
    target_compile_definitions(plume PUBLIC NOMINMAX)
    target_link_libraries(plume PUBLIC d3d12 dxgi)
    if(MINGW)
        target_include_directories(plume PRIVATE ${CMAKE_SOURCE_DIR}/cmake/mingw)
    endif()
else()
    add_library(plume STATIC ${PLUME_DIR}/plume_vulkan.cpp)
    target_include_directories(plume PUBLIC ${PLUME_VULKAN_INCLUDES})
    target_compile_definitions(plume PUBLIC PLUME_SDL3_VULKAN_ENABLED)
    target_link_libraries(plume PUBLIC SDL3::Headers)
endif()

target_include_directories(plume PUBLIC ${PLUME_DIR})
set_target_properties(plume PROPERTIES CXX_STANDARD 17 OBJCXX_STANDARD 17 CXX_VISIBILITY_PRESET hidden)
