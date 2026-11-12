# Build and test environment for the Linux runtime (tools/build_linux.sh). The project is mounted at /src, never copied
# into the image: ppc/ and the shader cache are derived from the player's disc.
#
# Native on arm64 and x86_64. On arm64 the prebuilt dxc of XenosRecomp's dxc-bin (x86_64 only) runs through the host's
# x86 emulation (OrbStack's Rosetta, or qemu-user binfmt), which needs the amd64 C/C++ runtime next to the arm64 one.
FROM ubuntu:24.04

ARG TARGETARCH
ENV DEBIAN_FRONTEND=noninteractive

RUN if [ "$TARGETARCH" = arm64 ]; then \
        dpkg --add-architecture amd64 && \
        sed -i 's/^Types: deb$/Types: deb\nArchitectures: arm64/' /etc/apt/sources.list.d/ubuntu.sources && \
        printf 'Types: deb\nURIs: http://archive.ubuntu.com/ubuntu\nSuites: noble noble-updates noble-security\nComponents: main universe\nArchitectures: amd64\nSigned-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n' \
            > /etc/apt/sources.list.d/amd64.sources; \
    fi && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        # Toolchain: the generated game code needs clang (__builtin_assume, __builtin_rotateleft*).
        clang lld cmake ninja-build make pkg-config git python3 perl curl xz-utils ca-certificates nasm file \
        zlib1g-dev \
        # SDL3: X11, Wayland, audio, input.
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
        libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl-dev libgl-dev libdrm-dev libgbm-dev \
        libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev libusb-1.0-0-dev \
        # Test runs: Vulkan loader, Mesa's software Vulkan (lavapipe), a virtual X server.
        libvulkan1 mesa-vulkan-drivers vulkan-tools xvfb xauth && \
    if [ "$TARGETARCH" = arm64 ]; then \
        apt-get install -y --no-install-recommends libc6:amd64 libstdc++6:amd64 libgcc-s1:amd64 zlib1g:amd64; \
    fi && \
    rm -rf /var/lib/apt/lists/*

# The build runs as the caller's user (build_linux.sh); the configure step runs git apply in the mounted submodules.
RUN git config --system --add safe.directory '*'
ENV CC=clang CXX=clang++ HOME=/tmp
WORKDIR /src
