# Math Module Report

This report describes the headers in `include/weird-engine/math/`.

The math module provides a small expression system used to describe 2D and 3D SDF shapes. It supports CPU evaluation through `getValue(const float* parameters)`, printable expression output through `print()`, and shader helper generation through `collectHelperFunctions()`.

## File Inventory

| Header | Role |
|---|---|
| `MathExpressions.h` | Core expression interface and basic math nodes. |
| `SDF.h` | Higher-level `Expr` wrapper, 2D SDF helpers, and complex SDF primitives. |
| `CompiledMathExpressions.h` | Experimental bytecode VM representation for math expressions. |
| `Default2DSDFs.h` | Registers default 2D SDF shapes with `Scene`. |
| `Default3DSDFs.h` | Registers default 3D SDF shapes with `Scene`. |
| `Primitives.h` | Standalone 2D primitive expression classes. |
| `Primitives3D.h` | Standalone 3D primitive expression classes. |
| `ShapeMacro.h` | Abstract base type for shape macros. |
| `StarShape.h` | Helper that builds a star SDF expression. |

## Core Expression Model

`MathExpressions.h` defines the central interface:

```cpp
struct IMathExpression
{
    virtual float getValue(const float* parameters) const = 0;
    virtual std::string print() const = 0;
    virtual bool isTrivial() const;
    virtual void getChildren(std::vector<std::shared_ptr<IMathExpression>>& out) const;
    virtual std::string printWithChildren(const std::vector<std::string>& childCode) const;
    virtual void collectHelperFunctions(std::unordered_set<std::string>& helpers) const;
    virtual std::string getHelperFunctions() const;
};
```

This interface is intentionally simple: an expression can be evaluated against a parameter array, printed as text, and optionally emit helper functions needed by generated shader code.

### Variables and Constants

`FloatVariable` represents a parameter lookup:

```cpp
struct FloatVariable : IMathExpression
{
    std::ptrdiff_t m_offset;
};
```

`getValue()` returns `parameters[m_offset]`. `print()` returns values such as `var0`, `var8`, or `var9`.

`FloatConstant` represents a constant float value.

### Arithmetic and Math Nodes

`MathExpressions.h` includes expression nodes for:

- One-float operations: `Sine`, `Cosine`, `Abs`, `Negation`, `Sqrt`.
- Two-float operations: `Addition`, `Subtraction`, `Multiplication`, `Division`, `Mod`, `Atan2`, `Length`, `Max`, `Min`.
- SDF operations: `SDFAddition`, `SDFSubtraction`, `SDFIntersection`, `SDFOnion`.
- Three-float operations: `Clamp`, `SDFSmoothAddition`, `SDFSmoothSubtraction`.

The SDF operation names are important:

| Node | Behavior | SDF Meaning |
|---|---|---|
| `SDFAddition` | `min(a, b)` | Union. |
| `SDFSubtraction` | `max(a, -b)` | Boolean subtraction. |
| `SDFIntersection` | `max(a, b)` | Intersection. |
| `SDFOnion` | `abs(a) - b` | Hollow/outline shape. |

## SDF Expression DSL

`SDF.h` adds a more convenient layer on top of `IMathExpression`.

### `Expr`

`Expr` wraps a `std::shared_ptr<IMathExpression>` and supports C++-style operators:

```cpp
struct Expr
{
    std::shared_ptr<IMathExpression> node;
};
```

It supports:

- `operator+`
- `operator-`
- `operator*`
- `operator/`
- unary `operator-`

The operators perform constant folding where possible. For example, if both operands are `FloatConstant`, the result is also a constant. If one operand is zero or one, the expression may be simplified.

### `Vec2Expr`

`Vec2Expr` is a pair of `Expr` values:

```cpp
struct Vec2Expr
{
    Expr x, y;
};
```

It supports vector addition, subtraction, scalar multiplication, and scalar division.

### Common Helpers

`SDF.h` defines helpers such as:

```cpp
inline Expr var(int index);
inline Expr time();
inline Vec2Expr worldPoint();
inline Vec2Expr uiPoint();
```

The parameter convention is:

| Variable | Meaning |
|---|---|
| `var0` through `var7` | Shape-specific parameters. |
| `var8` | Time. |
| `var9` | World X. |
| `var10` | World Y. |
| `var11` | UI X. |
| `var12` | UI Y. |

### 2D SDF Functions

The `SDF` namespace provides many 2D SDF builders:

- `translate`
- `rotate`
- `mirrorX`
- `sdCircle`
- `sdBox`
- `sdSegment`
- `sdLine`
- `sdfUnion`
- `sdfSubtract`
- `sdfIntersect`
- `sdfOnion`
- `sdfRound`
- `sdfErode`
- `sdfSmoothUnion`
- `sdfSmoothSubtract`
- `sdStar`
- `sdSineWave`
- `sdPolygon`
- `sdTerrain`
- `sdTriangle`
- `sdRamp`

Some of these are simple expression compositions. Others, such as `Polygon`, `Triangle`, and `Ramp`, are full `IMathExpression` implementations with CPU evaluation and shader helper generation.

## Default Registered Shapes

`Default2DSDFs.h` and `Default3DSDFs.h` register built-in shapes with `Scene` through `Scene::registerDefaultSDF(...)`. Each registered shape is exposed as an inline `uint16_t` constant.

## Default 2D Shapes

These are defined in `WeirdEngine::DefaultShapes`.

| Shape | Expression Used | Parameters |
|---|---|---|
| `CIRCLE` | `sdCircle` | `var0` X, `var1` Y, `var2` radius |
| `CIRCLE_LINE` | `sdfOnion(sdCircle(...))` | `var0` X, `var1` Y, `var2` radius, `var3` thickness |
| `BOX` | `sdBox` | `var0` X, `var1` Y, `var2` size X, `var3` size Y |
| `BOX_LINE` | `sdfOnion(sdBox(...))` | `var0` X, `var1` Y, `var2` size X, `var3` size Y, `var4` thickness |
| `TRIANGLE` | `sdTriangle` | `var0` X, `var1` Y, `var2` width, `var3` height |
| `TRIANGLE_LINE` | `sdfOnion(sdTriangle(...))` | `var0` X, `var1` Y, `var2` width, `var3` height, `var4` thickness |
| `LINE` | `sdLine` | `var0` A.X, `var1` A.Y, `var2` B.X, `var3` B.Y, `var4` width |
| `RAMP` | `sdRamp` | `var0` X, `var1` Y, `var2` width, `var3` height, `var4` skew |
| `SINE` | `sdSineWave` | `var0` amplitude, `var1` frequency, `var2` speed, `var3` offset |
| `STAR` | `sdStar` via `getStarShape()` | `var0` X, `var1` Y, `var2` radius, `var3` displacement, `var4` points, `var5` speed |
| `BOX_ROTATED` | rotated `sdBox` | `var0` X, `var1` Y, `var2` size X, `var3` size Y, `var4` angle |
| `BOX_LINE_ROTATED` | rotated `sdBox` + onion | `var0` X, `var1` Y, `var2` size X, `var3` size Y, `var4` angle, `var5` thickness |
| `TRIANGLE_ROTATED` | rotated `sdTriangle` | `var0` X, `var1` Y, `var2` width, `var3` height, `var4` angle |
| `TRIANGLE_LINE_ROTATED` | rotated `sdTriangle` + onion | `var0` X, `var1` Y, `var2` width, `var3` height, `var4` angle, `var5` thickness |
| `RAMP_ROTATED` | rotated `sdRamp` | `var0` X, `var1` Y, `var2` width, `var3` height, `var4` skew, `var5` angle |

## Default 3D Shapes

These are defined in `WeirdEngine::DefaultShapes3D`.

| Shape | Primitive Class | Parameters |
|---|---|---|
| `PLANE` | `Primitives3D::Plane` | `var0` height |
| `BOX` | `Primitives3D::Box` | `var0` X, `var1` Y, `var2` Z, `var3` size X, `var4` size Y, `var5` size Z |
| `SPHERE` | `Primitives3D::Sphere` | `var0` X, `var1` Y, `var2` Z, `var3` radius |
| `CYLINDER` | `Primitives3D::Cylinder` | `var0` X, `var1` Y, `var2` Z, `var3` radius, `var4` height |
| `TORUS` | `Primitives3D::Torus` | `var0` X, `var1` Y, `var2` Z, `var3` small radius, `var4` large radius |
| `CAPSULE` | `Primitives3D::Capsule` | `var0` X, `var1` Y, `var2` Z, `var3` radius, `var4` height |

## 2D Primitive Classes

`Primitives.h` defines `WeirdEngine::Primitives`, a set of 2D shape classes that directly implement `IMathExpression`.

Visible primitive classes include:

- `Circle`
- `Box`
- `SineWave`
- `Ramp`
- `Triangle`
- `Line`

There are also parameter-layout structs:

- `BoxRotated`
- `TriangleRotated`
- `RampRotated`
- `Star`

These classes duplicate much of the functionality available through the `SDF` expression DSL. They are useful when a shape needs a self-contained expression object with explicit parameter constants and shader printing behavior.

Example parameter constants from `Primitives::Circle`:

```cpp
static constexpr uint8_t POS_X = 0;
static constexpr uint8_t POS_Y = 1;
static constexpr uint8_t RADIUS = 2;
static constexpr uint8_t THICKNESS = 3;
static constexpr uint8_t TIME = 8;
static constexpr uint8_t WORLD_X = 9;
static constexpr uint8_t WORLD_Y = 10;
```

## 3D Primitive Classes

`Primitives3D.h` defines `WeirdEngine::Primitives3D`.

Visible primitive classes include:

- `Plane`
- `PerlinPlane`
- `Box`
- `Sphere`
- `Cylinder`
- `Torus`
- `Capsule`

These classes define parameter constants and `print()` methods that emit shader-style expressions such as:

```glsl
fBox(p - vec3(var0, var1, var2), vec3(var3, var4, var5))
fSphere(p - vec3(var0, var1, var2), var3)
fCylinder(p - vec3(var0, var1, var2), var3, var4)
fTorus(p - vec3(var0, var1, var2), var3, var4)
fCapsule(p - vec3(var0, var1, var2), var3, var4)
```

Their CPU `getValue()` implementations currently return `1000.0f`, which indicates that 3D primitives are primarily intended for shader-side evaluation rather than CPU-side SDF evaluation.

## Shape Macro Base

`ShapeMacro.h` defines:

```cpp
struct ShapeMacro : IMathExpression
{
    static constexpr uint8_t VALUES_SIZE = 11;
    static constexpr uint8_t TIME = 8;
    static constexpr uint8_t WORLD_X = 9;
    static constexpr uint8_t WORLD_Y = 10;

    virtual float getValue(const float* parameters) const = 0;
    virtual std::string print() const = 0;
};
```

It is an abstract base class for shape macros that share the common 2D parameter layout.

## Star Shape Helper

`StarShape.h` provides:

```cpp
inline std::shared_ptr<IMathExpression> getStarShape();
```

It constructs a star SDF from:

- `var0`: X position
- `var1`: Y position
- `var2`: radius
- `var3`: displacement
- `var4`: points
- `var5`: speed

The expression is built using `SDF::translate`, `SDF::worldPoint`, and `SDF::sdStar`.

## Compiled Math Expressions

`CompiledMathExpressions.h` introduces a bytecode-style representation:

```cpp
struct CompiledMathExpression
{
    enum class OpCode : uint8_t
    {
        PUSH_VAR,
        PUSH_CONST,
        SUBTRACT,
        LENGTH,
        MAX
    };

    struct Instruction
    {
        OpCode code;
        float operand;
    };

    std::vector<Instruction> m_program;
};
```

`getValue()` lazily calls `compile()` and then runs `evaluateVM()`, a small stack machine using a fixed-size CPU stack.

A `CompiledCircle` example compiles the circle SDF:

```text
length(world - pos) - radius
```

This appears to be an experimental alternate representation for expressions. It is separate from the main `IMathExpression` tree and currently has a limited opcode set.

## Observations

- The module has two main 2D expression styles:
  - The composable `SDF` DSL in `SDF.h`.
  - The standalone primitive classes in `Primitives.h`.
- `Default2DSDFs.h` primarily uses the `SDF` DSL.
- `Default3DSDFs.h` uses `Primitives3D` classes.
- 2D expressions generally support CPU evaluation.
- 3D expressions appear to be mainly shader-printing placeholders, because their CPU `getValue()` implementations return a large constant.
- `print()` output is shader-oriented, not necessarily valid C++.
- `collectHelperFunctions()` is used to emit GLSL helper functions for complex shapes such as polygons, triangles, and ramps.
- The compiled bytecode VM is smaller and more experimental than the expression tree system.
