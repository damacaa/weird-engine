#!/bin/bash
# WeirdSamples launcher for muOS (RG35XXH) — PortMaster compatible.
# Runs graphics-stack diagnostics, then the game (fbdev EGL first,
# SDL3/kmsdrm fallback), writing everything to log.txt next to the game.

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"

GAMEDIR="/$directory/ports/weird-samples"
cd "$GAMEDIR" || exit 1

export LD_LIBRARY_PATH="$GAMEDIR:$LD_LIBRARY_PATH"
chmod +x ./WeirdSamples 2>/dev/null

LOG="$GAMEDIR/log.txt"
: > "$LOG"

diag() {
  echo "================ WeirdSamples muOS diagnostics ================"
  date
  echo "--- uname ---"
  uname -a
  echo "--- muOS version ---"
  cat /opt/muos/config/version.txt 2>/dev/null
  cat /opt/muos/config/system/version 2>/dev/null
  echo "--- glibc ---"
  ls -la /lib/libc.so.6 2>/dev/null
  /lib/libc.so.6 2>&1 | head -n 1
  echo "--- environment ---"
  env | sort
  echo "--- display devices ---"
  ls -la /dev/dri/ 2>&1
  ls -la /dev/fb* 2>&1
  ls -la /dev/mali* 2>&1
  echo "--- fb0 info ---"
  for f in name virtual_size bits_per_pixel mode modes state; do
    printf "%s: " "$f"; cat "/sys/class/graphics/fb0/$f" 2>/dev/null
  done
  echo "--- kernel modules (gpu/drm) ---"
  grep -iE 'mali|gpu|drm' /proc/modules 2>/dev/null
  echo "--- graphics libraries ---"
  find /usr/lib /lib /opt -maxdepth 4 \
    \( -name 'libMali*' -o -name 'libEGL*' -o -name 'libGLES*' \
       -o -name 'libgbm*' -o -name 'libdrm*' \) 2>/dev/null | sort | while read -r lib; do
    printf '%s -> %s\n' "$lib" "$(readlink -f "$lib")"
  done
  echo "--- libgbm symbol check (gbm_surface_lock_front_buffer) ---"
  find /usr/lib /lib /opt "$GAMEDIR" -maxdepth 4 -name 'libgbm*' 2>/dev/null | sort -u | while read -r lib; do
    real=$(readlink -f "$lib")
    count=$(grep -c 'gbm_surface_lock_front_buffer' "$real" 2>/dev/null)
    echo "$lib ($real): symbol strings = $count"
  done
  echo "--- ldd WeirdSamples ---"
  ldd ./WeirdSamples 2>&1
  echo "--- ldd libSDL3.so.0 ---"
  ldd ./libSDL3.so.0 2>&1
  echo "==============================================================="
}

run_test() {
  name="$1"; shift
  echo ""
  echo "############ RUN: $name ############"
  echo "cmd: env $* ./WeirdSamples"
  if command -v timeout >/dev/null 2>&1; then
    env "$@" timeout -s KILL 90 ./WeirdSamples
  else
    env "$@" ./WeirdSamples
  fi
  code=$?
  echo "############ EXIT ($name): $code ############"
  echo ""
  return $code
}

{
  diag

  run_test "fbdev-egl" \
    WEIRD_VIDEO_BACKEND=fbdev \
    WEIRD_AUTO_QUIT_SECONDS=20 \
    WEIRD_SCREENSHOT_FRAME=90
  fbdev_code=$?

  if [ $fbdev_code -ne 0 ]; then
    run_test "sdl-kmsdrm" \
      WEIRD_VIDEO_BACKEND=sdl \
      SDL_VIDEO_DRIVER=kmsdrm \
      WEIRD_AUTO_QUIT_SECONDS=20 \
      WEIRD_SCREENSHOT_FRAME=90
  fi

  echo "=== DONE ==="
} >> "$LOG" 2>&1

sync
