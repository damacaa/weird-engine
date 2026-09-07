# Defining Shapes with Signed Distance Fields (SDFs)

This document explains how to construct, register, and instantiate Signed Distance Field (SDF) shapes in Weird Engine.
All instructions follow the ASD-STE100 Simplified Technical English standard.

---

## 1. Overview of SDF Shapes

A Signed Distance Field (SDF) evaluates the shortest distance from a world position to the surface of a shape.

- **Negative values**: Inside the shape.
- **Zero**: Exactly on the boundary.
- **Positive values**: Outside the shape.

In Weird Engine, shape definitions are expressed in pure C++ through the `Expr` and `Vec2Expr` domain-specific language (defined in `include/weird-engine/math/SDF.h`).

The engine evaluates expressions in two ways:
- On the CPU: Evaluates distance for physics collisions and CPU raymarching.
- On the GPU: Compiles the expression graph into optimized GLSL raymarching shaders.

---

## 2. Using Default Primitive Shapes

Weird Engine includes default shape primitives in `WeirdEngine::DefaultShapes` (defined in `include/weird-engine/math/Default2DSDFs.h`).

### Available Default Shapes

- **Standard Shapes**: `DefaultShapes::CIRCLE`, `DefaultShapes::BOX`, `DefaultShapes::TRIANGLE`, `DefaultShapes::LINE`, `DefaultShapes::RAMP`, `DefaultShapes::SINE`, `DefaultShapes::STAR`
- **Border / Line Shapes**: `DefaultShapes::CIRCLE_LINE`, `DefaultShapes::BOX_LINE`, `DefaultShapes::TRIANGLE_LINE`
- **Rotated Shapes**: `DefaultShapes::BOX_ROTATED`, `DefaultShapes::TRIANGLE_ROTATED`, `DefaultShapes::RAMP_ROTATED`
- **Rotated Border Shapes**: `DefaultShapes::BOX_LINE_ROTATED`, `DefaultShapes::TRIANGLE_LINE_ROTATED`

### Parameter Offset Constants

Each default shape provides parameter index constants under `DefaultShapes::<ShapeName>` (or `Primitives::<ShapeName>`):
- `DefaultShapes::Circle::POS_X`, `POS_Y`, `RADIUS`
- `DefaultShapes::Box::POS_X`, `POS_Y`, `SIZE_X`, `SIZE_Y`
- `DefaultShapes::Triangle::POS_X`, `POS_Y`, `WIDTH`, `HEIGHT`
- `DefaultShapes::Line::START_X`, `START_Y`, `END_X`, `END_Y`, `WIDTH`
- `DefaultShapes::Ramp::POS_X`, `POS_Y`, `WIDTH`, `HEIGHT`, `SKEW`

### Adding a Default Shape to a Scene

Use `services.shapes().addShape(...)` with a `ShapeConfig` struct to instantiate a shape entity:

```cpp
Entity circle = services.shapes().addShape({
	.shapeId = DefaultShapes::CIRCLE,
	.variables = {
		{DefaultShapes::Circle::POS_X, 15.0f},
		{DefaultShapes::Circle::POS_Y, 10.0f},
		{DefaultShapes::Circle::RADIUS, 5.0f}
	},
	.material = materialId,
	.combination = CombinationType::Addition,
	.hasCollision = true // Enable collisions
});
```

### Adding a UI Shape

For rendering shapes on the 2D user interface layer (which is screen-space and does not use physical collisions), use `addUIShape` with a `UIShapeConfig`:

```cpp
Entity uiBox = services.shapes().addUIShape({
	.shapeId = DefaultShapes::BOX,
	.variables = {Display::width * 0.5f, 90.0f, 40.0f, 14.0f}, // Inline positional array
	.material = 2,
	.combination = CombinationType::Addition
});
```

---

## 3. Creating Custom SDF Shapes with `Expr`

Build custom geometric shapes by composing mathematical expressions using `Expr` and `Vec2Expr`.

### Building Expressions

Include `#include "weird-engine/math/SDF.h"`.

- **Variables & Points**:
  - `var(index)`: Accesses entity parameter float at `index` (`0` to `7`).
  - `worldPoint()`: Returns the 2D evaluation coordinate (`worldPoint().x`, `worldPoint().y`).
  - `time()`: Returns the current scene elapsed time.
- **SDF Primitives**:
  - `sdCircle(p, radius)`
  - `sdBox(p, halfSize)`
  - `sdSegment(p, a, b)`
  - `sdLine(p, a, b, width)`
  - `sdTriangle(p, width, height)`
  - `sdRamp(p, width, height, skew)`
  - `sdPolygon(p, vertices)`
  - `sdTerrain(p, surfacePoints, valleyRadius)`
  - `sdStar(p, radius, displacement, points, speed)`
- **Transforms & CSG Combinations**:
  - `translate(p, offset)`
  - `rotate(p, angle)`
  - `sdfUnion(a, b)` (or `min(a, b)`)
  - `sdfSubtract(a, b)`
  - `sdfIntersect(a, b)`
  - `sdfSmoothUnion(a, b, radius)`
  - `sdfSmoothSubtract(a, b, radius)`
  - `sdfOnion(d, thickness)`
  - `sdfRound(d, radius)`
  - `sdfErode(d, radius)`

### Defining a Custom Ring SDF

The following example builds a ring by subtracting an inner circle from an outer circle:

```cpp
using namespace WeirdEngine::SDF;

// Translate evaluation point by entity position: var(0) = X, var(1) = Y
auto p = translate(worldPoint(), {var(0), var(1)});

// Subtract inner circle (radius = var(3)) from outer circle (radius = var(2))
Expr ring = sdfSubtract(sdCircle(p, var(2)), sdCircle(p, var(3)));
```

---

## 4. Registering and Instantiating Custom SDFs

### Registering the SDF

Register your `Expr` directly with `services.shapes().registerSDF(expr)` to obtain a `ShapeId`:

```cpp
ShapeId ringShapeId = services.shapes().registerSDF(ring);
```

You can also register global default SDFs before scene start using `Scene::registerDefaultSDF(ring)`.

### Adding the Custom Shape Entity

Pass a `ShapeConfig` struct to `addShape`. You can pass variables positionally or by index offset (`{{INDEX, value}, ...}`):

```cpp
// 1. Positional syntax
Entity ringEntity = services.shapes().addShape({
	.shapeId = ringShapeId,
	.variables = {15.0f, 10.0f, 5.0f, 4.0f}, // [pos_x, pos_y, outer_radius, inner_radius]
	.material = ringMaterial,
	.combination = CombinationType::Addition,
	.hasCollision = true,
	.group = 0
});

// 2. Indexed offset syntax using constants
Entity ringEntity2 = services.shapes().addShape({
	.shapeId = ringShapeId,
	.variables = {
		{0, 30.0f}, // pos_x
		{1, 5.0f},  // pos_y
		{2, 6.0f},  // outer_radius
		{3, 4.5f}   // inner_radius
	},
	.material = ringMaterial
});
```
