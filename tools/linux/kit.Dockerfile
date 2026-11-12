# Builds the Linux player kit (tools/make_kit.sh linux), amd64 only. The project is mounted at /src, never copied into
# the image. The compiler is the one the player's Arkeyan-Builder downloads: zig's bundled clang (ZIG_VERSION, checked by
# SHA-256), targeting glibc 2.34 (the baseline of the kit's prebuilt dxc) with zig's own libc++, so the kit's libraries and the player's game code share one C++
# library and ABI (docs/tools/arkeyan-builder.md). The -dev packages only give SDL's configure the X11, Wayland and audio headers:
# SDL loads those libraries at run time, the game links none of them.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ARG ZIG_VERSION=0.17.0
ARG ZIG_SHA256=1cbe9df9f27e6b78d14ccbca43b6703a404ef79ef1c463de901d7f088d4e2026

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        cmake ninja-build make pkg-config git python3 perl curl xz-utils ca-certificates nasm file zip rsync binutils \
        zlib1g-dev \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
        libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl-dev libgl-dev libdrm-dev libgbm-dev \
        libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev libusb-1.0-0-dev && \
    rm -rf /var/lib/apt/lists/* && \
    curl -fsSL -o /tmp/zig.tar.xz "https://ziglang.org/download/$ZIG_VERSION/zig-x86_64-linux-$ZIG_VERSION.tar.xz" && \
    echo "$ZIG_SHA256  /tmp/zig.tar.xz" | sha256sum -c - && \
    mkdir /opt/zig && tar -xJf /tmp/zig.tar.xz -C /opt/zig --strip-components=1 && rm /tmp/zig.tar.xz && \
    printf '#!/bin/sh\nexec /opt/zig/zig cc -target x86_64-linux-gnu.2.34 "$@"\n' > /usr/local/bin/zig-cc && \
    printf '#!/bin/sh\nexec /opt/zig/zig c++ -target x86_64-linux-gnu.2.34 "$@"\n' > /usr/local/bin/zig-c++ && \
    chmod +x /usr/local/bin/zig-cc /usr/local/bin/zig-c++

# The build runs as the caller's user; the configure step runs git apply in the mounted submodules.
RUN git config --system --add safe.directory '*'
ENV HOME=/tmp ZIG_GLOBAL_CACHE_DIR=/tmp/zig-cache ZIG_LOCAL_CACHE_DIR=/tmp/zig-cache
WORKDIR /src
