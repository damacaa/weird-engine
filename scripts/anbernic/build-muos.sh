#!/bin/bash
# Generic cross-compiler for muOS
set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <path_to_project>"
    exit 1
fi

PROJECT_DIR=$(realpath "$1")
WEIRD_ENGINE_DIR=$(realpath "$(dirname "$0")/../..")

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

if [ "$PROJECT_DIR" = "$WEIRD_ENGINE_DIR" ]; then
    MOUNT_WEIRD_ENGINE=""
    TOOLCHAIN_FILE="/workspace/toolchain-aarch64.cmake"
else
    MOUNT_WEIRD_ENGINE="-v $WEIRD_ENGINE_DIR:/weird-engine:z"
    TOOLCHAIN_FILE="/weird-engine/toolchain-aarch64.cmake"
fi

podman run --rm \
    -v "$PROJECT_DIR:/workspace:z" \
    $MOUNT_WEIRD_ENGINE \
    -w /workspace \
    "$IMAGE" bash -c "
set -e
export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/

cmake -B build-muos -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \
  -DWEIRD_USE_FBDEV_EGL=ON \
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

rm -f /weird-engine/lib/libWeirdEngine.a 2>/dev/null || true

cmake --build build-muos -j\$(nproc)
"

echo "== Build finished in $PROJECT_DIR/build-muos"
