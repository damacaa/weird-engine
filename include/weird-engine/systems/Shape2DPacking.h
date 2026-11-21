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
	namespace GpuShape2D
	{
		// Canonical built-in 2D GPU primitive IDs (collapsed from the 22 DefaultShapes variants).
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

		static_assert(DefaultShapes3D::TOTAL_BUILTIN_SHAPES > BuiltinCount,
					  "Custom ShapeIds must never overlap canonical GPU built-in IDs.");
		static_assert(DefaultShapes::COUNT_2D == 22,
					  "Update GpuShape2D::packInstance when adding or removing built-in 2D shapes.");

		constexpr uint32_t FLAG_ONION = 1u << 0;
		constexpr uint32_t FLAG_GLOBAL_GROUP = 1u << 1;
		constexpr uint32_t VEC4S_PER_SHAPE = 3;

		// Packs one 2D Shape/UIShape instance into 3 vec4 slots:
		//   outMeta = vec4(packed0, packed1, smoothFactor, onionThickness)
		//     packed0 = gpuShapeId (16b) | (combination (3b) << 16) | (flags (2b) << 19)
		//     packed1 = groupIdx (16b)   | (materialId (6b) << 16)
		//   outP0, outP1 = normalized per-primitive parameters (trig & constant subexpressions hoisted on CPU)
		inline void packInstance(uint16_t shapeId, CombinationType combination, uint16_t groupIdx, uint16_t material,
								 float smoothFactor, const float (&params)[8], vec4& outMeta, vec4& outP0, vec4& outP1)
		{
			uint16_t gpuShapeId = BuiltinType::Invalid;
			uint32_t flags = (groupIdx == Shape::GLOBAL_GROUP) ? FLAG_GLOBAL_GROUP : 0u;
			float onionThickness = 0.0f;

			outP0 = vec4(0.0f);
			outP1 = vec4(0.0f);

			auto packBox = [&](float px, float py, float sx, float sy, float angle, float roundR, bool applyRound)
			{
				gpuShapeId = BuiltinType::Box;
				const float r = applyRound ? std::max(roundR, 0.0f) : 0.0f;
				const float bx = applyRound ? std::max(sx - r, 0.0f) : sx;
				const float by = applyRound ? std::max(sy - r, 0.0f) : sy;
				const float cosA = (angle != 0.0f) ? std::cos(angle) : 1.0f;
				const float sinA = (angle != 0.0f) ? std::sin(angle) : 0.0f;
				outP0 = vec4(px, py, bx, by);
				outP1 = vec4(cosA, sinA, r, 0.0f);
			};

			auto packTriangle = [&](float px, float py, float w, float h, float angle)
			{
				gpuShapeId = BuiltinType::Triangle;
				const float cosA = (angle != 0.0f) ? std::cos(angle) : 1.0f;
				const float sinA = (angle != 0.0f) ? std::sin(angle) : 0.0f;
				outP0 = vec4(px, py, w, h);
				outP1 = vec4(cosA, sinA, 0.0f, 0.0f);
			};

			auto packRoundTriangle = [&](float px, float py, float w, float h, float radius, float angle)
			{
				gpuShapeId = BuiltinType::Triangle;
				const float aw = std::max(std::abs(w), 1e-4f);
				const float ah = std::max(std::abs(h), 1e-4f);
				const float sy = (h >= 0.0f) ? 1.0f : -1.0f;
				const float halfW = aw * 0.5f;
				const float leg = std::hypot(halfW, ah);
				const float inradius = (halfW * ah) / (halfW + leg);
				const float r = std::min(std::max(radius, 0.0f), inradius * 0.998f);
				const float scale = 1.0f - r / inradius;
				const float shiftY = (1.0f - scale) * (inradius - ah / 3.0f);
				const float cosA = (angle != 0.0f) ? std::cos(angle) : 1.0f;
				const float sinA = (angle != 0.0f) ? std::sin(angle) : 0.0f;

				// Mirroring both q.y and triangle height leaves distance and winding unchanged.
				outP0 = vec4(px, py, aw * scale, sy * ah * scale);
				outP1 = vec4(cosA, sinA, r, sy * shiftY);
			};

			auto packRamp = [&](float px, float py, float wi, float he, float sk, float angle)
			{
				gpuShapeId = BuiltinType::Ramp;
				const float cosA = (angle != 0.0f) ? std::cos(angle) : 1.0f;
				const float sinA = (angle != 0.0f) ? std::sin(angle) : 0.0f;
				const float dotEE = wi * wi + sk * sk;
				const float invDotEE = (dotEE > 1e-12f) ? (1.0f / dotEE) : 0.0f;
				outP0 = vec4(px, py, wi, he);
				outP1 = vec4(sk, cosA, sinA, invDotEE);
			};

			if (shapeId < DefaultShapes::COUNT_2D)
			{
				switch (shapeId)
				{
					case DefaultShapes::Circle:
						gpuShapeId = BuiltinType::Circle;
						outP0 = vec4(params[DefaultShapes::Circle::PosX], params[DefaultShapes::Circle::PosY],
									 params[DefaultShapes::Circle::Radius], 0.0f);
						break;
					case DefaultShapes::CircleLine:
						gpuShapeId = BuiltinType::Circle;
						flags |= FLAG_ONION;
						onionThickness = params[DefaultShapes::CircleLine::Thickness];
						outP0 = vec4(params[DefaultShapes::CircleLine::PosX], params[DefaultShapes::CircleLine::PosY],
									 params[DefaultShapes::CircleLine::Radius], 0.0f);
						break;
					case DefaultShapes::Box:
						packBox(params[DefaultShapes::Box::PosX], params[DefaultShapes::Box::PosY],
								params[DefaultShapes::Box::SizeX], params[DefaultShapes::Box::SizeY], 0.0f, 0.0f,
								false);
						break;
					case DefaultShapes::BoxLine:
						flags |= FLAG_ONION;
						onionThickness = params[DefaultShapes::BoxLine::Thickness];
						packBox(params[DefaultShapes::BoxLine::PosX], params[DefaultShapes::BoxLine::PosY],
								params[DefaultShapes::BoxLine::SizeX], params[DefaultShapes::BoxLine::SizeY], 0.0f,
								0.0f, false);
						break;
					case DefaultShapes::RoundBox:
						packBox(params[DefaultShapes::RoundBox::PosX], params[DefaultShapes::RoundBox::PosY],
								params[DefaultShapes::RoundBox::SizeX], params[DefaultShapes::RoundBox::SizeY], 0.0f,
								params[DefaultShapes::RoundBox::Radius], true);
						break;
					case DefaultShapes::RoundBoxLine:
						flags |= FLAG_ONION;
						onionThickness = params[DefaultShapes::RoundBoxLine::Thickness];
						packBox(params[DefaultShapes::RoundBoxLine::PosX], params[DefaultShapes::RoundBoxLine::PosY],
								params[DefaultShapes::RoundBoxLine::SizeX], params[DefaultShapes::RoundBoxLine::SizeY],
								0.0f, params[DefaultShapes::RoundBoxLine::Radius], true);
						break;
					case DefaultShapes::RoundedBox:
						packBox(params[DefaultShapes::RoundedBox::PosX], params[DefaultShapes::RoundedBox::PosY],
								params[DefaultShapes::RoundedBox::SizeX], params[DefaultShapes::RoundedBox::SizeY],
								0.0f, params[DefaultShapes::RoundedBox::Radius], true);
						break;
					case DefaultShapes::RoundedBoxLine:
						flags |= FLAG_ONION;
						onionThickness = params[DefaultShapes::RoundedBoxLine::Thickness];
						packBox(
							params[DefaultShapes::RoundedBoxLine::PosX], params[DefaultShapes::RoundedBoxLine::PosY],
							params[DefaultShapes::RoundedBoxLine::SizeX], params[DefaultShapes::RoundedBoxLine::SizeY],
							0.0f, params[DefaultShapes::RoundedBoxLine::Radius], true);
						break;
					case DefaultShapes::BoxRotated:
						packBox(params[DefaultShapes::BoxRotated::PosX], params[DefaultShapes::BoxRotated::PosY],
								params[DefaultShapes::BoxRotated::SizeX], params[DefaultShapes::BoxRotated::SizeY],
								params[DefaultShapes::BoxRotated::Angle], 0.0f, false);
						break;
					case DefaultShapes::BoxLineRotated:
						flags |= FLAG_ONION;
						onionThickness = params[DefaultShapes::BoxLineRotated::Thickness];
						packBox(
							params[DefaultShapes::BoxLineRotated::PosX], params[DefaultShapes::BoxLineRotated::PosY],
							params[DefaultShapes::BoxLineRotated::SizeX], params[DefaultShapes::BoxLineRotated::SizeY],
							params[DefaultShapes::BoxLineRotated::Angle], 0.0f, false);
						break;
					case DefaultShapes::RoundBoxRotated:
						packBox(params[DefaultShapes::RoundBoxRotated::PosX],
								params[DefaultShapes::RoundBoxRotated::PosY],
								params[DefaultShapes::RoundBoxRotated::SizeX],
								params[DefaultShapes::RoundBoxRotated::SizeY],
								params[DefaultShapes::RoundBoxRotated::Angle],
								params[DefaultShapes::RoundBoxRotated::Radius], true);
						break;
					case DefaultShapes::Triangle:
						packTriangle(params[DefaultShapes::Triangle::PosX], params[DefaultShapes::Triangle::PosY],
									 params[DefaultShapes::Triangle::Width], params[DefaultShapes::Triangle::Height],
									 0.0f);
						break;
					case DefaultShapes::TriangleLine:
						flags |= FLAG_ONION;
						onionThickness = params[DefaultShapes::TriangleLine::Thickness];
						packTriangle(params[DefaultShapes::TriangleLine::PosX],
									 params[DefaultShapes::TriangleLine::PosY],
									 params[DefaultShapes::TriangleLine::Width],
									 params[DefaultShapes::TriangleLine::Height], 0.0f);
						break;
					case DefaultShapes::TriangleRotated:
						packTriangle(params[DefaultShapes::TriangleRotated::PosX],
									 params[DefaultShapes::TriangleRotated::PosY],
									 params[DefaultShapes::TriangleRotated::Width],
									 params[DefaultShapes::TriangleRotated::Height],
									 params[DefaultShapes::TriangleRotated::Angle]);
						break;
					case DefaultShapes::TriangleLineRotated:
						flags |= FLAG_ONION;
						onionThickness = params[DefaultShapes::TriangleLineRotated::Thickness];
						packTriangle(params[DefaultShapes::TriangleLineRotated::PosX],
									 params[DefaultShapes::TriangleLineRotated::PosY],
									 params[DefaultShapes::TriangleLineRotated::Width],
									 params[DefaultShapes::TriangleLineRotated::Height],
									 params[DefaultShapes::TriangleLineRotated::Angle]);
						break;
					case DefaultShapes::RoundTriangle:
						packRoundTriangle(
							params[DefaultShapes::RoundTriangle::PosX], params[DefaultShapes::RoundTriangle::PosY],
							params[DefaultShapes::RoundTriangle::Width], params[DefaultShapes::RoundTriangle::Height],
							params[DefaultShapes::RoundTriangle::Radius], 0.0f);
						break;
					case DefaultShapes::RoundTriangleRotated:
						packRoundTriangle(params[DefaultShapes::RoundTriangleRotated::PosX],
										  params[DefaultShapes::RoundTriangleRotated::PosY],
										  params[DefaultShapes::RoundTriangleRotated::Width],
										  params[DefaultShapes::RoundTriangleRotated::Height],
										  params[DefaultShapes::RoundTriangleRotated::Radius],
										  params[DefaultShapes::RoundTriangleRotated::Angle]);
						break;
					case DefaultShapes::Line:
					{
						gpuShapeId = BuiltinType::Line;
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
						packRamp(params[DefaultShapes::Ramp::PosX], params[DefaultShapes::Ramp::PosY],
								 params[DefaultShapes::Ramp::Width], params[DefaultShapes::Ramp::Height],
								 params[DefaultShapes::Ramp::Skew], 0.0f);
						break;
					case DefaultShapes::RampRotated:
						packRamp(params[DefaultShapes::RampRotated::PosX], params[DefaultShapes::RampRotated::PosY],
								 params[DefaultShapes::RampRotated::Width], params[DefaultShapes::RampRotated::Height],
								 params[DefaultShapes::RampRotated::Skew], params[DefaultShapes::RampRotated::Angle]);
						break;
					case DefaultShapes::SineWave:
						gpuShapeId = BuiltinType::SineWave;
						outP0 =
							vec4(params[DefaultShapes::SineWave::Amplitude], params[DefaultShapes::SineWave::Period],
								 params[DefaultShapes::SineWave::Speed], params[DefaultShapes::SineWave::Offset]);
						break;
					case DefaultShapes::Star:
						gpuShapeId = BuiltinType::Star;
						outP0 = vec4(params[DefaultShapes::Star::PosX], params[DefaultShapes::Star::PosY],
									 params[DefaultShapes::Star::Radius], params[DefaultShapes::Star::Displacement]);
						outP1 =
							vec4(params[DefaultShapes::Star::Points], params[DefaultShapes::Star::Speed], 0.0f, 0.0f);
						break;
					default:
						break;
				}
			}
			else if (shapeId >= DefaultShapes3D::TOTAL_BUILTIN_SHAPES)
			{
				gpuShapeId = shapeId;
				outP0 = vec4(params[0], params[1], params[2], params[3]);
				outP1 = vec4(params[4], params[5], params[6], params[7]);
			}

			// Both packed integers stay below 2^23 so they are represented exactly in IEEE-754 float.
			const uint32_t packed0 = (static_cast<uint32_t>(gpuShapeId) & 0xFFFFu) |
									 ((static_cast<uint32_t>(combination) & 0x7u) << 16) | ((flags & 0x3u) << 19);
			const uint32_t packed1 =
				(static_cast<uint32_t>(groupIdx) & 0xFFFFu) | ((static_cast<uint32_t>(material) & 0x3Fu) << 16);

			outMeta = vec4(static_cast<float>(packed0), static_cast<float>(packed1), smoothFactor, onionThickness);
		}

		// Computes an exact tight world-space AABB for any collapsed built-in GPU primitive directly from packed
		// params.
		inline void computeBuiltinBounds(uint16_t gpuShapeId, uint32_t flags, float onionThickness, const vec4& p0,
										 const vec4& p1, vec2& outMin, vec2& outMax)
		{
			constexpr float kNegInf = -std::numeric_limits<float>::infinity();
			constexpr float kPosInf = std::numeric_limits<float>::infinity();
			const bool isOnion = (flags & FLAG_ONION) != 0u;
			const float onion = isOnion ? std::max(onionThickness, 0.0f) : 0.0f;

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
					const float ac = std::abs(p1.x);
					const float as = std::abs(p1.y);
					const float r = std::max(p1.z, 0.0f) + onion;
					const float hx = ac * bx + as * by + r;
					const float hy = as * bx + ac * by + r;
					outMin = vec2(p0.x - hx, p0.y - hy);
					outMax = vec2(p0.x + hx, p0.y + hy);
					return;
				}
				case BuiltinType::Triangle:
				{
					const float w = p0.z;
					const float h = p0.w;
					const float cosA = p1.x;
					const float sinA = p1.y;
					const float expand = std::max(p1.z, 0.0f) + onion;
					const float shiftY = p1.w;

					const vec2 localVerts[3] = {
						{-w * 0.5f, -h / 3.0f + shiftY},
						{w * 0.5f, -h / 3.0f + shiftY},
						{0.0f, (2.0f * h) / 3.0f + shiftY},
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
					const float cosA = p1.y;
					const float sinA = p1.z;

					const float c0y = sk + he;
					const float c1y = sk - he;
					const float hx =
						std::max(std::abs(cosA * wi + sinA * c0y), std::abs(cosA * wi + sinA * c1y)) + onion;
					const float hy =
						std::max(std::abs(-sinA * wi + cosA * c0y), std::abs(-sinA * wi + cosA * c1y)) + onion;
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

		// Collects, group-sorts, and packs all 2D shape instances into a contiguous GPU upload buffer,
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

			std::stable_sort(scratchOrder.begin(), scratchOrder.end(), [&](size_t a, size_t b)
							 { return shapeArray->getDataAtIdx(a).groupIdx < shapeArray->getDataAtIdx(b).groupIdx; });

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
				packInstance(s.distanceFieldId, s.combination, s.groupIdx, s.material, s.smoothFactor, s.parameters,
							 meta, p0, p1);

				if (outBoundsBuffer)
				{
					Shape2DAABB& aabb = (*outBoundsBuffer)[outIdx];
					aabb.groupIdx = s.groupIdx;
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
						(*sdfBounds)[s.distanceFieldId]->evaluate(evalParams, aabb.min, aabb.max);
					}
					else
					{
						aabb.min = vec2(Bounds2DExpr::kNegInf, Bounds2DExpr::kNegInf);
						aabb.max = vec2(Bounds2DExpr::kPosInf, Bounds2DExpr::kPosInf);
					}
				}
			}
		}
	} // namespace GpuShape2D
} // namespace WeirdEngine
