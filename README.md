# Weird Engine

Weird Engine is a C++20 game engine designed for 2D and 3D Signed Distance Field (SDF) rendering.

## Features

- **Ray Marching Renderer**: Renders 2D and 3D Signed Distance Fields using custom OpenGL ES shaders.
- **Physics Engine**: Calculates 2D Position-Based Dynamics (PBD) with SDF collision detection.
- **Entity Component System (ECS)**: Manages entities, component storage, and system dispatching.
- **Service Architecture**: Provides decoupled engine services through a single provider interface.

---

## Getting Started

### Prerequisites

Install the SDL3 development library for your operating system before building.
Refer to the [SDL3 Linux README](https://wiki.libsdl.org/SDL3/README-linux) for Linux package names.

### Building Your Project with the Template Preset

Weird Engine provides a project template in `examples/empty-project`.
Use this template to start building a new game.

1. Copy the `examples/empty-project` directory to your project location.
2. Open `CMakeLists.txt` in your new project directory.
3. Configure the engine source location:
   - **Local Engine (Default)**: Set `USE_LOCAL_WEIRD_ENGINE` to `ON`. Set `WEIRD_ENGINE_LOCAL_PATH` to your local engine directory.
   - **Automatic Download**: Set `USE_LOCAL_WEIRD_ENGINE` to `OFF`. CMake automatically downloads Weird Engine from GitHub.
4. Place your header files in `include/` and source files in `src/`.
5. Place your game assets in `assets/`.
6. Configure and build the project using CMake:

```bash
cmake -B build -S .
cmake --build build
```

7. Run the compiled executable from the build directory.

---

## Engine Architecture

For detailed guides, refer to:
- [Scene and ECS Architecture Guide](docs/SCENE_AND_ECS.md)
- [Defining Shapes with SDFs Guide](docs/SDF_SHAPES.md)
- [Procedural Audio & Music Guide](docs/AUDIO.md)

### Creating a Scene

Inherit from one of the scene base classes in `include/weird-engine/Scene.h`:

- `Scene2D`: Uses 2D ray marching and 2D physics.
- `Scene3D`: Uses 3D ray marching.
- `SceneBoth`: Combines 2D and 3D ray marching paths.

Register your scene in `main()` with the `SceneManager` instance:

```cpp
#include <weird-engine.h>

using namespace WeirdEngine;

class MyScene : public Scene2D
{
public:
	MyScene()
	{
		addStartSystem(onStartSystem);
	}
};

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<MyScene>("my-scene");
	start(sceneManager, {}, {}, {}, argc, argv);
}
```

### Entities and Components

Entities are unique numerical identifiers.
The `Registry` class manages entities and stores components.

#### Creating an Entity

Call `registry.createEntity()` to make a new entity:

```cpp
Entity entity = registry.createEntity();
```

#### Adding Components

Call `registry.addComponent<T>(entity)` to attach a component to an entity:

```cpp
auto& transform = registry.addComponent<Transform>(entity);
transform.position = vec3(0.0f, 10.0f, 0.0f);

auto& dot = registry.addComponent<Dot>(entity);
dot.materialId = services.materials2D().getHandle("my_mat").id;
```

If you modify a component after creation, mark it dirty if required:

```cpp
registry.setComponentDirty(transform);
```

#### Creating and Registering Custom Components

Define custom components as C++ structures:

```cpp
struct Health
{
	int current = 100;
	int max = 100;
};
```

The `Registry` automatically registers new component types when first accessed.
You can also register component types explicitly:

```cpp
registry.registerComponent<Health>();
```

#### Scene State Component Pattern

Store scene variables in an ECS component instead of global variables.
Create a `State` component and attach it to a dedicated entity:

```cpp
struct State
{
	int score = 0;
	float timer = 0.0f;
};

void onCreateSystem(Registry& registry, ServiceProvider& services)
{
	Entity stateEntity = registry.createEntity();
	registry.addComponent<State>(stateEntity);
	services.tags().tag(stateEntity, "state");
	services.serialization().blacklistEntity(stateEntity);
}
```

### Systems and Logic

Add game logic using the System Dispatcher or legacy callbacks.

#### System Dispatcher (Recommended)

Systems are plain free functions or lambdas with this signature:

```cpp
void system(Registry& registry, ServiceProvider& services);
```

Register systems inside your scene constructor:

```cpp
MyScene()
{
	addCreateSystem(onCreateSystem);
	addStartSystem(onStartSystem);
	addUpdateSystem(movementSystem);
	addUpdateSystem(combatSystem);
	addImGuiRenderSystem(uiSystem);
	addEntityCollisionSystem(onCollisionSystem);
	addEntityShapeCollisionSystem(onShapeCollisionSystem);
	addDestroySystem(onDestroySystem);
}
```

Systems registered to the same stage run sequentially in registration order.

#### Service Provider Interface

Systems access engine subsystems through the `ServiceProvider` facade:

- `services.input()`: Read keyboard, mouse, and gamepad inputs.
- `services.physics()`: Change gravity, damping, pause state, or run raycasts.
- `services.render()`: Control camera, lights, and force shader updates.
- `services.shapes()`: Register custom SDFs and add geometric shapes.
- `services.materials2D()`: Create, share, and query 2D materials.
- `services.materials3D()`: Create, share, and query 3D materials.
- `services.audio()`: Play sounds and check friction audio levels.
- `services.tags()`: Assign unique string tags to entities and look up entities by tag.
- `services.serialization()`: Save or load `.weird` scene files and blacklist entities.
- `services.time()`: Read frame delta time and total simulation time.
- `services.resources()`: Resolve asset paths and file input/output.
- `services.sceneControl()`: Trigger scene transitions.

#### Legacy Scene Callbacks

Override virtual methods in `Scene` to use legacy callbacks:

```cpp
class MyScene : public Scene2D
{
protected:
	void onStart(Registry& registry, ServiceProvider& services) override {}
	void onUpdate(Registry& registry, ServiceProvider& services) override {}
	void onRender(Registry& registry, ServiceProvider& services, WeirdRenderer::RenderTarget& target) override {}
};
```

Note: Use `onRender` specifically when you need custom 3D render pipeline operations.

#### Physics Thread Callbacks

Physics simulation steps run on a dedicated thread.
Override these virtual methods to execute logic mid-step:

- `onPhysicsStep(Simulation2D& simulation)`
- `onPhysicsRigidBodyCollision(Simulation2D& simulation, PhysicsCollisionEvent& event)`
- `onPhysicsShapeCollision(Simulation2D& simulation, PhysicsShapeCollisionEvent& event)`

Physics callbacks receive `Simulation2D&` only.
Physics callbacks cannot access `Registry` or `ServiceProvider` because the main thread owns the ECS.

To associate custom data with physics bodies, derive from `BodyUserData`:

```cpp
struct CharacterData : BodyUserData
{
	static constexpr int TYPE = 1;
	CharacterData() { type = TYPE; }
	float jumpStrength = 10.0f;
};

// Hand off ownership to the simulation:
services.physics().setUserData(rb.simulationId, std::make_unique<CharacterData>());

// Query data back in physics callbacks:
if (auto* data = simulation.getUserDataAs<CharacterData>(bodyId))
{
	simulation.addImpulseForce(bodyId, vec2(0.0f, data->jumpStrength));
}
```

---

## Anbernic muOS Deployment

Weird Engine includes scripts for building and deploying games to Anbernic handhelds running muOS.
Find these scripts in `scripts/anbernic/`.

### Prerequisites

- Install [Podman](https://podman.io/) on your PC.
- Mount the console SD card over USB using MTP (for example `mtp:/RG35XX-H/SD2`).

### Deploying a Game

Run `deploy-muos.sh` with your project path and MTP destination:

```bash
/path/to/weird-engine/scripts/anbernic/deploy-muos.sh . mtp:/RG35XX-H/SD2
```

### Fetching Device Logs

Pull log files and screenshots from the device:

```bash
/path/to/weird-engine/scripts/anbernic/fetch-logs.sh . mtp:/RG35XX-H/SD2
```

Logs are saved to `device-logs/` inside your project directory.
