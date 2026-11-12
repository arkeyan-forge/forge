#!/bin/sh
# Runs a macOS Vulkan test build (-DARKEYAN_MACOS_VULKAN=ON, docs/runtime/gpu/plume_backends.md) on MoltenVK: the loader and
# the driver are Homebrew's (brew install molten-vk vulkan-loader). Set as LIVE_EXE (the hub tests) or GAME_EXE
# (tools/perf/game_perf.sh) with VK_GAME_EXE naming the game binary: the paths are set here, after any `nice`, which is a
# protected binary and drops DYLD_LIBRARY_PATH.
#
#   VK_GAME_EXE=<build-vk>/ArkeyanRuntime/<game>.app/Contents/MacOS/<game> LIVE_EXE=$PWD/tools/regress/moltenvk_exe.sh \
#       tools/regress/superchargers_lobby.sh <run name>
brew=${HOMEBREW_PREFIX:-/opt/homebrew}
export VK_ICD_FILENAMES=$brew/opt/molten-vk/etc/vulkan/icd.d/MoltenVK_icd.json
export DYLD_LIBRARY_PATH=$brew/opt/vulkan-loader/lib:$brew/opt/molten-vk/lib
exec "${VK_GAME_EXE:?set VK_GAME_EXE to the game binary of the Vulkan build}" "$@"
