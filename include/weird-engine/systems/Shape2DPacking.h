#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/math/Default3DSDFs.h"
#include "weird-engine/math/SDF.h"
#include "weird-engine/vec.h"
#include "weird-renderer/components/Shape.h"

namespace WeirdEngine
{
	namespace Shape2D
	{
		// Canonical built-in 2D GPU primitive IDs (1:1 with DefaultShapes).
		// Must stay in sync with GPU_SHAPE_* constants in shaders/2d/sdf_shapes.frag.
		enum BuiltinType : uint16_t
		{
			Circle = 0,
			Box = 1,
			Triangle = 2,
			Line = 3,
			Ramp = 4,
			SineWave = 5,
			Star = 6,
			BuiltinCount = 7,
			Invalid = 0xFFFFu
		};

		static_assert(
			static_cast<uint16_t>(DefaultShapes::Circle) == static_cast<uint16_t>(BuiltinType::Circle) &&
				static_cast<uint16_t>(DefaultShapes::Box) == static_cast<uint16_t>(BuiltinType::Box) &&
				static_cast<uint16_t>(DefaultShapes::Triangle) == static_cast<uint16_t>(BuiltinType::Triangle) &&
				static_cast<uint16_t>(DefaultShapes::Line) == static_cast<uint16_t>(BuiltinType::Line) &&
				static_cast<uint16_t>(DefaultShapes::Ramp) == static_cast<uint16_t>(BuiltinType::Ramp) &&
				static_cast<uint16_t>(DefaultShapes::SineWave) == static_cast<uint16_t>(BuiltinType::SineWave) &&
				static_cast<uint16_t>(DefaultShapes::Star) == static_cast<uint16_t>(BuiltinType::Star) &&
				static_cast<uint16_t>(DefaultShapes::COUNT_2D) == static_cast<uint16_t>(BuiltinType::BuiltinCount),
			"DefaultShapes and Shape2D::BuiltinType must match 1:1.");
		static_assert(DefaultShapes3D::TOTAL_BUILTIN_SHAPES > BuiltinCount,
					  "Custom ShapeIds must never overlap canonical GPU built-in IDs.");

		constexpr uint32_t FLAG_OUTLINE = 1u << 0;
		constexpr uint32_t FLAG_GLOBAL_LAYER = 1u << 1;
		constexpr uint32_t FLAG_ROTATED = 1u << 2;
		constexpr uint32_t VEC4S_PER_SHAPE = 3;

		inline float getOutlineThickness(uint16_t shapeId, const float (&params)[8])
		{
			switch (shapeId)
			{
				case DefaultShapes::Circle:
					return params[DefaultShapes::Circle::OutlineThickness];
				case DefaultShapes::Box:
					return params[DefaultShapes::Box::OutlineThickness];
				case DefaultShapes::Triangle:
					return params[DefaultShapes::Triangle::OutlineThickness];
				case DefaultShapes::Line:
					return params[DefaultShapes::Line::OutlineThickness];
				case DefaultShapes::Ramp:
					return params[DefaultShapes::Ramp::OutlineThickness];
				case DefaultShapes::SineWave:
					return params[DefaultShapes::SineWave::OutlineThickness];
				case DefaultShapes::Star:
					return params[DefaultShapes::Star::OutlineThickness];
				default:
					return 0.0f;
			}
		}

		// Packs one 2D Shape/UIShape instance into 3 vec4 slots:
		//   outMeta = vec4(packed0, packed1, smoothFactor, outlineThickness)
		//     packed0 = gpuShapeId (16b) | (combination (3b) << 16) | (flags (3b) << 19)
		//     packed1 = layer (16b)      | (materialId (6b) << 16)
		//   outP0, outP1 = normalized per-primitive parameters (trig & rounding subexpressions hoisted on CPU;
		//                  skipped completely when Angle == 0 or Radius <= 0)
		inline void packInstance(uint16_t shapeId, CombinationType combination, uint16_t layer, uint16_t material,
								 float smoothFactor, const float (&params)[8], vec4& outMeta, vec4& outP0, vec4& outP1)
		{
			uint16_t gpuShapeId = BuiltinType::Invalid;
			uint32_t flags = (layer == Shape::GLOBAL_LAYER) ? FLAG_GLOBAL_LAYER : 0u;
			const float rawOutline = getOutlineThickness(shapeId, params);
			const float effOutline = (rawOutline > 0.0f) ? rawOutline : 0.0f;
			if (effOutline > 0.0f)
			{
				flags |= FLAG_OUTLINE;
			}

			outP0 = vec4(0.0f);
			outP1 = vec4(0.0f);

			if (shapeId < DefaultShapes::COUNT_2D)
			{
				gpuShapeId = shapeId;
				switch (shapeId)
				{
					case DefaultShapes::Circle:
						outP0 = vec4(params[DefaultShapes::Circle::PosX], params[DefaultShapes::Circle::PosY],
									 params[DefaultShapes::Circle::Radius], 0.0f);
						break;
					case DefaultShapes::Box:
					{
						const float r = std::max(params[DefaultShapes::Box::Radius], 0.0f);
						const float bx = std::max(params[DefaultShapes::Box::SizeX] - r, 0.0f);
						const float by = std::max(params[DefaultShapes::Box::SizeY] - r, 0.0f);
						const float angle = params[DefaultShapes::Box::Angle];
						float cosA = 1.0f;
						float sinA = 0.0f;
						if (angle != 0.0f)
						{
							flags |= FLAG_ROTATED;
							cosA = std::cos(angle);
							sinA = std::sin(angle);
						}
						outP0 = vec4(params[DefaultShapes::Box::PosX], params[DefaultShapes::Box::PosY], bx, by);
						outP1 = vec4(cosA, sinA, r, 0.0f);
						break;
					}
					case DefaultShapes::Triangle:
					{
						const float px = params[DefaultShapes::Triangle::PosX];
						const float py = params[DefaultShapes::Triangle::PosY];
						const float w = params[DefaultShapes::Triangle::Width];
						const float h = params[DefaultShapes::Triangle::Height];
						const float radius = params[DefaultShapes::Triangle::Radius];
						const float angle = params[DefaultShapes::Triangle::Angle];
						const float aw = std::max(std::abs(w), 1e-4f);
						const float ah = std::max(std::abs(h), 1e-4f);
						const float sy = (h >= 0.0f) ? 1.0f : -1.0f;

						float cosA = 1.0f;
						float sinA = 0.0f;
						if (angle != 0.0f)
						{
							flags |= FLAG_ROTATED;
							cosA = std::cos(angle);
							sinA = std::sin(angle);
						}

						if (radius > 0.0f)
						{
							const float halfW = aw * 0.5f;
							const float leg = std::hypot(halfW, ah);
							const float inradius = (halfW * ah) / (halfW + leg);
							const float r = std::min(radius, inradius * 0.998f);
							const float scale = 1.0f - r / inradius;
							const float shiftY = (1.0f - scale) * (inradius - ah / 3.0f);
							// Mirroring both q.y and triangle height leaves distance and winding unchanged.
							outP0 = vec4(px, py, aw * scale, sy * ah * scale);
							outP1 = vec4(cosA, sinA, r, sy * shiftY);
						}
						else
						{
							outP0 = vec4(px, py, aw, sy * ah);
							outP1 = vec4(cosA, sinA, 0.0f, 0.0f);
						}
						break;
					}
					case DefaultShapes::Line:
					{
						const float ax = params[DefaultShapes::Line::StartX];
						const float ay = params[DefaultShapes::Line::StartY];
						const float bax = params[DefaultShapes::Line::EndX] - ax;
						const float bay = params[DefaultShapes::Line::EndY] - ay;
						const float dotBa = bax * bax + bay * bay;
						const float invDotBa = (dotBa > 1e-12f) ? (1.0f / dotBa) : 0.0f;
						outP0 = vec4(ax, ay, bax, bay);
						outP1 = vec4(invDotBa, params[DefaultShapes::Line::Width], 0.0f, 0.0f);
						break;
					}
					case DefaultShapes::Ramp:
					{
						const float wi = params[DefaultShapes::Ramp::Width];
						const float he = params[DefaultShapes::Ramp::Height];
						const float sk = params[DefaultShapes::Ramp::Skew];
						const float angle = params[DefaultShapes::Ramp::Angle];
						float cosA = 1.0f;
						float sinA = 0.0f;
						if (angle != 0.0f)
						{
							flags |= FLAG_ROTATED;
							cosA = std::cos(angle);
							sinA = std::sin(angle);
						}
						const float dotEE = wi * wi + sk * sk;
						const float invDotEE = (dotEE > 1e-12f) ? (1.0f / dotEE) : 0.0f;
						outP0 = vec4(params[DefaultShapes::Ramp::PosX], params[DefaultShapes::Ramp::PosY], wi, he);
						outP1 = vec4(sk, cosA, sinA, invDotEE);
						break;
					}
					case DefaultShapes::SineWave:
						outP0 =
							vec4(params[DefaultShapes::SineWave::Amplitude], params[DefaultShapes::SineWave::Period],
								 params[DefaultShapes::SineWave::Speed], params[DefaultShapes::SineWave::Offset]);
						break;
					case DefaultShapes::Star:
						outP0 = vec4(params[DefaultShapes::Star::PosX], params[DefaultShapes::Star::PosY],
									 params[DefaultShapes::Star::Radius], params[DefaultShapes::Star::Displacement]);
						outP1 =
							vec4(params[DefaultShapes::Star::Points], params[DefaultShapes::Star::Speed], 0.0f, 0.0f);
						break;
					default:
						gpuShapeId = BuiltinType::Invalid;
						break;
				}
			}
			else if (shapeId >= DefaultShapes3D::TOTAL_BUILTIN_SHAPES)
			{
				gpuShapeId = shapeId;
				outP0 = vec4(params[0], params[1], params[2], params[3]);
				outP1 = vec4(params[4], params[5], params[6], params[7]);
			}

			// Both packed integers stay below 2^23 so they are represented with 0-ULP exactness in IEEE-754 float.
			const uint32_t packed0 = (static_cast<uint32_t>(gpuShapeId) & 0xFFFFu) |
									 ((static_cast<uint32_t>(combination) & 0x7u) << 16) | ((flags & 0x7u) << 19);
			const uint32_t packed1 =
				(static_cast<uint32_t>(layer) & 0xFFFFu) | ((static_cast<uint32_t>(material) & 0x3Fu) << 16);

			outMeta = vec4(static_cast<float>(packed0), static_cast<float>(packed1), smoothFactor, effOutline);
		}

		// CPU evaluator matching the 7 canonical GLSL primitives in shaders/2d/sdf_shapes.frag.
		// Skips rotation math when FLAG_ROTATED is unset and skips onion math when FLAG_OUTLINE is unset.
		inline float evalPackedBuiltin(uint16_t gpuShapeId, uint32_t flags, float outlineThickness, const vec4& p0,
									   const vec4& p1, vec2 p, float time = 0.0f)
		{
			float dist = 10000.0f;
			switch (gpuShapeId)
			{
				case BuiltinType::Circle:
					dist = glm::length(p - vec2(p0.x, p0.y)) - p0.z;
					break;
				case BuiltinType::Box:
				{
					vec2 q = p - vec2(p0.x, p0.y);
					if ((flags & FLAG_ROTATED) != 0u)
					{
						q = vec2(p1.x * q.x - p1.y * q.y, p1.y * q.x + p1.x * q.y);
					}
					const vec2 d = glm::abs(q) - vec2(p0.z, p0.w);
					dist = glm::length(glm::max(d, vec2(0.0f))) + std::min(std::max(d.x, d.y), 0.0f) - p1.z;
					break;
				}
				case BuiltinType::Triangle:
				{
					vec2 q = p - vec2(p0.x, p0.y);
					if ((flags & FLAG_ROTATED) != 0u)
					{
						q = vec2(p1.x * q.x - p1.y * q.y, p1.y * q.x + p1.x * q.y);
					}
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
					const float crossW = e0.x * e2.y - e0.y * e2.x;
					const float s = (crossW > 0.0f) ? 1.0f : ((crossW < 0.0f) ? -1.0f : 0.0f);
					const float dx = std::min({glm::dot(pq0, pq0), glm::dot(pq1, pq1), glm::dot(pq2, pq2)});
					const float dy = std::min({s * (d0.x * e0.y - d0.y * e0.x), s * (d1.x * e1.y - d1.y * e1.x),
											   s * (d2.x * e2.y - d2.y * e2.x)});
					const float signDy = (dy > 0.0f) ? 1.0f : ((dy < 0.0f) ? -1.0f : 0.0f);
					dist = -std::sqrt(dx) * signDy - p1.z;
					break;
				}
				case BuiltinType::Line:
				{
					const vec2 pa = p - vec2(p0.x, p0.y);
					const vec2 ba(p0.z, p0.w);
					const float h = std::clamp(glm::dot(pa, ba) * p1.x, 0.0f, 1.0f);
					dist = glm::length(pa - ba * h) - p1.y;
					break;
				}
				case BuiltinType::Ramp:
				{
					vec2 q = p - vec2(p0.x, p0.y);
					if ((flags & FLAG_ROTATED) != 0u)
					{
						q = vec2(p1.y * q.x - p1.z * q.y, p1.z * q.x + p1.y * q.y);
					}
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
				case BuiltinType::SineWave:
					dist = (p.y - p0.w) - p0.x * std::sin(p0.y * p.x + p0.z * time);
					break;
				case BuiltinType::Star:
				{
					const vec2 q = p - vec2(p0.x, p0.y);
					const float d = glm::length(q) - p0.z;
					const float angle = p1.x * std::atan2(q.y, q.x) - p1.y * time;
					dist = d + p0.w * std::sin(angle);
					break;
				}
				default:
					break;
			}

			if ((flags & FLAG_OUTLINE) != 0u)
			{
				dist = std::abs(dist) - outlineThickness;
			}
			return dist;
		}

		// Evaluates any 2D Shape or UIShape on the CPU, fast-pathing built-ins.
		template <typename ShapeLike>
		inline float evaluate(const ShapeLike& shape, const std::vector<std::shared_ptr<IMathExpression>>& sdfs, vec2 p,
							  float time = 0.0f, float audioVolume = 0.0f)
		{
			const auto builtins = Scene::getBuiltinSDFs();
			const bool isCanonicalBuiltin =
				shape.distanceFieldId < DefaultShapes::COUNT_2D &&
				(sdfs.empty() || (shape.distanceFieldId < sdfs.size() && shape.distanceFieldId < builtins.size() &&
								  builtins[shape.distanceFieldId] != nullptr &&
								  sdfs[shape.distanceFieldId].get() == builtins[shape.distanceFieldId].get()));
			if (isCanonicalBuiltin)
			{
				vec4 meta, p0, p1;
				packInstance(shape.distanceFieldId, shape.combination, shape.layer, shape.material, shape.smoothFactor,
							 shape.parameters, meta, p0, p1);
				const uint32_t packed0 = static_cast<uint32_t>(meta.x + 0.5f);
				const uint16_t gpuId = static_cast<uint16_t>(packed0 & 0xFFFFu);
				const uint32_t flags = packed0 >> 19u;
				return evalPackedBuiltin(gpuId, flags, meta.w, p0, p1, p, time);
			}

			if (shape.distanceFieldId < sdfs.size() && sdfs[shape.distanceFieldId])
			{
				float evalParams[12];
				packSdfParameters(evalParams, shape.parameters, time, p, audioVolume);
				return sdfs[shape.distanceFieldId]->getValue(evalParams);
			}
			return 10000.0f;
		}

		// Computes an exact tight world-space AABB for any canonical GPU primitive directly from packed params.
		inline void computeBuiltinBounds(uint16_t gpuShapeId, uint32_t flags, float outlineThickness, const vec4& p0,
										 const vec4& p1, vec2& outMin, vec2& outMax)
		{
			constexpr float kNegInf = -std::numeric_limits<float>::infinity();
			constexpr float kPosInf = std::numeric_limits<float>::infinity();
			const bool isOnion = (flags & FLAG_OUTLINE) != 0u;
			const bool isRotated = (flags & FLAG_ROTATED) != 0u;
			const float onion = isOnion ? std::max(outlineThickness, 0.0f) : 0.0f;

			switch (gpuShapeId)
			{
				case BuiltinType::Circle:
				{
					const float reach = std::max(p0.z, 0.0f) + onion;
					outMin = vec2(p0.x - reach, p0.y - reach);
					outMax = vec2(p0.x + reach, p0.y + reach);
					return;
				}
				case BuiltinType::Box:
				{
					const float bx = std::abs(p0.z);
					const float by = std::abs(p0.w);
					const float r = std::max(p1.z, 0.0f) + onion;
					float hx = bx + r;
					float hy = by + r;
					if (isRotated)
					{
						const float ac = std::abs(p1.x);
						const float as = std::abs(p1.y);
						hx = ac * bx + as * by + r;
						hy = as * bx + ac * by + r;
					}
					outMin = vec2(p0.x - hx, p0.y - hy);
					outMax = vec2(p0.x + hx, p0.y + hy);
					return;
				}
				case BuiltinType::Triangle:
				{
					const float w = p0.z;
					const float h = p0.w;
					const float expand = std::max(p1.z, 0.0f) + onion;
					const float shiftY = p1.w;
					const float yBot = -h / 3.0f + shiftY;
					const float yTop = (2.0f * h) / 3.0f + shiftY;

					if (!isRotated)
					{
						const float hw = std::abs(w) * 0.5f;
						outMin = vec2(p0.x - hw - expand, p0.y + std::min(yBot, yTop) - expand);
						outMax = vec2(p0.x + hw + expand, p0.y + std::max(yBot, yTop) + expand);
						return;
					}

					const float cosA = p1.x;
					const float sinA = p1.y;
					const vec2 localVerts[3] = {
						{-w * 0.5f, yBot},
						{w * 0.5f, yBot},
						{0.0f, yTop},
					};

					vec2 mn(kPosInf, kPosInf);
					vec2 mx(kNegInf, kNegInf);
					for (const vec2& v : localVerts)
					{
						// Inverse rotation R(-A) maps q_rot back to world space relative to (p0.x, p0.y)
						const float wx = p0.x + cosA * v.x + sinA * v.y;
						const float wy = p0.y - sinA * v.x + cosA * v.y;
						mn.x = std::min(mn.x, wx);
						mn.y = std::min(mn.y, wy);
						mx.x = std::max(mx.x, wx);
						mx.y = std::max(mx.y, wy);
					}
					outMin = mn - vec2(expand);
					outMax = mx + vec2(expand);
					return;
				}
				case BuiltinType::Line:
				{
					const float ax = p0.x;
					const float ay = p0.y;
					const float bx = ax + p0.z;
					const float by = ay + p0.w;
					const float reach = std::max(p1.y, 0.0f) + onion;
					outMin = vec2(std::min(ax, bx) - reach, std::min(ay, by) - reach);
					outMax = vec2(std::max(ax, bx) + reach, std::max(ay, by) + reach);
					return;
				}
				case BuiltinType::Ramp:
				{
					const float wi = p0.z;
					const float he = p0.w;
					const float sk = p1.x;
					const float c0y = sk + he;
					const float c1y = sk - he;
					float hx = std::abs(wi) + onion;
					float hy = std::max(std::abs(c0y), std::abs(c1y)) + onion;
					if (isRotated)
					{
						const float cosA = p1.y;
						const float sinA = p1.z;
						hx = std::max(std::abs(cosA * wi + sinA * c0y), std::abs(cosA * wi + sinA * c1y)) + onion;
						hy = std::max(std::abs(-sinA * wi + cosA * c0y), std::abs(-sinA * wi + cosA * c1y)) + onion;
					}
					outMin = vec2(p0.x - hx, p0.y - hy);
					outMax = vec2(p0.x + hx, p0.y + hy);
					return;
				}
				case BuiltinType::SineWave:
				{
					const float amp = std::abs(p0.x);
					const float offset = p0.w;
					if (isOnion)
					{
						outMin = vec2(kNegInf, offset - amp - onion);
						outMax = vec2(kPosInf, offset + amp + onion);
					}
					else
					{
						outMin = vec2(kNegInf, kNegInf);
						outMax = vec2(kPosInf, offset + amp);
					}
					return;
				}
				case BuiltinType::Star:
				{
					const float reach = std::max(p0.z, 0.0f) + std::abs(p0.w) + onion;
					outMin = vec2(p0.x - reach, p0.y - reach);
					outMax = vec2(p0.x + reach, p0.y + reach);
					return;
				}
				default:
					outMin = vec2(kNegInf, kNegInf);
					outMax = vec2(kPosInf, kPosInf);
					return;
			}
		}

		// Collects, layer/order-sorts, and packs all 2D shape instances into a contiguous GPU upload buffer,
		// optionally computing parallel world-space AABBs for the screen-space tile acceleration grid.
		template <typename ShapeClass>
		inline void packShapes(Registry& registry, std::vector<vec4>& outShapeBuffer, std::vector<size_t>& scratchOrder,
							   uint32_t& outShapeCount, std::vector<Shape2DAABB>* outBoundsBuffer = nullptr,
							   const std::vector<std::shared_ptr<Bounds2DExpr>>* sdfBounds = nullptr, float time = 0.0f,
							   float audioVolume = 0.0f)
		{
			auto shapeArray = registry.getComponentArray<ShapeClass>();
			if (!shapeArray)
			{
				outShapeCount = 0;
				outShapeBuffer.clear();
				if (outBoundsBuffer)
				{
					outBoundsBuffer->clear();
				}
				return;
			}

			const size_t count = shapeArray->getSize();
			outShapeCount = static_cast<uint32_t>(count);
			if (count == 0)
			{
				outShapeBuffer.clear();
				if (outBoundsBuffer)
				{
					outBoundsBuffer->clear();
				}
				return;
			}

			scratchOrder.resize(count);
			for (size_t i = 0; i < count; ++i)
			{
				scratchOrder[i] = i;
			}

			std::stable_sort(scratchOrder.begin(), scratchOrder.end(),
							 [&](size_t a, size_t b)
							 {
								 const auto& sa = shapeArray->getDataAtIdx(a);
								 const auto& sb = shapeArray->getDataAtIdx(b);
								 if (sa.layer != sb.layer)
								 {
									 return sa.layer < sb.layer;
								 }
								 return sa.order < sb.order;
							 });

			outShapeBuffer.resize(count * VEC4S_PER_SHAPE);
			if (outBoundsBuffer)
			{
				outBoundsBuffer->resize(count);
			}

			float evalParams[12] = {0.0f};
			evalParams[SystemParams::TIME] = time;
			evalParams[SystemParams::AUDIO_VOLUME] = audioVolume;

			for (size_t outIdx = 0; outIdx < count; ++outIdx)
			{
				const ShapeClass& s = shapeArray->getDataAtIdx(scratchOrder[outIdx]);
				const size_t base = outIdx * VEC4S_PER_SHAPE;
				vec4& meta = outShapeBuffer[base];
				vec4& p0 = outShapeBuffer[base + 1];
				vec4& p1 = outShapeBuffer[base + 2];
				packInstance(s.distanceFieldId, s.combination, s.layer, s.material, s.smoothFactor, s.parameters, meta,
							 p0, p1);

				if (outBoundsBuffer)
				{
					Shape2DAABB& aabb = (*outBoundsBuffer)[outIdx];
					aabb.layer = s.layer;
					aabb.combination = s.combination;
					aabb.smoothFactor = s.smoothFactor;

					if (s.distanceFieldId < DefaultShapes::COUNT_2D)
					{
						const uint32_t packed0 = static_cast<uint32_t>(meta.x + 0.5f);
						const uint16_t gpuShapeId = static_cast<uint16_t>(packed0 & 0xFFFFu);
						const uint32_t flags = packed0 >> 19u;
						computeBuiltinBounds(gpuShapeId, flags, meta.w, p0, p1, aabb.min, aabb.max);
					}
					else if (sdfBounds && s.distanceFieldId < sdfBounds->size() && (*sdfBounds)[s.distanceFieldId])
					{
						std::copy_n(s.parameters, 8, evalParams);
						const auto& boundsExpr = (*sdfBounds)[s.distanceFieldId];
						boundsExpr->evaluate(evalParams, aabb.min, aabb.max);
					}
					else
					{
						aabb.min = vec2(Bounds2DExpr::kNegInf, Bounds2DExpr::kNegInf);
						aabb.max = vec2(Bounds2DExpr::kPosInf, Bounds2DExpr::kPosInf);
					}
				}
			}
		}
	} // namespace Shape2D
} // namespace WeirdEngine
