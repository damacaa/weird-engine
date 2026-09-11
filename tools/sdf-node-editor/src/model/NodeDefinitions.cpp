#include "model/NodeRegistry.h"

#include "weird-engine/math/MathExpressions.h"
#include "weird-engine/math/Primitives.h"
#include "weird-engine/math/Primitives3D.h"
#include "weird-engine/math/SDF.h"

namespace WeirdEngine::Editor
{
#define REGISTER_UNARY_OP(TypeId, DisplayName, Func, CppName)                                                          \
	registerNode(NodeDef{TypeId,                                                                                       \
						 DisplayName,                                                                                  \
						 NodeCategory::MathUnary,                                                                      \
						 {{"in", PinType::Float, 0.0f}},                                                               \
						 {{"out", PinType::Float}},                                                                    \
						 [](const auto& in, const auto&) { return NodeValue(Func(asFloat(in[0]))); },                  \
						 [](const auto& in, const auto&) { return std::string(CppName) + "(" + in[0] + ")"; }})

#define REGISTER_BINARY_OP(Cat, TypeId, DisplayName, In1, In2, Def1, Def2, Func, CppExpr)                              \
	registerNode(NodeDef{TypeId,                                                                                       \
						 DisplayName,                                                                                  \
						 Cat,                                                                                          \
						 {{In1, PinType::Float, Def1}, {In2, PinType::Float, Def2}},                                   \
						 {{"out", PinType::Float}},                                                                    \
						 [](const auto& in, const auto&) { return NodeValue(Func(asFloat(in[0]), asFloat(in[1]))); },  \
						 [](const auto& in, const auto&) { return CppExpr; }})

#define REGISTER_TERNARY_OP(Cat, TypeId, DisplayName, In1, In2, In3, Def1, Def2, Def3, Func, CppExpr)                  \
	registerNode(NodeDef{TypeId,                                                                                       \
						 DisplayName,                                                                                  \
						 Cat,                                                                                          \
						 {{In1, PinType::Float, Def1}, {In2, PinType::Float, Def2}, {In3, PinType::Float, Def3}},      \
						 {{"out", PinType::Float}},                                                                    \
						 [](const auto& in, const auto&)                                                               \
						 { return NodeValue(Func(asFloat(in[0]), asFloat(in[1]), asFloat(in[2]))); },                  \
						 [](const auto& in, const auto&) { return CppExpr; }})

	void NodeRegistry::registerAllNodes()
	{
		// =====================================================================
		// 1. Inputs & Coordinates
		// =====================================================================
		registerNode(NodeDef{"const_float",
							 "Constant Float",
							 NodeCategory::Input,
							 {},
							 {{"val", PinType::Float}},
							 [](const auto&, const NodeInstance& node)
							 { return NodeValue(Expr(node.data.customFloat)); },
							 [](const auto&, const NodeInstance& node)
							 { return "Expr(" + std::to_string(node.data.customFloat) + "f)"; }});

		registerNode(NodeDef{"param_var",
							 "Variable (var0..var7)",
							 NodeCategory::Input,
							 {},
							 {{"val", PinType::Float}},
							 [](const auto&, const NodeInstance& node) { return NodeValue(var(node.data.customInt)); },
							 [](const auto&, const NodeInstance& node)
							 { return "var(" + std::to_string(node.data.customInt) + ")"; }});

		registerNode(NodeDef{"time",
							 "Time",
							 NodeCategory::Input,
							 {},
							 {{"time", PinType::Float}},
							 [](const auto&, const auto&) { return NodeValue(time()); },
							 [](const auto&, const auto&) { return "time()"; }});

		registerNode(NodeDef{"audio_volume",
							 "Audio Volume",
							 NodeCategory::Input,
							 {},
							 {{"vol", PinType::Float}},
							 [](const auto&, const auto&) { return NodeValue(audioVolume()); },
							 [](const auto&, const auto&) { return "audioVolume()"; }});

		registerNode(NodeDef{"point",
							 "Sample Point (p)",
							 NodeCategory::Input,
							 {},
							 {{"p", PinType::Vec2}},
							 [](const auto&, const auto&) { return NodeValue(point()); },
							 [](const auto&, const auto&) { return "point()"; }});

		// =====================================================================
		// 2. Vector Math
		// =====================================================================
		registerNode(NodeDef{"make_vec2",
							 "Make Vec2",
							 NodeCategory::Vector,
							 {{"x", PinType::Float, 0.0f}, {"y", PinType::Float, 0.0f}},
							 {{"vec2", PinType::Vec2}},
							 [](const auto& in, const auto&)
							 { return NodeValue(Vec2Expr(asFloat(in[0]), asFloat(in[1]))); },
							 [](const auto& in, const auto&) { return "Vec2Expr(" + in[0] + ", " + in[1] + ")"; }});

		registerNode(NodeDef{"break_vec2",
							 "Break Vec2",
							 NodeCategory::Vector,
							 {{"vec2", PinType::Vec2}},
							 {{"x", PinType::Float}, {"y", PinType::Float}},
							 [](const auto& in, const auto&)
							 {
								 Vec2Expr p = asVec2(in[0]);
								 return NodeValue(p.x);
							 },
							 [](const auto& in, const auto&) { return in[0] + ".x"; }});

		registerNode(NodeDef{"vec2_add",
							 "Vec2 Add",
							 NodeCategory::Vector,
							 {{"a", PinType::Vec2}, {"b", PinType::Vec2}},
							 {{"out", PinType::Vec2}},
							 [](const auto& in, const auto&) { return NodeValue(asVec2(in[0]) + asVec2(in[1])); },
							 [](const auto& in, const auto&) { return "(" + in[0] + " + " + in[1] + ")"; }});

		registerNode(NodeDef{"vec2_sub",
							 "Vec2 Subtract",
							 NodeCategory::Vector,
							 {{"a", PinType::Vec2}, {"b", PinType::Vec2}},
							 {{"out", PinType::Vec2}},
							 [](const auto& in, const auto&) { return NodeValue(asVec2(in[0]) - asVec2(in[1])); },
							 [](const auto& in, const auto&) { return "(" + in[0] + " - " + in[1] + ")"; }});

		registerNode(NodeDef{"vec2_scale",
							 "Vec2 Scale",
							 NodeCategory::Vector,
							 {{"p", PinType::Vec2}, {"scale", PinType::Float, 1.0f}},
							 {{"out", PinType::Vec2}},
							 [](const auto& in, const auto&) { return NodeValue(asVec2(in[0]) * asFloat(in[1])); },
							 [](const auto& in, const auto&) { return "(" + in[0] + " * " + in[1] + ")"; }});

		registerNode(NodeDef{"vec2_length",
							 "Vec2 Length",
							 NodeCategory::Vector,
							 {{"p", PinType::Vec2}},
							 {{"len", PinType::Float}},
							 [](const auto& in, const auto&) { return NodeValue(length(asVec2(in[0]))); },
							 [](const auto& in, const auto&) { return "length(" + in[0] + ")"; }});

		registerNode(NodeDef{"vec2_dot",
							 "Vec2 Dot",
							 NodeCategory::Vector,
							 {{"a", PinType::Vec2}, {"b", PinType::Vec2}},
							 {{"dot", PinType::Float}},
							 [](const auto& in, const auto&) { return NodeValue(dot(asVec2(in[0]), asVec2(in[1]))); },
							 [](const auto& in, const auto&) { return "dot(" + in[0] + ", " + in[1] + ")"; }});

		// =====================================================================
		// 3. Domain Transforms
		// =====================================================================
		registerNode(
			NodeDef{"translate",
					"Translate",
					NodeCategory::Transforms,
					{{"p", PinType::Vec2}, {"offset", PinType::Vec2, 0.0f, {0.0f, 0.0f}}},
					{{"out", PinType::Vec2}},
					[](const auto& in, const auto&) { return NodeValue(SDF::translate(asVec2(in[0]), asVec2(in[1]))); },
					[](const auto& in, const auto&) { return "SDF::translate(" + in[0] + ", " + in[1] + ")"; }});

		registerNode(NodeDef{"rotate",
							 "Rotate",
							 NodeCategory::Transforms,
							 {{"p", PinType::Vec2}, {"angle", PinType::Float, 0.0f}},
							 {{"out", PinType::Vec2}},
							 [](const auto& in, const auto&)
							 { return NodeValue(SDF::rotate(asVec2(in[0]), asFloat(in[1]))); },
							 [](const auto& in, const auto&) { return "SDF::rotate(" + in[0] + ", " + in[1] + ")"; }});

		registerNode(NodeDef{"mirror_x",
							 "Mirror X",
							 NodeCategory::Transforms,
							 {{"p", PinType::Vec2}},
							 {{"out", PinType::Vec2}},
							 [](const auto& in, const auto&) { return NodeValue(SDF::mirrorX(asVec2(in[0]))); },
							 [](const auto& in, const auto&) { return "SDF::mirrorX(" + in[0] + ")"; }});

		registerNode(NodeDef{"scale_sdf",
							 "Scale SDF",
							 NodeCategory::Transforms,
							 {{"d", PinType::Float, 0.0f}, {"factor", PinType::Float, 1.0f}},
							 {{"out", PinType::Float}},
							 [](const auto& in, const auto&)
							 { return NodeValue(SDF::scale(asFloat(in[0]), asFloat(in[1]))); },
							 [](const auto& in, const auto&) { return "SDF::scale(" + in[0] + ", " + in[1] + ")"; }});

		registerNode(NodeDef{"repeat",
							 "Repeat (Modulo)",
							 NodeCategory::Transforms,
							 {{"p", PinType::Vec2}, {"spacing", PinType::Float, 40.0f}},
							 {{"out", PinType::Vec2}},
							 [](const auto& in, const auto&)
							 { return NodeValue(SDF::repeat(asVec2(in[0]), asFloat(in[1]))); },
							 [](const auto& in, const auto&) { return "SDF::repeat(" + in[0] + ", " + in[1] + ")"; }});

		// =====================================================================
		// 4. 2D Primitives
		// =====================================================================
		registerNode(
			NodeDef{"circle",
					"Circle",
					NodeCategory::Primitives2D,
					{{"p", PinType::Vec2}, {"radius", PinType::Float, 20.0f}},
					{{"d", PinType::Float}},
					[](const auto& in, const auto&) { return NodeValue(SDF::sdCircle(asVec2(in[0]), asFloat(in[1]))); },
					[](const auto& in, const auto&) { return "SDF::sdCircle(" + in[0] + ", " + in[1] + ")"; }});

		registerNode(NodeDef{"box",
							 "Box",
							 NodeCategory::Primitives2D,
							 {{"p", PinType::Vec2}, {"halfSize", PinType::Vec2, 0.0f, {20.0f, 20.0f}}},
							 {{"d", PinType::Float}},
							 [](const auto& in, const auto&)
							 { return NodeValue(SDF::sdBox(asVec2(in[0]), asVec2(in[1]))); },
							 [](const auto& in, const auto&) { return "SDF::sdBox(" + in[0] + ", " + in[1] + ")"; }});

		registerNode(NodeDef{"segment",
							 "Line Segment",
							 NodeCategory::Primitives2D,
							 {{"p", PinType::Vec2},
							  {"a", PinType::Vec2, 0.0f, {-20.0f, 0.0f}},
							  {"b", PinType::Vec2, 0.0f, {20.0f, 0.0f}}},
							 {{"d", PinType::Float}},
							 [](const auto& in, const auto&)
							 { return NodeValue(SDF::sdSegment(asVec2(in[0]), asVec2(in[1]), asVec2(in[2]))); },
							 [](const auto& in, const auto&)
							 { return "SDF::sdSegment(" + in[0] + ", " + in[1] + ", " + in[2] + ")"; }});

		registerNode(
			NodeDef{"line",
					"Thick Line",
					NodeCategory::Primitives2D,
					{{"p", PinType::Vec2},
					 {"a", PinType::Vec2, 0.0f, {-20.0f, 0.0f}},
					 {"b", PinType::Vec2, 0.0f, {20.0f, 0.0f}},
					 {"width", PinType::Float, 2.0f}},
					{{"d", PinType::Float}},
					[](const auto& in, const auto&)
					{ return NodeValue(SDF::sdLine(asVec2(in[0]), asVec2(in[1]), asVec2(in[2]), asFloat(in[3]))); },
					[](const auto& in, const auto&)
					{ return "SDF::sdLine(" + in[0] + ", " + in[1] + ", " + in[2] + ", " + in[3] + ")"; }});

		registerNode(
			NodeDef{"triangle",
					"Triangle",
					NodeCategory::Primitives2D,
					{{"p", PinType::Vec2}, {"width", PinType::Float, 30.0f}, {"height", PinType::Float, 30.0f}},
					{{"d", PinType::Float}},
					[](const auto& in, const auto&)
					{ return NodeValue(SDF::sdTriangle(asVec2(in[0]), asFloat(in[1]), asFloat(in[2]))); },
					[](const auto& in, const auto&)
					{ return "SDF::sdTriangle(" + in[0] + ", " + in[1] + ", " + in[2] + ")"; }});

		registerNode(
			NodeDef{"ramp",
					"Ramp",
					NodeCategory::Primitives2D,
					{{"p", PinType::Vec2},
					 {"width", PinType::Float, 30.0f},
					 {"height", PinType::Float, 20.0f},
					 {"skew", PinType::Float, 10.0f}},
					{{"d", PinType::Float}},
					[](const auto& in, const auto&)
					{ return NodeValue(SDF::sdRamp(asVec2(in[0]), asFloat(in[1]), asFloat(in[2]), asFloat(in[3]))); },
					[](const auto& in, const auto&)
					{ return "SDF::sdRamp(" + in[0] + ", " + in[1] + ", " + in[2] + ", " + in[3] + ")"; }});

		registerNode(NodeDef{"star",
							 "Star",
							 NodeCategory::Primitives2D,
							 {{"p", PinType::Vec2},
							  {"radius", PinType::Float, 25.0f},
							  {"displacement", PinType::Float, 6.0f},
							  {"points", PinType::Float, 5.0f},
							  {"speed", PinType::Float, 0.5f}},
							 {{"d", PinType::Float}},
							 [](const auto& in, const auto&) {
								 return NodeValue(SDF::sdStar(asVec2(in[0]), asFloat(in[1]), asFloat(in[2]),
															  asFloat(in[3]), asFloat(in[4])));
							 },
							 [](const auto& in, const auto&) {
								 return "SDF::sdStar(" + in[0] + ", " + in[1] + ", " + in[2] + ", " + in[3] + ", " +
										in[4] + ")";
							 }});

		registerNode(NodeDef{"sine_wave",
							 "Sine Wave",
							 NodeCategory::Primitives2D,
							 {{"p", PinType::Vec2},
							  {"amplitude", PinType::Float, 15.0f},
							  {"frequency", PinType::Float, 0.05f},
							  {"speed", PinType::Float, 1.0f},
							  {"offset", PinType::Float, 0.0f}},
							 {{"d", PinType::Float}},
							 [](const auto& in, const auto&) {
								 return NodeValue(SDF::sdSineWave(asVec2(in[0]), asFloat(in[1]), asFloat(in[2]),
																  asFloat(in[3]), asFloat(in[4])));
							 },
							 [](const auto& in, const auto&) {
								 return "SDF::sdSineWave(" + in[0] + ", " + in[1] + ", " + in[2] + ", " + in[3] + ", " +
										in[4] + ")";
							 }});

		// =====================================================================
		// 5. CSG & Modifiers
		// =====================================================================
		REGISTER_BINARY_OP(NodeCategory::CSG, "union", "Union", "a", "b", 0.0f, 0.0f, SDF::sdfUnion,
						   "SDF::sdfUnion(" + in[0] + ", " + in[1] + ")");
		REGISTER_BINARY_OP(NodeCategory::CSG, "subtract", "Subtract", "a", "b", 0.0f, 0.0f, SDF::sdfSubtract,
						   "SDF::sdfSubtract(" + in[0] + ", " + in[1] + ")");
		REGISTER_BINARY_OP(NodeCategory::CSG, "intersect", "Intersect", "a", "b", 0.0f, 0.0f, SDF::sdfIntersect,
						   "SDF::sdfIntersect(" + in[0] + ", " + in[1] + ")");
		REGISTER_BINARY_OP(NodeCategory::CSG, "onion", "Onion (Hollow)", "d", "thickness", 0.0f, 2.0f, SDF::sdfOnion,
						   "SDF::sdfOnion(" + in[0] + ", " + in[1] + ")");
		REGISTER_BINARY_OP(NodeCategory::CSG, "round", "Round", "d", "radius", 0.0f, 2.0f, SDF::sdfRound,
						   "SDF::sdfRound(" + in[0] + ", " + in[1] + ")");
		REGISTER_BINARY_OP(NodeCategory::CSG, "erode", "Erode", "d", "radius", 0.0f, 2.0f, SDF::sdfErode,
						   "SDF::sdfErode(" + in[0] + ", " + in[1] + ")");

		REGISTER_TERNARY_OP(NodeCategory::CSG, "smooth_union", "Smooth Union", "a", "b", "radius", 0.0f, 0.0f, 5.0f,
							SDF::sdfSmoothUnion, "SDF::sdfSmoothUnion(" + in[0] + ", " + in[1] + ", " + in[2] + ")");
		REGISTER_TERNARY_OP(NodeCategory::CSG, "smooth_sub", "Smooth Subtract", "a", "b", "radius", 0.0f, 0.0f, 5.0f,
							SDF::sdfSmoothSubtract,
							"SDF::sdfSmoothSubtract(" + in[0] + ", " + in[1] + ", " + in[2] + ")");

		// =====================================================================
		// 6. Math (Unary)
		// =====================================================================
		REGISTER_UNARY_OP("sin", "Sin", WeirdEngine::sin, "sin");
		REGISTER_UNARY_OP("cos", "Cos", WeirdEngine::cos, "cos");
		REGISTER_UNARY_OP("abs", "Abs", WeirdEngine::abs, "abs");
		REGISTER_UNARY_OP("sqrt", "Sqrt", WeirdEngine::sqrt, "sqrt");
		REGISTER_UNARY_OP("negate", "Negate", [](const Expr& a) { return -a; }, "-");
		REGISTER_UNARY_OP("sign", "Sign", WeirdEngine::sign, "sign");

		// =====================================================================
		// 7. Math (Binary)
		// =====================================================================
		REGISTER_BINARY_OP(
			NodeCategory::MathBinary, "add", "Add (+)", "a", "b", 0.0f, 0.0f,
			[](const Expr& a, const Expr& b) { return a + b; }, "(" + in[0] + " + " + in[1] + ")");
		REGISTER_BINARY_OP(
			NodeCategory::MathBinary, "sub", "Subtract (-)", "a", "b", 0.0f, 0.0f,
			[](const Expr& a, const Expr& b) { return a - b; }, "(" + in[0] + " - " + in[1] + ")");
		REGISTER_BINARY_OP(
			NodeCategory::MathBinary, "mul", "Multiply (*)", "a", "b", 1.0f, 1.0f,
			[](const Expr& a, const Expr& b) { return a * b; }, "(" + in[0] + " * " + in[1] + ")");
		REGISTER_BINARY_OP(
			NodeCategory::MathBinary, "div", "Divide (/)", "a", "b", 1.0f, 1.0f,
			[](const Expr& a, const Expr& b) { return a / b; }, "(" + in[0] + " / " + in[1] + ")");
		REGISTER_BINARY_OP(NodeCategory::MathBinary, "min", "Min", "a", "b", 0.0f, 0.0f, WeirdEngine::min,
						   "min(" + in[0] + ", " + in[1] + ")");
		REGISTER_BINARY_OP(NodeCategory::MathBinary, "max", "Max", "a", "b", 0.0f, 0.0f, WeirdEngine::max,
						   "max(" + in[0] + ", " + in[1] + ")");
		REGISTER_BINARY_OP(NodeCategory::MathBinary, "mod", "Modulo (mod)", "a", "b", 0.0f, 1.0f, WeirdEngine::mod,
						   "mod(" + in[0] + ", " + in[1] + ")");
		REGISTER_BINARY_OP(NodeCategory::MathBinary, "atan2", "Atan2", "y", "x", 0.0f, 1.0f, WeirdEngine::atan2,
						   "atan2(" + in[0] + ", " + in[1] + ")");
		REGISTER_BINARY_OP(NodeCategory::MathBinary, "step", "Step", "edge", "x", 0.0f, 0.0f, WeirdEngine::step,
						   "step(" + in[0] + ", " + in[1] + ")");

		// =====================================================================
		// 8. Math (Ternary)
		// =====================================================================
		REGISTER_TERNARY_OP(NodeCategory::MathTernary, "clamp", "Clamp", "v", "min", "max", 0.0f, 0.0f, 1.0f,
							WeirdEngine::clamp, "clamp(" + in[0] + ", " + in[1] + ", " + in[2] + ")");

		// =====================================================================
		// 9. 3D Primitives
		// =====================================================================
		registerNode(
			NodeDef{"sphere_3d",
					"Sphere 3D",
					NodeCategory::Primitives3D,
					{{"px", PinType::Float, 0.0f},
					 {"py", PinType::Float, 0.0f},
					 {"pz", PinType::Float, 0.0f},
					 {"radius", PinType::Float, 5.0f}},
					{{"d", PinType::Float}},
					[](const auto& in, const auto&)
					{
						return NodeValue(Expr(std::make_shared<Primitives3D::Sphere>(
							asFloat(in[0]).node, asFloat(in[1]).node, asFloat(in[2]).node, asFloat(in[3]).node)));
					},
					[](const auto& in, const auto&)
					{
						return "std::make_shared<Primitives3D::Sphere>(" + in[0] + ".node, " + in[1] + ".node, " +
							   in[2] + ".node, " + in[3] + ".node)";
					}});

		registerNode(NodeDef{"box_3d",
							 "Box 3D",
							 NodeCategory::Primitives3D,
							 {{"px", PinType::Float, 0.0f},
							  {"py", PinType::Float, 0.0f},
							  {"pz", PinType::Float, 0.0f},
							  {"sx", PinType::Float, 5.0f},
							  {"sy", PinType::Float, 5.0f},
							  {"sz", PinType::Float, 5.0f}},
							 {{"d", PinType::Float}},
							 [](const auto& in, const auto&)
							 {
								 return NodeValue(Expr(std::make_shared<Primitives3D::Box>(
									 asFloat(in[0]).node, asFloat(in[1]).node, asFloat(in[2]).node, asFloat(in[3]).node,
									 asFloat(in[4]).node, asFloat(in[5]).node)));
							 },
							 [](const auto& in, const auto&)
							 {
								 return "std::make_shared<Primitives3D::Box>(" + in[0] + ".node, " + in[1] + ".node, " +
										in[2] + ".node, " + in[3] + ".node, " + in[4] + ".node, " + in[5] + ".node)";
							 }});

		registerNode(NodeDef{"plane_3d",
							 "Plane 3D",
							 NodeCategory::Primitives3D,
							 {{"height", PinType::Float, 0.0f}},
							 {{"d", PinType::Float}},
							 [](const auto& in, const auto&)
							 { return NodeValue(Expr(std::make_shared<Primitives3D::Plane>(asFloat(in[0]).node))); },
							 [](const auto& in, const auto&)
							 { return "std::make_shared<Primitives3D::Plane>(" + in[0] + ".node)"; }});

		registerNode(NodeDef{"cylinder_3d",
							 "Cylinder 3D",
							 NodeCategory::Primitives3D,
							 {{"px", PinType::Float, 0.0f},
							  {"py", PinType::Float, 0.0f},
							  {"pz", PinType::Float, 0.0f},
							  {"radius", PinType::Float, 3.0f},
							  {"height", PinType::Float, 8.0f}},
							 {{"d", PinType::Float}},
							 [](const auto& in, const auto&)
							 {
								 return NodeValue(Expr(std::make_shared<Primitives3D::Cylinder>(
									 asFloat(in[0]).node, asFloat(in[1]).node, asFloat(in[2]).node, asFloat(in[3]).node,
									 asFloat(in[4]).node)));
							 },
							 [](const auto& in, const auto&)
							 {
								 return "std::make_shared<Primitives3D::Cylinder>(" + in[0] + ".node, " + in[1] +
										".node, " + in[2] + ".node, " + in[3] + ".node, " + in[4] + ".node)";
							 }});

		registerNode(NodeDef{"torus_3d",
							 "Torus 3D",
							 NodeCategory::Primitives3D,
							 {{"px", PinType::Float, 0.0f},
							  {"py", PinType::Float, 0.0f},
							  {"pz", PinType::Float, 0.0f},
							  {"majorR", PinType::Float, 6.0f},
							  {"minorR", PinType::Float, 2.0f}},
							 {{"d", PinType::Float}},
							 [](const auto& in, const auto&)
							 {
								 return NodeValue(Expr(std::make_shared<Primitives3D::Torus>(
									 asFloat(in[0]).node, asFloat(in[1]).node, asFloat(in[2]).node, asFloat(in[3]).node,
									 asFloat(in[4]).node)));
							 },
							 [](const auto& in, const auto&)
							 {
								 return "std::make_shared<Primitives3D::Torus>(" + in[0] + ".node, " + in[1] +
										".node, " + in[2] + ".node, " + in[3] + ".node, " + in[4] + ".node)";
							 }});

		registerNode(NodeDef{"capsule_3d",
							 "Capsule 3D",
							 NodeCategory::Primitives3D,
							 {{"px", PinType::Float, 0.0f},
							  {"py", PinType::Float, 0.0f},
							  {"pz", PinType::Float, 0.0f},
							  {"radius", PinType::Float, 3.0f},
							  {"height", PinType::Float, 6.0f}},
							 {{"d", PinType::Float}},
							 [](const auto& in, const auto&)
							 {
								 return NodeValue(Expr(std::make_shared<Primitives3D::Capsule>(
									 asFloat(in[0]).node, asFloat(in[1]).node, asFloat(in[2]).node, asFloat(in[3]).node,
									 asFloat(in[4]).node)));
							 },
							 [](const auto& in, const auto&)
							 {
								 return "std::make_shared<Primitives3D::Capsule>(" + in[0] + ".node, " + in[1] +
										".node, " + in[2] + ".node, " + in[3] + ".node, " + in[4] + ".node)";
							 }});

		// =====================================================================
		// 10. Output Terminal Node
		// =====================================================================
		registerNode(NodeDef{"sdf_output",
							 "SDF Result Output",
							 NodeCategory::Output,
							 {{"shape", PinType::Float, 1.0f}},
							 {},
							 [](const auto& in, const auto&) { return in.empty() ? NodeValue(Expr(1.0f)) : in[0]; },
							 [](const auto& in, const auto&) { return in.empty() ? "Expr(1.0f)" : in[0]; }});
	}
} // namespace WeirdEngine::Editor
