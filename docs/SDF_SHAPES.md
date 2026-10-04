# Defining Shapes with Signed Distance Fields (SDFs)

This document explains how to construct, register, and instantiate Signed Distance Field (SDF) shapes in Weird Engine.
All instructions follow the ASD-STE100 Simplified Technical English standard.

---

## 1. Overview of SDF Shapes

A Signed Distance Field (SDF) evaluates the shortest distance from a coordinate position to the surface of a shape.

- **Negative values**: Inside the shape.
- **Zero**: Exactly on the boundary surface.
- **Positive values**: Outside the shape.

In Weird Engine, shape definitions are expressed in pure C++ through the `Expr`, `Vec2Expr`, and `Vec3Expr` domain-specific language (defined in `include/weird-engine/math/SDF.h`).

The engine evaluates expressions in two ways:
- **On the CPU**: Evaluates distance for physics collisions (via `getValue(parameters)`) and CPU queries.
- **On the GPU**: Compiles the expression graph into optimized GLSL raymarching shaders with Common Subexpression Elimination (CSE).

---

## 2. Using Default Primitive Shapes

Weird Engine includes pre-registered 2D and 3D default shape primitives.

### 2.1 Default 2D Shapes (`DefaultShapes`)

Defined in `include/weird-engine/math/Default2DSDFs.h` under namespace `WeirdEngine::DefaultShapes`:

- **Standard Shapes**: `Circle`, `Box`, `Triangle`, `Line`, `Ramp`, `SineWave`, `Star`
- **Border / Line Shapes**: `CircleLine`, `BoxLine`, `TriangleLine`
- **Rotated Shapes**: `BoxRotated`, `TriangleRotated`, `RampRotated`
- **Rotated Border Shapes**: `BoxLineRotated`, `TriangleLineRotated`

#### Parameter Constants (2D)

- `Circle::PosX`, `PosY`, `Radius`
- `CircleLine::PosX`, `PosY`, `Radius`, `Thickness`
- `Box::PosX`, `PosY`, `SizeX`, `SizeY` (half-extents)
- `BoxLine::PosX`, `PosY`, `SizeX`, `SizeY`, `Thickness`
- `Triangle::PosX`, `PosY`, `Width`, `Height`
- `TriangleLine::PosX`, `PosY`, `Width`, `Height`, `Thickness`
- `Line::StartX`, `StartY`, `EndX`, `EndY`, `Width`
- `Ramp::PosX`, `PosY`, `Width`, `Height`, `Skew`
- `SineWave::Amplitude`, `Period`, `Speed`, `Offset`
- `BoxRotated::PosX`, `PosY`, `SizeX`, `SizeY`, `Angle`
- `TriangleRotated::PosX`, `PosY`, `Width`, `Height`, `Angle`
- `RampRotated::PosX`, `PosY`, `Width`, `Height`, `Skew`, `Angle`

### 2.2 Default 3D Shapes (`DefaultShapes3D`)

Defined in `include/weird-engine/math/Default3DSDFs.h` under namespace `WeirdEngine::DefaultShapes3D`:

- `DefaultShapes3D::Plane`: Flat horizontal ground plane
- `DefaultShapes3D::Box`: 3D rectangular cuboid
- `DefaultShapes3D::Sphere`: 3D sphere
- `DefaultShapes3D::Cylinder`: Vertical cylinder along Y axis
- `DefaultShapes3D::Torus`: Torus on XZ plane
- `DefaultShapes3D::Capsule`: Vertical capsule along Y axis

#### Parameter Constants (3D)

- `Plane::Height`
- `Box::PosX`, `PosY`, `PosZ`, `SizeX`, `SizeY`, `SizeZ` (half-extents)
- `Sphere::PosX`, `PosY`, `PosZ`, `Radius`
- `Cylinder::PosX`, `PosY`, `PosZ`, `Radius`, `Height` (half-height)
- `Torus::PosX`, `PosY`, `PosZ`, `MajorRadius`, `MinorRadius`
- `Capsule::PosX`, `PosY`, `PosZ`, `Radius`, `Height` (half-height)

---

## 3. Adding Shapes to a Scene

### Adding a 2D Shape

Use `services.shapes().addShape(...)` with a `ShapeConfig` struct:

```cpp
Entity circle = services.shapes().addShape({
	.shapeId = DefaultShapes::Circle,
	.variables = {
		{DefaultShapes::Circle::PosX, 15.0f},
		{DefaultShapes::Circle::PosY, 10.0f},
		{DefaultShapes::Circle::Radius, 5.0f}
	},
	.material = materialId,
	.combination = CombinationType::Addition,
	.hasCollision = true // Enable 2D physics collisions
});
```

### Adding a 3D Shape

```cpp
Entity box = services.shapes().addShape({
	.shapeId = DefaultShapes3D::Box,
	.variables = {
		{DefaultShapes3D::Box::PosX, 0.0f},
		{DefaultShapes3D::Box::PosY, 2.5f},
		{DefaultShapes3D::Box::PosZ, 0.0f},
		{DefaultShapes3D::Box::SizeX, 2.0f},
		{DefaultShapes3D::Box::SizeY, 2.0f},
		{DefaultShapes3D::Box::SizeZ, 2.0f}
	},
	.material = whiteMat,
	.combination = CombinationType::Addition,
	.hasCollision = false
});
```

### Adding a UI Shape (2D Screen-Space)

For rendering shapes on the 2D user interface layer (which is screen-space and does not use physical collisions), use `addUIShape` with a `UIShapeConfig`:

```cpp
Entity uiBox = services.shapes().addUIShape({
	.shapeId = DefaultShapes::Box,
	.variables = {Display::width * 0.5f, 90.0f, 40.0f, 14.0f}, // Inline positional array
	.material = 2,
	.combination = CombinationType::Addition
});
```

---

## 4. Creating Custom SDF Shapes with `Expr`

Build custom geometric shapes by composing mathematical expressions using `Expr`, `Vec2Expr`, and `Vec3Expr`.

Include `#include "weird-engine/math/SDF.h"`.

### Variables & Coordinates

- `var(index)`: Accesses entity parameter float at `index` (`0` to `7`).
- `point()`: Returns the 2D evaluation coordinate (`Vec2Expr {point().x, point().y}`).
- `point3D()`: Returns the 3D evaluation coordinate (`Vec3Expr {point3D().x, point3D().y, point3D().z}`).
- `time()`: Returns the current scene elapsed time.

### 2D Primitives

- `sdCircle(p, radius)`
- `sdBox(p, halfSize)`
- `sdSegment(p, a, b)`
- `sdLine(p, a, b, width)`
- `sdTriangle(p, width, height)`
- `sdRamp(p, width, height, skew)`
- `sdPolygon(p, vertices)`
- `sdTerrain(p, surfacePoints, valleyRadius)`
- `sdStar(p, radius, displacement, points, speed)`
- `sdSineWave(p, amplitude, frequency, speed, offset)`

### 3D Primitives

- `sdPlane(p, height)` or `sdPlane(p, normal, distanceFromOrigin)`
- `sdSphere(p, radius)`
- `sdBox(p, halfSize)`
- `sdCylinder(p, radius, height)`
- `sdTorus(p, majorRadius, minorRadius)`
- `sdCapsule(p, radius, height)` (vertical) or `sdCapsule(p, a, b, radius)` (segment)

### Transforms

- **2D**: `translate(p, offset)`, `rotate(p, angle)`, `mirrorX(p)`, `repeat(p, spacing)`
- **3D**: `translate(p, offset)`, `rotateX(p, angle)`, `rotateY(p, angle)`, `rotateZ(p, angle)`

### CSG & Modifiers

- `sdfUnion(a, b)` (or `min(a, b)`)
- `sdfSubtract(a, b)` (or `max(a, -b)`)
- `sdfIntersect(a, b)` (or `max(a, b)`)
- `sdfSmoothUnion(a, b, radius)`
- `sdfSmoothSubtract(a, b, radius)`
- `sdfOnion(d, thickness)`
- `sdfRound(d, radius)`
- `sdfErode(d, radius)`

---

## 5. Registering and Instantiating Custom SDFs

### Registering Custom 2D / 3D SDF

Register your `Expr` with `services.shapes().registerSDF(expr)` to obtain a dynamic `ShapeId` for the current scene:

```cpp
using namespace WeirdEngine::SDF;

// Custom 2D Ring:
auto p2 = translate(point(), {var(0), var(1)});
Expr ring = sdfSubtract(sdCircle(p2, var(2)), sdCircle(p2, var(3)));
ShapeId ringShapeId = services.shapes().registerSDF(ring);

// Custom 3D Hollow Sphere:
auto p3 = translate(point3D(), {var(0), var(1), var(2)});
Expr hollowSphere = sdfOnion(sdSphere(p3, var(3)), var(4));
ShapeId hollowSphereId = services.shapes().registerSDF(hollowSphere);
```

### Adding Custom Shape Entity

```cpp
// Add custom 3D shape entity
Entity hollowSphereEntity = services.shapes().addShape({
	.shapeId = hollowSphereId,
	.variables = {0.0f, 5.0f, 0.0f, 3.0f, 0.2f}, // [pos_x, pos_y, pos_z, radius, thickness]
	.material = materialId,
	.combination = CombinationType::Addition,
	.hasCollision = false
});
```
