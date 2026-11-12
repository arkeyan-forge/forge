# Test image for tools/windows/d3d12_test.sh: Wine 10 and Mesa's lavapipe (a conformant CPU Vulkan driver) on x86_64,
# with an X server for Wine's display driver. vkd3d-proton is not in the image: the script puts its d3d12.dll next to
# the executable. Nothing from the checkout is copied in.
FROM debian:trixie
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    wine64 mesa-vulkan-drivers libvulkan1 vulkan-validationlayers xvfb xauth ca-certificates && rm -rf /var/lib/apt/lists/*
