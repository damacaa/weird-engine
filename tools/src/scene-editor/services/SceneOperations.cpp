#include "scene-editor/services/SceneOperations.h"

#include "ToolsSession.h"
#include "weird-engine.h"
#include "weird-engine/components/FlyMovement.h"
#include "weird-engine/components/FlyMovement2D.h"
#include "weird-engine/components/Transform.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-engine/systems/PhysicsSystem2D.h"
#include "weird-engine/systems/Shape2DPacking.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/components/Spring.h"
#include "weird-renderer/components/Button.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/components/Light2DComponent.h"
#include "weird-renderer/components/SDFRenderer.h"
#include "weird-renderer/components/Shape.h"
#include "weird-renderer/components/TextRenderer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <imgui.h>
#include <json/json.h>
#include <limits>
#include <unordered_set>

namespace WeirdEngine::Tools::SceneEd
{
	namespace
	{
		std::mutex& liveSceneOpsMutex()
		{
			static std::mutex m;
			return m;
		}

		std::unordered_set<SceneOperations*>& liveSceneOps()
		{
			static std::unordered_set<SceneOperations*> s;
			return s;
		}

		std::string toLowerAscii(std::string s)
		{
			for (char& ch : s)
			{
				ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
			}
			return s;
		}

		void findCustomShapePositionIndices(const CustomShapeData& customData, int& outPosXIdx, int& outPosYIdx,
											bool& outHasNamedParams)
		{
			outPosXIdx = -1;
			outPosYIdx = -1;
			outHasNamedParams = false;
			for (int p = 0; p < 8; ++p)
			{
				std::string nameLower = toLowerAscii(customData.parameterNames[p]);
				if (!nameLower.empty())
				{
					outHasNamedParams = true;
				}
				if (outPosXIdx == -1 && (nameLower == "posx" || nameLower == "pos x" || nameLower == "pos_x" ||
										 nameLower == "x" || nameLower == "centerx"))
				{
					outPosXIdx = p;
				}
				else if (outPosYIdx == -1 && (nameLower == "posy" || nameLower == "pos y" || nameLower == "pos_y" ||
											  nameLower == "y" || nameLower == "centery"))
				{
					outPosYIdx = p;
				}
			}
		}

		bool isFiniteVec2(vec2 v)
		{
			return std::isfinite(v.x) && std::isfinite(v.y);
		}

		float pointToBoxDistSq(vec2 p, vec2 boxMin, vec2 boxMax)
		{
			vec2 clamped = glm::clamp(p, boxMin, boxMax);
			vec2 diff = p - clamped;
			return diff.x * diff.x + diff.y * diff.y;
		}

		float pointToSegmentDistSq(vec2 p, vec2 a, vec2 b)
		{
			vec2 ab = b - a;
			float lenSq = ab.x * ab.x + ab.y * ab.y;
			if (lenSq <= 1e-12f)
			{
				vec2 d = p - a;
				return d.x * d.x + d.y * d.y;
			}
			float t = std::clamp(glm::dot(p - a, ab) / lenSq, 0.0f, 1.0f);
			vec2 proj = a + ab * t;
			vec2 d = p - proj;
			return d.x * d.x + d.y * d.y;
		}

		bool segmentIntersectsAABB(vec2 a, vec2 b, vec2 boxMin, vec2 boxMax)
		{
			vec2 dir = b - a;
			float tMin = 0.0f;
			float tMax = 1.0f;

			auto clipAxis = [&](float start, float delta, float mn, float mx) -> bool
			{
				if (std::abs(delta) <= 1e-8f)
				{
					return start >= mn && start <= mx;
				}
				float invD = 1.0f / delta;
				float t0 = (mn - start) * invD;
				float t1 = (mx - start) * invD;
				if (t0 > t1)
				{
					std::swap(t0, t1);
				}
				tMin = (std::max)(tMin, t0);
				tMax = (std::min)(tMax, t1);
				return tMin <= tMax;
			};

			return clipAxis(a.x, dir.x, boxMin.x, boxMax.x) && clipAxis(a.y, dir.y, boxMin.y, boxMax.y);
		}

		bool segmentIntersectsExpandedBox(vec2 a, vec2 b, vec2 boxMin, vec2 boxMax, float radius)
		{
			if (segmentIntersectsAABB(a, b, boxMin, boxMax))
			{
				return true;
			}
			const float rSq = (std::max)(0.0f, radius) * (std::max)(0.0f, radius);
			if (pointToBoxDistSq(a, boxMin, boxMax) <= rSq || pointToBoxDistSq(b, boxMin, boxMax) <= rSq)
			{
				return true;
			}
			const vec2 corners[4] = {
				vec2(boxMin.x, boxMin.y),
				vec2(boxMax.x, boxMin.y),
				vec2(boxMax.x, boxMax.y),
				vec2(boxMin.x, boxMax.y),
			};
			for (const vec2& corner : corners)
			{
				if (pointToSegmentDistSq(corner, a, b) <= rSq)
				{
					return true;
				}
			}
			return false;
		}

		bool isPointInAABB(vec2 p, vec2 boxMin, vec2 boxMax)
		{
			return p.x >= boxMin.x && p.x <= boxMax.x && p.y >= boxMin.y && p.y <= boxMax.y;
		}

		bool computeTextWorldBounds(const TextRenderer& tr, const Transform& t, vec2& outMin, vec2& outMax)
		{
			float hOff = 0.0f;
			switch (tr.horizontalAlignment)
			{
				case TextRenderer::HorizontalAlignment::Left:
					hOff = 0.0f;
					break;
				case TextRenderer::HorizontalAlignment::Center:
					hOff = -tr.width * 0.5f;
					break;
				case TextRenderer::HorizontalAlignment::Right:
					hOff = -tr.width;
					break;
			}

			float vOff = 0.0f;
			switch (tr.verticalAlignment)
			{
				case TextRenderer::VerticalAlignment::Bottom:
					vOff = 0.0f;
					break;
				case TextRenderer::VerticalAlignment::Center:
					vOff = -tr.height * 0.5f;
					break;
				case TextRenderer::VerticalAlignment::Top:
					vOff = -tr.height;
					break;
			}

			const float w = (std::max)(tr.width, 0.5f);
			const float h = (std::max)(tr.height, 0.5f);
			outMin = vec2(t.position.x + hOff, t.position.y + vOff);
			outMax = vec2(outMin.x + w, outMin.y + h);
			return isFiniteVec2(outMin) && isFiniteVec2(outMax);
		}

		template <typename ShapeClass>
		nlohmann::json serializeShapeFields(const ShapeClass& s, const ShapeService& shapeSvc)
		{
			nlohmann::json sj;
			sj["combination"] = static_cast<uint16_t>(s.combination);
			sj["parameters"] = std::vector<float>(s.parameters, s.parameters + 8);
			sj["layer"] = s.layer;
			sj["order"] = s.order;
			sj["material"] = s.material;
			sj["smoothFactor"] = s.smoothFactor;
			if constexpr (std::is_same_v<ShapeClass, Shape>)
			{
				sj["hasCollisions"] = s.hasCollisions;
			}
			if (const auto* customData = shapeSvc.getCustomShapeData(s.distanceFieldId))
			{
				sj["assetPath"] = customData->filePath;
			}
			else if (const char* builtinName = BuiltinShapes::toName(s.distanceFieldId))
			{
				sj["type"] = builtinName;
			}
			return sj;
		}

		template <typename TextClass> nlohmann::json serializeTextFields(const TextClass& t)
		{
			nlohmann::json tj;
			tj["text"] = std::string(t.text.data);
			tj["material"] = t.material;
			tj["width"] = t.width;
			tj["height"] = t.height;
			tj["verticalAlignment"] = static_cast<int>(t.verticalAlignment);
			tj["horizontalAlignment"] = static_cast<int>(t.horizontalAlignment);
			return tj;
		}

		nlohmann::json serializeEntityComponents(Registry& reg, ShapeService& shapeSvc, Entity e)
		{
			nlohmann::json comps = nlohmann::json::object();

			if (reg.hasComponent<Transform>(e))
			{
				const auto& t = reg.getComponent<Transform>(e);
				comps["transform"] = {
					{"position", {t.position.x, t.position.y, t.position.z}},
					{"rotation", {t.rotation.x, t.rotation.y, t.rotation.z}},
					{"scale", {t.scale.x, t.scale.y, t.scale.z}},
				};
			}
			if (reg.hasComponent<Shape>(e))
			{
				comps["shape"] = serializeShapeFields(reg.getComponent<Shape>(e), shapeSvc);
			}
			if (reg.hasComponent<UIShape>(e))
			{
				comps["uiShape"] = serializeShapeFields(reg.getComponent<UIShape>(e), shapeSvc);
			}
			if (reg.hasComponent<Dot>(e))
			{
				const auto& d = reg.getComponent<Dot>(e);
				comps["dot"] = {{"isStatic", d.isStatic}, {"materialId", d.materialId}};
			}
			if (reg.hasComponent<RigidBody2D>(e))
			{
				const auto& rb = reg.getComponent<RigidBody2D>(e);
				comps["rigidBody2D"] = {
					{"mass", rb.mass},
					{"type", static_cast<uint8_t>(rb.type)},
					{"isFixed", rb.isFixed},
					{"enableCollision", rb.enableCollision},
					{"enableShapeCollision", rb.enableShapeCollision},
				};
			}
			if (reg.hasComponent<TextRenderer>(e))
			{
				comps["textRenderer"] = serializeTextFields(reg.getComponent<TextRenderer>(e));
			}
			if (reg.hasComponent<UITextRenderer>(e))
			{
				comps["uiTextRenderer"] = serializeTextFields(reg.getComponent<UITextRenderer>(e));
			}
			if (reg.hasComponent<Light2DComponent>(e))
			{
				const auto& l = reg.getComponent<Light2DComponent>(e);
				comps["light2D"] = {
					{"type", static_cast<uint32_t>(l.type)},
					{"color", {l.color.r, l.color.g, l.color.b, l.color.a}},
					{"direction", {l.direction.x, l.direction.y}},
					{"radius", l.radius},
					{"coneAngle", l.coneAngle},
					{"conePenumbra", l.conePenumbra},
					{"castShadows", l.castShadows},
				};
			}
			if (reg.hasComponent<ShapeButton>(e))
			{
				const auto& b = reg.getComponent<ShapeButton>(e);
				comps["shapeButton"] = {
					{"mask", b.parameterModifierMask.to_ulong()},
					{"modifierAmount", b.modifierAmount},
					{"clickPadding", b.clickPadding},
					{"requireClickStart", b.requireClickStart},
				};
			}
			if (reg.hasComponent<ShapeToggle>(e))
			{
				const auto& tg = reg.getComponent<ShapeToggle>(e);
				comps["shapeToggle"] = {
					{"mask", tg.parameterModifierMask.to_ulong()},
					{"modifierAmount", tg.modifierAmount},
					{"active", tg.active},
					{"clickPadding", tg.clickPadding},
				};
			}
			if (reg.hasComponent<ECS::FlyMovement>(e))
			{
				const auto& fm = reg.getComponent<ECS::FlyMovement>(e);
				comps["flyMovement"] = {{"speed", fm.speed}, {"sensitivity", fm.sensitivity}};
			}
			if (reg.hasComponent<ECS::FlyMovement2D>(e))
			{
				const auto& fm2 = reg.getComponent<ECS::FlyMovement2D>(e);
				comps["flyMovement2D"] = {
					{"targetPosition", {fm2.targetPosition.x, fm2.targetPosition.y, fm2.targetPosition.z}},
					{"scrollSpeed", fm2.scrollSpeed},
					{"speed", fm2.speed},
					{"isSmooth", fm2.isSmooth},
				};
			}

			return comps;
		}

		template <typename TextClass> void deserializeTextFields(TextClass& t, const nlohmann::json& tj)
		{
			std::string str = tj.value("text", "");
			t.text = FixedString<32>(str.c_str());
			t.bufferedDotCount = 0;
			t.material = tj.value("material", static_cast<uint16_t>(0));
			t.width = tj.value("width", 0.0f);
			t.height = tj.value("height", 0.0f);
			t.verticalAlignment = static_cast<TextRenderer::VerticalAlignment>(
				tj.value("verticalAlignment", static_cast<int>(TextRenderer::VerticalAlignment::Bottom)));
			t.horizontalAlignment = static_cast<TextRenderer::HorizontalAlignment>(
				tj.value("horizontalAlignment", static_cast<int>(TextRenderer::HorizontalAlignment::Left)));
		}

		void offsetSerializedShapeFields(Shape& s, ShapeService& shapeSvc, vec2 deltaOffset)
		{
			if (s.distanceFieldId == DefaultShapes::Line)
			{
				s.parameters[DefaultShapes::Line::StartX] += deltaOffset.x;
				s.parameters[DefaultShapes::Line::StartY] += deltaOffset.y;
				s.parameters[DefaultShapes::Line::EndX] += deltaOffset.x;
				s.parameters[DefaultShapes::Line::EndY] += deltaOffset.y;
				return;
			}
			if (s.distanceFieldId == DefaultShapes::SineWave)
			{
				s.parameters[DefaultShapes::SineWave::Offset] += deltaOffset.y;
				return;
			}
			if (const auto* customData = shapeSvc.getCustomShapeData(s.distanceFieldId))
			{
				int posXIdx = -1;
				int posYIdx = -1;
				bool hasNamed = false;
				findCustomShapePositionIndices(*customData, posXIdx, posYIdx, hasNamed);
				if (posXIdx != -1 && posYIdx != -1)
				{
					s.parameters[posXIdx] += deltaOffset.x;
					s.parameters[posYIdx] += deltaOffset.y;
					return;
				}
				if (hasNamed)
				{
					return;
				}
			}
			s.parameters[0] += deltaOffset.x;
			s.parameters[1] += deltaOffset.y;
		}

		void applySerializedComponents(Registry& reg, ShapeService& shapeSvc, Simulation2D& sim, Entity e,
									   const nlohmann::json& comps, vec2 deltaOffset, bool overwriteExisting)
		{
			if (comps.contains("transform"))
			{
				const auto& tj = comps["transform"];
				auto& t =
					reg.hasComponent<Transform>(e) ? reg.getComponent<Transform>(e) : reg.addComponent<Transform>(e);
				if (tj.contains("position") && tj["position"].is_array() && tj["position"].size() >= 3)
				{
					t.position = vec3(tj["position"][0].get<float>() + deltaOffset.x,
									  tj["position"][1].get<float>() + deltaOffset.y, tj["position"][2].get<float>());
				}
				if (tj.contains("rotation") && tj["rotation"].is_array() && tj["rotation"].size() >= 3)
				{
					t.rotation = vec3(tj["rotation"][0].get<float>(), tj["rotation"][1].get<float>(),
									  tj["rotation"][2].get<float>());
				}
				if (tj.contains("scale") && tj["scale"].is_array() && tj["scale"].size() >= 3)
				{
					t.scale =
						vec3(tj["scale"][0].get<float>(), tj["scale"][1].get<float>(), tj["scale"][2].get<float>());
				}
				reg.setComponentDirty(t);
			}

			if (comps.contains("shape"))
			{
				const auto& sj = comps["shape"];
				const bool existed = reg.hasComponent<Shape>(e);
				const uint16_t layer = sj.value("layer", static_cast<uint16_t>(0));
				const int32_t nextOrder = ShapeService::computeNextOrder<Shape>(reg, layer);
				auto& s = existed ? reg.getComponent<Shape>(e) : reg.addComponent<Shape>(e);
				ShapeId dfId = 0;
				std::string assetPath = sj.value("assetPath", "");
				if (!assetPath.empty())
				{
					ShapeId loadedId = shapeSvc.loadCustomShape(assetPath);
					if (loadedId != 0)
					{
						dfId = loadedId;
					}
				}
				else if (sj.contains("type") && sj["type"].is_string())
				{
					BuiltinShapes::fromName(sj["type"].get<std::string>(), dfId);
				}
				s.distanceFieldId = dfId;
				s.combination = static_cast<CombinationType>(
					sj.value("combination", static_cast<uint16_t>(CombinationType::Addition)));
				s.layer = layer;
				s.order = overwriteExisting ? sj.value("order", nextOrder) : nextOrder;
				s.material = sj.value("material", static_cast<uint16_t>(1));
				s.smoothFactor = sj.value("smoothFactor", 1.0f);
				s.hasCollisions = sj.value("hasCollisions", true);
				if (sj.contains("parameters") && sj["parameters"].is_array())
				{
					for (size_t i = 0; i < (std::min<size_t>)(8, sj["parameters"].size()); ++i)
					{
						s.parameters[i] = sj["parameters"][i].get<float>();
					}
				}
				if (deltaOffset.x != 0.0f || deltaOffset.y != 0.0f)
				{
					offsetSerializedShapeFields(s, shapeSvc, deltaOffset);
				}
				reg.setComponentDirty(s);
			}

			if (comps.contains("uiShape"))
			{
				const auto& sj = comps["uiShape"];
				const bool existed = reg.hasComponent<UIShape>(e);
				const uint16_t layer = sj.value("layer", static_cast<uint16_t>(0));
				const int32_t nextOrder = ShapeService::computeNextOrder<UIShape>(reg, layer);
				auto& s = existed ? reg.getComponent<UIShape>(e) : reg.addComponent<UIShape>(e);
				ShapeId dfId = 0;
				std::string assetPath = sj.value("assetPath", "");
				if (!assetPath.empty())
				{
					ShapeId loadedId = shapeSvc.loadCustomShape(assetPath);
					if (loadedId != 0)
					{
						dfId = loadedId;
					}
				}
				else if (sj.contains("type") && sj["type"].is_string())
				{
					BuiltinShapes::fromName(sj["type"].get<std::string>(), dfId);
				}
				s.distanceFieldId = dfId;
				s.combination = static_cast<CombinationType>(
					sj.value("combination", static_cast<uint16_t>(CombinationType::Addition)));
				s.layer = layer;
				s.order = overwriteExisting ? sj.value("order", nextOrder) : nextOrder;
				s.material = sj.value("material", static_cast<uint16_t>(0));
				s.smoothFactor = sj.value("smoothFactor", 10.0f);
				if (sj.contains("parameters") && sj["parameters"].is_array())
				{
					for (size_t i = 0; i < (std::min<size_t>)(8, sj["parameters"].size()); ++i)
					{
						s.parameters[i] = sj["parameters"][i].get<float>();
					}
				}
				reg.setComponentDirty(s);
			}

			if (comps.contains("dot"))
			{
				const auto& dj = comps["dot"];
				auto& d = reg.hasComponent<Dot>(e) ? reg.getComponent<Dot>(e) : reg.addComponent<Dot>(e);
				d.isStatic = dj.value("isStatic", false);
				d.materialId = dj.value("materialId", 0u);
				reg.setComponentDirty(d);
			}

			if (comps.contains("rigidBody2D"))
			{
				const auto& rbj = comps["rigidBody2D"];
				bool hadRb = reg.hasComponent<RigidBody2D>(e);
				auto& rb = hadRb ? reg.getComponent<RigidBody2D>(e) : reg.addComponent<RigidBody2D>(e);
				rb.mass = rbj.value("mass", 1.0f);
				rb.type = static_cast<BodyType>(rbj.value("type", static_cast<uint8_t>(BodyType::Dynamic)));
				rb.isFixed = (rb.type == BodyType::Fixed || rbj.value("isFixed", false));
				rb.enableCollision = rbj.value("enableCollision", true);
				rb.enableShapeCollision = rbj.value("enableShapeCollision", true);
				rb.velocity = vec2(0.0f);
				rb.pendingImpulseForce = vec2(0.0f);
				rb.pendingContinuousForce = vec2(0.0f);
				reg.setComponentDirty(rb);
				if ((overwriteExisting || hadRb) && rb.simulationId != INVALID_SIMULATION_ID &&
					reg.hasComponent<Transform>(e))
				{
					vec2 pos(reg.getComponent<Transform>(e).position);
					sim.setPosition(rb.simulationId, pos);
					sim.setVelocity(rb.simulationId, vec2(0.0f));
				}
			}

			if (comps.contains("textRenderer"))
			{
				auto& tr = reg.hasComponent<TextRenderer>(e) ? reg.getComponent<TextRenderer>(e)
															 : reg.addComponent<TextRenderer>(e);
				deserializeTextFields(tr, comps["textRenderer"]);
				reg.setComponentDirty(tr);
			}

			if (comps.contains("uiTextRenderer"))
			{
				auto& utr = reg.hasComponent<UITextRenderer>(e) ? reg.getComponent<UITextRenderer>(e)
																: reg.addComponent<UITextRenderer>(e);
				deserializeTextFields(utr, comps["uiTextRenderer"]);
				reg.setComponentDirty(utr);
			}

			if (comps.contains("light2D"))
			{
				const auto& lj = comps["light2D"];
				auto& l = reg.hasComponent<Light2DComponent>(e) ? reg.getComponent<Light2DComponent>(e)
																: reg.addComponent<Light2DComponent>(e);
				l.type = static_cast<Light2DType>(lj.value("type", static_cast<uint32_t>(Light2DType::Directional)));
				if (lj.contains("color") && lj["color"].is_array() && lj["color"].size() >= 4)
				{
					l.color = vec4(lj["color"][0].get<float>(), lj["color"][1].get<float>(),
								   lj["color"][2].get<float>(), lj["color"][3].get<float>());
				}
				if (lj.contains("direction") && lj["direction"].is_array() && lj["direction"].size() >= 2)
				{
					l.direction = vec2(lj["direction"][0].get<float>(), lj["direction"][1].get<float>());
				}
				l.radius = lj.value("radius", 10.0f);
				l.coneAngle = lj.value("coneAngle", 45.0f);
				l.conePenumbra = lj.value("conePenumbra", 10.0f);
				l.castShadows = lj.value("castShadows", true);
				reg.setComponentDirty(l);
			}

			if (comps.contains("shapeButton"))
			{
				const auto& bj = comps["shapeButton"];
				auto& b = reg.hasComponent<ShapeButton>(e) ? reg.getComponent<ShapeButton>(e)
														   : reg.addComponent<ShapeButton>(e);
				b.state = ButtonState::Off;
				b.hovered = false;
				b.parameterModifierMask = std::bitset<8>(bj.value("mask", 0UL));
				b.modifierAmount = bj.value("modifierAmount", 0.0f);
				b.clickPadding = bj.value("clickPadding", 10.0f);
				b.requireClickStart = bj.value("requireClickStart", true);
				reg.setComponentDirty(b);
			}

			if (comps.contains("shapeToggle"))
			{
				const auto& tgj = comps["shapeToggle"];
				auto& tg = reg.hasComponent<ShapeToggle>(e) ? reg.getComponent<ShapeToggle>(e)
															: reg.addComponent<ShapeToggle>(e);
				tg.state = ButtonState::Off;
				tg.hovered = false;
				tg.parameterModifierMask = std::bitset<8>(tgj.value("mask", 0UL));
				tg.modifierAmount = tgj.value("modifierAmount", 0.0f);
				tg.active = tgj.value("active", false);
				tg.clickPadding = tgj.value("clickPadding", 10.0f);
				reg.setComponentDirty(tg);
			}

			if (comps.contains("flyMovement"))
			{
				const auto& fmj = comps["flyMovement"];
				auto& fm = reg.hasComponent<ECS::FlyMovement>(e) ? reg.getComponent<ECS::FlyMovement>(e)
																 : reg.addComponent<ECS::FlyMovement>(e);
				fm.speed = fmj.value("speed", 1.0f);
				fm.sensitivity = fmj.value("sensitivity", 0.12f);
				reg.setComponentDirty(fm);
			}

			if (comps.contains("flyMovement2D"))
			{
				const auto& fm2j = comps["flyMovement2D"];
				auto& fm2 = reg.hasComponent<ECS::FlyMovement2D>(e) ? reg.getComponent<ECS::FlyMovement2D>(e)
																	: reg.addComponent<ECS::FlyMovement2D>(e);
				if (fm2j.contains("targetPosition") && fm2j["targetPosition"].is_array() &&
					fm2j["targetPosition"].size() >= 3)
				{
					fm2.targetPosition = vec3(fm2j["targetPosition"][0].get<float>() + deltaOffset.x,
											  fm2j["targetPosition"][1].get<float>() + deltaOffset.y,
											  fm2j["targetPosition"][2].get<float>());
				}
				fm2.scrollSpeed = fm2j.value("scrollSpeed", 5.0f);
				fm2.speed = fm2j.value("speed", 1.0f);
				fm2.isSmooth = fm2j.value("isSmooth", true);
				reg.setComponentDirty(fm2);
			}
		}
	} // namespace

	SceneOperations::SceneOperations(EditorContext& context)
		: m_ctx(context)
	{
		std::lock_guard<std::mutex> lock(liveSceneOpsMutex());
		liveSceneOps().insert(this);
	}

	SceneOperations::~SceneOperations()
	{
		std::lock_guard<std::mutex> lock(liveSceneOpsMutex());
		liveSceneOps().erase(this);
	}
	// =====================================================================
	// Scene File I/O
	// =====================================================================

	void SceneOperations::newScene()
	{
		resetSceneContents();
		resetUndoBaseline(false);
		Logger::log("[SceneEditor] Cleared scene.");
	}

	void SceneOperations::resetSceneContents()
	{
		Entity camEntity = m_ctx.services->render().getCameraEntity();

		// 1. Destroy all non-camera entities
		for (Entity e = 0; e < m_ctx.registry->getEntityCount(); ++e)
		{
			if (e != camEntity && !m_ctx.registry->isEntityDestroyed(e))
			{
				m_ctx.registry->destroyEntity(e);
			}
		}

		// 2. Clear all names and tags
		m_ctx.services->names().clear();
		m_ctx.services->tags().clear();

		// 3. Free removed component arrays and entities immediately
		m_ctx.registry->freeRemovedComponents();

		// 4. Reset physics bodies, particles, and shapes (keep resumed so renderer simulation time stays active)
		m_ctx.simulation2D->resetBodiesAndShapes();
		m_ctx.simulation2D->resume();
		m_ctx.simulation2D->setGravity(0.0f);
		m_ctx.simulation2D->setDamping(1.0f);
		m_ctx.isSimulating = false;
		m_ctx.hasSnapshot = false;

		// 5. Clear clusters
		m_ctx.services->clusters().clear();
		m_ctx.clusterRestData.clear();
		m_ctx.clusterAssetWriteTimes.clear();

		// 6. Reset materials
		for (size_t i = 0; i < 16; ++i)
		{
			auto& mat = m_ctx.services->materials2D().get(static_cast<uint16_t>(i));
			mat.id = static_cast<uint16_t>(i);
			mat.name = (i == 0) ? "default" : ("mat_" + std::to_string(i));
			mat.color = (i < ColorPalette::Default.size()) ? vec4(ColorPalette::Default[i], 1.0f) : vec4(1.0f);
			mat.secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
			mat.pattern = Pattern2D::None;
			mat.patternScale = 1.0f;
		}

		// 7. Update render contexts
		m_ctx.worldRenderContext->shapesNeedUpdate = true;
		m_ctx.uiRenderContext->shapesNeedUpdate = true;

		// 8. Reset editor selection and state
		m_ctx.deselectAll();
		m_ctx.groups.clear();
		m_ctx.nextGroupId = 1;
		m_ctx.clusterRestData.clear();
		m_ctx.currentScenePath.clear();
		m_ctx.isSceneDirty = false;
		m_ctx.sceneUpdatedFromAssets = false;
		m_ctx.hasShiftAnchor = false;
		m_ctx.hasPendingShift = false;
		m_ctx.renamingGroupId = 0;
		m_ctx.layerEditEntity = INVALID_ENTITY;
		m_ctx.layerEditValue = 0;
		m_ctx.multiSelectShapeLayer = 0;
	}

	void SceneOperations::openLoadSceneDialog()
	{
		static SDL_DialogFileFilter filters[1] = {{"Weird Scene (*.weirdscene)", "weirdscene"}};
		std::string defaultPath = m_ctx.project.isOpen() ? m_ctx.project.getAssetsPath().string() : "";
		SDL_ShowOpenFileDialog(onOpenSceneCallback, this, nullptr, filters, 1,
							   defaultPath.empty() ? nullptr : defaultPath.c_str(), false);
	}

	void SceneOperations::openSaveSceneDialog()
	{
		static SDL_DialogFileFilter filters[1] = {{"Weird Scene (*.weirdscene)", "weirdscene"}};
		std::string defaultPath = m_ctx.currentScenePath;
		if (defaultPath.empty())
		{
			defaultPath = m_ctx.project.isOpen() ? (m_ctx.project.getAssetsPath() / "scene.weirdscene").string()
												 : "scene.weirdscene";
		}
		SDL_ShowSaveFileDialog(onSaveSceneCallback, this, nullptr, filters, 1, defaultPath.c_str());
	}

	void SceneOperations::saveScenePrompt()
	{
		if (!m_ctx.currentScenePath.empty())
		{
			saveScene(m_ctx.currentScenePath);
		}
		else
		{
			openSaveSceneDialog();
		}
	}

	void SceneOperations::loadScene(const std::string& path)
	{
		resetSceneContents();
		if (m_ctx.scene)
		{
			SceneSerializer::load(reinterpret_cast<Scene&>(*m_ctx.scene), path);
		}
		syncAllConstraintsClusterMembership();
		m_ctx.currentScenePath = path;
		m_ctx.isSceneDirty = false;
		m_ctx.sceneUpdatedFromAssets = false;

		// Restore editor groups from scene file before any cluster auto-reloads so entity remapping updates them
		// cleanly
		try
		{
			std::ifstream inFile(path);
			if (inFile.is_open())
			{
				nlohmann::json j;
				inFile >> j;
				inFile.close();

				if (j.contains("editorGroups") && j["editorGroups"].is_array())
				{
					for (const auto& gj : j["editorGroups"])
					{
						EditorGroup g;
						g.id = gj.value("id", m_ctx.nextGroupId);
						g.name = gj.value("name", "Group " + std::to_string(g.id));
						if (gj.contains("entities") && gj["entities"].is_array())
						{
							for (const auto& ej : gj["entities"])
							{
								Entity e = static_cast<Entity>(ej.get<uint32_t>());
								if (m_ctx.registry->isEntityAlive(e))
								{
									g.entities.push_back(e);
								}
							}
						}
						if (gj.contains("clusterIds") && gj["clusterIds"].is_array())
						{
							for (const auto& cj : gj["clusterIds"])
							{
								uint32_t cId = cj.get<uint32_t>();
								if (m_ctx.services->clusters().getById(cId))
								{
									g.clusterIds.push_back(cId);
								}
							}
						}
						m_ctx.nextGroupId = std::max(m_ctx.nextGroupId, g.id + 1);
						m_ctx.groups.push_back(std::move(g));
					}
				}
			}
		}
		catch (const std::exception& ex)
		{
			Logger::log("[SceneEditor] Error loading editor groups: " + std::string(ex.what()));
		}

		// Check if any cluster instance's asset file on disk has been modified since the scene was saved
		std::error_code sceneTimeEc;
		auto sceneWriteTime = std::filesystem::last_write_time(path, sceneTimeEc);
		std::vector<uint32_t> clustersToAutoReload;
		for (const auto& c : m_ctx.services->clusters().getAll())
		{
			if (c.assetPath.empty())
			{
				continue;
			}
			std::filesystem::path resolved = resolveClusterPath(c.assetPath);
			std::error_code ec;
			if (std::filesystem::exists(resolved, ec) && !ec)
			{
				auto assetWriteTime = std::filesystem::last_write_time(resolved, ec);
				if (!ec)
				{
					std::string normPath = ShapeService::normalizePath(resolved.string());
					m_ctx.clusterAssetWriteTimes[normPath] = assetWriteTime;
					if (!sceneTimeEc && assetWriteTime > sceneWriteTime)
					{
						clustersToAutoReload.push_back(c.id);
					}
				}
			}
		}
		for (uint32_t cId : clustersToAutoReload)
		{
			if (reloadCluster(cId))
			{
				m_ctx.sceneUpdatedFromAssets = true;
			}
		}

		resetUndoBaseline(m_ctx.isSceneDirty);
		Logger::log("[SceneEditor] Loaded scene: " + path);
		ToolsSession::getInstance().addRecentScene(path);
	}

	void SceneOperations::saveScene(const std::string& path)
	{
		if (m_ctx.isSimulating)
		{
			toggleSimulation();
		}

		if (m_ctx.services && m_ctx.scene && hasPendingEditorEdit())
		{
			syncUndoStateIfChanged();
		}

		std::string finalPath = path;
		if (!finalPath.ends_with(".weirdscene"))
		{
			finalPath += ".weirdscene";
		}

		m_ctx.services->serialization().saveScene(finalPath);
		m_ctx.currentScenePath = finalPath;
		m_ctx.isSceneDirty = false;
		m_ctx.sceneUpdatedFromAssets = false;
		m_ctx.project.scanAssets();

		// Persist editor groups in scene file
		try
		{
			std::ifstream inFile(finalPath);
			if (inFile.is_open())
			{
				nlohmann::json j;
				inFile >> j;
				inFile.close();

				nlohmann::json groupsJson = nlohmann::json::array();
				for (const auto& g : m_ctx.groups)
				{
					nlohmann::json gj;
					gj["id"] = g.id;
					gj["name"] = g.name;
					gj["entities"] = nlohmann::json::array();
					for (Entity e : g.entities)
					{
						gj["entities"].push_back(static_cast<uint32_t>(e));
					}
					gj["clusterIds"] = g.clusterIds;
					groupsJson.push_back(gj);
				}
				j["editorGroups"] = groupsJson;

				std::ofstream outFile(finalPath);
				if (outFile.is_open())
				{
					outFile << j.dump(4);
					outFile.close();
				}
			}
		}
		catch (const std::exception& ex)
		{
			Logger::log("[SceneEditor] Error saving editor groups: " + std::string(ex.what()));
		}

		if (m_ctx.services && m_ctx.scene)
		{
			flushDesignPhysicsSync();
			m_committedState = captureUndoState();
			m_hasCommittedState = true;
			m_undoHistory.markClean();
		}
		else
		{
			m_undoHistory.markClean();
		}
		m_ctx.isSceneDirty = false;
		m_pendingUndoSync = false;
		m_lastSyncedEditCount = m_ctx.isSceneDirty.editCount;

		Logger::log("[SceneEditor] Saved scene to " + finalPath);
		ToolsSession::getInstance().addRecentScene(finalPath);
	}

	void SceneOperations::confirmIfDirty(std::function<void()> action)
	{
		if (m_ctx.services && m_ctx.scene && !m_ctx.isSimulating && !m_ctx.isDragging && hasPendingEditorEdit())
		{
			syncUndoStateIfChanged();
		}
		std::string docName = !m_ctx.currentScenePath.empty()
								  ? std::filesystem::path(m_ctx.currentScenePath).filename().string()
								  : "Untitled Scene";
		std::string customMessage;
		if (m_ctx.sceneUpdatedFromAssets)
		{
			customMessage = "Cluster assets referenced by \"" + docName +
							"\" were updated since the scene was last saved.\nSave the updated scene before leaving?";
		}
		m_ctx.unsavedPrompt.request(m_ctx.isSceneDirty, std::move(docName), std::move(action),
									std::move(customMessage));
	}

	void SceneOperations::requestNewScene()
	{
		confirmIfDirty([this]() { newScene(); });
	}

	void SceneOperations::requestOpenLoadSceneDialog()
	{
		confirmIfDirty([this]() { openLoadSceneDialog(); });
	}

	void SceneOperations::requestLoadScene(const std::string& path)
	{
		confirmIfDirty([this, path]() { loadScene(path); });
	}

	void SceneOperations::renderUnsavedChangesModal()
	{
		m_ctx.unsavedPrompt.render(
			[this]() -> bool
			{
				if (m_ctx.isSimulating)
				{
					toggleSimulation();
				}
				if (!m_ctx.currentScenePath.empty())
				{
					saveScene(m_ctx.currentScenePath);
					return !m_ctx.isSceneDirty;
				}
				openSaveSceneDialog();
				return false;
			},
			[this]()
			{
				if (m_ctx.isSimulating)
				{
					toggleSimulation();
				}
				m_ctx.isSceneDirty = false;
				m_ctx.sceneUpdatedFromAssets = false;
				m_undoHistory.markClean();
			});
	}

	void SceneOperations::pollFileActions()
	{
		std::string loadPath;
		std::string savePath;
		bool saveCancelled = false;
		{
			std::lock_guard<std::mutex> lock(m_ctx.fileMutex);
			loadPath = std::move(m_ctx.pendingLoadScenePath);
			savePath = std::move(m_ctx.pendingSaveScenePath);
			saveCancelled = m_ctx.pendingSaveSceneCancelled;
			m_ctx.pendingLoadScenePath.clear();
			m_ctx.pendingSaveScenePath.clear();
			m_ctx.pendingSaveSceneCancelled = false;
		}

		if (!loadPath.empty())
		{
			loadScene(loadPath);
		}
		if (!savePath.empty())
		{
			saveScene(savePath);
			m_ctx.unsavedPrompt.notifySaveFinished(!m_ctx.isSceneDirty);
		}
		else if (saveCancelled)
		{
			m_ctx.unsavedPrompt.notifySaveFinished(false);
		}
	}

	void SDLCALL SceneOperations::onOpenSceneCallback(void* userdata, const char* const* filelist, int /*filter*/)
	{
		auto* self = static_cast<SceneOperations*>(userdata);
		if (!self || !filelist || !*filelist)
		{
			return;
		}
		std::lock_guard<std::mutex> liveLock(liveSceneOpsMutex());
		if (liveSceneOps().count(self) > 0)
		{
			std::lock_guard<std::mutex> lock(self->m_ctx.fileMutex);
			self->m_ctx.pendingLoadScenePath = *filelist;
		}
	}

	void SDLCALL SceneOperations::onSaveSceneCallback(void* userdata, const char* const* filelist, int /*filter*/)
	{
		auto* self = static_cast<SceneOperations*>(userdata);
		if (!self)
		{
			return;
		}
		std::lock_guard<std::mutex> liveLock(liveSceneOpsMutex());
		if (liveSceneOps().count(self) > 0)
		{
			std::lock_guard<std::mutex> lock(self->m_ctx.fileMutex);
			if (!filelist || !*filelist)
			{
				self->m_ctx.pendingSaveSceneCancelled = true;
			}
			else
			{
				self->m_ctx.pendingSaveScenePath = *filelist;
				self->m_ctx.pendingSaveSceneCancelled = false;
			}
		}
	}

	// =====================================================================
	// Spawning
	// =====================================================================

	Entity SceneOperations::spawnDefaultShape(uint16_t shapeType, vec2 pos)
	{
		Entity e = m_ctx.registry->createEntity();
		const int32_t nextOrder = ShapeService::computeNextOrder<Shape>(*m_ctx.registry, 0);

		auto& s = m_ctx.registry->addComponent<Shape>(e);
		s.distanceFieldId = shapeType;
		s.combination = CombinationType::Addition;
		s.hasCollisions = true;
		s.layer = 0;
		s.order = nextOrder;
		s.material = 1;
		s.smoothFactor = 1.0f;

		// Defaults for canonical 2D shapes
		if (shapeType == DefaultShapes::Circle)
		{
			s.parameters[DefaultShapes::Circle::PosX] = pos.x;
			s.parameters[DefaultShapes::Circle::PosY] = pos.y;
			s.parameters[DefaultShapes::Circle::Radius] = 1.5f;
		}
		else if (shapeType == DefaultShapes::Box)
		{
			s.parameters[DefaultShapes::Box::PosX] = pos.x;
			s.parameters[DefaultShapes::Box::PosY] = pos.y;
			s.parameters[DefaultShapes::Box::SizeX] = 2.0f;
			s.parameters[DefaultShapes::Box::SizeY] = 1.0f;
		}
		else if (shapeType == DefaultShapes::Triangle)
		{
			s.parameters[DefaultShapes::Triangle::PosX] = pos.x;
			s.parameters[DefaultShapes::Triangle::PosY] = pos.y;
			s.parameters[DefaultShapes::Triangle::Width] = 2.0f;
			s.parameters[DefaultShapes::Triangle::Height] = 2.0f;
		}
		else if (shapeType == DefaultShapes::Line)
		{
			s.parameters[DefaultShapes::Line::StartX] = pos.x - 1.5f;
			s.parameters[DefaultShapes::Line::StartY] = pos.y;
			s.parameters[DefaultShapes::Line::EndX] = pos.x + 1.5f;
			s.parameters[DefaultShapes::Line::EndY] = pos.y;
			s.parameters[DefaultShapes::Line::Width] = 0.15f;
		}
		else if (shapeType == DefaultShapes::Ramp)
		{
			s.parameters[DefaultShapes::Ramp::PosX] = pos.x;
			s.parameters[DefaultShapes::Ramp::PosY] = pos.y;
			s.parameters[DefaultShapes::Ramp::Width] = 3.0f;
			s.parameters[DefaultShapes::Ramp::Height] = 1.5f;
			s.parameters[DefaultShapes::Ramp::Skew] = 0.5f;
		}
		else if (shapeType == DefaultShapes::Star)
		{
			s.parameters[DefaultShapes::Star::PosX] = pos.x;
			s.parameters[DefaultShapes::Star::PosY] = pos.y;
			s.parameters[DefaultShapes::Star::Radius] = 1.5f;
			s.parameters[DefaultShapes::Star::Displacement] = 0.5f;
			s.parameters[DefaultShapes::Star::Points] = 5.0f;
			s.parameters[DefaultShapes::Star::Speed] = 0.0f;
		}
		else if (shapeType == DefaultShapes::SineWave)
		{
			s.parameters[DefaultShapes::SineWave::Amplitude] = 1.0f;
			s.parameters[DefaultShapes::SineWave::Period] = 1.0f;
			s.parameters[DefaultShapes::SineWave::Speed] = 0.0f;
			s.parameters[DefaultShapes::SineWave::Offset] = pos.y;
		}
		else
		{
			s.parameters[0] = pos.x;
			s.parameters[1] = pos.y;
		}

		m_ctx.registry->setComponentDirty(s);
		m_ctx.worldRenderContext->shapesNeedUpdate = true;

		selectEntity(e, false);
		m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(e)};
		m_ctx.hasShiftAnchor = true;
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
		return e;
	}

	Entity SceneOperations::spawnCustomShape(const std::string& assetPath, vec2 pos)
	{
		ShapeId id = m_ctx.services->shapes().loadCustomShape(assetPath);
		Entity e = m_ctx.registry->createEntity();
		const int32_t nextOrder = ShapeService::computeNextOrder<Shape>(*m_ctx.registry, 0);

		auto& s = m_ctx.registry->addComponent<Shape>(e);
		s.distanceFieldId = id;
		s.combination = CombinationType::Addition;
		s.hasCollisions = true;
		s.layer = 0;
		s.order = nextOrder;
		s.material = 2;
		s.smoothFactor = 1.0f;
		if (const auto* customData = m_ctx.services->shapes().getCustomShapeData(id))
		{
			std::copy(customData->defaultParameters.begin(), customData->defaultParameters.end(), s.parameters);
		}

		if (pos.x != 0.0f || pos.y != 0.0f)
		{
			offsetShapeParameters(s, pos);
		}

		m_ctx.registry->setComponentDirty(s);
		m_ctx.worldRenderContext->shapesNeedUpdate = true;

		std::filesystem::path p(assetPath);
		std::string baseName = p.stem().string();
		if (!baseName.empty())
		{
			std::string name = baseName + "_" + std::to_string(static_cast<uint32_t>(e));
			m_ctx.services->names().setName(e, name);
		}

		selectEntity(e, false);
		m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(e)};
		m_ctx.hasShiftAnchor = true;
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
		return e;
	}

	void SceneOperations::spawnCluster(const std::string& assetPath, vec2 pos)
	{
		auto& cluster = m_ctx.services->clusters().instantiate(assetPath, pos);
		syncAllConstraintsClusterMembership();
		selectCluster(cluster.id, false);
		m_ctx.shiftAnchorItem = {HierarchyItemType::Cluster, cluster.id};
		m_ctx.hasShiftAnchor = true;
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;

		std::filesystem::path resolved = resolveClusterPath(assetPath);
		std::error_code ec;
		if (std::filesystem::exists(resolved, ec))
		{
			auto writeTime = std::filesystem::last_write_time(resolved, ec);
			if (!ec)
			{
				m_ctx.clusterAssetWriteTimes[ShapeService::normalizePath(resolved.string())] = writeTime;
			}
		}
	}

	vec2 SceneOperations::getCameraCenterWorld() const
	{
		auto& cam = m_ctx.registry->getComponent<Transform>(m_ctx.services->render().getCameraEntity());
		return vec2(cam.position.x, cam.position.y);
	}

	// =====================================================================
	// Entity / Shape manipulation
	// =====================================================================

	void SceneOperations::offsetShapeParameters(Shape& s, vec2 delta)
	{
		offsetSerializedShapeFields(s, m_ctx.services->shapes(), delta);
	}

	vec2 SceneOperations::getShapePosition(const Shape& s, Entity e) const
	{
		if (s.distanceFieldId == DefaultShapes::Line)
		{
			return vec2((s.parameters[DefaultShapes::Line::StartX] + s.parameters[DefaultShapes::Line::EndX]) * 0.5f,
						(s.parameters[DefaultShapes::Line::StartY] + s.parameters[DefaultShapes::Line::EndY]) * 0.5f);
		}

		if (s.distanceFieldId == DefaultShapes::SineWave)
		{
			float x = m_ctx.registry->hasComponent<Transform>(e) ? m_ctx.registry->getComponent<Transform>(e).position.x
																 : 0.0f;
			return vec2(x, s.parameters[DefaultShapes::SineWave::Offset]);
		}

		const auto* customData = m_ctx.services->shapes().getCustomShapeData(s.distanceFieldId);
		if (customData)
		{
			int posXIdx = -1;
			int posYIdx = -1;
			bool hasNamed = false;
			findCustomShapePositionIndices(*customData, posXIdx, posYIdx, hasNamed);
			if (posXIdx != -1 && posYIdx != -1)
			{
				return vec2(s.parameters[posXIdx], s.parameters[posYIdx]);
			}
			if (hasNamed && m_ctx.registry->hasComponent<Transform>(e))
			{
				return vec2(m_ctx.registry->getComponent<Transform>(e).position);
			}
		}

		if (s.distanceFieldId < DefaultShapes::COUNT_2D)
		{
			return vec2(s.parameters[0], s.parameters[1]);
		}

		if (m_ctx.registry->hasComponent<Transform>(e))
		{
			return vec2(m_ctx.registry->getComponent<Transform>(e).position);
		}

		return vec2(s.parameters[0], s.parameters[1]);
	}

	vec2 SceneOperations::getEntityPosition(Entity e) const
	{
		if (m_ctx.registry->hasComponent<Shape>(e))
		{
			return getShapePosition(m_ctx.registry->getComponent<Shape>(e), e);
		}
		if (m_ctx.registry->hasComponent<Transform>(e))
		{
			return vec2(m_ctx.registry->getComponent<Transform>(e).position);
		}
		return vec2(0.0f);
	}

	void SceneOperations::setEntityPosition(Entity e, vec2 newPos)
	{
		vec2 currentPos = getEntityPosition(e);
		vec2 delta = newPos - currentPos;

		if (m_ctx.registry->hasComponent<Transform>(e))
		{
			auto& t = m_ctx.registry->getComponent<Transform>(e);
			t.position.x = newPos.x;
			t.position.y = newPos.y;
			m_ctx.registry->setComponentDirty(t);
			if (m_ctx.registry->hasComponent<RigidBody2D>(e))
			{
				auto& rb = m_ctx.registry->getComponent<RigidBody2D>(e);
				m_ctx.simulation2D->setPosition(rb.simulationId, newPos);
				m_ctx.simulation2D->setVelocity(rb.simulationId, vec2(0.0f));
				rb.velocity = vec2(0.0f);
				m_ctx.registry->setComponentDirty(rb);
			}
		}
		if (m_ctx.registry->hasComponent<Shape>(e))
		{
			auto& s = m_ctx.registry->getComponent<Shape>(e);
			offsetShapeParameters(s, delta);
			m_ctx.registry->setComponentDirty(s);
		}
		if (m_ctx.services)
		{
			m_ctx.services->clusters().markEntityOverride(e, ClusterOverrideFlag::Position);
		}
	}

	vec2 SceneOperations::getLineEndpoint(Entity e, int endpointIndex) const
	{
		if (e == INVALID_ENTITY || m_ctx.registry == nullptr || m_ctx.registry->isEntityDestroyed(e) ||
			!m_ctx.registry->hasComponent<Shape>(e))
		{
			return vec2(0.0f);
		}
		const auto& s = m_ctx.registry->getComponent<Shape>(e);
		if (s.distanceFieldId != DefaultShapes::Line)
		{
			return getShapePosition(s, e);
		}
		if (endpointIndex == 0)
		{
			return vec2(s.parameters[DefaultShapes::Line::StartX], s.parameters[DefaultShapes::Line::StartY]);
		}
		return vec2(s.parameters[DefaultShapes::Line::EndX], s.parameters[DefaultShapes::Line::EndY]);
	}

	void SceneOperations::setLineEndpoint(Entity e, int endpointIndex, vec2 newPos)
	{
		if (e == INVALID_ENTITY || m_ctx.registry == nullptr || m_ctx.registry->isEntityDestroyed(e) ||
			!m_ctx.registry->hasComponent<Shape>(e))
		{
			return;
		}
		auto& s = m_ctx.registry->getComponent<Shape>(e);
		if (s.distanceFieldId != DefaultShapes::Line)
		{
			setEntityPosition(e, newPos);
			return;
		}
		if (endpointIndex == 0)
		{
			s.parameters[DefaultShapes::Line::StartX] = newPos.x;
			s.parameters[DefaultShapes::Line::StartY] = newPos.y;
		}
		else
		{
			s.parameters[DefaultShapes::Line::EndX] = newPos.x;
			s.parameters[DefaultShapes::Line::EndY] = newPos.y;
		}
		m_ctx.registry->setComponentDirty(s);
		if (m_ctx.worldRenderContext)
		{
			m_ctx.worldRenderContext->shapesNeedUpdate = true;
		}

		const vec2 mid((s.parameters[DefaultShapes::Line::StartX] + s.parameters[DefaultShapes::Line::EndX]) * 0.5f,
					   (s.parameters[DefaultShapes::Line::StartY] + s.parameters[DefaultShapes::Line::EndY]) * 0.5f);
		if (m_ctx.registry->hasComponent<Transform>(e))
		{
			auto& t = m_ctx.registry->getComponent<Transform>(e);
			t.position.x = mid.x;
			t.position.y = mid.y;
			m_ctx.registry->setComponentDirty(t);
			if (m_ctx.registry->hasComponent<RigidBody2D>(e))
			{
				auto& rb = m_ctx.registry->getComponent<RigidBody2D>(e);
				m_ctx.simulation2D->setPosition(rb.simulationId, mid);
				m_ctx.simulation2D->setVelocity(rb.simulationId, vec2(0.0f));
				rb.velocity = vec2(0.0f);
				m_ctx.registry->setComponentDirty(rb);
			}
		}

		if (m_ctx.services)
		{
			m_ctx.services->clusters().markEntityOverride(e, ClusterOverrideFlag::ShapeParameters |
																 ClusterOverrideFlag::Position);
		}
	}

	// =====================================================================
	// Cluster manipulation
	// =====================================================================

	std::filesystem::path SceneOperations::resolveClusterPath(const std::string& assetPath) const
	{
		std::string clean = assetPath;
		std::replace(clean.begin(), clean.end(), '\\', '/');
		std::filesystem::path p(clean);
		if (std::filesystem::exists(p))
			return p;

		std::string filename = p.filename().string();
		std::string relAfterAssets;
		auto assetsPos = clean.find("assets/");
		if (assetsPos != std::string::npos)
		{
			relAfterAssets = clean.substr(assetsPos + 7);
		}

		if (m_ctx.project.isOpen())
		{
			auto assetsRoot = m_ctx.project.getAssetsPath();
			if (!relAfterAssets.empty() && std::filesystem::exists(assetsRoot / relAfterAssets))
				return assetsRoot / relAfterAssets;
			if (std::filesystem::exists(assetsRoot / p))
				return assetsRoot / p;
			if (std::filesystem::exists(assetsRoot / filename))
				return assetsRoot / filename;
		}

		std::filesystem::path sampleAssets("examples/sample-scenes/assets");
		if (!relAfterAssets.empty() && std::filesystem::exists(sampleAssets / relAfterAssets))
			return sampleAssets / relAfterAssets;
		if (std::filesystem::exists(sampleAssets / p))
			return sampleAssets / p;
		if (std::filesystem::exists(sampleAssets / filename))
			return sampleAssets / filename;

		return p;
	}

	vec2 SceneOperations::computeClusterCentroid(const ClusterInstance& cluster) const
	{
		vec2 centroid(0.0f);
		size_t count = 0;
		for (Entity e : cluster.entities)
		{
			if (m_ctx.registry->hasComponent<Transform>(e))
			{
				centroid += vec2(m_ctx.registry->getComponent<Transform>(e).position);
				count++;
			}
		}
		if (count > 0)
		{
			centroid /= static_cast<float>(count);
			return centroid;
		}
		return cluster.position;
	}

	ClusterRestData& SceneOperations::getOrCreateClusterRestData(const ClusterInstance& cluster)
	{
		auto it = m_ctx.clusterRestData.find(cluster.id);
		if (it != m_ctx.clusterRestData.end())
		{
			return it->second;
		}

		ClusterRestData data;

		std::filesystem::path path = resolveClusterPath(cluster.assetPath);
		std::ifstream file(path);
		if (file.is_open())
		{
			try
			{
				json j;
				file >> j;
				if (j.contains("entities") && j["entities"].is_array())
				{
					std::vector<vec2> positions;
					std::vector<float> rotations;
					vec2 centroid(0.0f);
					size_t count = 0;
					for (const auto& ej : j["entities"])
					{
						vec2 p(0.0f);
						float r = 0.0f;
						if (ej.contains("transform"))
						{
							if (ej["transform"].contains("position") && ej["transform"]["position"].is_array() &&
								ej["transform"]["position"].size() >= 2)
							{
								p.x = ej["transform"]["position"][0].get<float>();
								p.y = ej["transform"]["position"][1].get<float>();
								centroid += p;
								count++;
							}
							if (ej["transform"].contains("rotation") && ej["transform"]["rotation"].is_array() &&
								ej["transform"]["rotation"].size() >= 3)
							{
								r = ej["transform"]["rotation"][2].get<float>();
							}
						}
						positions.push_back(p);
						rotations.push_back(r);
					}
					if (count > 0)
					{
						centroid /= static_cast<float>(count);
					}
					for (const auto& p : positions)
					{
						data.restOffsets.push_back(p - centroid);
					}
					data.restRotations = std::move(rotations);
				}
			}
			catch (...)
			{
			}
		}

		// Ensure rest offsets and rotations cover all cluster entities
		if (!data.restOffsets.empty())
		{
			while (data.restOffsets.size() < cluster.entities.size())
			{
				data.restOffsets.push_back(vec2(0.0f));
			}
			while (data.restRotations.size() < cluster.entities.size())
			{
				data.restRotations.push_back(0.0f);
			}
		}
		else
		{
			// Fallback: only if asset file could not be read
			data.restOffsets.clear();
			data.restRotations.clear();
			vec2 centroid = computeClusterCentroid(cluster);
			for (Entity e : cluster.entities)
			{
				if (m_ctx.registry->hasComponent<Transform>(e))
				{
					vec2 p(m_ctx.registry->getComponent<Transform>(e).position);
					data.restOffsets.push_back(p - centroid);
					data.restRotations.push_back(m_ctx.registry->getComponent<Transform>(e).rotation.z);
				}
				else
				{
					data.restOffsets.push_back(vec2(0.0f));
					data.restRotations.push_back(0.0f);
				}
			}
		}

		auto [insIt, _] = m_ctx.clusterRestData.emplace(cluster.id, std::move(data));
		return insIt->second;
	}

	void SceneOperations::resetClusterShapeAtPosition(ClusterInstance& cluster, vec2 targetPos)
	{
		auto& restData = getOrCreateClusterRestData(cluster);
		const auto& offsets = restData.restOffsets;
		const auto& rotations = restData.restRotations;

		float cosR = std::cos(cluster.rotation);
		float sinR = std::sin(cluster.rotation);

		cluster.position = targetPos;

		for (size_t i = 0; i < cluster.entities.size(); ++i)
		{
			Entity e = cluster.entities[i];
			vec2 rel = (i < offsets.size()) ? offsets[i] : vec2(0.0f);
			vec2 rotated = vec2(rel.x * cosR - rel.y * sinR, rel.x * sinR + rel.y * cosR);
			vec2 worldPos = targetPos + rotated;
			float relRot = (i < rotations.size()) ? rotations[i] : 0.0f;

			if (m_ctx.registry->hasComponent<Transform>(e))
			{
				auto& t = m_ctx.registry->getComponent<Transform>(e);
				t.position.x = worldPos.x;
				t.position.y = worldPos.y;
				t.position.z = 0.0f;
				t.rotation.z = relRot + cluster.rotation;
				m_ctx.registry->setComponentDirty(t);
			}

			if (m_ctx.registry->hasComponent<RigidBody2D>(e))
			{
				auto& rb = m_ctx.registry->getComponent<RigidBody2D>(e);
				rb.velocity = vec2(0.0f);
				m_ctx.simulation2D->setPosition(rb.simulationId, worldPos);
				m_ctx.simulation2D->setVelocity(rb.simulationId, vec2(0.0f));
				m_ctx.registry->setComponentDirty(rb);
			}

			if (m_ctx.registry->hasComponent<Shape>(e))
			{
				auto& s = m_ctx.registry->getComponent<Shape>(e);
				vec2 currentPos = getShapePosition(s, e);
				vec2 delta = worldPos - currentPos;
				offsetShapeParameters(s, delta);
				m_ctx.registry->setComponentDirty(s);
			}
			cluster.clearOverride(e, ClusterOverrideFlag::Position | ClusterOverrideFlag::Rotation);
		}

		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
	}

	void SceneOperations::resetClusterShapeAtCurrentCentroid(ClusterInstance& cluster)
	{
		resetClusterShapeAtPosition(cluster, computeClusterCentroid(cluster));
	}

	void SceneOperations::setClusterPosition(uint32_t cId, vec2 newPos)
	{
		auto* c = m_ctx.services->clusters().getById(cId);
		if (!c)
			return;

		vec2 delta = newPos - c->position;
		c->position = newPos;

		for (Entity e : c->entities)
		{
			if (m_ctx.registry->hasComponent<Transform>(e))
			{
				auto& t = m_ctx.registry->getComponent<Transform>(e);
				t.position.x += delta.x;
				t.position.y += delta.y;
				m_ctx.registry->setComponentDirty(t);
				if (m_ctx.registry->hasComponent<RigidBody2D>(e))
				{
					auto& rb = m_ctx.registry->getComponent<RigidBody2D>(e);
					m_ctx.simulation2D->setPosition(rb.simulationId, vec2(t.position));
					m_ctx.simulation2D->setVelocity(rb.simulationId, vec2(0.0f));
					rb.velocity = vec2(0.0f);
					m_ctx.registry->setComponentDirty(rb);
				}
			}
			if (m_ctx.registry->hasComponent<Shape>(e))
			{
				auto& s = m_ctx.registry->getComponent<Shape>(e);
				offsetShapeParameters(s, delta);
				m_ctx.registry->setComponentDirty(s);
			}
		}
	}

	bool SceneOperations::reloadCluster(uint32_t clusterId)
	{
		if (!m_ctx.services)
		{
			return false;
		}
		auto* cluster = m_ctx.services->clusters().getById(clusterId);
		if (!cluster || cluster->assetPath.empty())
		{
			return false;
		}

		std::filesystem::path resolved = resolveClusterPath(cluster->assetPath);
		if (!std::filesystem::exists(resolved))
		{
			Logger::error("[SceneEditor] Cannot reload cluster " + cluster->name + ": asset file not found at " +
						  cluster->assetPath);
			return false;
		}

		std::unordered_set<Entity> oldClusterEntities(cluster->entities.begin(), cluster->entities.end());
		std::string resolvedStr = resolved.string();
		std::unordered_map<Entity, Entity> oldToNew;
		if (!m_ctx.services->clusters().reloadInstance(clusterId, resolvedStr, &oldToNew))
		{
			return false;
		}

		// Remap selection and editor groups onto the newly spawned cluster entities
		if (oldClusterEntities.count(m_ctx.selectedEntity))
		{
			auto it = oldToNew.find(m_ctx.selectedEntity);
			m_ctx.selectedEntity = (it != oldToNew.end()) ? it->second : INVALID_ENTITY;
		}
		{
			std::unordered_set<Entity> updatedSelection;
			for (Entity e : m_ctx.selectedEntities)
			{
				if (oldClusterEntities.count(e))
				{
					if (auto it = oldToNew.find(e); it != oldToNew.end())
						updatedSelection.insert(it->second);
				}
				else
				{
					updatedSelection.insert(e);
				}
			}
			m_ctx.selectedEntities = std::move(updatedSelection);
		}
		if (m_ctx.selectedEntity != INVALID_ENTITY && m_ctx.registry->isEntityDestroyed(m_ctx.selectedEntity))
		{
			m_ctx.selectedEntity = m_ctx.selectedEntities.empty() ? INVALID_ENTITY : *m_ctx.selectedEntities.begin();
		}
		for (auto& group : m_ctx.groups)
		{
			std::vector<Entity> updatedEntities;
			updatedEntities.reserve(group.entities.size());
			for (Entity e : group.entities)
			{
				if (oldClusterEntities.count(e))
				{
					if (auto it = oldToNew.find(e); it != oldToNew.end())
						updatedEntities.push_back(it->second);
				}
				else
				{
					updatedEntities.push_back(e);
				}
			}
			group.entities = std::move(updatedEntities);
		}

		// Clear cached rest data so it's recomputed fresh from the updated file
		m_ctx.clusterRestData.erase(clusterId);
		syncAllConstraintsClusterMembership();

		std::error_code ec;
		auto writeTime = std::filesystem::last_write_time(resolved, ec);
		if (!ec)
		{
			m_ctx.clusterAssetWriteTimes[ShapeService::normalizePath(resolvedStr)] = writeTime;
		}

		m_ctx.worldRenderContext->shapesNeedUpdate = true;
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
		Logger::log("[SceneEditor] Reloaded cluster " + cluster->name + " from " + resolvedStr);
		return true;
	}

	int SceneOperations::reloadClustersFromAsset(const std::string& assetPath)
	{
		if (!m_ctx.services)
		{
			return 0;
		}
		std::filesystem::path resolved = resolveClusterPath(assetPath);
		std::filesystem::path resolvedP =
			std::filesystem::exists(resolved) ? resolved : std::filesystem::path(assetPath);
		std::string targetFilename = resolvedP.filename().string();
		std::string targetStem = resolvedP.stem().string();

		std::vector<uint32_t> toReload;
		for (const auto& c : m_ctx.services->clusters().getAll())
		{
			std::filesystem::path cp(c.assetPath);
			if (cp == resolvedP || cp.filename().string() == targetFilename || c.type == targetStem)
			{
				toReload.push_back(c.id);
			}
		}

		int count = 0;
		for (uint32_t id : toReload)
		{
			if (reloadCluster(id))
			{
				count++;
			}
		}

		std::error_code ec;
		if (std::filesystem::exists(resolvedP, ec))
		{
			auto writeTime = std::filesystem::last_write_time(resolvedP, ec);
			if (!ec)
			{
				m_ctx.clusterAssetWriteTimes[ShapeService::normalizePath(resolvedP.string())] = writeTime;
			}
		}

		return count;
	}

	int SceneOperations::reloadAllClusters()
	{
		if (!m_ctx.services)
		{
			return 0;
		}
		std::vector<uint32_t> allIds;
		for (const auto& c : m_ctx.services->clusters().getAll())
		{
			if (!c.assetPath.empty())
			{
				allIds.push_back(c.id);
			}
		}
		int count = 0;
		for (uint32_t id : allIds)
		{
			if (reloadCluster(id))
			{
				count++;
			}
		}
		Logger::log("[SceneEditor] Reloaded " + std::to_string(count) + " cluster(s) from asset files.");
		return count;
	}

	bool SceneOperations::reloadCustomShape(const std::string& assetPath)
	{
		if (!m_ctx.services)
		{
			return false;
		}
		if (m_ctx.services->shapes().reloadCustomShape(assetPath))
		{
			const auto* customData = m_ctx.services->shapes().getCustomShapeData(assetPath);
			if (customData)
			{
				notifyShapeReloaded(customData->id);
			}
			else
			{
				m_ctx.worldRenderContext->shapesNeedUpdate = true;
				m_ctx.uiRenderContext->shapesNeedUpdate = true;
			}
			return true;
		}
		return false;
	}

	void SceneOperations::notifyShapeReloaded(ShapeId id)
	{
		if (!m_ctx.registry)
		{
			return;
		}

		bool anyEntityUpdated = false;
		for (Entity e = 0; e < m_ctx.registry->getEntityCount(); ++e)
		{
			if (m_ctx.registry->isEntityValid(e) && m_ctx.registry->hasComponent<Shape>(e))
			{
				auto& s = m_ctx.registry->getComponent<Shape>(e);
				if (s.distanceFieldId == id)
				{
					m_ctx.registry->setComponentDirty(s);
					anyEntityUpdated = true;
				}
			}
			if (m_ctx.registry->isEntityValid(e) && m_ctx.registry->hasComponent<UIShape>(e))
			{
				auto& s = m_ctx.registry->getComponent<UIShape>(e);
				if (s.distanceFieldId == id)
				{
					m_ctx.registry->setComponentDirty(s);
					anyEntityUpdated = true;
				}
			}
		}

		m_ctx.worldRenderContext->shapesNeedUpdate = true;
		m_ctx.uiRenderContext->shapesNeedUpdate = true;
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;

		if (m_ctx.services)
		{
			if (const auto* customData = m_ctx.services->shapes().getCustomShapeData(id))
			{
				Logger::log("[SceneEditor] Hot-reloaded custom shape: " + customData->filePath);
			}
		}
	}

	// =====================================================================
	// Grouping
	// =====================================================================

	EditorGroup* SceneOperations::findGroup(uint32_t gId)
	{
		return m_ctx.findGroup(gId);
	}

	EditorGroup& SceneOperations::createGroup(const std::string& name)
	{
		EditorGroup g;
		g.id = m_ctx.nextGroupId++;
		g.name = name;
		m_ctx.groups.push_back(std::move(g));
		selectGroup(m_ctx.groups.back().id);
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
		return m_ctx.groups.back();
	}

	void SceneOperations::groupSelected()
	{
		if (m_ctx.selectedEntities.empty() && m_ctx.selectedClusterIds.empty())
			return;

		EditorGroup group;
		group.id = m_ctx.nextGroupId++;
		group.name = "Group " + std::to_string(group.id);

		// Remove selected items from any existing groups
		for (auto& g : m_ctx.groups)
		{
			for (Entity e : m_ctx.selectedEntities)
			{
				auto it = std::find(g.entities.begin(), g.entities.end(), e);
				if (it != g.entities.end())
					g.entities.erase(it);
			}
			for (uint32_t cId : m_ctx.selectedClusterIds)
			{
				auto it = std::find(g.clusterIds.begin(), g.clusterIds.end(), cId);
				if (it != g.clusterIds.end())
					g.clusterIds.erase(it);
			}
		}

		for (Entity e : m_ctx.selectedEntities)
		{
			auto* c = m_ctx.services->clusters().getClusterForEntity(e);
			if (c)
			{
				if (std::find(group.clusterIds.begin(), group.clusterIds.end(), c->id) == group.clusterIds.end())
				{
					group.clusterIds.push_back(c->id);
				}
			}
			else
			{
				if (std::find(group.entities.begin(), group.entities.end(), e) == group.entities.end())
				{
					group.entities.push_back(e);
				}
			}
		}
		for (uint32_t cId : m_ctx.selectedClusterIds)
		{
			if (std::find(group.clusterIds.begin(), group.clusterIds.end(), cId) == group.clusterIds.end())
			{
				group.clusterIds.push_back(cId);
			}
		}

		m_ctx.groups.push_back(std::move(group));
		selectGroup(m_ctx.groups.back().id);
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
		Logger::log("[SceneEditor] Grouped selected items into: " + m_ctx.groups.back().name);
	}

	void SceneOperations::ungroupSelected()
	{
		if (m_ctx.selectedGroupId != 0)
		{
			auto it = std::find_if(m_ctx.groups.begin(), m_ctx.groups.end(),
								   [this](const EditorGroup& g) { return g.id == m_ctx.selectedGroupId; });
			if (it != m_ctx.groups.end())
			{
				Logger::log("[SceneEditor] Ungrouped: " + it->name);
				m_ctx.groups.erase(it);
				m_ctx.selectedGroupId = 0;
				m_ctx.isSceneDirty = true;
				m_pendingUndoSync = true;
			}
		}
	}

	bool SceneOperations::canGroupSelected() const
	{
		return (m_ctx.selectedEntities.size() + m_ctx.selectedClusterIds.size() > 0);
	}

	bool SceneOperations::isEntityInAnyGroup(Entity e) const
	{
		for (const auto& g : m_ctx.groups)
		{
			if (std::find(g.entities.begin(), g.entities.end(), e) != g.entities.end())
				return true;
		}
		return false;
	}

	bool SceneOperations::isClusterInAnyGroup(uint32_t cId) const
	{
		for (const auto& g : m_ctx.groups)
		{
			if (std::find(g.clusterIds.begin(), g.clusterIds.end(), cId) != g.clusterIds.end())
				return true;
		}
		return false;
	}

	void SceneOperations::addEntityToGroup(uint32_t gId, Entity e)
	{
		auto* c = m_ctx.services->clusters().getClusterForEntity(e);
		if (c)
		{
			addClusterToGroup(gId, c->id);
			return;
		}
		removeFromAllGroups(e);
		auto* target = findGroup(gId);
		if (target)
		{
			if (std::find(target->entities.begin(), target->entities.end(), e) == target->entities.end())
			{
				target->entities.push_back(e);
				m_ctx.isSceneDirty = true;
				m_pendingUndoSync = true;
			}
		}
	}

	void SceneOperations::addClusterToGroup(uint32_t gId, uint32_t cId)
	{
		removeClusterFromAllGroups(cId);
		auto* target = findGroup(gId);
		if (target)
		{
			if (std::find(target->clusterIds.begin(), target->clusterIds.end(), cId) == target->clusterIds.end())
			{
				target->clusterIds.push_back(cId);
				m_ctx.isSceneDirty = true;
				m_pendingUndoSync = true;
			}
		}
	}

	void SceneOperations::removeFromAllGroups(Entity e)
	{
		for (auto& g : m_ctx.groups)
		{
			auto it = std::find(g.entities.begin(), g.entities.end(), e);
			if (it != g.entities.end())
			{
				g.entities.erase(it);
				m_ctx.isSceneDirty = true;
				m_pendingUndoSync = true;
			}
		}
	}

	void SceneOperations::removeClusterFromAllGroups(uint32_t cId)
	{
		for (auto& g : m_ctx.groups)
		{
			auto it = std::find(g.clusterIds.begin(), g.clusterIds.end(), cId);
			if (it != g.clusterIds.end())
			{
				g.clusterIds.erase(it);
				m_ctx.isSceneDirty = true;
				m_pendingUndoSync = true;
			}
		}
	}

	void SceneOperations::cleanGroups()
	{
		for (auto& g : m_ctx.groups)
		{
			g.entities.erase(std::remove_if(g.entities.begin(), g.entities.end(),
											[this](Entity e) { return !m_ctx.registry->isEntityAlive(e); }),
							 g.entities.end());

			g.clusterIds.erase(std::remove_if(g.clusterIds.begin(), g.clusterIds.end(), [this](uint32_t cId)
											  { return m_ctx.services->clusters().getById(cId) == nullptr; }),
							   g.clusterIds.end());
		}
	}

	// =====================================================================
	// Constraints
	// =====================================================================

	void SceneOperations::syncConstraintClusterMembership(Entity constraintEnt, Entity entA, Entity entB)
	{
		if (entA == INVALID_ENTITY || entB == INVALID_ENTITY)
			return;

		auto* clusterA = m_ctx.services->clusters().getClusterForEntity(entA);
		auto* clusterB = m_ctx.services->clusters().getClusterForEntity(entB);
		auto* currentCluster = m_ctx.services->clusters().getClusterForEntity(constraintEnt);

		if (clusterA && clusterA == clusterB)
		{
			if (currentCluster != clusterA)
			{
				if (currentCluster)
				{
					auto it =
						std::find(currentCluster->entities.begin(), currentCluster->entities.end(), constraintEnt);
					if (it != currentCluster->entities.end())
						currentCluster->entities.erase(it);
				}
				if (std::find(clusterA->entities.begin(), clusterA->entities.end(), constraintEnt) ==
					clusterA->entities.end())
				{
					clusterA->entities.push_back(constraintEnt);
				}
			}
		}
		else if (currentCluster)
		{
			auto it = std::find(currentCluster->entities.begin(), currentCluster->entities.end(), constraintEnt);
			if (it != currentCluster->entities.end())
				currentCluster->entities.erase(it);
		}
	}

	void SceneOperations::syncAllConstraintsClusterMembership()
	{
		auto springArray = m_ctx.registry->getComponentArray<Spring>();
		if (springArray)
		{
			for (size_t i = 0; i < springArray->getSize(); ++i)
			{
				Entity spEnt = springArray->getEntityAtIdx(i);
				auto& sp = springArray->getDataAtIdx(i);
				syncConstraintClusterMembership(spEnt, sp.entityA, sp.entityB);
			}
		}

		auto dcArray = m_ctx.registry->getComponentArray<DistanceConstraint>();
		if (dcArray)
		{
			for (size_t i = 0; i < dcArray->getSize(); ++i)
			{
				Entity dcEnt = dcArray->getEntityAtIdx(i);
				auto& dc = dcArray->getDataAtIdx(i);
				syncConstraintClusterMembership(dcEnt, dc.entityA, dc.entityB);
			}
		}
	}

	// =====================================================================
	// Selection & Editing Commands
	// =====================================================================

	vec2 SceneOperations::getSelectedPosition() const
	{
		if (m_ctx.selectedGroupId != 0)
		{
			auto* g = m_ctx.findGroup(m_ctx.selectedGroupId);
			if (g && (!g->entities.empty() || !g->clusterIds.empty()))
			{
				if (!g->entities.empty())
					return getEntityPosition(g->entities[0]);
				if (!g->clusterIds.empty())
				{
					auto* c = m_ctx.services->clusters().getById(g->clusterIds[0]);
					if (c)
						return c->position;
				}
			}
		}
		if (m_ctx.selectedClusterId != 0)
		{
			auto* c = m_ctx.services->clusters().getById(m_ctx.selectedClusterId);
			if (c)
			{
				return c->position;
			}
		}
		if (m_ctx.selectedEntity != INVALID_ENTITY)
		{
			return getEntityPosition(m_ctx.selectedEntity);
		}
		return vec2(0.0f);
	}

	void SceneOperations::setSelectedPosition(vec2 newPos)
	{
		if (m_ctx.selectedClusterId != 0)
		{
			setClusterPosition(m_ctx.selectedClusterId, newPos);
		}
		else if (m_ctx.selectedEntity != INVALID_ENTITY)
		{
			setEntityPosition(m_ctx.selectedEntity, newPos);
		}
	}

	void SceneOperations::deleteSelected()
	{
		// Collect entities to delete
		std::unordered_set<Entity> entitiesToDelete = m_ctx.selectedEntities;
		if (m_ctx.selectedEntity != INVALID_ENTITY)
			entitiesToDelete.insert(m_ctx.selectedEntity);

		// Collect clusters to delete
		std::unordered_set<uint32_t> clustersToDelete = m_ctx.selectedClusterIds;
		if (m_ctx.selectedClusterId != 0)
			clustersToDelete.insert(m_ctx.selectedClusterId);

		if (m_ctx.selectedGroupId != 0 && entitiesToDelete.empty() && clustersToDelete.empty())
		{
			ungroupSelected();
			return;
		}
		if (entitiesToDelete.empty() && clustersToDelete.empty())
		{
			return;
		}

		for (uint32_t cId : clustersToDelete)
		{
			removeClusterFromAllGroups(cId);
			m_ctx.clusterRestData.erase(cId);
			m_ctx.services->clusters().destroyCluster(cId);
		}

		// If any Spring or DistanceConstraint connects to an entity being deleted, destroy that constraint entity too
		auto springArray = m_ctx.registry->getComponentArray<Spring>();
		if (springArray)
		{
			for (size_t i = 0; i < springArray->getSize(); ++i)
			{
				auto& sp = springArray->getDataAtIdx(i);
				if (entitiesToDelete.count(sp.entityA) || entitiesToDelete.count(sp.entityB))
				{
					Entity spEnt = springArray->getEntityAtIdx(i);
					if (!entitiesToDelete.count(spEnt) && !m_ctx.registry->isEntityDestroyed(spEnt))
					{
						m_ctx.services->names().removeName(spEnt);
						m_ctx.services->tags().removeAllTags(spEnt);
						m_ctx.registry->destroyEntity(spEnt);
					}
				}
			}
		}

		auto dcArray = m_ctx.registry->getComponentArray<DistanceConstraint>();
		if (dcArray)
		{
			for (size_t i = 0; i < dcArray->getSize(); ++i)
			{
				auto& dc = dcArray->getDataAtIdx(i);
				if (entitiesToDelete.count(dc.entityA) || entitiesToDelete.count(dc.entityB))
				{
					Entity dcEnt = dcArray->getEntityAtIdx(i);
					if (!entitiesToDelete.count(dcEnt) && !m_ctx.registry->isEntityDestroyed(dcEnt))
					{
						m_ctx.services->names().removeName(dcEnt);
						m_ctx.services->tags().removeAllTags(dcEnt);
						m_ctx.registry->destroyEntity(dcEnt);
					}
				}
			}
		}

		Entity camEntity = m_ctx.services->render().getCameraEntity();
		for (Entity e : entitiesToDelete)
		{
			if (e == camEntity || m_ctx.registry->isEntityDestroyed(e))
				continue; // Do not delete main camera or already destroyed cluster members

			removeFromAllGroups(e);

			auto* cluster = m_ctx.services->clusters().getClusterForEntity(e);
			if (cluster)
			{
				auto it = std::find(cluster->entities.begin(), cluster->entities.end(), e);
				if (it != cluster->entities.end())
				{
					cluster->entities.erase(it);
				}

				for (auto nit = cluster->localNames.begin(); nit != cluster->localNames.end();)
				{
					if (nit->second == e)
					{
						nit = cluster->localNames.erase(nit);
					}
					else
					{
						++nit;
					}
				}
			}
			m_ctx.services->names().removeName(e);
			m_ctx.services->tags().removeAllTags(e);
			m_ctx.registry->destroyEntity(e);
		}

		m_ctx.registry->freeRemovedComponents();
		m_ctx.worldRenderContext->shapesNeedUpdate = true;
		m_ctx.uiRenderContext->shapesNeedUpdate = true;
		deselectAll();
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
	}

	std::string SceneOperations::makeUniqueEntityName(const std::string& baseName) const
	{
		if (baseName.empty())
		{
			return "";
		}
		if (m_ctx.services->names().getEntityByName(baseName) == MAX_ENTITIES)
		{
			return baseName;
		}

		std::string stem = baseName;
		const size_t copyPos = stem.rfind("_copy");
		if (copyPos != std::string::npos)
		{
			const std::string suffix = stem.substr(copyPos + 5);
			bool isCopySuffix = suffix.empty();
			if (!isCopySuffix && suffix.size() >= 2 && suffix[0] == '_')
			{
				isCopySuffix =
					std::all_of(suffix.begin() + 1, suffix.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
			}
			if (isCopySuffix)
			{
				stem = stem.substr(0, copyPos);
			}
		}

		std::string candidate = stem + "_copy";
		if (m_ctx.services->names().getEntityByName(candidate) == MAX_ENTITIES)
		{
			return candidate;
		}
		for (int idx = 2;; ++idx)
		{
			candidate = stem + "_copy_" + std::to_string(idx);
			if (m_ctx.services->names().getEntityByName(candidate) == MAX_ENTITIES)
			{
				return candidate;
			}
		}
	}

	std::string SceneOperations::makeUniqueClusterName(const std::string& baseName) const
	{
		if (baseName.empty())
		{
			return "";
		}
		if (m_ctx.services->clusters().getByName(baseName) == nullptr)
		{
			return baseName;
		}

		std::string stem = baseName;
		const size_t copyPos = stem.rfind("_copy");
		if (copyPos != std::string::npos)
		{
			const std::string suffix = stem.substr(copyPos + 5);
			bool isCopySuffix = suffix.empty();
			if (!isCopySuffix && suffix.size() >= 2 && suffix[0] == '_')
			{
				isCopySuffix =
					std::all_of(suffix.begin() + 1, suffix.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
			}
			if (isCopySuffix)
			{
				stem = stem.substr(0, copyPos);
			}
		}

		std::string candidate = stem + "_copy";
		if (m_ctx.services->clusters().getByName(candidate) == nullptr)
		{
			return candidate;
		}
		for (int idx = 2;; ++idx)
		{
			candidate = stem + "_copy_" + std::to_string(idx);
			if (m_ctx.services->clusters().getByName(candidate) == nullptr)
			{
				return candidate;
			}
		}
	}

	bool SceneOperations::captureSelectionPayload(nlohmann::json& outJson, vec2& outAnchor) const
	{
		if (m_ctx.registry == nullptr || m_ctx.services == nullptr)
		{
			return false;
		}

		const Entity camEntity = m_ctx.services->render().getCameraEntity();

		std::unordered_set<uint32_t> clusterSet = m_ctx.selectedClusterIds;
		if (m_ctx.selectedClusterId != 0)
		{
			clusterSet.insert(m_ctx.selectedClusterId);
		}

		std::unordered_set<Entity> entitySet = m_ctx.selectedEntities;
		if (m_ctx.selectedEntity != INVALID_ENTITY)
		{
			entitySet.insert(m_ctx.selectedEntity);
		}

		if (m_ctx.selectedGroupId != 0)
		{
			if (const auto* g = m_ctx.findGroup(m_ctx.selectedGroupId))
			{
				for (Entity ge : g->entities)
				{
					entitySet.insert(ge);
				}
				for (uint32_t gc : g->clusterIds)
				{
					clusterSet.insert(gc);
				}
			}
		}

		std::vector<uint32_t> clusterIds;
		clusterIds.reserve(clusterSet.size());
		std::unordered_set<Entity> capturedClusterMembers;
		for (uint32_t cId : clusterSet)
		{
			const auto* cluster = m_ctx.services->clusters().getById(cId);
			if (!cluster)
			{
				continue;
			}
			clusterIds.push_back(cId);
			for (Entity member : cluster->entities)
			{
				capturedClusterMembers.insert(member);
			}
		}
		std::sort(clusterIds.begin(), clusterIds.end());

		std::vector<Entity> standaloneEntities;
		standaloneEntities.reserve(entitySet.size());
		for (Entity e : entitySet)
		{
			if (e == INVALID_ENTITY || e == camEntity || m_ctx.registry->isEntityDestroyed(e) ||
				capturedClusterMembers.count(e))
			{
				continue;
			}
			const bool isPureConstraint =
				(m_ctx.registry->hasComponent<Spring>(e) || m_ctx.registry->hasComponent<DistanceConstraint>(e)) &&
				!m_ctx.registry->hasComponent<Transform>(e) && !m_ctx.registry->hasComponent<Shape>(e) &&
				!m_ctx.registry->hasComponent<UIShape>(e) && !m_ctx.registry->hasComponent<Dot>(e) &&
				!m_ctx.registry->hasComponent<TextRenderer>(e) && !m_ctx.registry->hasComponent<UITextRenderer>(e) &&
				!m_ctx.registry->hasComponent<Light2DComponent>(e);
			if (isPureConstraint)
			{
				continue;
			}
			standaloneEntities.push_back(e);
		}
		std::sort(standaloneEntities.begin(), standaloneEntities.end(),
				  [&](Entity a, Entity b)
				  {
					  const bool aShape = m_ctx.registry->hasComponent<Shape>(a);
					  const bool bShape = m_ctx.registry->hasComponent<Shape>(b);
					  if (aShape && bShape)
					  {
						  const auto& sa = m_ctx.registry->getComponent<Shape>(a);
						  const auto& sb = m_ctx.registry->getComponent<Shape>(b);
						  if (sa.layer != sb.layer)
							  return sa.layer < sb.layer;
						  if (sa.order != sb.order)
							  return sa.order < sb.order;
					  }
					  return a < b;
				  });

		if (standaloneEntities.empty() && clusterIds.empty())
		{
			return false;
		}

		outJson = nlohmann::json::object();
		outJson["version"] = 1;

		std::unordered_map<Entity, size_t> entityToPayloadIdx;
		nlohmann::json entitiesJson = nlohmann::json::array();

		vec2 boundsMin(std::numeric_limits<float>::max());
		vec2 boundsMax(-std::numeric_limits<float>::max());
		bool hasBounds = false;
		vec2 posAccum(0.0f);
		size_t posCount = 0;

		auto accumulateEntityAnchor = [&](Entity ent)
		{
			vec2 mn(0.0f);
			vec2 mx(0.0f);
			if (getEntityWorldBounds(ent, mn, mx))
			{
				boundsMin = glm::min(boundsMin, mn);
				boundsMax = glm::max(boundsMax, mx);
				hasBounds = true;
			}
			posAccum += getEntityPosition(ent);
			++posCount;
		};

		for (Entity e : standaloneEntities)
		{
			entityToPayloadIdx[e] = entitiesJson.size();
			nlohmann::json ej = nlohmann::json::object();
			std::string gName = m_ctx.services->names().getName(e);
			if (!gName.empty())
			{
				ej["name"] = gName;
			}
			const auto& tags = m_ctx.services->tags().getTags(e);
			if (!tags.empty())
			{
				ej["tags"] = tags;
			}
			ej["components"] = serializeEntityComponents(*m_ctx.registry, m_ctx.services->shapes(), e);
			entitiesJson.push_back(std::move(ej));
			accumulateEntityAnchor(e);
		}
		outJson["entities"] = std::move(entitiesJson);

		nlohmann::json constraintsJson = nlohmann::json::array();
		if (auto springArray = m_ctx.registry->getComponentArray<Spring>())
		{
			for (size_t i = 0; i < springArray->getSize(); ++i)
			{
				Entity ce = springArray->getEntityAtIdx(i);
				const auto& sp = springArray->getDataAtIdx(i);
				auto itA = entityToPayloadIdx.find(sp.entityA);
				auto itB = entityToPayloadIdx.find(sp.entityB);
				if (itA != entityToPayloadIdx.end() && itB != entityToPayloadIdx.end())
				{
					nlohmann::json cj;
					cj["kind"] = "spring";
					cj["indexA"] = itA->second;
					cj["indexB"] = itB->second;
					cj["stiffness"] = sp.stiffness;
					cj["restDistance"] = sp.restDistance;
					std::string cName = m_ctx.services->names().getName(ce);
					if (!cName.empty())
					{
						cj["name"] = cName;
					}
					constraintsJson.push_back(std::move(cj));
				}
			}
		}
		if (auto dcArray = m_ctx.registry->getComponentArray<DistanceConstraint>())
		{
			for (size_t i = 0; i < dcArray->getSize(); ++i)
			{
				Entity ce = dcArray->getEntityAtIdx(i);
				const auto& dc = dcArray->getDataAtIdx(i);
				auto itA = entityToPayloadIdx.find(dc.entityA);
				auto itB = entityToPayloadIdx.find(dc.entityB);
				if (itA != entityToPayloadIdx.end() && itB != entityToPayloadIdx.end())
				{
					nlohmann::json cj;
					cj["kind"] = "distanceConstraint";
					cj["indexA"] = itA->second;
					cj["indexB"] = itB->second;
					cj["distance"] = dc.distance;
					std::string cName = m_ctx.services->names().getName(ce);
					if (!cName.empty())
					{
						cj["name"] = cName;
					}
					constraintsJson.push_back(std::move(cj));
				}
			}
		}
		outJson["constraints"] = std::move(constraintsJson);

		nlohmann::json clustersJson = nlohmann::json::array();
		for (uint32_t cId : clusterIds)
		{
			const auto* cluster = m_ctx.services->clusters().getById(cId);
			if (!cluster)
			{
				continue;
			}
			nlohmann::json cj = nlohmann::json::object();
			cj["name"] = cluster->name;
			cj["type"] = cluster->type;
			cj["assetPath"] = cluster->assetPath;
			cj["position"] = {cluster->position.x, cluster->position.y};
			cj["rotation"] = cluster->rotation;
			cj["tags"] = cluster->tags;
			cj["hasOverrideMetadata"] = cluster->hasOverrideMetadata;

			std::unordered_map<Entity, size_t> memberToIdx;
			nlohmann::json membersJson = nlohmann::json::array();
			for (size_t mi = 0; mi < cluster->entities.size(); ++mi)
			{
				Entity me = cluster->entities[mi];
				if (me == INVALID_ENTITY || m_ctx.registry->isEntityDestroyed(me))
				{
					continue;
				}
				memberToIdx[me] = membersJson.size();

				nlohmann::json mj = nlohmann::json::object();
				Entity srcId = cluster->getSourceEntityId(me);
				if (srcId != INVALID_ENTITY)
				{
					mj["sourceEntityId"] = srcId;
				}
				for (const auto& [ln, le] : cluster->localNames)
				{
					if (le == me)
					{
						mj["localName"] = ln;
						break;
					}
				}
				std::string gName = m_ctx.services->names().getName(me);
				if (!gName.empty())
				{
					mj["globalName"] = gName;
				}
				mj["overrides"] = cluster->getOverrides(me);
				const auto& mTags = m_ctx.services->tags().getTags(me);
				if (!mTags.empty())
				{
					mj["tags"] = mTags;
				}
				mj["components"] = serializeEntityComponents(*m_ctx.registry, m_ctx.services->shapes(), me);
				membersJson.push_back(std::move(mj));
				accumulateEntityAnchor(me);
			}
			cj["members"] = std::move(membersJson);

			if (cluster->entities.empty())
			{
				posAccum += cluster->position;
				++posCount;
			}

			// Fallback internal constraints in case asset file is unavailable when pasting
			nlohmann::json clusterConstraintsJson = nlohmann::json::array();
			if (auto springArray = m_ctx.registry->getComponentArray<Spring>())
			{
				for (size_t i = 0; i < springArray->getSize(); ++i)
				{
					const auto& sp = springArray->getDataAtIdx(i);
					auto itA = memberToIdx.find(sp.entityA);
					auto itB = memberToIdx.find(sp.entityB);
					if (itA != memberToIdx.end() && itB != memberToIdx.end())
					{
						clusterConstraintsJson.push_back({{"kind", "spring"},
														  {"indexA", itA->second},
														  {"indexB", itB->second},
														  {"stiffness", sp.stiffness},
														  {"restDistance", sp.restDistance}});
					}
				}
			}
			if (auto dcArray = m_ctx.registry->getComponentArray<DistanceConstraint>())
			{
				for (size_t i = 0; i < dcArray->getSize(); ++i)
				{
					const auto& dc = dcArray->getDataAtIdx(i);
					auto itA = memberToIdx.find(dc.entityA);
					auto itB = memberToIdx.find(dc.entityB);
					if (itA != memberToIdx.end() && itB != memberToIdx.end())
					{
						clusterConstraintsJson.push_back({{"kind", "distanceConstraint"},
														  {"indexA", itA->second},
														  {"indexB", itB->second},
														  {"distance", dc.distance}});
					}
				}
			}
			cj["constraints"] = std::move(clusterConstraintsJson);
			clustersJson.push_back(std::move(cj));
		}
		outJson["clusters"] = std::move(clustersJson);

		if (hasBounds)
		{
			outAnchor = (boundsMin + boundsMax) * 0.5f;
		}
		else if (posCount > 0)
		{
			outAnchor = posAccum / static_cast<float>(posCount);
		}
		else
		{
			outAnchor = vec2(0.0f);
		}
		outJson["anchor"] = {outAnchor.x, outAnchor.y};
		return true;
	}

	void SceneOperations::instantiateSelectionPayload(const nlohmann::json& j, vec2 deltaOffset)
	{
		if (m_ctx.registry == nullptr || m_ctx.services == nullptr || m_ctx.simulation2D == nullptr)
		{
			return;
		}

		std::vector<Entity> spawnedEntities;
		std::vector<uint32_t> spawnedClusterIds;

		if (j.contains("entities") && j["entities"].is_array())
		{
			for (const auto& ej : j["entities"])
			{
				Entity newEnt = m_ctx.registry->createEntity();
				if (ej.contains("components") && ej["components"].is_object())
				{
					applySerializedComponents(*m_ctx.registry, m_ctx.services->shapes(), *m_ctx.simulation2D, newEnt,
											  ej["components"], deltaOffset, false);
				}

				if (ej.contains("name") && ej["name"].is_string())
				{
					std::string uniqueName = makeUniqueEntityName(ej["name"].get<std::string>());
					if (!uniqueName.empty())
					{
						m_ctx.services->names().setName(newEnt, uniqueName);
					}
				}

				if (ej.contains("tags") && ej["tags"].is_array())
				{
					for (const auto& t : ej["tags"])
					{
						if (t.is_string())
						{
							m_ctx.services->tags().addTag(newEnt, t.get<std::string>());
						}
					}
				}

				spawnedEntities.push_back(newEnt);
			}
		}

		if (j.contains("constraints") && j["constraints"].is_array())
		{
			for (const auto& cj : j["constraints"])
			{
				size_t idxA = cj.value("indexA", static_cast<size_t>(0));
				size_t idxB = cj.value("indexB", static_cast<size_t>(0));
				if (idxA >= spawnedEntities.size() || idxB >= spawnedEntities.size())
				{
					continue;
				}
				Entity entA = spawnedEntities[idxA];
				Entity entB = spawnedEntities[idxB];
				std::string kind = cj.value("kind", "");

				Entity cEnt = m_ctx.registry->createEntity();
				if (kind == "spring")
				{
					auto& sp = m_ctx.registry->addComponent<Spring>(cEnt);
					sp.entityA = entA;
					sp.entityB = entB;
					sp.stiffness = cj.value("stiffness", 1.0f);
					sp.restDistance = cj.value("restDistance", 1.0f);
					m_ctx.registry->setComponentDirty(sp);
				}
				else if (kind == "distanceConstraint")
				{
					auto& dc = m_ctx.registry->addComponent<DistanceConstraint>(cEnt);
					dc.entityA = entA;
					dc.entityB = entB;
					dc.distance = cj.value("distance", 1.0f);
					m_ctx.registry->setComponentDirty(dc);
				}

				if (cj.contains("name") && cj["name"].is_string())
				{
					std::string uniqueName = makeUniqueEntityName(cj["name"].get<std::string>());
					if (!uniqueName.empty())
					{
						m_ctx.services->names().setName(cEnt, uniqueName);
					}
				}
			}
		}

		if (j.contains("clusters") && j["clusters"].is_array())
		{
			for (const auto& cj : j["clusters"])
			{
				std::string assetPath = cj.value("assetPath", "");
				std::string origName = cj.value("name", "");
				std::string uniqueName = makeUniqueClusterName(origName);
				vec2 basePos(0.0f);
				if (cj.contains("position") && cj["position"].is_array() && cj["position"].size() >= 2)
				{
					basePos.x = cj["position"][0].get<float>();
					basePos.y = cj["position"][1].get<float>();
				}
				vec2 targetPos = basePos + deltaOffset;
				float rotation = cj.value("rotation", 0.0f);
				std::vector<std::string> clusterTags;
				if (cj.contains("tags") && cj["tags"].is_array())
				{
					for (const auto& t : cj["tags"])
					{
						if (t.is_string())
						{
							clusterTags.push_back(t.get<std::string>());
						}
					}
				}

				std::filesystem::path resolved = resolveClusterPath(assetPath);
				std::error_code ec;
				ClusterInstance* newCluster = nullptr;

				if (!assetPath.empty() && std::filesystem::exists(resolved, ec))
				{
					newCluster = &m_ctx.services->clusters().instantiate(assetPath, targetPos, rotation, uniqueName,
																		 clusterTags);
					m_ctx.clusterAssetWriteTimes[ShapeService::normalizePath(resolved.string())] =
						std::filesystem::last_write_time(resolved, ec);
				}
				else
				{
					ClusterInstance fallback;
					fallback.name = uniqueName;
					fallback.type = cj.value("type", "");
					fallback.assetPath = assetPath;
					fallback.position = targetPos;
					fallback.rotation = rotation;
					fallback.tags = clusterTags;
					fallback.hasOverrideMetadata = cj.value("hasOverrideMetadata", false);
					newCluster = &m_ctx.services->clusters().addInstance(std::move(fallback));
				}

				if (newCluster && cj.contains("members") && cj["members"].is_array())
				{
					std::vector<Entity> matchedByOrder;
					std::unordered_set<Entity> alreadyMatched;
					const bool manualCluster = newCluster->entities.empty();

					for (size_t mi = 0; mi < cj["members"].size(); ++mi)
					{
						const auto& mj = cj["members"][mi];
						Entity targetMember = INVALID_ENTITY;

						if (manualCluster)
						{
							targetMember = m_ctx.registry->createEntity();
							newCluster->entities.push_back(targetMember);
							if (mj.contains("sourceEntityId"))
							{
								newCluster->sourceEntityIds[targetMember] = mj["sourceEntityId"].get<Entity>();
							}
							if (mj.contains("localName") && mj["localName"].is_string())
							{
								newCluster->localNames[mj["localName"].get<std::string>()] = targetMember;
							}
						}
						else
						{
							if (mj.contains("sourceEntityId"))
							{
								Entity srcId = mj["sourceEntityId"].get<Entity>();
								for (const auto& [me, se] : newCluster->sourceEntityIds)
								{
									if (se == srcId && !alreadyMatched.count(me))
									{
										targetMember = me;
										break;
									}
								}
							}
							if (targetMember == INVALID_ENTITY && mj.contains("localName") &&
								mj["localName"].is_string())
							{
								Entity byName = newCluster->getEntity(mj["localName"].get<std::string>());
								if (byName != INVALID_ENTITY && !alreadyMatched.count(byName))
								{
									targetMember = byName;
								}
							}
							if (targetMember == INVALID_ENTITY && mi < newCluster->entities.size() &&
								!alreadyMatched.count(newCluster->entities[mi]))
							{
								targetMember = newCluster->entities[mi];
							}
						}

						if (targetMember == INVALID_ENTITY)
						{
							continue;
						}
						alreadyMatched.insert(targetMember);
						matchedByOrder.push_back(targetMember);

						if (mj.contains("components") && mj["components"].is_object())
						{
							applySerializedComponents(*m_ctx.registry, m_ctx.services->shapes(), *m_ctx.simulation2D,
													  targetMember, mj["components"], deltaOffset, true);
						}

						if (mj.contains("globalName") && mj["globalName"].is_string())
						{
							std::string uniqueGlobalName = makeUniqueEntityName(mj["globalName"].get<std::string>());
							if (!uniqueGlobalName.empty())
							{
								m_ctx.services->names().setName(targetMember, uniqueGlobalName);
							}
						}

						if (mj.contains("tags") && mj["tags"].is_array())
						{
							for (const auto& t : mj["tags"])
							{
								if (t.is_string())
								{
									m_ctx.services->tags().addTag(targetMember, t.get<std::string>());
								}
							}
						}

						uint32_t overrides = mj.value("overrides", 0u);
						if (overrides != 0u)
						{
							newCluster->entityOverrides[targetMember] = overrides;
							newCluster->hasOverrideMetadata = true;
						}
					}

					if (manualCluster && cj.contains("constraints") && cj["constraints"].is_array())
					{
						for (const auto& ccj : cj["constraints"])
						{
							size_t idxA = ccj.value("indexA", static_cast<size_t>(0));
							size_t idxB = ccj.value("indexB", static_cast<size_t>(0));
							if (idxA >= matchedByOrder.size() || idxB >= matchedByOrder.size())
							{
								continue;
							}
							Entity entA = matchedByOrder[idxA];
							Entity entB = matchedByOrder[idxB];
							std::string kind = ccj.value("kind", "");
							Entity cEnt = m_ctx.registry->createEntity();
							if (kind == "spring")
							{
								auto& sp = m_ctx.registry->addComponent<Spring>(cEnt);
								sp.entityA = entA;
								sp.entityB = entB;
								sp.stiffness = ccj.value("stiffness", 1.0f);
								sp.restDistance = ccj.value("restDistance", 1.0f);
								m_ctx.registry->setComponentDirty(sp);
							}
							else if (kind == "distanceConstraint")
							{
								auto& dc = m_ctx.registry->addComponent<DistanceConstraint>(cEnt);
								dc.entityA = entA;
								dc.entityB = entB;
								dc.distance = ccj.value("distance", 1.0f);
								m_ctx.registry->setComponentDirty(dc);
							}
						}
					}
				}

				if (newCluster)
				{
					m_ctx.clusterRestData.erase(newCluster->id);
					spawnedClusterIds.push_back(newCluster->id);
				}
			}
		}

		syncAllConstraintsClusterMembership();
		if (!m_ctx.isSimulating)
		{
			flushDesignPhysicsSync();
		}

		m_ctx.worldRenderContext->shapesNeedUpdate = true;
		m_ctx.uiRenderContext->shapesNeedUpdate = true;

		deselectAll();
		for (Entity e : spawnedEntities)
		{
			m_ctx.selectedEntities.insert(e);
		}
		for (uint32_t cId : spawnedClusterIds)
		{
			m_ctx.selectedClusterIds.insert(cId);
		}
		m_ctx.selectedEntity = spawnedEntities.empty() ? INVALID_ENTITY : spawnedEntities.front();
		m_ctx.selectedClusterId = spawnedClusterIds.empty() ? 0 : spawnedClusterIds.front();

		if (!spawnedClusterIds.empty())
		{
			m_ctx.shiftAnchorItem = {HierarchyItemType::Cluster, spawnedClusterIds.front()};
			m_ctx.hasShiftAnchor = true;
		}
		else if (!spawnedEntities.empty())
		{
			m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(spawnedEntities.front())};
			m_ctx.hasShiftAnchor = true;
		}

		if (!spawnedEntities.empty() || !spawnedClusterIds.empty())
		{
			m_ctx.isSceneDirty = true;
			m_pendingUndoSync = true;
		}
	}

	void SceneOperations::copySelected()
	{
		nlohmann::json payload;
		vec2 anchor(0.0f);
		if (!captureSelectionPayload(payload, anchor))
		{
			return;
		}

		auto& clipboard = Tools::ToolsSession::getInstance().sceneClipboard();
		clipboard.hasData = true;
		clipboard.fromCut = false;
		clipboard.pasteCount = 0;
		clipboard.anchorWorld = anchor;
		clipboard.data = std::move(payload);
	}

	void SceneOperations::cutSelected()
	{
		nlohmann::json payload;
		vec2 anchor(0.0f);
		if (!captureSelectionPayload(payload, anchor))
		{
			return;
		}

		auto& clipboard = Tools::ToolsSession::getInstance().sceneClipboard();
		clipboard.hasData = true;
		clipboard.fromCut = true;
		clipboard.pasteCount = 0;
		clipboard.anchorWorld = anchor;
		clipboard.data = std::move(payload);

		deleteSelected();
	}

	bool SceneOperations::canPaste() const
	{
		const auto& clipboard = Tools::ToolsSession::getInstance().sceneClipboard();
		if (!clipboard.hasData || !clipboard.data.is_object())
		{
			return false;
		}
		const bool hasEnts = clipboard.data.contains("entities") && clipboard.data["entities"].is_array() &&
							 !clipboard.data["entities"].empty();
		const bool hasClusters = clipboard.data.contains("clusters") && clipboard.data["clusters"].is_array() &&
								 !clipboard.data["clusters"].empty();
		return hasEnts || hasClusters;
	}

	void SceneOperations::pasteClipboard(const vec2* targetWorldCenter)
	{
		if (!canPaste())
		{
			return;
		}

		auto& clipboard = Tools::ToolsSession::getInstance().sceneClipboard();
		vec2 deltaOffset(0.0f);

		if (targetWorldCenter != nullptr)
		{
			deltaOffset = *targetWorldCenter - clipboard.anchorWorld;
		}
		else
		{
			const auto& cam = m_ctx.registry->getComponent<Transform>(m_ctx.services->render().getCameraEntity());
			const float winW = (std::max)(1.0f, static_cast<float>(WeirdRenderer::Display::width));
			const float winH = (std::max)(1.0f, static_cast<float>(WeirdRenderer::Display::height));
			const vec2 vpTopLeft = ECS::Camera::screenPositionToWorldPosition2D(
				cam, vec2(EditorContext::LEFT_PANEL_WIDTH, EditorContext::BOTTOM_BAR_HEIGHT));
			const vec2 vpBottomRight = ECS::Camera::screenPositionToWorldPosition2D(
				cam, vec2(winW - EditorContext::RIGHT_PANEL_WIDTH, winH - m_ctx.menuBarHeight));
			const vec2 viewMin = glm::min(vpTopLeft, vpBottomRight);
			const vec2 viewMax = glm::max(vpTopLeft, vpBottomRight);

			const bool anchorInView = isPointInAABB(clipboard.anchorWorld, viewMin, viewMax);
			const vec2 step = (m_ctx.gridSnap && m_ctx.gridSize > 0.001f) ? vec2(m_ctx.gridSize) : vec2(1.0f);

			if (anchorInView)
			{
				if (clipboard.fromCut && clipboard.pasteCount == 0)
				{
					deltaOffset = vec2(0.0f);
				}
				else
				{
					const int stepNum = clipboard.fromCut ? clipboard.pasteCount : (clipboard.pasteCount + 1);
					deltaOffset = step * static_cast<float>(stepNum);
				}
			}
			else
			{
				const vec2 baseOffset = getCameraCenterWorld() - clipboard.anchorWorld;
				deltaOffset = baseOffset + step * static_cast<float>(clipboard.pasteCount);
			}
		}

		instantiateSelectionPayload(clipboard.data, deltaOffset);
		++clipboard.pasteCount;
	}

	void SceneOperations::duplicateSelected()
	{
		nlohmann::json payload;
		vec2 anchor(0.0f);
		if (!captureSelectionPayload(payload, anchor))
		{
			return;
		}
		const vec2 step = (m_ctx.gridSnap && m_ctx.gridSize > 0.001f) ? vec2(m_ctx.gridSize) : vec2(1.0f);
		instantiateSelectionPayload(payload, step);
	}

	// =====================================================================
	// Picking & Selection Helpers
	// =====================================================================

	bool SceneOperations::getEntityWorldBounds(Entity e, vec2& outMin, vec2& outMax) const
	{
		if (e == INVALID_ENTITY || m_ctx.registry == nullptr || m_ctx.services == nullptr ||
			m_ctx.registry->isEntityDestroyed(e))
		{
			return false;
		}

		if (m_ctx.registry->hasComponent<Shape>(e))
		{
			const auto& s = m_ctx.registry->getComponent<Shape>(e);
			if (s.distanceFieldId == DefaultShapes::SineWave)
			{
				return false;
			}
			if (s.distanceFieldId < DefaultShapes::COUNT_2D)
			{
				WeirdEngine::vec4 meta, p0, p1;
				Shape2D::packInstance(s.distanceFieldId, s.combination, s.layer, s.material, s.smoothFactor,
									  s.parameters, meta, p0, p1);
				const uint32_t packed0 = static_cast<uint32_t>(meta.x + 0.5f);
				const uint16_t gpuShapeId = static_cast<uint16_t>(packed0 & 0xFFFFu);
				const uint32_t flags = packed0 >> 19u;
				Shape2D::computeBuiltinBounds(gpuShapeId, flags, meta.w, p0, p1, outMin, outMax);
				return isFiniteVec2(outMin) && isFiniteVec2(outMax);
			}
			const auto* sdfBounds = m_ctx.services->shapes().getSDFBounds();
			if (sdfBounds && s.distanceFieldId < sdfBounds->size() && (*sdfBounds)[s.distanceFieldId])
			{
				float evalParams[12] = {0.0f};
				std::copy_n(s.parameters, 8, evalParams);
				evalParams[SystemParams::TIME] = m_ctx.services->time().time();
				evalParams[SystemParams::AUDIO_VOLUME] = 0.0f;
				const auto& boundsExpr = (*sdfBounds)[s.distanceFieldId];
				boundsExpr->evaluate(evalParams, outMin, outMax);
				if (isFiniteVec2(outMin) && isFiniteVec2(outMax))
				{
					return true;
				}
			}
			return false;
		}

		if (m_ctx.registry->hasComponent<TextRenderer>(e) && m_ctx.registry->hasComponent<Transform>(e))
		{
			return computeTextWorldBounds(m_ctx.registry->getComponent<TextRenderer>(e),
										  m_ctx.registry->getComponent<Transform>(e), outMin, outMax);
		}

		if (m_ctx.registry->hasComponent<Transform>(e))
		{
			vec2 pos(m_ctx.registry->getComponent<Transform>(e).position);
			outMin = pos - vec2(0.35f);
			outMax = pos + vec2(0.35f);
			return isFiniteVec2(outMin) && isFiniteVec2(outMax);
		}

		return false;
	}

	bool SceneOperations::doesEntityOverlapWorldBox(Entity e, vec2 boxMin, vec2 boxMax) const
	{
		if (e == INVALID_ENTITY || m_ctx.registry == nullptr || m_ctx.services == nullptr ||
			m_ctx.registry->isEntityDestroyed(e) || e == m_ctx.services->render().getCameraEntity())
		{
			return false;
		}

		const vec2 normMin = glm::min(boxMin, boxMax);
		const vec2 normMax = glm::max(boxMin, boxMax);

		if (m_ctx.registry->hasComponent<Shape>(e))
		{
			const auto& s = m_ctx.registry->getComponent<Shape>(e);
			const float timeNow = m_ctx.services->time().time();

			if (s.distanceFieldId == DefaultShapes::SineWave)
			{
				const float amp = std::abs(s.parameters[DefaultShapes::SineWave::Amplitude]);
				const float offset = s.parameters[DefaultShapes::SineWave::Offset];
				return normMax.y >= (offset - amp - 0.05f) && normMin.y <= (offset + amp + 0.05f);
			}

			if (s.distanceFieldId == DefaultShapes::Line && s.parameters[DefaultShapes::Line::OutlineThickness] <= 0.0f)
			{
				const vec2 a(s.parameters[DefaultShapes::Line::StartX], s.parameters[DefaultShapes::Line::StartY]);
				const vec2 b(s.parameters[DefaultShapes::Line::EndX], s.parameters[DefaultShapes::Line::EndY]);
				const float radius = (std::max)(0.0f, s.parameters[DefaultShapes::Line::Width]);
				return segmentIntersectsExpandedBox(a, b, normMin, normMax, radius);
			}

			if (s.distanceFieldId == DefaultShapes::Circle)
			{
				const vec2 center(s.parameters[DefaultShapes::Circle::PosX], s.parameters[DefaultShapes::Circle::PosY]);
				const float radius = (std::max)(0.0f, s.parameters[DefaultShapes::Circle::Radius]);
				const float thick = s.parameters[DefaultShapes::Circle::OutlineThickness];
				if (thick > 0.0f)
				{
					const float dMin = std::sqrt(pointToBoxDistSq(center, normMin, normMax));
					const vec2 farOffset = glm::max(glm::abs(normMin - center), glm::abs(normMax - center));
					const float dMax = glm::length(farOffset);
					return dMin <= (radius + thick) && dMax >= (std::max)(0.0f, radius - thick);
				}
				return pointToBoxDistSq(center, normMin, normMax) <= radius * radius;
			}

			if (s.distanceFieldId == DefaultShapes::Box && s.parameters[DefaultShapes::Box::Angle] == 0.0f &&
				s.parameters[DefaultShapes::Box::Radius] <= 0.0f &&
				s.parameters[DefaultShapes::Box::OutlineThickness] <= 0.0f)
			{
				vec2 entMin, entMax;
				if (getEntityWorldBounds(e, entMin, entMax))
				{
					return entMax.x >= normMin.x && entMin.x <= normMax.x && entMax.y >= normMin.y &&
						   entMin.y <= normMax.y;
				}
			}

			vec2 entMin, entMax;
			const bool hasFiniteBounds = getEntityWorldBounds(e, entMin, entMax);
			if (hasFiniteBounds)
			{
				if (entMax.x < normMin.x || entMin.x > normMax.x || entMax.y < normMin.y || entMin.y > normMax.y)
				{
					return false;
				}
				if (entMin.x >= normMin.x && entMax.x <= normMax.x && entMin.y >= normMin.y && entMax.y <= normMax.y)
				{
					return true;
				}
			}

			// Exact vertex/edge check for rotated boxes so diagonal thin bars never miss between samples
			if (s.distanceFieldId == DefaultShapes::Box && s.parameters[DefaultShapes::Box::Angle] != 0.0f)
			{
				const float cx = s.parameters[DefaultShapes::Box::PosX];
				const float cy = s.parameters[DefaultShapes::Box::PosY];
				const float sx = std::abs(s.parameters[DefaultShapes::Box::SizeX]);
				const float sy = std::abs(s.parameters[DefaultShapes::Box::SizeY]);
				const float angle = s.parameters[DefaultShapes::Box::Angle];
				const float extra = (std::max)(0.0f, s.parameters[DefaultShapes::Box::OutlineThickness]);
				const float c = std::cos(angle);
				const float sn = std::sin(angle);
				// Note rotated SDF uses R(angle)*(p - c), so inverse maps local corners via R(-angle)
				const vec2 local[4] = {{-sx - extra, -sy - extra},
									   {sx + extra, -sy - extra},
									   {sx + extra, sy + extra},
									   {-sx - extra, sy + extra}};
				vec2 corners[4];
				for (int k = 0; k < 4; ++k)
				{
					corners[k] = vec2(cx + c * local[k].x + sn * local[k].y, cy - sn * local[k].x + c * local[k].y);
					if (isPointInAABB(corners[k], normMin, normMax))
					{
						return true;
					}
				}
				for (int k = 0; k < 4; ++k)
				{
					if (segmentIntersectsAABB(corners[k], corners[(k + 1) & 3], normMin, normMax))
					{
						return true;
					}
				}
			}

			const auto& sdfs = m_ctx.services->shapes().getSDFs();
			if (s.distanceFieldId < DefaultShapes::COUNT_2D ||
				(s.distanceFieldId < sdfs.size() && sdfs[s.distanceFieldId]))
			{
				auto evalAt = [&](vec2 p) -> float
				{ return m_ctx.services->shapes().evaluateShape2D(s, p, timeNow, 0.0f); };

				const vec2 searchMin = hasFiniteBounds ? glm::max(entMin, normMin) : normMin;
				const vec2 searchMax = hasFiniteBounds ? glm::min(entMax, normMax) : normMax;
				if (searchMax.x >= searchMin.x && searchMax.y >= searchMin.y)
				{
					const vec2 boxCenter = (searchMin + searchMax) * 0.5f;
					const vec2 clampedEntityPos = glm::clamp(getShapePosition(s, e), searchMin, searchMax);
					vec2 bestPt = boxCenter;
					float bestDist = evalAt(bestPt);
					if (bestDist <= 0.0f)
					{
						return true;
					}

					const float dClamped = evalAt(clampedEntityPos);
					if (dClamped <= 0.0f)
					{
						return true;
					}
					if (dClamped < bestDist)
					{
						bestDist = dClamped;
						bestPt = clampedEntityPos;
					}

					constexpr int kSamples = 5;
					for (int iy = 0; iy < kSamples; ++iy)
					{
						const float ty = static_cast<float>(iy) / static_cast<float>(kSamples - 1);
						const float y = searchMin.y + (searchMax.y - searchMin.y) * ty;
						for (int ix = 0; ix < kSamples; ++ix)
						{
							const float tx = static_cast<float>(ix) / static_cast<float>(kSamples - 1);
							const vec2 p(searchMin.x + (searchMax.x - searchMin.x) * tx, y);
							const float d = evalAt(p);
							if (d <= 0.0f)
							{
								return true;
							}
							if (d < bestDist)
							{
								bestDist = d;
								bestPt = p;
							}
						}
					}

					// Projected SDF descent inside overlap domain to catch thin grazing intersections
					const vec2 span = searchMax - searchMin;
					const float eps = std::clamp((std::max)(span.x, span.y) * 0.01f, 1e-4f, 0.02f);
					vec2 p = bestPt;
					for (int stepIdx = 0; stepIdx < 4; ++stepIdx)
					{
						const float d = evalAt(p);
						if (d <= 0.0f)
						{
							return true;
						}
						const float gx = (evalAt(vec2(p.x + eps, p.y)) - evalAt(vec2(p.x - eps, p.y))) / (2.0f * eps);
						const float gy = (evalAt(vec2(p.x, p.y + eps)) - evalAt(vec2(p.x, p.y - eps))) / (2.0f * eps);
						const vec2 grad(gx, gy);
						const float gradLen = glm::length(grad);
						if (gradLen <= 1e-5f)
						{
							break;
						}
						vec2 nextP = glm::clamp(p - (d / gradLen) * (grad / gradLen), searchMin, searchMax);
						if (glm::distance(nextP, p) <= 1e-5f)
						{
							break;
						}
						p = nextP;
					}
					if (evalAt(p) <= 0.0f)
					{
						return true;
					}
				}
			}

			return false;
		}

		if (m_ctx.registry->hasComponent<TextRenderer>(e) && m_ctx.registry->hasComponent<Transform>(e))
		{
			vec2 tMin, tMax;
			if (computeTextWorldBounds(m_ctx.registry->getComponent<TextRenderer>(e),
									   m_ctx.registry->getComponent<Transform>(e), tMin, tMax))
			{
				return tMax.x >= normMin.x && tMin.x <= normMax.x && tMax.y >= normMin.y && tMin.y <= normMax.y;
			}
		}

		if (m_ctx.registry->hasComponent<Transform>(e))
		{
			const vec2 pos(m_ctx.registry->getComponent<Transform>(e).position);
			const float pad = 0.25f;
			return pos.x + pad >= normMin.x && pos.x - pad <= normMax.x && pos.y + pad >= normMin.y &&
				   pos.y - pad <= normMax.y;
		}

		return false;
	}

	void SceneOperations::selectInWorldBox(vec2 boxMin, vec2 boxMax, const std::unordered_set<Entity>& baseEntities,
										   const std::unordered_set<uint32_t>& baseClusterIds)
	{
		if (m_ctx.registry == nullptr || m_ctx.services == nullptr)
		{
			return;
		}

		const vec2 normMin = glm::min(boxMin, boxMax);
		const vec2 normMax = glm::max(boxMin, boxMax);

		std::unordered_set<Entity> newEntities = baseEntities;
		std::unordered_set<uint32_t> newClusterIds = baseClusterIds;

		if (!m_ctx.selectIndividualClusterEntities)
		{
			for (const auto& cluster : m_ctx.services->clusters().getAll())
			{
				for (Entity member : cluster.entities)
				{
					if (doesEntityOverlapWorldBox(member, normMin, normMax))
					{
						newClusterIds.insert(cluster.id);
						break;
					}
				}
			}
		}

		const Entity camEntity = m_ctx.services->render().getCameraEntity();
		for (Entity e = 0; e < m_ctx.registry->getEntityCount(); ++e)
		{
			if (e == camEntity || m_ctx.registry->isEntityDestroyed(e))
			{
				continue;
			}
			if (!m_ctx.selectIndividualClusterEntities && m_ctx.services->clusters().getClusterForEntity(e) != nullptr)
			{
				continue;
			}
			const bool hasVisual = m_ctx.registry->hasComponent<Shape>(e) ||
								   m_ctx.registry->hasComponent<TextRenderer>(e) ||
								   m_ctx.registry->hasComponent<Transform>(e);
			if (!hasVisual)
			{
				continue;
			}
			if (doesEntityOverlapWorldBox(e, normMin, normMax))
			{
				newEntities.insert(e);
			}
		}

		// Ensure no entity owned by a selected cluster is duplicated in selectedEntities
		for (uint32_t cId : newClusterIds)
		{
			if (const auto* cluster = m_ctx.services->clusters().getById(cId))
			{
				for (Entity member : cluster->entities)
				{
					newEntities.erase(member);
				}
			}
		}

		m_ctx.selectedGroupId = 0;
		m_ctx.selectedEntities = std::move(newEntities);
		m_ctx.selectedClusterIds = std::move(newClusterIds);
		m_ctx.selectedEntity = m_ctx.selectedEntities.empty() ? INVALID_ENTITY : *m_ctx.selectedEntities.begin();
		m_ctx.selectedClusterId = m_ctx.selectedClusterIds.empty() ? 0 : *m_ctx.selectedClusterIds.begin();

		if (m_ctx.selectedClusterId != 0)
		{
			m_ctx.shiftAnchorItem = {HierarchyItemType::Cluster, m_ctx.selectedClusterId};
			m_ctx.hasShiftAnchor = true;
		}
		else if (m_ctx.selectedEntity != INVALID_ENTITY)
		{
			m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(m_ctx.selectedEntity)};
			m_ctx.hasShiftAnchor = true;
		}
		else
		{
			m_ctx.hasShiftAnchor = false;
		}
	}

	WorldPickResult SceneOperations::hitTestAtWorld(vec2 worldPos, float worldPerPixel) const
	{
		if (m_ctx.registry == nullptr || m_ctx.services == nullptr)
		{
			return {};
		}

		if (worldPerPixel <= 0.0f)
		{
			Entity camEntity = m_ctx.services->render().getCameraEntity();
			if (m_ctx.registry->hasComponent<Transform>(camEntity))
			{
				const auto& cam = m_ctx.registry->getComponent<Transform>(camEntity);
				const float halfH = (std::max)(1.0f, static_cast<float>(WeirdRenderer::Display::height) * 0.5f);
				worldPerPixel = std::abs(cam.position.z) / halfH;
			}
		}

		const float edgeTol = (worldPerPixel > 0.0f) ? std::clamp(worldPerPixel * 4.0f, 0.03f, 0.18f) : 0.1f;
		const float markerTol = (worldPerPixel > 0.0f) ? std::clamp(worldPerPixel * 12.0f, 0.15f, 0.45f) : 0.35f;
		const bool pickIndividual = m_ctx.selectIndividualClusterEntities || ImGui::GetIO().KeyAlt;
		const Entity camEntity = m_ctx.services->render().getCameraEntity();

		auto allowsEdgeProximity = [&](const Shape& s) -> bool
		{
			if (Shape2D::getOutlineThickness(s.distanceFieldId, s.parameters) > 0.0f)
			{
				return true;
			}
			return s.distanceFieldId == DefaultShapes::Line &&
				   std::abs(s.parameters[DefaultShapes::Line::Width]) < edgeTol;
		};

		auto makeHitForEntity = [&](Entity e, bool directInside) -> WorldPickResult
		{
			WorldPickResult res;
			const auto* parentCluster = m_ctx.services->clusters().getClusterForEntity(e);
			if (parentCluster && !pickIndividual)
			{
				res.kind = WorldPickResult::Kind::Cluster;
				res.clusterId = parentCluster->id;
			}
			else
			{
				res.kind = WorldPickResult::Kind::Entity;
				res.entity = e;
			}
			res.isDirectInside = directInside;
			return res;
		};

		WorldPickResult bestInsideHit;
		uint16_t bestInsideLayer = 0;
		int32_t bestInsideOrder = std::numeric_limits<int32_t>::min();
		size_t bestInsideIdx = 0;

		WorldPickResult bestEdgeHit;
		float bestEdgeDist = edgeTol;

		WorldPickResult bestUnboundedHit;
		uint16_t bestUnboundedLayer = 0;
		int32_t bestUnboundedOrder = std::numeric_limits<int32_t>::min();
		size_t bestUnboundedIdx = 0;

		auto isLaterShape = [](uint16_t layer, int32_t order, size_t idx, uint16_t bestLayer, int32_t bestOrder,
							   size_t bestIdx) -> bool
		{
			if (layer != bestLayer)
				return layer > bestLayer;
			if (order != bestOrder)
				return order > bestOrder;
			return idx >= bestIdx;
		};

		if (auto shapeArray = m_ctx.registry->getComponentArray<Shape>())
		{
			const auto& sdfs = m_ctx.services->shapes().getSDFs();
			const float timeNow = m_ctx.services->time().time();

			for (size_t i = 0; i < shapeArray->getSize(); ++i)
			{
				Entity e = shapeArray->getEntityAtIdx(i);
				if (e == camEntity || m_ctx.registry->isEntityDestroyed(e))
				{
					continue;
				}
				const auto& s = shapeArray->getDataAtIdx(i);
				if (s.distanceFieldId >= DefaultShapes::COUNT_2D &&
					(s.distanceFieldId >= sdfs.size() || !sdfs[s.distanceFieldId]))
				{
					continue;
				}

				const float dist = m_ctx.services->shapes().evaluateShape2D(s, worldPos, timeNow, 0.0f);

				if (s.distanceFieldId == DefaultShapes::SineWave)
				{
					if (dist <= 0.0f &&
						(bestUnboundedHit.kind == WorldPickResult::Kind::None ||
						 isLaterShape(s.layer, s.order, i, bestUnboundedLayer, bestUnboundedOrder, bestUnboundedIdx)))
					{
						bestUnboundedLayer = s.layer;
						bestUnboundedOrder = s.order;
						bestUnboundedIdx = i;
						bestUnboundedHit = makeHitForEntity(e, true);
					}
					continue;
				}

				if (dist <= 0.0f)
				{
					if (bestInsideHit.kind == WorldPickResult::Kind::None ||
						isLaterShape(s.layer, s.order, i, bestInsideLayer, bestInsideOrder, bestInsideIdx))
					{
						bestInsideLayer = s.layer;
						bestInsideOrder = s.order;
						bestInsideIdx = i;
						bestInsideHit = makeHitForEntity(e, true);
					}
				}
				else if (dist < bestEdgeDist && allowsEdgeProximity(s))
				{
					bestEdgeDist = dist;
					bestEdgeHit = makeHitForEntity(e, false);
				}
			}
		}

		if (bestInsideHit.kind != WorldPickResult::Kind::None)
		{
			return bestInsideHit;
		}

		if (auto textArray = m_ctx.registry->getComponentArray<TextRenderer>())
		{
			for (int i = static_cast<int>(textArray->getSize()) - 1; i >= 0; --i)
			{
				Entity e = textArray->getEntityAtIdx(i);
				if (e == camEntity || !m_ctx.registry->hasComponent<Transform>(e))
				{
					continue;
				}
				vec2 tMin, tMax;
				if (computeTextWorldBounds(textArray->getDataAtIdx(i), m_ctx.registry->getComponent<Transform>(e), tMin,
										   tMax) &&
					isPointInAABB(worldPos, tMin, tMax))
				{
					return makeHitForEntity(e, true);
				}
			}
		}

		WorldPickResult bestMarkerHit;
		float bestMarkerDist = markerTol;
		if (auto transformArray = m_ctx.registry->getComponentArray<Transform>())
		{
			for (size_t i = 0; i < transformArray->getSize(); ++i)
			{
				Entity e = transformArray->getEntityAtIdx(i);
				if (e == camEntity || m_ctx.registry->isEntityDestroyed(e) || m_ctx.registry->hasComponent<Shape>(e))
				{
					continue;
				}
				const vec2 pos(transformArray->getDataAtIdx(i).position);
				const float dist = glm::distance(worldPos, pos);
				if (dist < bestMarkerDist)
				{
					bestMarkerDist = dist;
					bestMarkerHit = makeHitForEntity(e, false);
				}
			}
		}

		if (bestEdgeHit.kind != WorldPickResult::Kind::None &&
			(bestMarkerHit.kind == WorldPickResult::Kind::None || bestEdgeDist <= bestMarkerDist))
		{
			return bestEdgeHit;
		}
		if (bestMarkerHit.kind != WorldPickResult::Kind::None)
		{
			return bestMarkerHit;
		}
		if (bestUnboundedHit.kind != WorldPickResult::Kind::None)
		{
			return bestUnboundedHit;
		}

		return {};
	}

	void SceneOperations::pickAtWorld(vec2 worldPos)
	{
		const bool isCtrl = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
		const WorldPickResult hit = hitTestAtWorld(worldPos);

		if (hit.kind == WorldPickResult::Kind::Cluster)
		{
			if (!isCtrl && m_ctx.isClusterSelected(hit.clusterId))
			{
				m_ctx.shiftAnchorItem = {HierarchyItemType::Cluster, hit.clusterId};
				m_ctx.hasShiftAnchor = true;
				return;
			}
			selectCluster(hit.clusterId, isCtrl);
			m_ctx.shiftAnchorItem = {HierarchyItemType::Cluster, hit.clusterId};
			m_ctx.hasShiftAnchor = true;
			return;
		}

		if (hit.kind == WorldPickResult::Kind::Entity)
		{
			if (!isCtrl && m_ctx.isEntitySelected(hit.entity))
			{
				m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(hit.entity)};
				m_ctx.hasShiftAnchor = true;
				return;
			}
			selectEntity(hit.entity, isCtrl);
			m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(hit.entity)};
			m_ctx.hasShiftAnchor = true;
			return;
		}

		if (!isCtrl)
		{
			deselectAll();
		}
	}

	void SceneOperations::selectEntity(Entity e, bool multiSelect)
	{
		if (e == INVALID_ENTITY)
		{
			if (!multiSelect)
			{
				deselectAll();
			}
			return;
		}

		if (multiSelect)
		{
			if (m_ctx.selectedGroupId != 0)
			{
				if (const auto* g = m_ctx.findGroup(m_ctx.selectedGroupId))
				{
					for (Entity ge : g->entities)
					{
						m_ctx.selectedEntities.insert(ge);
					}
					for (uint32_t gc : g->clusterIds)
					{
						m_ctx.selectedClusterIds.insert(gc);
					}
				}
				m_ctx.selectedGroupId = 0;
			}
			if (m_ctx.selectedEntities.count(e))
			{
				m_ctx.selectedEntities.erase(e);
				if (m_ctx.selectedEntity == e)
				{
					m_ctx.selectedEntity =
						m_ctx.selectedEntities.empty() ? INVALID_ENTITY : *m_ctx.selectedEntities.begin();
				}
			}
			else
			{
				m_ctx.selectedEntities.insert(e);
				m_ctx.selectedEntity = e;
			}
		}
		else
		{
			m_ctx.selectedClusterIds.clear();
			m_ctx.selectedClusterId = 0;
			m_ctx.selectedGroupId = 0;
			m_ctx.selectedEntities.clear();
			m_ctx.selectedEntities.insert(e);
			m_ctx.selectedEntity = e;
		}
	}

	void SceneOperations::selectCluster(uint32_t cId, bool multiSelect)
	{
		if (cId == 0)
		{
			if (!multiSelect)
			{
				deselectAll();
			}
			return;
		}

		if (multiSelect)
		{
			if (m_ctx.selectedGroupId != 0)
			{
				if (const auto* g = m_ctx.findGroup(m_ctx.selectedGroupId))
				{
					for (Entity ge : g->entities)
					{
						m_ctx.selectedEntities.insert(ge);
					}
					for (uint32_t gc : g->clusterIds)
					{
						m_ctx.selectedClusterIds.insert(gc);
					}
				}
				m_ctx.selectedGroupId = 0;
			}
			if (m_ctx.selectedClusterIds.count(cId))
			{
				m_ctx.selectedClusterIds.erase(cId);
				if (m_ctx.selectedClusterId == cId)
				{
					m_ctx.selectedClusterId = m_ctx.selectedClusterIds.empty() ? 0 : *m_ctx.selectedClusterIds.begin();
				}
			}
			else
			{
				m_ctx.selectedClusterIds.insert(cId);
				m_ctx.selectedClusterId = cId;
				auto* c = m_ctx.services->clusters().getById(cId);
				if (c)
				{
					for (Entity member : c->entities)
					{
						m_ctx.selectedEntities.erase(member);
					}
					if (m_ctx.selectedEntity != INVALID_ENTITY && !m_ctx.selectedEntities.count(m_ctx.selectedEntity))
					{
						m_ctx.selectedEntity =
							m_ctx.selectedEntities.empty() ? INVALID_ENTITY : *m_ctx.selectedEntities.begin();
					}
				}
			}
		}
		else
		{
			m_ctx.selectedEntities.clear();
			m_ctx.selectedEntity = INVALID_ENTITY;
			m_ctx.selectedGroupId = 0;
			m_ctx.selectedClusterIds.clear();
			m_ctx.selectedClusterIds.insert(cId);
			m_ctx.selectedClusterId = cId;
		}
	}

	void SceneOperations::selectGroup(uint32_t gId)
	{
		deselectAll();
		m_ctx.selectedGroupId = gId;
	}

	void SceneOperations::selectItem(const HierarchyItem& item, bool multiSelect)
	{
		if (item.type == HierarchyItemType::Entity)
		{
			selectEntity(static_cast<Entity>(item.id), multiSelect);
		}
		else if (item.type == HierarchyItemType::Cluster)
		{
			selectCluster(item.id, multiSelect);
		}
		else if (item.type == HierarchyItemType::Group)
		{
			selectGroup(item.id);
		}
	}

	void SceneOperations::handleHierarchyItemClick(const HierarchyItem& item)
	{
		bool isShift = ImGui::GetIO().KeyShift;
		bool isCtrl = ImGui::GetIO().KeyCtrl;

		if (isShift && m_ctx.hasShiftAnchor)
		{
			// Shift-click range selection!
			m_ctx.pendingShiftTarget = item;
			m_ctx.pendingShiftIsCtrl = isCtrl;
			m_ctx.hasPendingShift = true;
		}
		else if (isCtrl)
		{
			selectItem(item, true);
			m_ctx.shiftAnchorItem = item;
			m_ctx.hasShiftAnchor = true;
		}
		else
		{
			selectItem(item, false);
			m_ctx.shiftAnchorItem = item;
			m_ctx.hasShiftAnchor = true;
		}
	}

	void SceneOperations::applyRangeSelection(const HierarchyItem& fromItem, const HierarchyItem& toItem,
											  bool keepExistingSelection)
	{
		int idxA = -1;
		int idxB = -1;

		for (int i = 0; i < static_cast<int>(m_ctx.visibleHierarchyItems.size()); ++i)
		{
			if (m_ctx.visibleHierarchyItems[i] == fromItem)
				idxA = i;
			if (m_ctx.visibleHierarchyItems[i] == toItem)
				idxB = i;
		}

		if (idxA == -1 || idxB == -1)
		{
			selectItem(toItem, keepExistingSelection);
			m_ctx.shiftAnchorItem = toItem;
			m_ctx.hasShiftAnchor = true;
			return;
		}

		if (!keepExistingSelection)
		{
			deselectAll();
		}

		int startIdx = std::min(idxA, idxB);
		int endIdx = std::max(idxA, idxB);

		// Pre-gather any clusters in range to avoid double-selecting their member entities
		std::unordered_set<uint32_t> clustersInRange;
		for (int i = startIdx; i <= endIdx; ++i)
		{
			if (m_ctx.visibleHierarchyItems[i].type == HierarchyItemType::Cluster)
			{
				clustersInRange.insert(m_ctx.visibleHierarchyItems[i].id);
			}
		}

		for (int i = startIdx; i <= endIdx; ++i)
		{
			const auto& item = m_ctx.visibleHierarchyItems[i];
			if (item.type == HierarchyItemType::Entity)
			{
				Entity e = static_cast<Entity>(item.id);
				auto* c = m_ctx.services->clusters().getClusterForEntity(e);
				if (c && clustersInRange.count(c->id))
				{
					continue; // Parent cluster is already selected in range
				}
				m_ctx.selectedEntities.insert(e);
				m_ctx.selectedEntity = e;
			}
			else if (item.type == HierarchyItemType::Cluster)
			{
				m_ctx.selectedClusterIds.insert(item.id);
				m_ctx.selectedClusterId = item.id;
				auto* c = m_ctx.services->clusters().getById(item.id);
				if (c)
				{
					for (Entity member : c->entities)
					{
						m_ctx.selectedEntities.erase(member);
					}
				}
			}
			else if (item.type == HierarchyItemType::Group)
			{
				auto* g = findGroup(item.id);
				if (g)
				{
					for (Entity e : g->entities)
						m_ctx.selectedEntities.insert(e);
					for (uint32_t cId : g->clusterIds)
						m_ctx.selectedClusterIds.insert(cId);
				}
			}
		}

		// Keep shiftAnchorItem unchanged so subsequent Shift-clicks adjust the range!
		m_ctx.hasShiftAnchor = true;
	}

	void SceneOperations::deselectAll()
	{
		m_ctx.deselectAll();
	}

	// =====================================================================
	// Simulation
	// =====================================================================

	void SceneOperations::toggleSimulation()
	{
		m_ctx.clearDragState();
		m_ctx.clearBoxSelectState();
		if (!m_ctx.isSimulating)
		{
			if (hasPendingEditorEdit())
			{
				syncUndoStateIfChanged();
			}
			m_ctx.isSimulating = true;
			// Take temporary snapshot before physics simulation
			m_ctx.services->serialization().saveSnapshot("temp_editor.weirdsnap");
			m_ctx.hasSnapshot = true;
			m_ctx.simulation2D->setGravity(m_ctx.playGravity);
			m_ctx.simulation2D->setDamping(m_ctx.playDamping);
			m_ctx.simulation2D->resume();
			Logger::log("[SceneEditor] Simulation started (play mode).");
		}
		else
		{
			m_ctx.isSimulating = false;
			// Revert to snapshot on returning to design mode (keep simulation resumed so renderer time stays alive)
			if (m_ctx.hasSnapshot)
			{
				m_ctx.services->serialization().loadSnapshot("temp_editor.weirdsnap");
				Logger::log("[SceneEditor] Simulation stopped (design mode, reverted to snapshot).");
			}
			m_ctx.simulation2D->setGravity(0.0f);
			m_ctx.simulation2D->setDamping(1.0f);
			m_ctx.simulation2D->resume();
			flushDesignPhysicsSync();
		}
	}

	// =====================================================================
	// Shape Layers & Evaluation Ordering
	// =====================================================================

	std::vector<Entity> SceneOperations::getShapesInLayer(uint16_t layer) const
	{
		std::vector<Entity> result;
		if (m_ctx.registry == nullptr || m_ctx.services == nullptr)
		{
			return result;
		}

		const Entity camEntity = m_ctx.services->render().getCameraEntity();
		auto shapeArray = m_ctx.registry->getComponentArray<Shape>();
		if (!shapeArray)
		{
			return result;
		}

		struct Entry
		{
			Entity entity;
			int32_t order;
			size_t compIdx;
		};
		std::vector<Entry> entries;
		entries.reserve(shapeArray->getSize());

		for (size_t i = 0; i < shapeArray->getSize(); ++i)
		{
			Entity e = shapeArray->getEntityAtIdx(i);
			if (e == camEntity || !m_ctx.registry->isEntityAlive(e))
			{
				continue;
			}
			const auto& s = shapeArray->getDataAtIdx(i);
			if (s.layer == layer)
			{
				entries.push_back({e, s.order, i});
			}
		}

		std::sort(entries.begin(), entries.end(),
				  [](const Entry& a, const Entry& b)
				  {
					  if (a.order != b.order)
						  return a.order < b.order;
					  return a.compIdx < b.compIdx;
				  });

		result.reserve(entries.size());
		for (const auto& entry : entries)
		{
			result.push_back(entry.entity);
		}
		return result;
	}

	std::vector<uint16_t> SceneOperations::getUsedShapeLayers() const
	{
		std::vector<uint16_t> layers;
		if (m_ctx.registry == nullptr || m_ctx.services == nullptr)
		{
			return layers;
		}

		const Entity camEntity = m_ctx.services->render().getCameraEntity();
		auto shapeArray = m_ctx.registry->getComponentArray<Shape>();
		if (!shapeArray)
		{
			return layers;
		}

		for (size_t i = 0; i < shapeArray->getSize(); ++i)
		{
			Entity e = shapeArray->getEntityAtIdx(i);
			if (e == camEntity || !m_ctx.registry->isEntityAlive(e))
			{
				continue;
			}
			layers.push_back(shapeArray->getDataAtIdx(i).layer);
		}

		std::sort(layers.begin(), layers.end());
		layers.erase(std::unique(layers.begin(), layers.end()), layers.end());
		return layers;
	}

	void SceneOperations::normalizeLayerOrders(uint16_t layer)
	{
		auto shapes = getShapesInLayer(layer);
		for (size_t i = 0; i < shapes.size(); ++i)
		{
			auto& s = m_ctx.registry->getComponent<Shape>(shapes[i]);
			const int32_t expected = static_cast<int32_t>(i);
			if (s.order != expected)
			{
				s.order = expected;
				m_ctx.registry->setComponentDirty(s);
				m_ctx.services->clusters().markEntityOverride(shapes[i], ClusterOverrideFlag::ShapeOrder);
			}
		}
	}

	void SceneOperations::setShapeLayer(Entity e, uint16_t newLayer)
	{
		if (m_ctx.registry == nullptr || !m_ctx.registry->isEntityAlive(e) || !m_ctx.registry->hasComponent<Shape>(e))
		{
			return;
		}

		auto& s = m_ctx.registry->getComponent<Shape>(e);
		if (s.layer == newLayer)
		{
			return;
		}

		const uint16_t oldLayer = s.layer;
		auto targetShapes = getShapesInLayer(newLayer);
		int32_t maxOrder = -1;
		for (Entity other : targetShapes)
		{
			maxOrder = (std::max)(maxOrder, m_ctx.registry->getComponent<Shape>(other).order);
		}

		s.layer = newLayer;
		s.order = maxOrder + 1;
		m_ctx.registry->setComponentDirty(s);
		m_ctx.services->clusters().markEntityOverride(e, ClusterOverrideFlag::ShapeLayer | ClusterOverrideFlag::ShapeOrder);

		normalizeLayerOrders(oldLayer);
		normalizeLayerOrders(newLayer);

		m_ctx.worldRenderContext->shapesNeedUpdate = true;
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
	}

	void SceneOperations::moveShapeOrderInLayer(Entity e, int delta)
	{
		if (delta == 0 || m_ctx.registry == nullptr || !m_ctx.registry->isEntityAlive(e) ||
			!m_ctx.registry->hasComponent<Shape>(e))
		{
			return;
		}

		const uint16_t layer = m_ctx.registry->getComponent<Shape>(e).layer;
		auto shapes = getShapesInLayer(layer);
		auto it = std::find(shapes.begin(), shapes.end(), e);
		if (it == shapes.end() || shapes.size() <= 1)
		{
			return;
		}

		const int curIdx = static_cast<int>(it - shapes.begin());
		const int newIdx = std::clamp(curIdx + delta, 0, static_cast<int>(shapes.size()) - 1);
		if (newIdx == curIdx)
		{
			return;
		}

		shapes.erase(shapes.begin() + curIdx);
		shapes.insert(shapes.begin() + newIdx, e);

		for (size_t i = 0; i < shapes.size(); ++i)
		{
			auto& s = m_ctx.registry->getComponent<Shape>(shapes[i]);
			s.order = static_cast<int32_t>(i);
			m_ctx.registry->setComponentDirty(s);
			m_ctx.services->clusters().markEntityOverride(shapes[i], ClusterOverrideFlag::ShapeOrder);
		}

		m_ctx.worldRenderContext->shapesNeedUpdate = true;
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
	}

	void SceneOperations::moveShapeToLayerEdge(Entity e, bool toEnd)
	{
		if (m_ctx.registry == nullptr || !m_ctx.registry->isEntityAlive(e) || !m_ctx.registry->hasComponent<Shape>(e))
		{
			return;
		}

		const uint16_t layer = m_ctx.registry->getComponent<Shape>(e).layer;
		auto shapes = getShapesInLayer(layer);
		auto it = std::find(shapes.begin(), shapes.end(), e);
		if (it == shapes.end() || shapes.size() <= 1)
		{
			return;
		}

		const size_t curIdx = static_cast<size_t>(it - shapes.begin());
		const size_t targetIdx = toEnd ? (shapes.size() - 1) : 0;
		if (curIdx == targetIdx)
		{
			return;
		}

		shapes.erase(shapes.begin() + static_cast<ptrdiff_t>(curIdx));
		shapes.insert(shapes.begin() + static_cast<ptrdiff_t>(targetIdx), e);

		for (size_t i = 0; i < shapes.size(); ++i)
		{
			auto& s = m_ctx.registry->getComponent<Shape>(shapes[i]);
			s.order = static_cast<int32_t>(i);
			m_ctx.registry->setComponentDirty(s);
			m_ctx.services->clusters().markEntityOverride(shapes[i], ClusterOverrideFlag::ShapeOrder);
		}

		m_ctx.worldRenderContext->shapesNeedUpdate = true;
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
	}

	void SceneOperations::reorderShapeRelativeTo(Entity srcEntity, Entity dstEntity, bool insertAfter)
	{
		if (srcEntity == dstEntity || m_ctx.registry == nullptr || !m_ctx.registry->isEntityAlive(srcEntity) ||
			!m_ctx.registry->isEntityAlive(dstEntity) || !m_ctx.registry->hasComponent<Shape>(srcEntity) ||
			!m_ctx.registry->hasComponent<Shape>(dstEntity))
		{
			return;
		}

		auto& srcShape = m_ctx.registry->getComponent<Shape>(srcEntity);
		const uint16_t oldLayer = srcShape.layer;
		const uint16_t targetLayer = m_ctx.registry->getComponent<Shape>(dstEntity).layer;

		auto targetShapes = getShapesInLayer(targetLayer);
		targetShapes.erase(std::remove(targetShapes.begin(), targetShapes.end(), srcEntity), targetShapes.end());

		auto dstIt = std::find(targetShapes.begin(), targetShapes.end(), dstEntity);
		if (dstIt == targetShapes.end())
		{
			return;
		}

		if (insertAfter)
		{
			++dstIt;
		}
		targetShapes.insert(dstIt, srcEntity);

		if (oldLayer != targetLayer)
		{
			srcShape.layer = targetLayer;
			m_ctx.services->clusters().markEntityOverride(srcEntity, ClusterOverrideFlag::ShapeLayer);
		}

		for (size_t i = 0; i < targetShapes.size(); ++i)
		{
			auto& s = m_ctx.registry->getComponent<Shape>(targetShapes[i]);
			s.order = static_cast<int32_t>(i);
			m_ctx.registry->setComponentDirty(s);
			m_ctx.services->clusters().markEntityOverride(targetShapes[i], ClusterOverrideFlag::ShapeOrder);
		}

		if (oldLayer != targetLayer)
		{
			normalizeLayerOrders(oldLayer);
		}

		m_ctx.worldRenderContext->shapesNeedUpdate = true;
		m_ctx.isSceneDirty = true;
		m_pendingUndoSync = true;
	}

	uint32_t SceneOperations::getEntityGroupId(Entity e) const
	{
		for (const auto& g : m_ctx.groups)
		{
			if (std::find(g.entities.begin(), g.entities.end(), e) != g.entities.end())
			{
				return g.id;
			}
		}
		return 0;
	}

	// =====================================================================
	// Formatting
	// =====================================================================

	std::string SceneOperations::formatEntityLabel(Entity e, const ClusterInstance* parentCluster) const
	{
		if (e == m_ctx.services->render().getCameraEntity())
		{
			return "Main Camera (Entity " + std::to_string(static_cast<uint32_t>(e)) + ")";
		}

		if (parentCluster)
		{
			for (const auto& [name, ent] : parentCluster->localNames)
			{
				if (ent == e)
				{
					return name + " (Entity " + std::to_string(static_cast<uint32_t>(e)) + ")";
				}
			}
		}

		std::string name = m_ctx.services->names().getName(e);
		if (!name.empty())
		{
			return name + " (Entity " + std::to_string(static_cast<uint32_t>(e)) + ")";
		}

		return "Entity " + std::to_string(static_cast<uint32_t>(e));
	}

	// =====================================================================
	// Undo / Redo
	// =====================================================================

	bool SceneOperations::hasPendingEditorEdit() const
	{
		return m_pendingUndoSync || (m_ctx.isSceneDirty.editCount != m_lastSyncedEditCount);
	}

	void SceneOperations::flushDesignPhysicsSync()
	{
		if (m_ctx.registry == nullptr || m_ctx.simulation2D == nullptr || m_ctx.isSimulating)
		{
			return;
		}

		// Flush any queued physics commands (e.g. from SceneSerializer::loadFromJson,
		// ClusterService::instantiate, or setEntityPosition), push any dirty ECS edits via
		// PhysicsSystem2D::update, and flush again so Simulation2D read buffers match ECS before
		// captureUndoState(). Simulation2D stays resumed so constraints keep resolving freely.
		m_ctx.simulation2D->flushCommands();
		ECS::PhysicsSystem2D::update(*m_ctx.registry, *m_ctx.simulation2D);
		m_ctx.simulation2D->flushCommands();
	}

	bool SceneOperations::statesEquivalent(const SceneUndoState& a, const SceneUndoState& b)
	{
		if (a.sceneJson != b.sceneJson || a.playGravity != b.playGravity || a.playDamping != b.playDamping ||
			a.groups.size() != b.groups.size())
		{
			return false;
		}

		for (size_t i = 0; i < a.groups.size(); ++i)
		{
			const auto& ga = a.groups[i];
			const auto& gb = b.groups[i];
			if (ga.id != gb.id || ga.name != gb.name || ga.entities != gb.entities || ga.clusterIds != gb.clusterIds)
			{
				return false;
			}
		}

		return true;
	}

	SceneOperations::SceneUndoState SceneOperations::captureUndoState() const
	{
		SceneUndoState state;
		if (m_ctx.scene == nullptr || m_ctx.services == nullptr)
		{
			return state;
		}

		state.sceneJson = SceneSerializer::serializeToJson(static_cast<Scene&>(*m_ctx.scene), m_ctx.currentScenePath);
		state.sceneJson.erase("camera");
		state.groups = m_ctx.groups;
		state.nextGroupId = m_ctx.nextGroupId;
		state.clusterRestData = m_ctx.clusterRestData;
		state.playGravity = m_ctx.playGravity;
		state.playDamping = m_ctx.playDamping;
		state.sceneUpdatedFromAssets = m_ctx.sceneUpdatedFromAssets;

		state.selectedEntity = m_ctx.selectedEntity;
		state.selectedClusterId = m_ctx.selectedClusterId;
		state.selectedGroupId = m_ctx.selectedGroupId;
		state.selectedEntities = m_ctx.selectedEntities;
		state.selectedClusterIds = m_ctx.selectedClusterIds;
		return state;
	}

	void SceneOperations::applyUndoState(const SceneUndoState& state)
	{
		if (m_ctx.scene == nullptr || m_ctx.services == nullptr || m_ctx.registry == nullptr)
		{
			return;
		}

		const std::string savedScenePath = m_ctx.currentScenePath;
		const auto savedAssetWriteTimes = m_ctx.clusterAssetWriteTimes;

		resetSceneContents();

		m_ctx.currentScenePath = savedScenePath;
		m_ctx.clusterAssetWriteTimes = savedAssetWriteTimes;

		std::unordered_map<Entity, Entity> newToSaved;
		SceneSerializer::loadFromJson(static_cast<Scene&>(*m_ctx.scene), state.sceneJson, m_ctx.currentScenePath,
									  nullptr, false, nullptr, &newToSaved);

		std::unordered_map<Entity, Entity> savedToNew;
		savedToNew.reserve(newToSaved.size());
		for (const auto& [newEnt, savedEnt] : newToSaved)
		{
			savedToNew[savedEnt] = newEnt;
		}

		const auto remapEntity = [&](Entity savedEnt) -> Entity
		{
			auto it = savedToNew.find(savedEnt);
			if (it != savedToNew.end() && m_ctx.registry->isEntityAlive(it->second))
			{
				return it->second;
			}
			return INVALID_ENTITY;
		};

		m_ctx.clusterRestData = state.clusterRestData;
		m_ctx.playGravity = state.playGravity;
		m_ctx.playDamping = state.playDamping;
		m_ctx.sceneUpdatedFromAssets = state.sceneUpdatedFromAssets;

		m_ctx.groups.clear();
		m_ctx.nextGroupId = 1;
		for (const auto& sg : state.groups)
		{
			EditorGroup g;
			g.id = sg.id;
			g.name = sg.name;
			for (Entity savedEnt : sg.entities)
			{
				Entity mapped = remapEntity(savedEnt);
				if (mapped != INVALID_ENTITY)
				{
					g.entities.push_back(mapped);
				}
			}
			for (uint32_t cId : sg.clusterIds)
			{
				if (m_ctx.services->clusters().getById(cId) != nullptr)
				{
					g.clusterIds.push_back(cId);
				}
			}
			m_ctx.nextGroupId = (std::max)(m_ctx.nextGroupId, g.id + 1);
			m_ctx.groups.push_back(std::move(g));
		}
		m_ctx.nextGroupId = (std::max)(m_ctx.nextGroupId, state.nextGroupId);

		syncAllConstraintsClusterMembership();
		cleanGroups();

		if (state.selectedEntity != INVALID_ENTITY)
		{
			m_ctx.selectedEntity = remapEntity(state.selectedEntity);
		}
		for (Entity savedEnt : state.selectedEntities)
		{
			Entity mapped = remapEntity(savedEnt);
			if (mapped != INVALID_ENTITY)
			{
				m_ctx.selectedEntities.insert(mapped);
			}
		}
		if (state.selectedClusterId != 0 && m_ctx.services->clusters().getById(state.selectedClusterId) != nullptr)
		{
			m_ctx.selectedClusterId = state.selectedClusterId;
		}
		for (uint32_t cId : state.selectedClusterIds)
		{
			if (m_ctx.services->clusters().getById(cId) != nullptr)
			{
				m_ctx.selectedClusterIds.insert(cId);
			}
		}
		if (state.selectedGroupId != 0 && findGroup(state.selectedGroupId) != nullptr)
		{
			m_ctx.selectedGroupId = state.selectedGroupId;
		}

		flushDesignPhysicsSync();
		m_ctx.worldRenderContext->shapesNeedUpdate = true;
		m_ctx.uiRenderContext->shapesNeedUpdate = true;
		m_ctx.isSceneDirty = m_undoHistory.isDirty();
		m_committedState = captureUndoState();
		m_hasCommittedState = true;
		m_pendingUndoSync = false;
		m_lastSyncedEditCount = m_ctx.isSceneDirty.editCount;
	}

	bool SceneOperations::canUndo() const
	{
		return !m_ctx.isSimulating && m_undoHistory.canUndo();
	}

	bool SceneOperations::canRedo() const
	{
		return !m_ctx.isSimulating && m_undoHistory.canRedo();
	}

	void SceneOperations::resetUndoBaseline(bool dirty)
	{
		m_undoHistory.clear(dirty);
		m_pendingUndoSync = false;
		m_wasAnyImGuiItemActive = false;
		if (m_ctx.scene != nullptr && m_ctx.services != nullptr)
		{
			flushDesignPhysicsSync();
			m_committedState = captureUndoState();
			m_hasCommittedState = true;
			m_ctx.isSceneDirty = m_undoHistory.isDirty();
		}
		else
		{
			m_hasCommittedState = false;
			m_ctx.isSceneDirty = dirty;
		}
		m_lastSyncedEditCount = m_ctx.isSceneDirty.editCount;
	}

	void SceneOperations::noteViewportDragState(bool isDragging)
	{
		if (isDragging)
		{
			m_pendingUndoSync = true;
		}
	}

	void SceneOperations::syncUndoStateIfChanged()
	{
		if (m_ctx.scene == nullptr || m_ctx.services == nullptr || m_ctx.isSimulating)
		{
			return;
		}

		flushDesignPhysicsSync();
		SceneUndoState current = captureUndoState();
		m_pendingUndoSync = false;
		if (!m_hasCommittedState)
		{
			m_committedState = std::move(current);
			m_hasCommittedState = true;
			m_lastSyncedEditCount = m_ctx.isSceneDirty.editCount;
			return;
		}

		if (!statesEquivalent(current, m_committedState))
		{
			m_undoHistory.push(std::move(m_committedState));
			m_committedState = std::move(current);
			m_ctx.isSceneDirty = m_undoHistory.isDirty();
		}
		m_lastSyncedEditCount = m_ctx.isSceneDirty.editCount;
	}

	void SceneOperations::syncUndoStateAfterUI()
	{
		const bool isItemActive = ImGui::IsAnyItemActive();
		m_wasAnyImGuiItemActive = isItemActive;

		if (m_ctx.isSimulating || m_ctx.isDragging || m_ctx.isBoxSelecting || isItemActive ||
			ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			return;
		}

		const bool dirtyFlagMismatch = static_cast<bool>(m_ctx.isSceneDirty) != m_undoHistory.isDirty();
		if (hasPendingEditorEdit() || dirtyFlagMismatch)
		{
			syncUndoStateIfChanged();
			if (static_cast<bool>(m_ctx.isSceneDirty) && !m_undoHistory.isDirty())
			{
				m_undoHistory.markDirty();
			}
			m_lastSyncedEditCount = m_ctx.isSceneDirty.editCount;
		}
	}

	void SceneOperations::undo()
	{
		if (m_ctx.isSimulating || m_ctx.isDragging || m_ctx.isBoxSelecting || m_ctx.scene == nullptr ||
			m_ctx.services == nullptr || ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			return;
		}

		if (hasPendingEditorEdit())
		{
			syncUndoStateIfChanged();
		}

		SceneUndoState target;
		if (m_undoHistory.undo(captureUndoState(), target))
		{
			applyUndoState(target);
		}
	}

	void SceneOperations::redo()
	{
		if (m_ctx.isSimulating || m_ctx.isDragging || m_ctx.isBoxSelecting || m_ctx.scene == nullptr ||
			m_ctx.services == nullptr || ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			return;
		}

		if (hasPendingEditorEdit())
		{
			syncUndoStateIfChanged();
		}

		SceneUndoState target;
		if (m_undoHistory.redo(captureUndoState(), target))
		{
			applyUndoState(target);
		}
	}

} // namespace WeirdEngine::Tools::SceneEd
