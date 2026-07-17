# Building & Installing WeirdSamples on the RG35XXH (muOS)

This guide covers cross-compiling the sample game for the Anbernic RG35XXH
(Allwinner H700, Mali-G31) running muOS, and installing/updating it over USB
(MTP). Everything is wrapped in three scripts:

| Script | Purpose |
|---|---|
| `./build-muos.sh` | Cross-compile for aarch64/muOS (fast, incremental) |
| `./deploy-muos.sh` | Build + package + copy to the device over MTP |
| `./fetch-logs.sh` | Pull `log.txt` and screenshots back from the device |

---

## 1. Prerequisites (host PC)

- **podman** — used for the Ubuntu 22.04 aarch64 cross toolchain
  (no ARM compilers needed on the host).
- **kioclient5** (KDE) — used for MTP file transfer. The device must be
  plugged in via USB with MTP enabled and reachable as `mtp:/RG35XX-H/`
  (check with: `kioclient5 ls "mtp:/RG35XX-H/SD2/"`).
- The muOS SD2 card must contain the port folder and launcher:
  - `SD2/ports/weird-samples/` (game files — created automatically on first deploy)
  - `SD2/Roms/PORTS/Weird Samples.sh` (launcher — deployed by the script)

## 2. Build

```bash
./build-muos.sh
```

- The first run builds a persistent podman image named `weird-muos-builder`
  (Ubuntu 22.04 + aarch64 gcc + Mesa/ALSA dev libs). This takes a few minutes
  and only happens once.
- Subsequent runs reuse the image and the `build-muos/` CMake tree, so
  rebuilds are incremental and quick.
- Output: `build-muos/examples/sample-scenes/WeirdSamples` (+ `libSDL3.so.0.*`).

Useful overrides:

```bash
# Force a from-scratch CMake configure + rebuild
rm -rf build-muos && ./build-muos.sh

# Force recreation of the toolchain image itself
podman rmi weird-muos-builder && ./build-muos.sh
```

Build options baked in (see `build-muos.sh`): Release, ImGui disabled
(`WEIRD_DISABLE_IMGUI=ON`), fbdev EGL backend enabled
(`WEIRD_USE_FBDEV_EGL=ON`), runtime assets (`./shaders`, `./fonts`, `./assets`
loaded from the game directory), statically linked libgcc/libstdc++.

## 3. Install / update on the device

Connect the RG35XXH via USB (MTP), then:

```bash
./deploy-muos.sh            # build + deploy
./deploy-muos.sh --no-build # deploy only (skip the build step)
```

The script:
1. Stages files into `dist-muos/` (`WeirdSamples`, `libSDL3.so.0`,
   `assets/`, `fonts/`, `shaders/`, launcher script).
2. Removes stale files from the device (old logs, build junk, previous
   `assets/fonts/shaders`).
3. Uploads the game files to `SD2/ports/weird-samples/`.
4. Uploads the launcher to `SD2/Roms/PORTS/Weird Samples.sh`.

> MTP notes: symlinks can't be transferred, so `libSDL3.so.0` is uploaded as a
> real file. Folder overwrites are not supported either — the script deletes
> remote folders before re-uploading them.

## 4. Run on the device

On the handheld: **Ports → Weird Samples**.

The launcher writes a full log to `SD2/ports/weird-samples/log.txt`:
graphics-stack diagnostics, then the game run. With the current test
configuration the game **quits by itself after 20 seconds**
(`WEIRD_AUTO_QUIT_SECONDS`) and saves a **screenshot on frame 90**
(`WEIRD_SCREENSHOT_FRAME`) into the game folder. A 90s watchdog
(`timeout -s KILL`) guarantees the console returns to muOS even if the game
hangs.

## 5. Fetch results

```bash
./fetch-logs.sh
```

Downloads `log.txt` and any `screenshot_*.bmp` into
`device-logs/<timestamp>/` and prints the log.

## 6. Runtime environment variables (testing hooks)

Set in `Weird Samples.sh` when launching `WeirdSamples`:

| Variable | Values | Effect |
|---|---|---|
| `WEIRD_VIDEO_BACKEND` | `fbdev` (default) / `sdl` | `fbdev`: direct framebuffer EGL (the working path on muOS). `sdl`: SDL3's own windowing (kmsdrm) — kept for comparison, does not work on muOS (no GBM). |
| `WEIRD_AUTO_QUIT_SECONDS` | e.g. `20` | Cleanly exits the game after N seconds. Unset = run forever (normal game behavior). |
| `WEIRD_SCREENSHOT_FRAME` | e.g. `90` | Saves `screenshot_<time>.bmp` on frame N (proves rendering works even with a black screen). |

For a "real" installation (no diagnostics, no auto-quit), edit
`Weird Samples.sh` and remove the diagnostics block and the env vars.

## 7. How the video path works (why fbdev?)

muOS on the H700 ships ARM's proprietary **fbdev** Mali driver
(`/usr/lib/libmali.so`, `/dev/mali0`, `/dev/fb0`). There is **no GBM/KMS**
stack, so SDL3's kmsdrm video driver can never work here. The engine
therefore:

1. Initializes SDL3 with the `dummy` video driver (events/timers/audio only).
2. Creates the OpenGL ES context itself via `eglGetDisplay(EGL_DEFAULT_DISPLAY)`
   + `eglCreateWindowSurface` with the Mali `fbdev_window` struct
   (`src/weird-renderer/core/WeirdFBDevEGL.cpp`).
3. Presents frames with `eglSwapBuffers`.

## 8. Troubleshooting

- **`error while loading shared libraries: libSDL3.so.0`** — the shared lib
  wasn't copied next to the game; re-run `./deploy-muos.sh --no-build`.
- **Black screen but exit code 0** — check `log.txt` for the EGL section and
  look at the screenshot; report both.
- **MTP errors during deploy** — unplug/replug the device, confirm
  `kioclient5 ls "mtp:/RG35XX-H/SD2/"` works, retry.
- **Game won't start from the Ports menu** — make sure the launcher exists at
  `SD2/Roms/PORTS/Weird Samples.sh` and the game folder is
  `SD2/ports/weird-samples/` (both lowercase `ports`).
- **Weird CMake state after changing flags** — `rm -rf build-muos` and build
  again.
