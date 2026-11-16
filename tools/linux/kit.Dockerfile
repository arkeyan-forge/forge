# Builds the Linux player kit (tools/make_kit.sh linux), an x86-64 kit, in an image of the build machine's architecture
# (amd64 or arm64): zig's clang cross-compiles, so an Apple Silicon Mac compiles natively instead of under emulation.
# The project is mounted at /src, never copied into the image. The compiler is the one the player's Arkeyan-Builder
# downloads: zig's bundled clang (ZIG_VERSION, checked by SHA-256), targeting glibc 2.34 (the baseline of the kit's
# prebuilt dxc) with zig's own libc++, so the kit's libraries and the player's game code share one C++ library and ABI
# (docs/tools/arkeyan-builder.md). Everything the build reads or runs for the target is amd64 (Ubuntu's multiarch
# packages on arm64): libz.a and zlib.h, the X11, Wayland and audio headers SDL's configure looks for (SDL loads those
# libraries at run time, the game links none of them), the binutils that archive and strip x86-64 objects, and the
# runtime libraries of the x86-64 programs the build runs (dxc, configure probes), emulated on arm64.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ARG TARGETARCH
ARG ZIG_VERSION=0.17.0
ARG ZIG_SHA256_AMD64=1cbe9df9f27e6b78d14ccbca43b6703a404ef79ef1c463de901d7f088d4e2026
ARG ZIG_SHA256_ARM64=9e8d11661d4ae3bd57702a3832781e23ad151dde5798e16a5ccd503f65234ff8

# On arm64, Ubuntu serves its own packages from ports.ubuntu.com and the amd64 ones from the main archive: each source
# is limited to its architecture, or apt looks for each architecture in both.
RUN set -eu; \
    case "$TARGETARCH" in \
        amd64) x86= zig=x86_64 sha=$ZIG_SHA256_AMD64 ;; \
        arm64) \
            x86=:amd64 zig=aarch64 sha=$ZIG_SHA256_ARM64; \
            dpkg --add-architecture amd64; \
            sed -i '/^Types:/a Architectures: arm64' /etc/apt/sources.list.d/ubuntu.sources; \
            printf '%s\n' 'Types: deb' 'URIs: http://archive.ubuntu.com/ubuntu/' 'Suites: noble noble-updates noble-backports' \
                'Components: main universe restricted multiverse' 'Architectures: amd64' \
                'Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg' '' \
                'Types: deb' 'URIs: http://security.ubuntu.com/ubuntu/' 'Suites: noble-security' \
                'Components: main universe restricted multiverse' 'Architectures: amd64' \
                'Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg' > /etc/apt/sources.list.d/ubuntu-amd64.sources ;; \
        *) echo "unsupported architecture: $TARGETARCH" >&2; exit 1 ;; \
    esac; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        cmake ninja-build make pkg-config git python3 perl curl xz-utils ca-certificates nasm file zip rsync ccache \
        binutils binutils-x86-64-linux-gnu wayland-protocols \
        $(for p in libc6 libstdc++6 zlib1g-dev \
            libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
            libxkbcommon-dev libwayland-dev libdecor-0-dev libegl-dev libgl-dev libdrm-dev libgbm-dev \
            libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev libusb-1.0-0-dev; do echo "$p$x86"; done); \
    rm -rf /var/lib/apt/lists/*; \
    for t in addr2line ar as nm objcopy objdump ranlib readelf size strings strip; do \
        ln -s "/usr/bin/x86_64-linux-gnu-$t" "/usr/local/bin/$t"; \
    done; \
    curl -fsSL -o /tmp/zig.tar.xz "https://ziglang.org/download/$ZIG_VERSION/zig-$zig-linux-$ZIG_VERSION.tar.xz"; \
    echo "$sha  /tmp/zig.tar.xz" | sha256sum -c -; \
    mkdir /opt/zig; tar -xJf /tmp/zig.tar.xz -C /opt/zig --strip-components=1; rm /tmp/zig.tar.xz; \
    printf '#!/bin/sh\nexec /opt/zig/zig cc -target x86_64-linux-gnu.2.34 "$@"\n' > /usr/local/bin/zig-cc; \
    printf '#!/bin/sh\nexec /opt/zig/zig c++ -target x86_64-linux-gnu.2.34 "$@"\n' > /usr/local/bin/zig-c++; \
    chmod +x /usr/local/bin/zig-cc /usr/local/bin/zig-c++

# pkg-config looks in the amd64 folders on any host (its default search path on amd64).
ENV PKG_CONFIG_LIBDIR=/usr/local/lib/x86_64-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig
# The build runs as the caller's user; the configure step runs git apply in the mounted submodules.
RUN git config --system --add safe.directory '*'
ENV HOME=/tmp ZIG_GLOBAL_CACHE_DIR=/tmp/zig-cache ZIG_LOCAL_CACHE_DIR=/tmp/zig-cache
WORKDIR /src
