# Building for the Web (Emscripten / WebAssembly)

This guide covers compiling WeirdEngine and games built on it (e.g.
`weird-golfing`) to WebAssembly with [Emscripten], both locally and in GitHub
Actions.

Reference workflows in the repos:

| Repo | Workflow | What it does |
|---|---|---|
| `weird-engine` | `.github/workflows/cmake-multi-platform.yml` (web job) | Builds `WeirdSamples` for the web |
| `weird-engine` | `.github/workflows/deploy-itch.yml` | Builds and pushes to itch.io with Butler |
| `weird-golfing` | `.github/workflows/build-web.yml` | Builds the game for the web and uploads an artifact |

---

## 1. How the web build works

The build is a normal CMake build with the Emscripten toolchain injected by
`emcmake`:

1. `emcmake cmake ..` configures with `CMAKE_TOOLCHAIN_FILE` set to the
   Emscripten toolchain. This defines `EMSCRIPTEN`, which the project CMakeLists
   uses to auto-enable `BUILD_WEB`.
2. `BUILD_WEB=ON` forces `DEPLOY_STANDALONE=ON` and
   `WEIRD_ENGINE_USE_RUNTIME_ASSETS=ON`, so at runtime the engine looks for
   `./assets/`, `./fonts/` and `./shaders/` instead of absolute source paths.
3. `emmake make` runs the build with the Emscripten compiler/linker wrappers.
4. `--preload-file <dir>@/<mount>` linker flags pack those directories into a
   single `.data` file and mount them inside the WASM virtual filesystem at
   `/assets`, `/fonts` and `/shaders`, which is where the runtime paths resolve.
5. The linker emits `index.html` plus `index.js`/`index.wasm`/`index.data`
   (and possibly a separate `.worker.js`). These are the web export. Example
   and tool targets generate their HTML shell from the shared
   `cmake/web/index.html` template (`configure_file` + `WEIRD_WEB_TITLE`) and
   copy it over the generated page; without a shell, the export falls back to
   Emscripten's default page with its header toolbar.

Threads (`-pthread`) use Web Workers plus `SharedArrayBuffer`, which browsers
only expose to **cross-origin isolated** pages. Any server hosting the build
must send the `Cross-Origin-Opener-Policy` and `Cross-Origin-Embedder-Policy`
headers (see section 5), and itch.io needs its *SharedArrayBuffer support*
option enabled (section 7). Without them the game loads and then hangs or
errors out.

### Linker/compiler flags used

| Flag | Purpose |
|---|---|
| `-pthread` (C, C++, link) | Enables pthreads support (`SharedArrayBuffer`) |
| `-sPTHREAD_POOL_SIZE=4` | Pre-spawns 4 workers |
| `-sINITIAL_MEMORY=33554432` | 32 MB initial linear memory |
| `-sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2` | Force WebGL 2 |
| `-o index.html` / `-DCMAKE_EXECUTABLE_SUFFIX=".html"` | Emit an `.html` shell |
| `--preload-file <src>@/<mount>` | Pack assets/fonts/shaders into the `.data` blob |

---

## 2. Prerequisites

- CMake, `make` (or Ninja) and `git` on the host.
- The Emscripten SDK. Install it once and activate it in every shell you build
  from:

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh          # adds emcc/emcmake/emmake/emrun to PATH

emcc --version                 # sanity check
```

Use a dedicated build directory (e.g. `build-web`); never reuse the native
`build/` tree, since a CMake cache configured for the host cannot be reused by
Emscripten. When in doubt, delete it and reconfigure.

---

## 3. Building WeirdEngine examples locally

From the `weird-engine` root (`WeirdSamples` is the main demo):

```bash
git submodule update --init --recursive   # SDL3 + imgui

mkdir -p build-web && cd build-web

emcmake cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DWEIRD_ENGINE_BUILD_EXAMPLES=ON \
  -DBUILD_WEB=ON \
  -DCMAKE_C_FLAGS="-pthread" \
  -DCMAKE_CXX_FLAGS="-pthread" \
  -DCMAKE_EXECUTABLE_SUFFIX=".html" \
  -DCMAKE_EXE_LINKER_FLAGS="-pthread -sPTHREAD_POOL_SIZE=4 -sINITIAL_MEMORY=33554432 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2 --preload-file $PWD/../examples/sample-scenes/assets@/assets --preload-file $PWD/../src/weird-renderer/fonts@/fonts --preload-file $PWD/../src/weird-renderer/shaders@/shaders"

emmake make WeirdSamples -j"$(nproc)"
```

Output: `build-web/examples/sample-scenes/index.{html,js,wasm,data}`.

The bundled editors in `tools/` build the same way. Use
`-DWEIRD_ENGINE_BUILD_TOOLS=ON` instead of `WEIRD_ENGINE_BUILD_EXAMPLES`, add
the tool's `assets/` directory to the preload flags, and build its target. For
example, the SDF node editor:

```bash
mkdir -p build-web && cd build-web

emcmake cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DWEIRD_ENGINE_BUILD_TOOLS=ON \
  -DBUILD_WEB=ON \
  -DCMAKE_C_FLAGS="-pthread" \
  -DCMAKE_CXX_FLAGS="-pthread" \
  -DCMAKE_EXECUTABLE_SUFFIX=".html" \
  -DCMAKE_EXE_LINKER_FLAGS="-pthread -sPTHREAD_POOL_SIZE=4 -sINITIAL_MEMORY=33554432 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2 --preload-file $PWD/../tools/sdf-node-editor/assets@/assets --preload-file $PWD/../src/weird-renderer/fonts@/fonts --preload-file $PWD/../src/weird-renderer/shaders@/shaders"

emmake make WeirdSdfNodeEditor -j"$(nproc)"
```

Output: `build-web/tools/sdf-node-editor/index.{html,js,wasm,data}`.

---

## 4. Building a game project locally (weird-golfing)

From the `weird-golfing` root, using the local engine checkout
(`USE_LOCAL_WEIRD_ENGINE=ON`, the default):

```bash
mkdir -p build-web && cd build-web

emcmake cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_WEB=ON \
  -DUSE_LOCAL_WEIRD_ENGINE=ON \
  -DCMAKE_C_FLAGS="-pthread" \
  -DCMAKE_CXX_FLAGS="-pthread" \
  -DCMAKE_EXE_LINKER_FLAGS="-pthread -sPTHREAD_POOL_SIZE=4 -sINITIAL_MEMORY=33554432 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2 --preload-file $PWD/../assets@/assets --preload-file $PWD/../weird-engine/src/weird-renderer/fonts@/fonts --preload-file $PWD/../weird-engine/src/weird-renderer/shaders@/shaders"

emmake make -j"$(nproc)"
```

To build against the engine fetched from GitHub instead of the sibling
directory, use `-DUSE_LOCAL_WEIRD_ENGINE=OFF` and point the font/shader preloads
at the fetched copy:
`--preload-file _deps/weirdengine-src/src/weird-renderer/fonts@/fonts` (and
likewise for `shaders`). `_deps/weirdengine-src` only exists after the
configure step has downloaded the engine.

Output: `build-web/index.html` (or the project's custom shell copied by the
post-build step) plus `build-web/WeirdGolfing.{js,wasm,data}`. The exact base
name of the JS/WASM files depends on the Emscripten version, so stage both
`index.*` and `<TargetName>.*` when packaging.

---

## 5. Running locally

Emscripten ships `emrun`, which serves a directory with the required
cross-origin isolation headers and opens a browser:

```bash
cd build-web/examples/sample-scenes
emrun index.html                 # add --no_browser --port 8000 to control it
```

If you prefer your own static server, it **must** send:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

A plain `python3 -m http.server` will make threaded builds fail. Minimal
drop-in server:

```python
#!/usr/bin/env python3
# serve.py -- static server with the headers pthreads builds require
import http.server
import socketserver
import sys

class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(("", port), Handler) as httpd:
    print(f"Serving on http://localhost:{port}")
    httpd.serve_forever()
```

Run it from the export folder (`python3 serve.py 8000`). In the browser console,
`crossOriginIsolated` must be `true`.

---

## 6. Setting up a GitHub Action

The job is short: checkout, install Emscripten, configure with `emcmake`,
build with `emmake`, stage the export and upload it as an artifact. Points to
keep in mind:

- Use `ubuntu-22.04` (both repos do). It is a known-good runner for this
  toolchain.
- Use `mymindstorm/setup-emsdk@v14` with `actions-cache-folder` so the SDK is
  cached between runs.
- Host packages (`libgl-dev`, `libwayland-dev`, ...) are **not** needed for the
  web target — Emscripten provides its own sysroot. The extra `apt-get` step in
  `weird-golfing/.github/workflows/build-web.yml` is only relevant to its
  native builds.
- `weird-engine` needs `submodules: recursive` on the checkout step (SDL3,
  imgui). Games using FetchContent do not.
- Artifact names and wildcard copies: stage `index.*` **and**
  `<TargetName>.*`, because newer Emscripten versions may name the outputs after
  the CMake target. A `.worker.js` may or may not be emitted (pthread worker
  code can be embedded in the main JS), so never hardcode the file list.
- The `assets/`, `fonts/` and `shaders/` folders that the post-build step copies
  next to the executable are **not** fetched over HTTP: `--preload-file` packs
  them into the `.data` blob, which is what the page loads. Uploading them too
  is harmless but only bloats the artifact.

### Full workflow (game project, like weird-golfing)

```yaml
name: Build Web

on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]
  workflow_dispatch:

permissions:
  contents: read

jobs:
  build-web:
    runs-on: ubuntu-22.04

    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Setup Emscripten
        uses: mymindstorm/setup-emsdk@v14
        with:
          version: latest
          # Cache the SDK so later runs skip the download
          actions-cache-folder: emsdk-cache

      - name: Configure (Emscripten)
        run: |
          mkdir -p build-web && cd build-web
          emcmake cmake .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_WEB=ON \
            -DUSE_LOCAL_WEIRD_ENGINE=OFF \
            -DCMAKE_C_FLAGS="-pthread" \
            -DCMAKE_CXX_FLAGS="-pthread" \
            -DCMAKE_EXE_LINKER_FLAGS="-pthread -sPTHREAD_POOL_SIZE=4 -sINITIAL_MEMORY=33554432 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2 --preload-file ${{ github.workspace }}/assets@/assets --preload-file _deps/weirdengine-src/src/weird-renderer/fonts@/fonts --preload-file _deps/weirdengine-src/src/weird-renderer/shaders@/shaders"

      - name: Build (Emscripten)
        run: emmake make -C build-web -j"$(nproc)"

      - name: Prepare Artifact
        run: |
          mkdir -p game_export
          cp build-web/index.* game_export/ 2>/dev/null || true
          cp build-web/WeirdGolfing.* game_export/ 2>/dev/null || true
          # assets/fonts/shaders are packed into the .data blob by --preload-file

      - name: Upload Artifact
        uses: actions/upload-artifact@v4
        with:
          name: weird-golfing-web
          path: game_export/
```

### Engine-only variant

Building an engine example (`WeirdSamples`) is the same job with four changes:

- Checkout with `submodules: recursive` (and `fetch-depth: 0` is harmless).
- Add `-DWEIRD_ENGINE_BUILD_EXAMPLES=ON` and
  `-DCMAKE_EXECUTABLE_SUFFIX=".html"` to the configure command, and preload from
  `${{ github.workspace }}/examples/sample-scenes/assets`,
  `${{ github.workspace }}/src/weird-renderer/fonts` and
  `${{ github.workspace }}/src/weird-renderer/shaders`.
- Build the single target: `emmake make -C build-web WeirdSamples`.
- Stage from `build-web/examples/sample-scenes/` (`index.*` only — the
  `assets/fonts/shaders` folders are inside `index.data`).

The complete known-good version of this job is in
`weird-engine/.github/workflows/cmake-multi-platform.yml`.

---

## 7. Hosting

- **itch.io** works for threaded builds: upload the export with Butler
  (`remarkablegames/setup-butler@v2`, secret `BUTLER_API_KEY`,
  `butler push ./game_export/ user/game:html` — see
  `weird-engine/.github/workflows/deploy-itch.yml`), then enable
  **Embed options → Frame options → SharedArrayBuffer support** on the project
  page. Without that checkbox the game gets no COOP/COEP headers.
  For a manual upload, zip the export with `index.html` at the root of the
  archive (no wrapping folder) and upload it as an HTML game, then enable
  SharedArrayBuffer support the same way:
  `zip -r mygame-web.zip index.html index.js index.wasm index.data` (run in the
  build output directory).
- **GitHub Pages cannot set the COOP/COEP headers**, so a `-pthread` build will
  not run there. Use itch.io, a host that lets you set headers, or a
  `coi-serviceworker` style shim. Building without `-pthread` (single-threaded)
  is the alternative if you must use Pages.

---

## 8. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `SharedArrayBuffer is not defined`, game hangs after load | Server missing COOP/COEP headers. Use `emrun` or the `serve.py` above; check `crossOriginIsolated` in the console. |
| 404 for `*.worker.js` | A separate worker file was emitted and not staged. Copy all outputs (`index.*` / `<Target>.*`), don't hardcode names. |
| Black screen | Check the browser console: WebGL2 context creation, missing data file, or wrong `--preload-file` path. Try the browser's WebGL2 support and the `MAX/MIN_WEBGL_VERSION` flags. |
| Assets/fonts/shaders not found at runtime | Preload path points at the wrong source directory, or the mount name doesn't match `./assets`, `./fonts`, `./shaders`. Re-run configure with corrected `--preload-file` flags. |
| CMake reuses host settings / weird compiler errors | You configured in a directory that was previously a native build. `rm -rf build-web` and configure fresh. |
| Build breaks after an Emscripten update | Pin `version:` in `setup-emsdk` (and update the local `emsdk activate` command to the same version). |
| `_deps/weirdengine-src` missing during link | The FetchContent clone only exists after `emcmake cmake`; re-run configure before `emmake make`. |

[Emscripten]: https://emscripten.org/
