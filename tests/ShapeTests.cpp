#include "TestHarness.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/services/ShapeService.h"
#include "weird-engine/shapes/NodeGraphCompiler.h"
#include "weird-engine/systems/Shape2DPacking.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

using namespace WeirdEngine;
using WeirdTests::require;

namespace
{
	void testNodeGraphMultiOutputAndMemoization()
	{
		NodeGraph bvGraph;
		int p = bvGraph.addNode("point");
		int bv = bvGraph.addNode("break_vec2");
		int sub = bvGraph.addNode("sub");
		int out = bvGraph.addNode("sdf_output");
		bvGraph.addLink(makePinId(p, false, 0), makePinId(bv, true, 0));
		bvGraph.addLink(makePinId(bv, false, 0), makePinId(sub, true, 0));
		bvGraph.addLink(makePinId(bv, false, 1), makePinId(sub, true, 1));
		bvGraph.addLink(makePinId(sub, false, 0), makePinId(out, true, 0));

		Expr bvExpr = NodeGraphCompiler::evaluate(bvGraph);
		float params[12] = {0.0f};
		params[SystemParams::POINT_X] = 7.5f;
		params[SystemParams::POINT_Y] = -3.25f;
		require(bvExpr.node != nullptr && std::abs(bvExpr.node->getValue(params) - 10.75f) < 1e-5f,
				"break_vec2 must evaluate distinct x (pin 0) and y (pin 1) outputs.");

		std::string bvCpp = NodeGraphCompiler::generateCppCode(bvGraph);
		require(bvCpp.find(".x") != std::string::npos && bvCpp.find(".y") != std::string::npos,
				"break_vec2 C++ code generation must preserve both .x and .y.");

		NodeGraph diamondGraph;
		int prev = diamondGraph.addNode("param_var");
		diamondGraph.getNode(prev)->data.customInt = 0;
		for (int i = 0; i < 32; ++i)
		{
			int next = diamondGraph.addNode("add");
			diamondGraph.addLink(makePinId(prev, false, 0), makePinId(next, true, 0));
			diamondGraph.addLink(makePinId(prev, false, 0), makePinId(next, true, 1));
			prev = next;
		}
		int dOut = diamondGraph.addNode("sdf_output");
		diamondGraph.addLink(makePinId(prev, false, 0), makePinId(dOut, true, 0));

		Expr dExpr = NodeGraphCompiler::evaluate(diamondGraph);
		std::string dCpp = NodeGraphCompiler::generateCppCode(diamondGraph);
		params[0] = 1.0f;
		require(dExpr.node != nullptr && std::abs(dExpr.node->getValue(params) - 4294967296.0f) < 1.0f,
				"Memoized NodeGraphCompiler must evaluate deep diamond DAGs accurately without blowup.");
		require(dCpp.find("auto node") != std::string::npos && dCpp.size() < 8192,
				"Memoized generateCppCode must hoist shared DAG intermediate nodes.");
	}

	void testBuiltinRotatedAndRoundedShapes()
	{
		Scene::registerBuiltinSDFs();
		auto builtins = Scene::getBuiltinSDFs();
		require(builtins.size() >= DefaultShapes::COUNT_2D, "Builtin SDF table must cover all 2D DefaultShapes.");

		auto evalBuiltin = [&](ShapeId id, const float (&shapeVars)[8], vec2 p) -> float
		{
			const auto& expr = builtins[id];
			require(expr != nullptr, "Builtin shape AST must not be null.");
			float params[12]{};
			packSdfParameters(params, shapeVars, 0.0f, p, 0.0f);
			float val = expr->getValue(params);
			require(std::isfinite(val), "Builtin shape distance must be finite.");
			return val;
		};

		// Box with Angle != 0: halfSize=(2, 1), angle=pi/2 around origin -> swaps X/Y extents
		constexpr float kPiOver4 = 0.78539816339f;
		constexpr float kPiOver2 = 1.57079632679f;
		const float rotBoxVars[8] = {0.0f, 0.0f, 2.0f, 1.0f, 0.0f, kPiOver2, 0.0f, 0.0f};
		require(evalBuiltin(DefaultShapes::Box, rotBoxVars, {0.5f, 1.5f}) < 0.0f &&
					evalBuiltin(DefaultShapes::Box, rotBoxVars, {1.5f, 0.5f}) > 0.0f,
				"Rotated Box at 90deg must swap X/Y extents.");

		// Triangle with Angle != 0: contains local origin
		const float rotTriVars[8] = {0.0f, 0.0f, 2.0f, 2.0f, 0.0f, kPiOver4, 0.0f, 0.0f};
		require(evalBuiltin(DefaultShapes::Triangle, rotTriVars, {0.0f, 0.0f}) < 0.0f,
				"Rotated Triangle must contain its local origin.");

		// Triangle: unrounded (r=0) has sharp apex; rounded (r=0.25) preserves flat bottom & rounds apex
		const float triVars[8] = {0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f};
		const float roundTriVars[8] = {0.0f, 0.0f, 2.0f, 2.0f, 0.25f, 0.0f, 0.0f, 0.0f};
		const float sharpApex = evalBuiltin(DefaultShapes::Triangle, triVars, {0.0f, 4.0f / 3.0f});
		require(std::abs(sharpApex) < 1e-4f, "Triangle with radius=0 must have sharp apex at 2h/3.");

		const float roundedBottomMid = evalBuiltin(DefaultShapes::Triangle, roundTriVars, {0.0f, -2.0f / 3.0f});
		const float roundedApex = evalBuiltin(DefaultShapes::Triangle, roundTriVars, {0.0f, 4.0f / 3.0f});
		require(std::abs(roundedBottomMid) < 1e-3f && roundedApex > 0.25f,
				"Rounded Triangle must keep the bottom midpoint on the boundary and pull back the sharp apex.");

		// Excessive radius must clamp cleanly without NaN and remain inside/tangent to the triangle
		const float clampedVars[8] = {0.0f, 0.0f, 2.0f, 2.0f, 50.0f, 0.0f, 0.0f, 0.0f};
		require(evalBuiltin(DefaultShapes::Triangle, clampedVars, {0.0f, -2.0f / 3.0f + 0.618f}) < 0.0f &&
					evalBuiltin(DefaultShapes::Triangle, clampedVars, {0.0f, 1.3f}) > 0.0f,
				"Triangle must clamp oversized radius to inscribed circle without NaN.");

		// Rotated + rounded Triangle: verify GLSL print and rotation
		const float rotRoundTriVars[8] = {1.0f, -2.0f, 2.0f, 2.0f, 0.25f, kPiOver2, 0.0f, 0.0f};
		require(evalBuiltin(DefaultShapes::Triangle, rotRoundTriVars, {1.0f, -2.0f}) < 0.0f,
				"Rotated rounded Triangle must contain its translated center.");
		std::string glsl = builtins[DefaultShapes::Triangle]->print();
		require(!glsl.empty() && glsl.find("nan") == std::string::npos,
				"Triangle GLSL expression generation must succeed.");

		// Rotated + rounded Box: 90deg rotation swaps SizeX/SizeY and rounds corners
		const float rotRoundBoxVars[8] = {1.0f, -2.0f, 2.0f, 1.0f, 0.3f, kPiOver2, 0.0f, 0.0f};
		require(evalBuiltin(DefaultShapes::Box, rotRoundBoxVars, {1.5f, -0.5f}) < 0.0f &&
					evalBuiltin(DefaultShapes::Box, rotRoundBoxVars, {2.5f, -1.5f}) > 0.0f &&
					evalBuiltin(DefaultShapes::Box, rotRoundBoxVars, {2.0f, 0.0f}) > 0.0f,
				"Rotated rounded Box must rotate extents and round sharp corners.");

		// Verify DefaultShapes::Star exposes its named parameter indices
		static_assert(DefaultShapes::Star::PosX == 0 && DefaultShapes::Star::PosY == 1 &&
					  DefaultShapes::Star::Radius == 2 && DefaultShapes::Star::Displacement == 3 &&
					  DefaultShapes::Star::Points == 4 && DefaultShapes::Star::Speed == 5);

		// Verify round_triangle NodeGraph node compiles cleanly
		NodeGraph graph;
		int pNode = graph.addNode("point", {0.0f, 0.0f});
		int triNode = graph.addNode("round_triangle", {120.0f, 0.0f});
		int outNode = graph.addNode("sdf_output", {260.0f, 0.0f});
		graph.addLink(makePinId(pNode, false, 0), makePinId(triNode, true, 0));
		graph.addLink(makePinId(triNode, false, 0), makePinId(outNode, true, 0));
		Expr compiled = NodeGraphCompiler::evaluate(graph);
		require(compiled.node != nullptr && !compiled.node->print().empty(),
				"NodeGraph round_triangle node must evaluate and print GLSL.");
	}

	void testShape2DPackingParity()
	{
		Scene::registerBuiltinSDFs();
		const auto builtins = Scene::getBuiltinSDFs();
		constexpr float kTime = 1.37f;

		// CPU mirror of the 7 collapsed GLSL evaluators in shaders/2d/sdf_shapes.frag
		auto evalPackedBuiltin = [&](const vec4& meta, const vec4& p0, const vec4& p1, vec2 p) -> float
		{
			const uint32_t packed0 = static_cast<uint32_t>(meta.x + 0.5f);
			const uint16_t shapeId = static_cast<uint16_t>(packed0 & 0xFFFFu);
			const uint32_t flags = packed0 >> 19u;
			const float outlineThickness = meta.w;

			float dist = 10000.0f;
			switch (shapeId)
			{
				case Shape2D::Circle:
					dist = glm::length(p - vec2(p0.x, p0.y)) - p0.z;
					break;
				case Shape2D::Box:
				{
					vec2 q = p - vec2(p0.x, p0.y);
					q = vec2(p1.x * q.x - p1.y * q.y, p1.y * q.x + p1.x * q.y);
					vec2 d = glm::abs(q) - vec2(p0.z, p0.w);
					dist = glm::length(glm::max(d, vec2(0.0f))) + std::min(std::max(d.x, d.y), 0.0f) - p1.z;
					break;
				}
				case Shape2D::Triangle:
				{
					vec2 q = p - vec2(p0.x, p0.y);
					q = vec2(p1.x * q.x - p1.y * q.y, p1.y * q.x + p1.x * q.y);
					q.y -= p1.w;
					const float w = p0.z;
					const float h = p0.w;
					const vec2 v0(-w * 0.5f, -h / 3.0f);
					const vec2 v1(w * 0.5f, -h / 3.0f);
					const vec2 v2(0.0f, 2.0f * h / 3.0f);
					const vec2 e0 = v1 - v0, e1 = v2 - v1, e2 = v0 - v2;
					const vec2 d0 = q - v0, d1 = q - v1, d2 = q - v2;
					const vec2 pq0 =
						d0 - e0 * std::clamp(glm::dot(d0, e0) / std::max(glm::dot(e0, e0), 1e-12f), 0.0f, 1.0f);
					const vec2 pq1 =
						d1 - e1 * std::clamp(glm::dot(d1, e1) / std::max(glm::dot(e1, e1), 1e-12f), 0.0f, 1.0f);
					const vec2 pq2 =
						d2 - e2 * std::clamp(glm::dot(d2, e2) / std::max(glm::dot(e2, e2), 1e-12f), 0.0f, 1.0f);
					const float s = (e0.x * e2.y - e0.y * e2.x) >= 0.0f ? 1.0f : -1.0f;
					const float dx = std::min({glm::dot(pq0, pq0), glm::dot(pq1, pq1), glm::dot(pq2, pq2)});
					const float dy = std::min({s * (d0.x * e0.y - d0.y * e0.x), s * (d1.x * e1.y - d1.y * e1.x),
											   s * (d2.x * e2.y - d2.y * e2.x)});
					const float signDy = (dy > 0.0f) ? 1.0f : ((dy < 0.0f) ? -1.0f : 0.0f);
					dist = -std::sqrt(dx) * signDy - p1.z;
					break;
				}
				case Shape2D::Line:
				{
					const vec2 pa = p - vec2(p0.x, p0.y);
					const vec2 ba(p0.z, p0.w);
					const float h = std::clamp(glm::dot(pa, ba) * p1.x, 0.0f, 1.0f);
					dist = glm::length(pa - ba * h) - p1.y;
					break;
				}
				case Shape2D::Ramp:
				{
					vec2 q = p - vec2(p0.x, p0.y);
					q = vec2(p1.y * q.x - p1.z * q.y, p1.z * q.x + p1.y * q.y);
					const float wi = p0.z;
					const float he = p0.w;
					const float sk = p1.x;
					const vec2 e(wi, sk);
					q *= (q.x >= 0.0f ? 1.0f : -1.0f);
					const vec2 w = q - e;
					const vec2 wClamped(w.x, w.y - std::clamp(w.y, -he, he));
					const float dx0 = glm::dot(wClamped, wClamped);
					const float dy0 = -w.x;
					const float s = q.y * e.x - q.x * e.y;
					q *= (s >= 0.0f ? 1.0f : -1.0f);
					vec2 v = q - vec2(0.0f, he);
					const float h = std::clamp(glm::dot(v, e) * p1.w, -1.0f, 1.0f);
					v -= e * h;
					const float dx1 = glm::dot(v, v);
					const float dy1 = wi * he - std::abs(s);
					const float mdy = -std::min(dy0, dy1);
					const float signMdy = (mdy > 0.0f) ? 1.0f : ((mdy < 0.0f) ? -1.0f : 0.0f);
					dist = std::sqrt(std::min(dx0, dx1)) * signMdy;
					break;
				}
				case Shape2D::SineWave:
					dist = (p.y - p0.w) - p0.x * std::sin(p0.y * p.x + p0.z * kTime);
					break;
				case Shape2D::Star:
				{
					const vec2 q = p - vec2(p0.x, p0.y);
					const float d = glm::length(q) - p0.z;
					const float angle = p1.x * std::atan2(q.y, q.x) - p1.y * kTime;
					dist = d + p0.w * std::sin(angle);
					break;
				}
				default:
					break;
			}

			if ((flags & Shape2D::FLAG_OUTLINE) != 0u)
			{
				dist = std::abs(dist) - outlineThickness;
			}
			return dist;
		};

		struct BuiltinCase
		{
			ShapeId id;
			float vars[8];
			bool expectRotatedFlag;
		};

		const BuiltinCase cases[] = {
			{DefaultShapes::Circle, {1.2f, -0.7f, 2.5f, 0.0f, 0, 0, 0, 0}, false},
			{DefaultShapes::Circle, {1.2f, -0.7f, 2.5f, 0.25f, 0, 0, 0, 0}, false},
			{DefaultShapes::Box, {-1.5f, 0.8f, 2.2f, 1.1f, 0, 0, 0.0f, 0}, false},
			{DefaultShapes::Box, {-1.5f, 0.8f, 2.2f, 1.1f, 0, 0, 0.2f, 0}, false},
			{DefaultShapes::Box, {0.4f, -1.1f, 2.0f, 1.5f, 0.35f, 0, 0.0f, 0}, false},
			{DefaultShapes::Box, {0.4f, -1.1f, 2.0f, 1.5f, 0.35f, 0, 0.15f, 0}, false},
			{DefaultShapes::Box, {1.0f, -1.0f, 2.4f, 0.9f, 0.0f, 0.65f, 0.0f, 0}, true},
			{DefaultShapes::Box, {1.0f, -1.0f, 2.4f, 0.9f, 0.0f, 0.65f, 0.18f, 0}, true},
			{DefaultShapes::Box, {0.9f, 0.2f, 2.1f, 1.3f, 0.35f, -0.7f, 0.0f, 0}, true},
			{DefaultShapes::Box, {0.9f, 0.2f, 2.1f, 1.3f, 0.35f, -0.7f, 0.14f, 0}, true},
			{DefaultShapes::Triangle, {0.5f, -0.4f, 2.6f, 2.1f, 0, 0, 0.0f, 0}, false},
			{DefaultShapes::Triangle, {0.5f, -0.4f, 2.6f, 2.1f, 0, 0, 0.18f, 0}, false},
			{DefaultShapes::Triangle, {-0.8f, 0.5f, 2.5f, 1.9f, 0.0f, -0.9f, 0.0f, 0}, true},
			{DefaultShapes::Triangle, {-0.8f, 0.5f, 2.5f, 1.9f, 0.0f, -0.9f, 0.15f, 0}, true},
			{DefaultShapes::Triangle, {0.3f, 0.4f, 2.4f, 2.0f, 0.3f, 0, 0.0f, 0}, false},
			{DefaultShapes::Triangle, {0.3f, 0.4f, 2.4f, -2.0f, 0.3f, 0, 0.0f, 0}, false},
			{DefaultShapes::Triangle, {-0.5f, -0.6f, 2.2f, -1.8f, 0.25f, 1.1f, 0.12f, 0}, true},
			{DefaultShapes::Line, {-2.0f, -1.0f, 3.0f, 1.5f, 0.4f, 0.0f, 0, 0}, false},
			{DefaultShapes::Line, {-2.0f, -1.0f, 3.0f, 1.5f, 0.4f, 0.1f, 0, 0}, false},
			{DefaultShapes::Ramp, {0.2f, 0.3f, 2.0f, 1.2f, 0.6f, 0, 0.0f, 0}, false},
			{DefaultShapes::Ramp, {0.7f, -0.2f, 1.8f, 1.1f, -0.5f, 0.8f, 0.1f, 0}, true},
			{DefaultShapes::SineWave, {0.8f, 1.5f, 2.2f, -0.5f, 0.0f, 0, 0, 0}, false},
			{DefaultShapes::SineWave, {0.8f, 1.5f, 2.2f, -0.5f, 0.2f, 0, 0, 0}, false},
			{DefaultShapes::Star, {-0.6f, 0.7f, 2.0f, 0.45f, 5.0f, 1.3f, 0.0f, 0}, false},
			{DefaultShapes::Star, {-0.6f, 0.7f, 2.0f, 0.45f, 5.0f, 1.3f, 0.15f, 0}, false},
		};

		const vec2 samplePoints[] = {
			{0.0f, 0.0f}, {1.2f, -0.7f}, {-2.3f, 1.8f}, {3.1f, -2.4f}, {0.35f, 0.95f}, {-0.9f, -1.6f},
		};

		for (const auto& tc : cases)
		{
			vec4 meta, p0, p1;
			Shape2D::packInstance(tc.id, CombinationType::Addition, 0, 3, 1.0f, tc.vars, meta, p0, p1);
			const uint32_t packed0 = static_cast<uint32_t>(meta.x + 0.5f);
			const uint16_t gpuId = static_cast<uint16_t>(packed0 & 0xFFFFu);
			const uint32_t flags = packed0 >> 19u;
			const float outline = Shape2D::getOutlineThickness(tc.id, tc.vars);
			require(((flags & Shape2D::FLAG_ROTATED) != 0u) == tc.expectRotatedFlag,
					"FLAG_ROTATED must be set iff Angle != 0.");
			require(((flags & Shape2D::FLAG_OUTLINE) != 0u) == (outline > 0.0f),
					"FLAG_OUTLINE must be set iff OutlineThickness > 0.");

			for (const vec2& pt : samplePoints)
			{
				float fullParams[12];
				packSdfParameters(fullParams, tc.vars, kTime, pt, 0.0f);
				const float refDist = builtins[tc.id]->getValue(fullParams);
				const float gpuDist = evalPackedBuiltin(meta, p0, p1, pt);
				const float cpuFastDist = Shape2D::evalPackedBuiltin(gpuId, flags, meta.w, p0, p1, pt, kTime);
				require(
					std::isfinite(gpuDist) && std::abs(refDist - gpuDist) < 1e-4f &&
						std::abs(gpuDist - cpuFastDist) < 1e-5f,
					"Canonical GPU and CPU evaluators must match reference AST SDF across all modifier combinations.");
			}
		}

		// Verify BuiltinShapes string round-trip for all 2D and 3D built-ins (and collision-free 2D vs 3D Box)
		{
			for (ShapeId id = 0; id < static_cast<ShapeId>(DefaultShapes3D::TOTAL_BUILTIN_SHAPES); ++id)
			{
				const char* name = BuiltinShapes::toName(id);
				require(name != nullptr && name[0] != '\0', "Every built-in 2D and 3D shape must have a string name.");
				ShapeId parsed = 0xFFFFu;
				require(BuiltinShapes::fromName(name, parsed) && parsed == id,
						"BuiltinShapes::fromName must round-trip BuiltinShapes::toName for every built-in shape.");
			}
			require(std::string_view(BuiltinShapes::toName(DefaultShapes::Box)) == "Box" &&
						std::string_view(BuiltinShapes::toName(DefaultShapes3D::Box)) == "3D/Box",
					"2D Box and 3D Box must serialize to distinct string names.");
		}

		// Verify metadata bit packing, custom shape passthrough, and (layer, order) sorting in packShapes
		Registry reg;
		reg.registerComponent<Shape>();
		const Entity eGlobal = reg.createEntity();
		{
			auto& s = reg.addComponent<Shape>(eGlobal);
			s.distanceFieldId = DefaultShapes::Circle;
			s.combination = CombinationType::SmoothSubtraction;
			s.layer = Shape::GLOBAL_LAYER;
			s.order = 0;
			s.material = 12;
			s.smoothFactor = 2.5f;
		}
		const Entity eLayer2Late = reg.createEntity();
		{
			auto& s = reg.addComponent<Shape>(eLayer2Late);
			s.distanceFieldId = static_cast<ShapeId>(DefaultShapes3D::TOTAL_BUILTIN_SHAPES + 3);
			s.combination = CombinationType::SmoothAddition;
			s.layer = 2;
			s.order = 10;
			s.material = 7;
			s.smoothFactor = 0.75f;
			s.parameters[0] = 42.0f;
			s.parameters[7] = -9.5f;
		}
		const Entity eLayer2Early = reg.createEntity();
		{
			auto& s = reg.addComponent<Shape>(eLayer2Early);
			s.distanceFieldId = DefaultShapes::Triangle;
			s.combination = CombinationType::Addition;
			s.layer = 2;
			s.order = 2;
			s.material = 4;
		}
		const Entity eLayer0 = reg.createEntity();
		{
			auto& s = reg.addComponent<Shape>(eLayer0);
			s.distanceFieldId = DefaultShapes::Box;
			s.combination = CombinationType::Addition;
			s.layer = 0;
			s.order = 5;
			s.material = 15;
			s.parameters[DefaultShapes::Box::OutlineThickness] = 0.3f;
		}

		std::vector<vec4> packed;
		std::vector<size_t> scratch;
		uint32_t shapeCount = 0;
		Shape2D::packShapes<Shape>(reg, packed, scratch, shapeCount);

		require(shapeCount == 4 && packed.size() == 12, "packShapes must emit 3 vec4s per shape.");

		auto unpack = [&](size_t idx)
		{
			const vec4& m = packed[idx * 3];
			const uint32_t p0Bits = static_cast<uint32_t>(m.x + 0.5f);
			const uint32_t p1Bits = static_cast<uint32_t>(m.y + 0.5f);
			struct Unpacked
			{
				uint16_t shapeId;
				uint16_t comb;
				uint32_t flags;
				uint16_t layer;
				uint16_t mat;
				float smooth;
				float outline;
			};
			return Unpacked{static_cast<uint16_t>(p0Bits & 0xFFFFu),
							static_cast<uint16_t>((p0Bits >> 16u) & 0x7u),
							p0Bits >> 19u,
							static_cast<uint16_t>(p1Bits & 0xFFFFu),
							static_cast<uint16_t>((p1Bits >> 16u) & 0x3Fu),
							m.z,
							m.w};
		};

		const auto u0 = unpack(0);
		const auto u1 = unpack(1);
		const auto u2 = unpack(2);
		const auto u3 = unpack(3);
		require(u0.layer == 0 && u0.shapeId == Shape2D::Box && (u0.flags & Shape2D::FLAG_OUTLINE) != 0u &&
					u0.mat == 15 && std::abs(u0.outline - 0.3f) < 1e-5f,
				"First sorted shape must be layer 0 Box with FLAG_OUTLINE.");
		require(u1.layer == 2 && u1.shapeId == Shape2D::Triangle && u1.mat == 4,
				"Earlier order within layer 2 must sort before later order regardless of insertion order.");
		require(u2.layer == 2 && u2.shapeId == DefaultShapes3D::TOTAL_BUILTIN_SHAPES + 3 &&
					u2.comb == static_cast<uint16_t>(CombinationType::SmoothAddition) && u2.mat == 7 &&
					std::abs(u2.smooth - 0.75f) < 1e-5f && packed[7].x == 42.0f && packed[8].w == -9.5f,
				"Later order within layer 2 must preserve custom ShapeId and raw parameters.");
		require(u3.layer == Shape::GLOBAL_LAYER && (u3.flags & Shape2D::FLAG_GLOBAL_LAYER) != 0u &&
					u3.shapeId == Shape2D::Circle &&
					u3.comb == static_cast<uint16_t>(CombinationType::SmoothSubtraction) && u3.mat == 12,
				"GLOBAL_LAYER shape must sort last with FLAG_GLOBAL_LAYER set.");

		// Verify ShapeService increments sdfDefinitionsVersion on registerSDF
		uint64_t defVersion = 1;
		PhysicsSettings settings{};
		Simulation2D sim(16, settings);
		std::vector<std::shared_ptr<IMathExpression>> sdfs(builtins.begin(), builtins.end());
		ShapeService shapeService(reg, sim, sdfs, nullptr, &defVersion);
		shapeService.registerSDF(SDF::sdCircle(SDF::point(), 1.0f));
		require(defVersion == 2, "ShapeService::registerSDF must increment sdfDefinitionsVersion.");
	}

	void testBounds2DExprAndAutoPropagation()
	{
		using namespace SDF;

		Scene::registerBuiltinSDFs();
		const auto builtins = Scene::getBuiltinSDFs();
		constexpr float kTime = 1.25f;
		constexpr float kAudio = 0.6f;

		// 1. Verify computeBuiltinBounds encloses every inside point across canonical primitives & modifiers
		struct BuiltinBoundsCase
		{
			ShapeId id;
			float vars[8];
			bool expectFiniteX;
			bool expectFiniteYMin;
		};

		const BuiltinBoundsCase cases[] = {
			{DefaultShapes::Circle, {1.2f, -0.7f, 2.0f, 0.0f, 0, 0, 0, 0}, true, true},
			{DefaultShapes::Circle, {1.2f, -0.7f, 2.0f, 0.3f, 0, 0, 0, 0}, true, true},
			{DefaultShapes::Box, {-1.5f, 0.8f, 2.2f, 1.1f, 0, 0, 0.0f, 0}, true, true},
			{DefaultShapes::Box, {-1.5f, 0.8f, 2.2f, 1.1f, 0, 0, 0.25f, 0}, true, true},
			{DefaultShapes::Box, {0.4f, -1.1f, 2.0f, 1.5f, 0.35f, 0, 0.2f, 0}, true, true},
			{DefaultShapes::Box, {1.0f, -1.0f, 2.4f, 0.9f, 0.0f, 0.65f, 0.2f, 0}, true, true},
			{DefaultShapes::Box, {0.9f, 0.2f, 2.1f, 1.3f, 0.35f, -0.7f, 0.15f, 0}, true, true},
			{DefaultShapes::Triangle, {0.5f, -0.4f, 2.6f, 2.1f, 0, 0, 0.2f, 0}, true, true},
			{DefaultShapes::Triangle, {-0.8f, 0.5f, 2.5f, 1.9f, 0.0f, -0.9f, 0.15f, 0}, true, true},
			{DefaultShapes::Triangle, {0.3f, 0.4f, 2.4f, 2.0f, 0.3f, 0, 0.0f, 0}, true, true},
			{DefaultShapes::Triangle, {-0.5f, -0.6f, 2.2f, -1.8f, 0.25f, 1.1f, 0.0f, 0}, true, true},
			{DefaultShapes::Line, {-2.0f, -1.0f, 3.0f, 1.5f, 0.4f, 0.1f, 0, 0}, true, true},
			{DefaultShapes::Ramp, {0.2f, 0.3f, 2.0f, 1.2f, 0.6f, 0, 0.0f, 0}, true, true},
			{DefaultShapes::Ramp, {0.7f, -0.2f, 1.8f, 1.1f, -0.5f, 0.8f, 0.15f, 0}, true, true},
			{DefaultShapes::SineWave, {0.8f, 1.5f, 2.2f, -0.5f, 0.0f, 0, 0, 0}, false, false},
			{DefaultShapes::SineWave, {0.8f, 1.5f, 2.2f, -0.5f, 0.2f, 0, 0, 0}, false, true},
			{DefaultShapes::Star, {-0.6f, 0.7f, 1.8f, 0.45f, 5.0f, 1.3f, 0.15f, 0}, true, true},
		};

		for (const auto& tc : cases)
		{
			vec4 meta, p0, p1;
			Shape2D::packInstance(tc.id, CombinationType::Addition, 0, 1, 1.0f, tc.vars, meta, p0, p1);
			const uint32_t packed0 = static_cast<uint32_t>(meta.x + 0.5f);
			const uint16_t gpuId = static_cast<uint16_t>(packed0 & 0xFFFFu);
			const uint32_t flags = packed0 >> 19u;

			vec2 bMin, bMax;
			Shape2D::computeBuiltinBounds(gpuId, flags, meta.w, p0, p1, bMin, bMax);

			if (tc.expectFiniteX)
			{
				require(std::isfinite(bMin.x) && std::isfinite(bMax.x) && bMin.x < bMax.x,
						"Bounded builtin shape must produce finite non-empty X bounds.");
			}
			else
			{
				require(std::isinf(bMin.x) && bMin.x < 0.0f && std::isinf(bMax.x) && bMax.x > 0.0f,
						"SineWave must be horizontally unbounded.");
			}
			if (tc.expectFiniteYMin)
			{
				require(std::isfinite(bMin.y) && std::isfinite(bMax.y) && bMin.y < bMax.y,
						"Bounded builtin shape must produce finite non-empty Y bounds.");
			}
			else
			{
				require(std::isinf(bMin.y) && bMin.y < 0.0f && std::isfinite(bMax.y),
						"SineWave must be half-bounded from above (finite maxY, -inf minY).");
			}

			// Dense sample grid check: no inside point (sdf <= 0) may ever fall outside [bMin, bMax]
			for (int gy = -16; gy <= 16; ++gy)
			{
				for (int gx = -16; gx <= 16; ++gx)
				{
					const vec2 pt(gx * 0.35f, gy * 0.35f);
					const float d = Shape2D::evalPackedBuiltin(gpuId, flags, meta.w, p0, p1, pt, kTime);
					if (d <= 0.0f)
					{
						require(pt.x >= bMin.x - 1e-3f && pt.x <= bMax.x + 1e-3f && pt.y >= bMin.y - 1e-3f &&
									pt.y <= bMax.y + 1e-3f,
								"Built-in AABB must conservatively enclose every interior point.");
					}
				}
			}
		}

		// 2. Explicit Bounds2DExpr API (time + audioVolume + combinators)
		{
			auto posY = var(1) + var(4) * sin(var(5) * time());
			auto boxB = boundsBox({var(0), posY}, {var(2), var(3)});
			auto waveB = boundsHalfPlaneBelow(var(3) + abs(var(0)) + audioVolume() * var(4));

			float vars[8] = {2.0f, 5.0f, 1.5f, 1.0f, 3.0f, 2.0f, 0.0f, 0.0f};
			float params[12];
			packSdfParameters(params, vars, kTime, {0.0f, 0.0f}, kAudio);

			vec2 mn, mx;
			boxB.evaluate(params, mn, mx);
			const float expectedY = 5.0f + 3.0f * std::sin(2.0f * kTime);
			require(std::abs(mn.x - 0.5f) < 1e-4f && std::abs(mx.x - 3.5f) < 1e-4f &&
						std::abs(mn.y - (expectedY - 1.0f)) < 1e-4f && std::abs(mx.y - (expectedY + 1.0f)) < 1e-4f,
					"Explicit animated Bounds2DExpr must evaluate live time() bounds accurately.");

			waveB.evaluate(params, mn, mx);
			const float expectedMaxY = 1.0f + 2.0f + kAudio * 3.0f;
			require(std::isinf(mn.x) && std::isinf(mx.x) && std::isinf(mn.y) && std::abs(mx.y - expectedMaxY) < 1e-4f,
					"boundsHalfPlaneBelow must evaluate audioVolume()-reactive crest height.");
		}

		// 3. Automatic bounds propagation through SDF transforms, scaling, mirrorX, CSG, and repeat
		{
			// Scaled local space (PieceShapes pattern): translate -> divide by s -> union + smoothUnion + onion -> * s
			auto s = var(2);
			auto pScaled = translate(point(), {var(0), var(1)}) / s;
			Expr base = sdBox(translate(pScaled, {0.0f, -0.5f}), {0.5f, 0.25f});
			Expr top = sdCircle(translate(pScaled, {0.0f, 0.5f}), 0.5f);
			Expr piece = sdfOnion(sdfSmoothUnion(base, top, 0.1f), 0.05f) * s;
			require(piece.bounds != nullptr && !piece.bounds->isUnbounded,
					"Composed scaled SDF expression must auto-derive bounded Bounds2DExpr.");

			float vars[8] = {10.0f, -4.0f, 2.0f, 0, 0, 0, 0, 0};
			float params[12];
			packSdfParameters(params, vars, kTime, {0.0f, 0.0f}, 0.0f);
			vec2 mn, mx;
			piece.bounds->evaluate(params, mn, mx);
			// Local union: x in [-0.5, 0.5], y in [-0.75, 1.0]; scaled by 2 around (10, -4):
			// x in [9, 11], y in [-5.5, -2]; smoothUnion(+0.1*2) + onion(+0.05*2) = +0.3 world pad
			require(std::abs(mn.x - 8.7f) < 1e-3f && std::abs(mx.x - 11.3f) < 1e-3f &&
						std::abs(mn.y - (-5.8f)) < 1e-3f && std::abs(mx.y - (-1.7f)) < 1e-3f,
					"Scaled domain auto-bounds must scale smoothUnion and onion margins by frame metricScale.");

			// mirrorX propagation
			Expr eyes = sdCircle(translate(mirrorX(translate(point(), {10.0f, 5.0f})), {2.0f, 1.0f}), 0.5f);
			require(eyes.bounds != nullptr, "mirrorX SDF must auto-derive symmetric bounds.");
			eyes.bounds->evaluate(params, mn, mx);
			require(std::abs(mn.x - 7.5f) < 1e-4f && std::abs(mx.x - 12.5f) < 1e-4f && std::abs(mn.y - 5.5f) < 1e-4f &&
						std::abs(mx.y - 6.5f) < 1e-4f,
					"mirrorX auto-bounds must mirror local X bounds across the mirror plane.");

			// Subtraction preserves base bounds even when subtrahend is unbounded (repeat)
			Expr repeatedCut = sdCircle(repeat(point(), 2.0f), 0.25f);
			require(repeatedCut.bounds == nullptr, "Repeated domain SDF must fall back to unbounded.");
			Expr carved = sdfSubtract(sdBox(translate(point(), {1.0f, 2.0f}), {3.0f, 4.0f}), repeatedCut);
			require(carved.bounds != nullptr, "sdfSubtract must preserve base shape bounds.");
			carved.bounds->evaluate(params, mn, mx);
			require(std::abs(mn.x - (-2.0f)) < 1e-4f && std::abs(mx.x - 4.0f) < 1e-4f &&
						std::abs(mn.y - (-2.0f)) < 1e-4f && std::abs(mx.y - 6.0f) < 1e-4f,
					"sdfSubtract bounds must match base AABB.");

			// sdTerrain auto-bounds
			std::vector<glm::vec2> pts = {{0.0f, 5.0f}, {10.0f, 12.0f}, {20.0f, 3.0f}};
			Expr terrain = sdTerrain(translate(point(), {var(0), 0.0f}), pts, 2.0f, -100.0f);
			require(terrain.bounds != nullptr, "sdTerrain must auto-derive vertex AABB.");
			terrain.bounds->evaluate(params, mn, mx);
			// x: [0-2+10, 20+2+10] = [8, 32], y: [-100, 12+2] = [-100, 14]
			require(std::abs(mn.x - 8.0f) < 1e-4f && std::abs(mx.x - 32.0f) < 1e-4f &&
						std::abs(mn.y - (-100.0f)) < 1e-4f && std::abs(mx.y - 14.0f) < 1e-4f,
					"sdTerrain auto-bounds must track translated vertex extents.");
		}

		// 4. NodeGraphCompiler auto-bounds + ShapeService registerSDF priority rules
		{
			NodeGraph graph;
			int pNode = graph.addNode("point", {0.0f, 0.0f});
			int cNode = graph.addNode("circle", {120.0f, 0.0f});
			int outNode = graph.addNode("sdf_output", {260.0f, 0.0f});
			graph.addLink(makePinId(pNode, false, 0), makePinId(cNode, true, 0));
			graph.addLink(makePinId(cNode, false, 0), makePinId(outNode, true, 0));
			Expr compiled = NodeGraphCompiler::evaluate(graph);
			require(compiled.bounds != nullptr && !compiled.bounds->isUnbounded,
					"NodeGraphCompiler::evaluate must automatically propagate Bounds2DExpr.");

			Registry reg;
			reg.registerComponent<Shape>();
			PhysicsSettings settings{};
			Simulation2D sim(16, settings);
			std::vector<std::shared_ptr<IMathExpression>> sdfs(builtins.begin(), builtins.end());
			std::vector<std::shared_ptr<Bounds2DExpr>> sdfBounds(builtins.size(), nullptr);
			uint64_t defVersion = 1;
			ShapeService shapeService(reg, sim, sdfs, nullptr, &defVersion, &sdfBounds);

			ShapeId autoId = shapeService.registerSDF(sdCircle(translate(point(), {var(0), var(1)}), var(2)));
			ShapeId overrideId = shapeService.registerSDF(sdCircle(translate(point(), {var(0), var(1)}), var(2)),
														  boundsBox({var(0), var(1)}, {5.0f, 5.0f}));
			ShapeId rawId = shapeService.registerSDF(std::make_shared<FloatConstant>(1.0f));

			const Entity eAuto = shapeService.addShape({.shapeId = autoId, .variables = {3.0f, 4.0f, 2.0f}});
			const Entity eOverride = shapeService.addShape({.shapeId = overrideId, .variables = {3.0f, 4.0f, 2.0f}});
			const Entity eRaw = shapeService.addShape({.shapeId = rawId});
			(void)eAuto;
			(void)eOverride;
			(void)eRaw;

			std::vector<vec4> packed;
			std::vector<Shape2DAABB> aabbs;
			std::vector<size_t> scratch;
			uint32_t shapeCount = 0;
			Shape2D::packShapes<Shape>(reg, packed, scratch, shapeCount, &aabbs, &sdfBounds, kTime, kAudio);
			require(shapeCount == 3 && aabbs.size() == 3, "packShapes must emit parallel AABBs.");
			require(std::abs(aabbs[0].min.x - 1.0f) < 1e-4f && std::abs(aabbs[0].max.x - 5.0f) < 1e-4f,
					"Auto-derived custom bounds must evaluate via packShapes.");
			require(std::abs(aabbs[1].min.x - (-2.0f)) < 1e-4f && std::abs(aabbs[1].max.x - 8.0f) < 1e-4f,
					"Explicit registerSDF bounds must override auto-derived bounds.");
			require(std::isinf(aabbs[2].min.x) && std::isinf(aabbs[2].max.x),
					"Raw IMathExpression without bounds must safely fall back to unbounded.");
		}
	}
} // namespace

int main(int argc, char** argv)
{
	return WeirdTests::runTests(argc, argv, "WeirdShapeTests",
								{
									{"NodeGraphMultiOutputAndMemoization", testNodeGraphMultiOutputAndMemoization},
									{"BuiltinRotatedAndRoundedShapes", testBuiltinRotatedAndRoundedShapes},
									{"Shape2DPackingParity", testShape2DPackingParity},
									{"Bounds2DExprAndAutoPropagation", testBounds2DExprAndAutoPropagation},
								});
}
