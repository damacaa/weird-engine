#!/bin/bash
# Builds (unless --no-build), packages and deploys WeirdSamples to the
# Anbernic RG35XXH connected via MTP (KDE kio).
#
# Device layout used:
#   mtp:/RG35XX-H/SD2/ports/weird-samples/     <- game files
#   mtp:/RG35XX-H/SD2/Roms/PORTS/Weird Samples.sh  <- launcher

set -e
cd "$(dirname "$0")/../.."

MTP_GAME="mtp:/RG35XX-H/SD2/ports/weird-samples"
MTP_PORTS="mtp:/RG35XX-H/SD2/Roms/PORTS"
BUILD_DIR="build-muos/examples/sample-scenes"
STAGE="dist-muos"

BUILD="yes"
LAUNCHER="scripts/anbernic/Weird_Samples_MTP.sh"

for arg in "$@"; do
  if [ "$arg" = "--no-build" ]; then
    BUILD="no"
  elif [ "$arg" = "--diag" ]; then
    LAUNCHER="scripts/anbernic/Weird Samples.sh"
  elif [ "$arg" = "--mtp" ]; then
    LAUNCHER="scripts/anbernic/Weird_Samples_MTP.sh"
  fi
done

if [ "$BUILD" = "yes" ]; then
  ./scripts/anbernic/build-muos.sh
fi

echo "== Staging files in $STAGE/"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$BUILD_DIR/WeirdSamples" "$STAGE/"
# Ship the shared lib under its SONAME so no symlinks are needed (MTP-safe)
cp "$BUILD_DIR/libSDL3.so.0.2.16" "$STAGE/libSDL3.so.0"
cp -r "$BUILD_DIR/assets" "$STAGE/"
cp -r "$BUILD_DIR/fonts" "$STAGE/"
cp -r "$BUILD_DIR/shaders" "$STAGE/"
cp "$LAUNCHER" "$STAGE/Weird Samples.sh"
# Strip stale junk the build system leaves behind
rm -rf "$STAGE/shaders/.vscode" 2>/dev/null || true

echo "== Cleaning stale files on device..."
for f in libgbm.so.1 Makefile cmake_install.cmake find_log.txt \
         "Weird Samples.sh" CMakeFiles log.txt \
         assets fonts shaders; do
  kioclient5 --noninteractive rm "$MTP_GAME/$f" >/dev/null 2>&1 && echo "  removed $f" || true
done

echo "== Uploading game files to $MTP_GAME ..."
for item in WeirdSamples libSDL3.so.0 assets fonts shaders; do
  echo "  -> $item"
  kioclient5 --noninteractive --overwrite cp "$PWD/$STAGE/$item" "$MTP_GAME/"
done

echo "== Uploading launcher to $MTP_PORTS ..."
kioclient5 --noninteractive --overwrite cp "$PWD/$STAGE/Weird Samples.sh" "$MTP_PORTS/"

echo "== Deploy complete."
echo "   On the device, run 'Weird Samples' from the Ports menu."
echo "   Afterwards fetch the log with:  ./scripts/anbernic/fetch-logs.sh"
