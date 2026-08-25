# Scene and ECS Architecture Guide

This document explains how to build scenes, manage entities and components, and write system logic in Weird Engine.
All instructions follow the ASD-STE100 Simplified Technical English standard.

---

## 1. Project Setup and Building

Weird Engine includes a pre-configured template project in `examples/empty-project`.
Use this template to create new games.

### Build Configuration

Open `CMakeLists.txt` in your game directory to configure engine linking options.

#### Option A: Using a Local Engine Copy

Set `USE_LOCAL_WEIRD_ENGINE` to `ON` (default).
Set `WEIRD_ENGINE_LOCAL_PATH` to your engine directory relative to `CMakeLists.txt`:

```cmake
set(USE_LOCAL_WEIRD_ENGINE ON)
set(WEIRD_ENGINE_LOCAL_PATH "../weird-engine/")
```

#### Option B: Automatic GitHub Download

Set `USE_LOCAL_WEIRD_ENGINE` to `OFF`.
CMake uses `FetchContent` to download Weird Engine from GitHub:

```cmake
set(USE_LOCAL_WEIRD_ENGINE OFF)
```

### Compiling Your Project

Run these CMake commands from your project root:

```bash
cmake -B build -S .
cmake --build build
```

---

## 2. Building a Scene

Scenes inherit from one of three base classes defined in `include/weird-engine/Scene.h`:

- `Scene2D`: Standard 2D Signed Distance Field (SDF) ray marching and PBD physics.
- `Scene3D`: 3D SDF ray marching path.
- `SceneBoth`: Combined 2D and 3D ray marching paths.

### Registering Scenes

Register your scene class in `main()` with the singleton `SceneManager`:

```cpp
#include <weird-engine.h>

using namespace WeirdEngine;

class MainGameScene : public Scene2D
{
public:
	MainGameScene()
	{
		addCreateSystem(onCreateSystem);
		addStartSystem(onStartSystem);
		addUpdateSystem(onUpdateSystem);
	}
};

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<MainGameScene>("main-game");

	DisplaySettings displaySettings{};
	PhysicsSettings physicsSettings{};
	AudioSettings audioSettings{};

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
}
```

---

## 3. Entity Component System (ECS)

The `Registry` class manages entity IDs and component storage.

### Creating and Destroying Entities

Call `registry.createEntity()` to generate an entity ID:

```cpp
Entity entity = registry.createEntity();
```

Call `registry.destroyEntity(entity)` to remove an entity and all attached components:

```cpp
registry.destroyEntity(entity);
```

### Adding and Accessing Components

Add components to an entity using `registry.addComponent<T>(entity)`:

```cpp
auto& transform = registry.addComponent<Transform>(entity);
transform.position = vec3(10.0f, 5.0f, 0.0f);

auto& dot = registry.addComponent<Dot>(entity);
dot.materialId = services.materials2D().getHandle("my_mat").id;

auto& rb = registry.addComponent<RigidBody2D>(entity);
rb.velocity = vec2(0.0f, 5.0f);
```

Retrieve components using `registry.getComponent<T>(entity)`:

```cpp
auto& transform = registry.getComponent<Transform>(entity);
```

Check component existence using `registry.hasComponent<T>(entity)`:

```cpp
if (registry.hasComponent<RigidBody2D>(entity))
{
	// Perform action
}
```

Mark modified components dirty when required by rendering or physics systems:

```cpp
registry.setComponentDirty(rb);
```

### Creating Custom Component Types

Define custom components as plain C++ structures:

```cpp
struct Health
{
	int current = 100;
	int max = 100;
};
```

The `Registry` registers component types automatically during first access.
You can also register component types explicitly:

```cpp
registry.registerComponent<Health>();
```

### ECS Native Scene State Pattern

Systems do not keep local state variables inside the scene class.
Store scene state inside an ECS component attached to a dedicated state entity:

```cpp
struct SceneState
{
	int score = 0;
	float spawnTimer = 0.0f;
	Entity playerEntity = INVALID_ENTITY;
};

void onCreateSystem(Registry& registry, ServiceProvider& services)
{
	Entity stateEntity = registry.createEntity();
	registry.addComponent<SceneState>(stateEntity);
	services.tags().tag(stateEntity, "state");
	services.serialization().blacklistEntity(stateEntity);
}

inline SceneState& getState(Registry& registry, ServiceProvider& services)
{
	return registry.getComponentArray<SceneState>()->getDataAtIdx(0);
}
```

---

## 4. System Dispatcher Architecture

Add scene logic by registering systems to stage dispatchers.

### System Function Signature

Systems are free functions or lambdas matching the `CoreSystem` signature:

```cpp
void systemName(Registry& registry, ServiceProvider& services);
```

### Registering Stage Systems

Register systems inside your Scene constructor:

- `addCreateSystem`: Runs after ECS initialization before scene loading.
- `addStartSystem`: Runs once after initial scene setup.
- `addUpdateSystem`: Runs every frame for game logic.
- `addImGuiRenderSystem`: Runs during ImGui interface rendering.
- `addEntityCollisionSystem`: Dispatches main-thread entity collision events.
- `addEntityShapeCollisionSystem`: Dispatches main-thread entity shape collision events.
- `addDestroySystem`: Runs when replacing or exiting the scene.

```cpp
class ShowcaseScene : public Scene2D
{
public:
	ShowcaseScene()
	{
		addCreateSystem(onCreateSystem);
		addStartSystem(onStartSystem);
		addUpdateSystem(spawnSystem);
		addUpdateSystem(inputSystem);
		addUpdateSystem(uiSystem);
		addDestroySystem(onDestroySystem);
	}
};
```

Multiple systems registered to the same stage execute sequentially in registration order.

---

## 5. ServiceProvider Subsystems

The `ServiceProvider` parameter provides controlled access to engine subsystems.

| Subsystem | Service Call | Purpose |
|---|---|---|
| Input | `services.input()` | Query keys, mouse coordinates, and gamepads |
| Physics | `services.physics()` | Control gravity, damping, pause state, and raycasts |
| Render | `services.render()` | Access camera, lights, and trigger shader updates |
| Shapes | `services.shapes()` | Register custom SDFs and spawn shapes (see [SDF Shapes Guide](SDF_SHAPES.md)) |
| Materials 2D | `services.materials2D()` | Create, share, and query 2D material definitions |
| Materials 3D | `services.materials3D()` | Create, share, and query 3D material definitions |
| Audio | `services.audio()` | Queue audio requests and read friction audio levels |
| Tags | `services.tags()` | Assign and query unique string tags on entities |
| Serialization | `services.serialization()` | Save and load `.weird` files and blacklist entities |
| Time | `services.time()` | Read simulation time and frame delta time |
| Resources | `services.resources()` | Resolve asset paths and file storage operations |
| Scene Control | `services.sceneControl()` | Request scene transitions |
| Debug | `services.debug()` | Toggle fly camera and input debugging options |

---

## 6. Legacy Callbacks and Physics Thread Interaction

### Legacy Virtual Callbacks

You can override virtual callbacks in `Scene`:

- `onStart(Registry& registry, ServiceProvider& services)`
- `onUpdate(Registry& registry, ServiceProvider& services)`
- `onRender(Registry& registry, ServiceProvider& services, WeirdRenderer::RenderTarget& target)`

Use `onRender` when issuing direct 3D draw commands to the render target.

### Physics Thread Callbacks

Physics simulation steps run on a dedicated physics thread.
Override these virtual methods for mid-step physics logic:

```cpp
void onPhysicsStep(Simulation2D& simulation) override;
void onPhysicsRigidBodyCollision(Simulation2D& simulation, PhysicsCollisionEvent& event) override;
void onPhysicsShapeCollision(Simulation2D& simulation, PhysicsShapeCollisionEvent& event) override;
```

Physics thread callbacks receive `Simulation2D&` only.
Physics thread callbacks must NOT access `Registry` or `ServiceProvider` because the main thread owns the ECS.

### Physics Body User Data

Attach custom C++ structs to physics bodies by deriving from `BodyUserData`:

```cpp
struct CharacterData : BodyUserData
{
	static constexpr int TYPE = 1;

	CharacterData()
	{
		type = TYPE;
	}

	float jumpStrength = 10.0f;
	float restitution = 1.5f;
};
```

Pass ownership of the user data to the physics simulation:

```cpp
auto data = std::make_unique<CharacterData>();
data->jumpStrength = 12.0f;
services.physics().setUserData(rb.simulationId, std::move(data));
```

Query user data in physics thread callbacks safely using `getUserDataAs<T>`:

```cpp
void onPhysicsShapeCollision(Simulation2D& simulation, PhysicsShapeCollisionEvent& event) override
{
	if (auto* data = simulation.getUserDataAs<CharacterData>(event.body))
	{
		event.absortion *= 1.0f / data->restitution;
		event.friction *= 0.5f;
	}
}
```

---

## 7. Sample Scene Reference

For a complete working example demonstrating systems, `ServiceProvider`, custom SDFs, UI text, and physics callbacks, consult:

`examples/sample-scenes/include/ServiceShowcaseScene.h`
