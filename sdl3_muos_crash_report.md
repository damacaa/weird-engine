# Technical Report: SDL3 / KMSDRM Crash on Anbernic RG35XXH (muOS)

## Overview
We are attempting to run a custom C++ game engine (`WeirdEngine`) compiled with **SDL3** and **OpenGL ES 3.0** on an **Anbernic RG35XXH** running **muOS 2601.1**. The device features an Allwinner H700 SoC with a Mali-G31 MP2 GPU. The application is launched via a PortMaster integration script (`control.txt`). 

The current blocker is a runtime crash immediately upon execution: 
`error while loading shared libraries: libgbm.so.1: cannot open shared object file: No such file or directory`

This is unexpected because we have explicitly configured the engine and SDL3 to load the KMSDRM/GBM libraries dynamically via `dlopen`, and verified that the compiled binaries have no hard-linked `libgbm.so.1` dependency.

## Build Environment & Configuration
- **Host:** Ubuntu 22.04 `aarch64` container (via Podman).
- **Target OS:** muOS 2601.1 / PortMaster.
- **Engine Stack:** C++20, SDL3, GLAD (GLES 3.0).
- **SDL3 Configuration:** 
  The engine builds SDL3 from source as a CMake subdirectory. We pass the following definitions to force SDL3's KMSDRM driver to use dynamic loading:
  ```cmake
  target_compile_definitions(SDL3-shared PRIVATE 
      SDL_VIDEO_DRIVER_KMSDRM_DYNAMIC="libdrm.so.2"
      SDL_VIDEO_DRIVER_KMSDRM_DYNAMIC_GBM="libgbm.so.1"
  )
  ```
  We also manually stripped `PC_LIBDRM` and `PC_GBM` from SDL3's `LINK_LIBRARIES` target properties to prevent CMake's `pkg-config` module from hard-linking the libraries during the build.

## Timeline of the Issue

### 1. Initial Crash: `undefined symbol: gbm_surface_lock_front_buffer`
Initially, when the application was launched, it crashed with:
`symbol lookup error: undefined symbol: gbm_surface_lock_front_buffer`

This indicated that while a `libgbm.so.1` was found in the environment, it lacked the `gbm_surface_lock_front_buffer` function required by SDL3's KMSDRM video backend for hardware page flipping. 

### 2. Attempted Fix: Patching SDL3
To bypass the missing symbol, we patched SDL3's `SDL_kmsdrmsym.h` to make the `gbm_surface_lock_front_buffer` and `gbm_surface_release_buffer` symbols **optional** (`SDL_KMSDRM_SYM_OPT`), rather than required. 

We then modified `SDL_kmsdrmopengles.c` (`KMSDRM_GLES_SwapWindow`) to check for the existence of `KMSDRM_gbm_surface_lock_front_buffer`. If it is `NULL`, we bypass the GBM locking and DRM `drmModePageFlip` logic, and rely purely on `eglSwapBuffers` (assuming the underlying legacy Mali EGL implementation handles the framebuffer swap implicitly).

### 3. Current State: Ghost Dependency
After patching SDL3 and rebuilding, we verified the dependencies of the resulting `libSDL3.so.0` and `WeirdSamples` executable using `readelf -d` in the build container. 
**Neither binary lists `libgbm.so.1` or `libdrm.so.2` in their `NEEDED` entries.**
```text
 0x0000000000000001 (NEEDED)             Shared library: [libSDL3.so.0]
 0x0000000000000001 (NEEDED)             Shared library: [libm.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [ld-linux-aarch64.so.1]
```

Despite this, when running the application on the muOS console through the PortMaster launcher script, the dynamic linker instantly aborts:
`./WeirdSamples: error while loading shared libraries: libgbm.so.1: cannot open shared object file: No such file or directory`

## Key Questions for the Expert

1. **Where is `libgbm.so.1` being requested from?** 
   If `readelf` confirms our binaries aren't hard-linked to it, could a library preloaded by PortMaster's `control.txt` (like `gl4es` or a custom `libEGL.so`) be forcing the dependency at load time? 
2. **Is there a standard `libgbm.so.1` provided by muOS/Knulli?** 
   Documentation suggests Knulli/muOS ship proper KMS/DRM + EGL support with the Panfrost/Mali driver. Why would `libgbm.so.1` be entirely missing from the library path when launching a PortMaster game?
3. **Hardware Page Flipping on Mali-G31:** 
   If we provide a standard `libgbm.so.1` (compiled from Mesa/minigbm) to satisfy the linker, will `eglSwapBuffers` correctly update the screen without needing `gbm_surface_lock_front_buffer` and `drmModePageFlip`, or does the Panfrost driver strictly require the full GBM/KMS DRM pipeline to present frames?
