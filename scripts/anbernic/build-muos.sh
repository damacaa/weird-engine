#!/bin/bash
# Cross-compiles WeirdSamples for the Anbernic RG35XXH (aarch64, muOS).
#
# Uses a persistent podman image (weird-muos-builder) with all dependencies
# preinstalled so rebuilds are incremental and fast. Delete the image with
#   podman rmi weird-muos-builder
# to force a fresh toolchain setup.

set -e
cd "$(dirname "$0")/../.."

IMAGE=weird-muos-builder

if ! podman image exists "$IMAGE"; then
  echo "== Building toolchain image '$IMAGE' (one time only)..."
  podman build -t "$IMAGE" - <<'EOF'
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN dpkg --add-architecture arm64 \
 && sed -i 's/^deb http/deb [arch=amd64] http/g' /etc/apt/sources.list \
 && printf '%s\n' \
      'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy main restricted universe multiverse' \
      'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-updates main restricted universe multiverse' \
      'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-security main restricted universe multiverse' \
      >> /etc/apt/sources.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      gcc-aarch64-linux-gnu g++-aarch64-linux-gnu cmake make git pkg-config \
      libdrm-dev:arm64 libgbm-dev:arm64 libegl1-mesa-dev:arm64 libgles2-mesa-dev:arm64 \
      libudev-dev:arm64 libasound2-dev:arm64 \
 && rm -rf /var/lib/apt/lists/*
EOF
fi

podman run --rm -v "$(pwd):/workspace:z" -w /workspace "$IMAGE" bash -c "
set -e
export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/

cmake -B build-muos -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DWEIRD_DISABLE_IMGUI=ON \
  -DCMAKE_TOOLCHAIN_FILE=toolchain-aarch64.cmake \
  -DWEIRD_ENGINE_BUILD_EXAMPLES=ON \
  -DWEIRD_USE_FBDEV_EGL=ON \
  -DWEIRD_ENGINE_USE_RUNTIME_ASSETS=ON \
  -DDEPLOY_STANDALONE=ON \
  -DCMAKE_EXE_LINKER_FLAGS='-static-libgcc -static-libstdc++' \
  -DSDL_UNIX_CONSOLE_BUILD=ON \
  -DSDL_KMSDRM=ON \
  -DSDL_KMSDRM_SHARED=ON \
  -DSDL_DEPS_SHARED=ON \
  -DSDL_ALSA=ON \
  -DSDL_ALSA_SHARED=ON \
  -DALSA_INCLUDE_DIR=/usr/include \
  -DALSA_LIBRARY=/usr/lib/aarch64-linux-gnu/libasound.so

cmake --build build-muos -j\$(nproc) --target WeirdSamples

# Ship the SDL3 shared library next to the executable
find build-muos -name 'libSDL3.so*' -exec cp -a {} build-muos/examples/sample-scenes/ \;
"

echo "== Build finished: build-muos/examples/sample-scenes/WeirdSamples"
