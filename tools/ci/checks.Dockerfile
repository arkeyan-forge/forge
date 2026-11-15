# The checks of tools/ci/run.sh that need Linux: clang-format and clang-tidy, the host tests on Linux and with
# AddressSanitizer, and the Windows tests (llvm-mingw, the kit's toolchain) under Wine. amd64: the build runs
# XenosRecomp's dxc-bin (x86_64 only). No game data: the checkout is copied into the job's volume, never into the image.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
# The repository's one clang-format version (docs/CONVENTIONS.md, tools/git-hooks/pre-commit).
ARG CLANG_FORMAT=23.1.3

ARG TARGETARCH
RUN if [ "$TARGETARCH" = amd64 ]; then dpkg --add-architecture i386; wine="wine wine64 xvfb xauth"; fi && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        ninja-build cmake clang clang-tidy libclang-rt-18-dev git python3 python3-pip rsync curl xz-utils ca-certificates \
        libusb-1.0-0-dev zlib1g-dev libvulkan-dev libx11-dev ${wine:-} && \
    rm -rf /var/lib/apt/lists/* && \
    pip3 install --no-cache-dir --break-system-packages clang-format==$CLANG_FORMAT

# The jobs run as an ordinary user, as on a player's machine: root passes the permission checks the tests expect to fail.
RUN useradd -m ci && mkdir -p /work && chown ci /work
USER ci
ENV HOME=/tmp LANG=C.UTF-8 LC_ALL=C.UTF-8 WINEDEBUG=-all WINEDLLOVERRIDES=mscoree,mshtml=
WORKDIR /work
