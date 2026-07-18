# AGENTS.md

## Build

```bash
# Native (host) build with examples
cmake -B build -DWEIRD_ENGINE_BUILD_EXAMPLES=ON
cmake --build build

# Run the main example
./build/examples/sample-scenes/WeirdSamples

# muOS cross-compile (aarch64, uses podman)
./scripts/anbernic/build-muos.sh
./scripts/anbernic/deploy-muos.sh          # build + deploy via MTP
./scripts/anbernic/deploy-muos.sh --no-build  # deploy only
./scripts/anbernic/fetch-logs.sh           # pull log.txt + screenshots
```

There are no tests. CI runs `ctest` but no test targets are defined.

## CMake options

| Option | Default | Purpose |
|---|---|---|
| `WEIRD_ENGINE_BUILD_EXAMPLES` | `OFF` | Must be `ON` to build executables in `examples/` |
| `WEIRD_DISABLE_IMGUI` | `OFF` | Strip ImGui (used for muOS build) |
| `WEIRD_TEST_HOOKS` | `OFF` | Enables `WEIRD_AUTO_QUIT_SECONDS` / `WEIRD_SCREENSHOT_FRAME` env vars |
| `WEIRD_ENGINE_ENABLE_ASAN` | `OFF` | AddressSanitizer |
| `WEIRD_USE_FBDEV_EGL` | `OFF` | fbdev EGL backend for Mali devices (no GBM/KMS) |
| `WEIRD_ENGINE_USE_RUNTIME_ASSETS` | `OFF` | Load shaders/fonts from `./shaders/` `./fonts/` instead of source tree |

## Architecture

- **Engine is a static library** (`libWeirdEngine.a`) built from `src/` + `include/`.
- Three engine modules (all under one target):
  - `weird-engine/` — core: ECS, scenes, input, logging, serialization
  - `weird-renderer/` — OpenGL ES SDF ray-marching renderer, audio (miniaudio), shaders, fonts
  - `weird-physics/` — Position-Based Dynamics with SDF collision
- **Examples are separate executables** that `add_subdirectory` the engine root and link `WeirdEngine`. Each has its own `src/`, `include/`, `assets/`.
  - `sample-scenes` → `WeirdSamples` (main demo)
  - `3d-experiments`, `opengl-experiments` — other demos
  - `empty-project` — starter template
- Entry point for games: `WeirdEngine::start(sceneManager, ...)` in `include/weird-engine.h`.

## Dependencies

- **Git submodules**: SDL3, imgui (init with `git submodule update --init --recursive`; CMake also runs this automatically).
- **Vendored in `third-party/`**: glad, glm, stb, miniaudio, nlohmann/json, KHR.

## Style

- C++20, clang-format (`.clang-format`): **Allman braces, tabs, 120-column limit**, Microsoft base style, pointer-left (`int* p`).
- Run: `clang-format -i <file>` (no project-wide format target).

## Key gotchas

- Shaders and fonts are loaded at runtime from paths baked in at compile time (`SHADERS_PATH`, `FONTS_PATH`, `ASSETS_PATH` macros). The post-build step copies them next to the executable for example projects.
- The `build/` and `build-muos/` directories are separate CMake trees; do not mix them.
- `compile_flags.txt` exists for clangd; it does not drive the actual build.
- The `.vscode/settings.json` enables `WEIRD_ENGINE_BUILD_EXAMPLES=ON` by default.
