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
inline Expr audioVolume();
inline Vec2Expr point();
inline Vec2Expr samplePoint();
```

The parameter convention is:

| Variable | Meaning |
|---|---|
| `var0` through `var7` | Shape-specific parameters. |
| `var8` | Time. |
| `var9` | Sample Point X. |
| `var10` | Sample Point Y. |
| `var11` | Audio Volume. |

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

Built-in engine shapes are defined in `Default2DSDFs.h` (`DefaultShapes`) and `Default3DSDFs.h` (`DefaultShapes3D`) via compile-time X-macro definition tables (`WEIRD_BUILTIN_SHAPES_2D` and `WEIRD_BUILTIN_SHAPES_3D`).

The table automatically generates:
1. Compile-time `ShapeId` enum constants (e.g. `DefaultShapes::Circle`, `DefaultShapes::Box`).
2. Scoped parameter index structures (e.g. `DefaultShapes::Circle::PosX`, `POS_Y`, `RADIUS`).
3. Deterministic registration in `Scene::registerBuiltinSDFs()` stored in a fixed `std::array` without heap allocations.

## Default 2D Shapes

These are defined in `WeirdEngine::DefaultShapes`.

| Shape | Expression Used | Parameters |
|---|---|---|
| `Circle` | `sdCircle` | `var0` X, `var1` Y, `var2` radius |
| `CircleLine` | `sdfOnion(sdCircle(...))` | `var0` X, `var1` Y, `var2` radius, `var3` thickness |
| `Box` | `sdBox` | `var0` X, `var1` Y, `var2` size X, `var3` size Y |
| `BoxLine` | `sdfOnion(sdBox(...))` | `var0` X, `var1` Y, `var2` size X, `var3` size Y, `var4` thickness |
| `Triangle` | `sdTriangle` | `var0` X, `var1` Y, `var2` width, `var3` height |
| `TriangleLine` | `sdfOnion(sdTriangle(...))` | `var0` X, `var1` Y, `var2` width, `var3` height, `var4` thickness |
| `Line` | `sdLine` | `var0` A.X, `var1` A.Y, `var2` B.X, `var3` B.Y, `var4` width |
| `Ramp` | `sdRamp` | `var0` X, `var1` Y, `var2` width, `var3` height, `var4` skew |
| `SineWave` | `sdSineWave` | `var0` amplitude, `var1` frequency, `var2` speed, `var3` offset |
| `Star` | `sdStar` via `getStarShape()` | `var0` X, `var1` Y, `var2` radius, `var3` displacement, `var4` points, `var5` speed |
| `BoxRotated` | rotated `sdBox` | `var0` X, `var1` Y, `var2` size X, `var3` size Y, `var4` angle |
| `BoxLineRotated` | rotated `sdBox` + onion | `var0` X, `var1` Y, `var2` size X, `var3` size Y, `var4` angle, `var5` thickness |
| `TriangleRotated` | rotated `sdTriangle` | `var0` X, `var1` Y, `var2` width, `var3` height, `var4` angle |
| `TriangleLineRotated` | rotated `sdTriangle` + onion | `var0` X, `var1` Y, `var2` width, `var3` height, `var4` angle, `var5` thickness |
| `RampRotated` | rotated `sdRamp` | `var0` X, `var1` Y, `var2` width, `var3` height, `var4` skew, `var5` angle |

## Default 3D Shapes

These are defined in `WeirdEngine::DefaultShapes3D` using the unified `SDF.h` expression system (`sdPlane`, `sdBox`, `sdSphere`, `sdCylinder`, `sdTorus`, `sdCapsule`).

| Shape | Expression | Parameters |
|---|---|---|
| `Plane` | `sdPlane` | `var0` height |
| `Box` | `sdBox` | `var0` X, `var1` Y, `var2` Z, `var3` size X, `var4` size Y, `var5` size Z |
| `Sphere` | `sdSphere` | `var0` X, `var1` Y, `var2` Z, `var3` radius |
| `Cylinder` | `sdCylinder` | `var0` X, `var1` Y, `var2` Z, `var3` radius, `var4` height |
| `Torus` | `sdTorus` | `var0` X, `var1` Y, `var2` Z, `var3` major radius, `var4` minor radius |
| `Capsule` | `sdCapsule` | `var0` X, `var1` Y, `var2` Z, `var3` radius, `var4` height |

In 3D evaluation, coordinates are sampled via `point3D()` (`var9` = X, `var10` = Y, `var11` = Z, with slot 11 reused for Z). Expressions support full AST traversal, CSE shader optimization, and CPU evaluation.

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

The expression is built using `SDF::translate`, `SDF::point`, and `SDF::sdStar`.

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

- The module defines both 2D and 3D expressions via the composable `SDF` DSL in `SDF.h`.
- `Default2DSDFs.h` and `Default3DSDFs.h` use the same X-macro and `SDF` expression architecture.
- Both 2D and 3D expressions support CPU evaluation and GLSL shader generation with CSE optimization.
- `print()` output is shader-oriented, not necessarily valid C++.
- `collectHelperFunctions()` is used to emit GLSL helper functions for complex shapes such as polygons, triangles, and ramps.
- The compiled bytecode VM is smaller and more experimental than the expression tree system.
