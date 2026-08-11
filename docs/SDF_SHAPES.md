# Defining Shapes with Signed Distance Fields (SDFs)

This document explains how to construct, register, and instantiate Signed Distance Field (SDF) shapes in Weird Engine.
All instructions follow the ASD-STE100 Simplified Technical English standard.

---

## 1. Overview of SDF Shapes

A Signed Distance Field (SDF) evaluates the shortest distance from a world position to the surface of a shape.

- **Negative values**: Inside the shape.
- **Zero**: Exactly on the boundary.
- **Positive values**: Outside the shape.

In Weird Engine, shape definitions inherit from `IMathExpression` (defined in `include/weird-engine/math/MathExpressions.h`).

Each `IMathExpression` provides two functions:

- `getValue(const float* parameters)`: Evaluates distance on the CPU for physics collisions and CPU raymarching.
- `print()`: Returns GLSL code string to generate OpenGL raymarching shaders.

---

## 2. Using Default Primitive Shapes

Weird Engine includes default shape primitives in `WeirdEngine::DefaultShapes` (defined in `include/weird-engine/math/Default2DSDFs.h`).

### Available Default Shapes

- `DefaultShapes::CIRCLE`
- `DefaultShapes::BOX`
- `DefaultShapes::TRIANGLE`
- `DefaultShapes::LINE`
- `DefaultShapes::RAMP`
- `DefaultShapes::SINE`
- `DefaultShapes::STAR`
- `DefaultShapes::CIRCLE_LINE`
- `DefaultShapes::BOX_LINE`

### Adding a Default Shape to a Scene

Use `services.shapes().addShape(...)` with a `ShapeConfig` struct to instantiate a shape entity:

```cpp
Entity sphere = services.shapes().addShape({
	.shapeId = DefaultShapes::CIRCLE,
	.variables = {{Primitives::Circle::POS_X, 15.0f}, {Primitives::Circle::POS_Y, 10.0f}, {Primitives::Circle::RADIUS, 5.0f}},
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

## 3. Creating Custom SDF Shapes

You can build custom geometric shapes by combining mathematical expressions.

### Expression Nodes

Build expression trees using shared pointers to `IMathExpression` nodes:

- `FloatVariable(offset)`: Reads a float value from the parameter array at the specified index.
- `FloatConstant(value)`: Represents a fixed float constant.
- **Math Operations**: `Addition`, `Subtraction`, `Multiplication`, `Division`, `Sine`, `Abs`, `Min`, `Max`.
- **CSG Operations**: `SDFAddition`, `SDFSubtraction`, `SDFIntersection`, `SDFSmoothAddition`, `SDFSmoothSubtraction`, `SDFOnion`.
- **Primitives**: `Primitives::Circle`, `Primitives::Box`, `Primitives::Triangle`, `Primitives::Line`, `Primitives::Ramp`, `Primitives::SineWave`.

### Defining a Custom Ring SDF

The following example builds a ring by subtracting an inner circle from an outer circle:

```cpp
// Define variable index bindings
auto x = std::make_shared<FloatVariable>(0);
auto y = std::make_shared<FloatVariable>(1);
auto outerRadius = std::make_shared<FloatVariable>(2);
auto innerRadius = std::make_shared<FloatVariable>(3);

// Construct primitive circles
auto outer = std::make_shared<Primitives::Circle>(x, y, outerRadius);
auto inner = std::make_shared<Primitives::Circle>(x, y, innerRadius);

// Subtract inner circle from outer circle using Max(outer, -inner)
auto negatedInner = std::make_shared<Multiplication>(-1.0f, inner);
auto ringExpression = std::make_shared<Max>(outer, negatedInner);
```

---

## 4. Registering and Instantiating Custom SDFs

### Registering the SDF

Register your expression tree with `services.shapes().registerSDF(...)` to obtain a `ShapeId`:

```cpp
ShapeId ringShapeId = services.shapes().registerSDF(ringExpression);
```

You can also register global default SDFs before scene start using `Scene::registerDefaultSDF(expression)`.

### Adding the Custom Shape Entity

Pass a `ShapeConfig` struct to `addShape`. You can pass variables positionally or by index offset (`{{INDEX, value}, ...}`):

```cpp
// 1. Positional syntax
Entity ringEntity = services.shapes().addShape({
	.shapeId = ringShapeId,
	.variables = { 15.0f, 20.0f, 5.0f, 4.0f }, // POS_X, POS_Y, outerRadius, innerRadius
	.material = ringMaterial,
	.combination = CombinationType::Addition,
	.hasCollision = true, // Enable collision
	.group = 0            // Group index
});

// 2. Indexed offset syntax using constants (e.g. Primitives::Box or custom constants)
static constexpr uint8_t POS_X = 0;
static constexpr uint8_t POS_Y = 1;
static constexpr uint8_t OUTER_R = 2;
static constexpr uint8_t INNER_R = 3;

Entity ringEntity2 = services.shapes().addShape({
	.shapeId = ringShapeId,
	.variables = {{POS_X, 15.0f}, {POS_Y, 20.0f}, {OUTER_R, 5.0f}, {INNER_R, 4.0f}},
	.material = ringMaterial
});

// Adjust smooth factor on the CustomShape component
registry.getComponent<CustomShape>(ringEntity).smoothFactor = 2.0f;
```

---

## 5. Shape Combination Types

When adding shapes to a scene, specify how the shape blends with existing scene geometry:

- `CombinationType::Addition`: Standard CSG union (`min`).
- `CombinationType::Subtraction`: CSG subtraction (`max(a, -b)`).
- `CombinationType::SmoothAddition`: Smooth blending union.
- `CombinationType::SmoothSubtraction`: Smooth blending subtraction.

Example of creating a subtractive pit in the floor:

```cpp
services.shapes().addShape({
	.shapeId = DefaultShapes::CIRCLE,
	.variables = {{Primitives::Circle::POS_X, 30.0f}, {Primitives::Circle::POS_Y, 5.0f}, {Primitives::Circle::RADIUS, 4.0f}},
	.material = 0, // No material required for subtraction
	.combination = CombinationType::Subtraction,
	.hasCollision = true,
	.group = CustomShape::GLOBAL_GROUP
});
```

---

## 6. Code Reference

For full implementation examples of custom SDF registration, subtraction shapes, and parameter passing, see:

`examples/sample-scenes/include/ServiceShowcaseScene.h`

