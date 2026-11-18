# Weird Editor

Unified authoring suite for **WeirdEngine**, combining the **Main Menu**, **Scene Editor**, **Cluster Editor**, and visual **Shape Editor** in one project (`WeirdEditor`), with an option to build only the Shape Editor (`WeirdShapeEditor`).

---

## Architecture & Project Structure

**Weird Editor** lives in `tools/` (`WeirdEditor`) and includes `MainMenuScene`, `SceneEditor`, `ClusterEditor`, and `ShapeEditorScene` (`WEIRD_TOOLS_SHAPE_EDITOR_ONLY=ON` builds `WeirdShapeEditor` containing only the Shape Editor):

```
tools/
├── CMakeLists.txt              # Unified build config (WeirdEditor, or WeirdShapeEditor if WEIRD_TOOLS_SHAPE_EDITOR_ONLY=ON)
├── README.md                   # This documentation
├── assets/                     # Runtime assets (presets, icons)
├── imnodes/                    # Vendored immediate-mode node editor library for ImGui
│   ├── imnodes.h
│   ├── imnodes_internal.h
│   └── imnodes.cpp
├── include/
│   ├── MainMenuScene.h         # ImGui launcher hub & recent items browser
│   ├── ToolsSession.h          # Cross-tool navigation, recent history & session persistence
│   ├── scene-editor/           # Scene Editor headers
│   ├── cluster-editor/         # Cluster Editor header
│   └── shape-editor/           # Shape Editor headers (model, services, ui, ShapeEditorScene.h)
└── src/
    ├── main.cpp                # Shared entry point (or Shape-Editor-only entry when WEIRD_TOOLS_SHAPE_EDITOR_ONLY=ON)
    ├── MainMenuScene.cpp
    ├── ToolsSession.cpp
    ├── scene-editor/
    └── shape-editor/
```

### Component Breakdown

* **`include/shape-editor/model/NodeRegistry.h`**:
  * Central catalog of available node types, input/output pins, and default values.
  * Pin types: `PinType::Float` (scalar values / distances) and `PinType::Vec2` (domain coordinates / 2D vectors).
  * Categories:
    * **Inputs & Coordinates**: `point` (evaluation position), `time`, `param_var` (generic parameters `var(0)`–`var(7)`).
    * **Vector Math**: `vec2_compose`, `vec2_split`, `vec2_length`, `vec2_distance`, `vec2_dot`, `vec2_normalize`.
    * **Domain Transforms**: `translate`, `rotate`, `scale`, `onion` (outline thickness).
    * **2D Primitives**: `circle`, `box`, `box_rounded`, `triangle`, `line`, `ramp`, `sine_wave`, `star`.
    * **3D Primitives**: `sphere_3d`, `box_3d`, `torus_3d`, `cylinder_3d`.
    * **CSG & Modifiers**: `union`, `subtraction`, `intersection`, `smooth_union`, `smooth_subtraction`, `smooth_intersection`.
    * **Math (Unary)**: `abs`, `negate`, `sin`, `cos`, `sqrt`, `floor`, `fract`.
    * **Math (Binary)**: `add`, `subtract`, `multiply`, `divide`, `min`, `max`, `pow`.
    * **Math (Ternary)**: `clamp`, `ternary` (condition >= 1 ? A : B), `mix` (linear interpolation), `smoothstep`.
    * **Output**: `sdf_output` (root sink for evaluated shape).
  * Each node definition registers an `evaluate` callback mapping pin values to engine `Expr` / `Vec2Expr` nodes.

* **`include/shape-editor/model/NodeGraph.h`**:
  * **Graph Model**: Stores node instances, positions, inline attribute defaults, and link connections.
  * **Evaluation Engine**: Performs topological sort on the Directed Acyclic Graph (DAG) starting from `sdf_output`. Handles cycle detection and evaluates upstream subgraphs recursively.
  * **Auto-Layout**: Layer-based hierarchical layout placing input nodes on the left and cascading rightwards toward the output.
  * **Serialization**: Full JSON load/save support via `nlohmann::json`, capturing node IDs, positions, inline pin inputs, and link topologies.
  * **Code Generation**:
    * **GLSL**: Emits mathematical shader expression strings via `IMathExpression::print()`.
    * **C++**: Emits copy-pasteable WeirdEngine C++ constructor code matching `Expr` DSL syntax.
  * **Built-in Presets**:
    * *Star*: Harmonic star composed of rotated and modulated primitives.
    * *Aquatic Wave*: Multi-frequency sine waves combined with CSG smoothing.
    * *CSG Ring*: Hollow shape demonstrating smooth subtraction.
    * *Basic Circle*: Minimal starter shape.

* **`include/shape-editor/ShapeEditorScene.h`**:
  * Inherits from `WeirdEngine::Scene2D`.
  * **Canvas**: Hosts the ImNodes graph canvas with zoom, pan, minimap, right-click context menu, and link creation/deletion.
  * **Live Viewport**: Transparent viewport cutout displaying the real-time raymarched/rasterized shape in WeirdEngine.
  * **Audio Coupling**: Passes the compiled `Expr` to `WeirdAudio::SdfSong` and `AudioService`. Displays live musical properties:
    * Fundamental Pitch (MIDI note & Hz)
    * Detected Musical Scale (Pentatonic, Dorian, Natural Minor, etc.)
    * Shape Area, Circularity, and Harmonic Density
    * Fill Ratio & Dynamic Motion Levels
  * **Inspector Panel**: Shows generated GLSL / C++ code with one-click clipboard copying.
  * **File Dialogs**: Asynchronous native file pickers using `SDL_ShowOpenFileDialog` and `SDL_ShowSaveFileDialog`. In the web build these are replaced by an HTML file input (open) and a browser download (save).

* **`src/main.cpp`**:
  * Configures `EngineSettings` (1280×800 window, VSync, dark styling).
  * Registers `ShapeEditorScene` with `SceneManager` and starts the engine loop.

---

## Key Features & Functionality

### 1. Visual Node Canvas
- Drag-and-drop connections between compatible pins (Float-to-Float, Vec2-to-Vec2).
- Automatic type validation preventing invalid pin connections.
- Unconnected input pins render inline editable controls (drag floats, vector pickers).
- Right-click anywhere on the canvas to open the category-organized node spawn menu.
- Interactive Minimap anchored in the bottom-left corner.

### 2. Live SDF Compilation & Math Expression Trees
Instead of compiling a heavy shader program on every edit, the graph compiles directly into WeirdEngine's lightweight AST classes:
- Nodes yield `std::shared_ptr<IMathExpression>` instances wrapped in `Expr` and `Vec2Expr`.
- System variables (`point()`, `time()`, `var(i)`) are dynamically bound at render time.
- Changes propagate immediately to visual rendering and physics/audio subsystems with zero frame drops.

### 3. Procedural Audio & Music Synthesis
The editor integrates with the engine's `SdfMusicEngine`:
- The evaluated shape is sampled across spatial rays to extract geometric moments.
- **Perimeter & Surface Area** govern musical scale, octave range, and note densities.
- **Symmetry & Smoothness** shape ADSR envelopes, waveform harmonics (sine vs. saw vs. square), and modulation frequency.
- The inspector provides real-time monitoring of volume, tempo, scale, active notes, and audio playback toggles.

### 4. Code Export
The right-hand inspector generates live code snippets ready to copy into game projects:
- **GLSL Code**: Pure shader code representation.
- **C++ Code**: Native engine syntax, e.g.:
  ```cpp
  Expr shape = SDF::sdCircle(SDF::point(), 50.0f);
  ```

---

## Controls & Shortcuts

| Action | Shortcut / Gesture |
|---|---|
| **Add Node** | Right-click canvas |
| **Delete Selected** | <kbd>Delete</kbd> or <kbd>Backspace</kbd> |
| **Auto-Layout Nodes** | <kbd>Ctrl</kbd> + <kbd>L</kbd> (or `Edit -> Auto Layout`) |
| **New Graph** | <kbd>Ctrl</kbd> + <kbd>N</kbd> (or `File -> New Graph`) |
| **Open Graph JSON** | <kbd>Ctrl</kbd> + <kbd>O</kbd> (or `File -> Open Graph JSON...`) |
| **Save Graph JSON** | <kbd>Ctrl</kbd> + <kbd>S</kbd> (or `File -> Save Graph JSON...`) |
| **Zoom In / Out** | <kbd>Ctrl</kbd> + <kbd>+</kbd> / <kbd>Ctrl</kbd> + <kbd>-</kbd> or <kbd>Ctrl</kbd> + Mouse Wheel |
| **Reset Zoom (100%)** | <kbd>Ctrl</kbd> + <kbd>0</kbd> |
| **Pan Canvas** | Middle-click drag or Right-click drag |
| **Box Select** | Left-click drag on empty canvas |

---

## Building and Running

Ensure `WEIRD_ENGINE_BUILD_TOOLS=ON` is enabled in your CMake configuration:

```bash
# Configure (full tools suite)
cmake -B build -DWEIRD_ENGINE_BUILD_TOOLS=ON

# Build unified WeirdEditor executable
cmake --build build --target WeirdEditor

# Run on Linux / macOS
./build/tools/bin/WeirdEditor

# Run on Windows
.\build\tools\bin\Debug\WeirdEditor.exe

# Optional: build Shape-Editor-only version (excludes Main Menu, Scene Editor, Cluster Editor)
cmake -B build-shape -DWEIRD_ENGINE_BUILD_TOOLS=ON -DWEIRD_TOOLS_SHAPE_EDITOR_ONLY=ON
cmake --build build-shape --target WeirdShapeEditor
```
