# Our changes to submodules are kept as patches in patches/; each is applied to a fresh checkout. Included by
# CMakeLists.txt, and run alone (cmake -P cmake/patches.cmake) by tools/make_kit.sh prepare, before the kits' parallel
# configures, which would otherwise apply them at the same time.
get_filename_component(ARKEYAN_PATCH_ROOT ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)

function(arkeyan_apply_patch directory patch)
    execute_process(COMMAND git apply --reverse --check ${patch}
        WORKING_DIRECTORY ${directory} RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
    if(NOT already_applied EQUAL 0)
        execute_process(COMMAND git apply ${patch} WORKING_DIRECTORY ${directory} RESULT_VARIABLE result)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Could not apply ${patch} to ${directory}")
        endif()
    endif()
endfunction()

# Patches that change the lines around each other's: applied in order unless the last one already is (the first's
# reverse check fails once a later one changed its context).
function(arkeyan_apply_patch_chain directory)
    list(GET ARGN -1 last)
    execute_process(COMMAND git apply --reverse --check ${last}
        WORKING_DIRECTORY ${directory} RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
    if(NOT already_applied EQUAL 0)
        foreach(patch IN LISTS ARGN)
            arkeyan_apply_patch(${directory} ${patch})
        endforeach()
    endif()
endfunction()

# XenonRecomp: missing instructions, fixes; then the Wii's write-gather pipe, its stores the host's (write_gather_pipe).
arkeyan_apply_patch_chain(${ARKEYAN_PATCH_ROOT}/thirdparty/XenonRecomp ${ARKEYAN_PATCH_ROOT}/patches/xenonrecomp.patch
    ${ARKEYAN_PATCH_ROOT}/patches/xenonrecomp_gather_pipe.patch)
# XenosRecomp: Skylanders shader conventions; shaders it cannot translate are skipped instead of aborting.
arkeyan_apply_patch(${ARKEYAN_PATCH_ROOT}/thirdparty/XenosRecomp ${ARKEYAN_PATCH_ROOT}/patches/xenosrecomp.patch)
# plume: MinGW headers, UWP CoreWindow swapchains, pipelines Metal rejects reported as null.
arkeyan_apply_patch(${ARKEYAN_PATCH_ROOT}/thirdparty/plume ${ARKEYAN_PATCH_ROOT}/patches/plume.patch)
# plume (on top of plume.patch): Metal command buffers of a queue run in order, as on D3D12 and Vulkan.
arkeyan_apply_patch(${ARKEYAN_PATCH_ROOT}/thirdparty/plume ${ARKEYAN_PATCH_ROOT}/patches/plume_queue_order.patch)
# plume (on top of the two above): R8G8B8A8_USCALED vertex format (Vulkan rejects an integer attribute read as float),
# and a Vulkan pipeline the driver rejects is returned as null like Metal's and D3D12's.
arkeyan_apply_patch(${ARKEYAN_PATCH_ROOT}/thirdparty/plume ${ARKEYAN_PATCH_ROOT}/patches/plume_vulkan_strict.patch)
# plume (on top of the three above): Metal render and depth targets without shader-write usage, which keeps their
# lossless compression on Apple GPUs (docs/runtime/gpu/plume_backends.md).
arkeyan_apply_patch(${ARKEYAN_PATCH_ROOT}/thirdparty/plume ${ARKEYAN_PATCH_ROOT}/patches/plume_metal_compression.patch)
# plume (on top of the four above): the swap chain's last acquire or present result (Vulkan: the VkResult), so the
# runtime can name a failure and rebuild the swap chain (docs/runtime/gpu/plume_backends.md).
arkeyan_apply_patch(${ARKEYAN_PATCH_ROOT}/thirdparty/plume ${ARKEYAN_PATCH_ROOT}/patches/plume_swapchain_result.patch)
# plume (on top of the five above): D3D12 pipelines give each render target its own blend and write mask
# (IndependentBlendEnable), as Metal and Vulkan do.
arkeyan_apply_patch(${ARKEYAN_PATCH_ROOT}/thirdparty/plume ${ARKEYAN_PATCH_ROOT}/patches/plume_d3d12_blend.patch)
# plume (on top of the six above): vsync that shows the newest frame at each refresh without waiting for the display
# (Vulkan MAILBOX, D3D12 sync interval 0 without tearing), and the frames the display has shown (D3D12 frame
# statistics, Metal presented handlers), for refresh rates that are not a multiple of the frame rate (gpu/present_mode.h);
# and, with vsync on, a minimum time each frame stays on the display (Metal presentAfterMinimumDuration), so a 60 FPS game
# on a 120 Hz display shows every frame for two refreshes.
arkeyan_apply_patch(${ARKEYAN_PATCH_ROOT}/thirdparty/plume ${ARKEYAN_PATCH_ROOT}/patches/plume_present_latest.patch)
