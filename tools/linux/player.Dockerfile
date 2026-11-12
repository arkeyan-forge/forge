# A player's Linux machine for tools/linux/kit_test.sh: no compiler, no -dev package, only what a desktop has (curl,
# tar, xz, libstdc++, zlib) and a Vulkan driver (Mesa's software lavapipe here). Nothing of the game or the kit is in
# the image: both are mounted.
#
#   docker build --platform linux/amd64 -f tools/linux/player.Dockerfile --build-arg BASE=ubuntu:22.04 -t ssa-player-ubuntu tools/linux
#   docker build --platform linux/amd64 -f tools/linux/player.Dockerfile --build-arg BASE=archlinux:latest -t ssa-player-arch tools/linux
ARG BASE=ubuntu:22.04
FROM $BASE

RUN if command -v apt-get > /dev/null; then \
        apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
            ca-certificates curl xz-utils libvulkan1 mesa-vulkan-drivers && rm -rf /var/lib/apt/lists/*; \
    elif command -v pacman > /dev/null; then \
        pacman -Syu --noconfirm --disable-sandbox curl xz tar vulkan-icd-loader vulkan-swrast && pacman -Scc --noconfirm; \
    elif command -v dnf > /dev/null; then \
        dnf install -y curl xz tar vulkan-loader mesa-vulkan-drivers && dnf clean all; \
    fi && \
    if command -v cc c++ gcc clang > /dev/null 2>&1; then echo "error: a compiler is installed" >&2; exit 1; fi
ENV HOME=/tmp
