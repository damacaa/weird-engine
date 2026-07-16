#include "weird-engine/SceneSerializer.h"
#include "weird-engine/Scene.h"

#include <fstream>
#include <iostream>
#include <json/json.h>
#include <unordered_map>
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/Spring.h"
#include "weird-physics/components/GlobalPhysicsSettings.h"

namespace WeirdEngine
{
	void SceneSerializer::save(Scene& scene, const std::string& filename)
	{
		using json = nlohmann::json;

		json j;
		j["version"] = 1;

		// Save camera state
		{
			auto& camTransform = scene.m_ecs.getComponent<Transform>(scene.m_mainCamera);
			j["camera"]["position"] = {camTransform.position.x, camTransform.position.y, camTransform.position.z};
			j["camera"]["rotation"] = {camTransform.rotation.x, camTransform.rotation.y, camTransform.rotation.z};
			j["camera"]["scale"] = {camTransform.scale.x, camTransform.scale.y, camTransform.scale.z};
		}

		// Save entities (skip the main camera entity and blacklisted entities)
		json entitiesJson = json::array();

		const auto& blacklist = scene.m_serializationBlacklist;
		auto isBlacklisted = [&](Entity e) { return e == scene.m_mainCamera || blacklist.count(e); };

		{
			auto transformArray = scene.m_ecs.getComponentArray<Transform>();
			auto customShapeArray = scene.m_ecs.getComponentArray<CustomShape>();
			auto uiShapeArray = scene.m_ecs.getComponentArray<UIShape>();
			auto dotArray = scene.m_ecs.getComponentArray<Dot>();
			auto rigidBodyArray = scene.m_ecs.getComponentArray<RigidBody2D>();
			auto textArray = scene.m_ecs.getComponentArray<TextRenderer>();

			std::unordered_map<Entity, json> entityMap;

			auto collectEntity = [&](Entity e) -> json&
			{
				if (entityMap.find(e) == entityMap.end())
				{
					entityMap[e] = json::object();
					entityMap[e]["id"] = e;
				}
				return entityMap[e];
			};

			// Transform (skip camera and blacklisted)
			for (size_t i = 0; i < transformArray->getSize(); i++)
			{
				Entity e = transformArray->getEntityAtIdx(i);
				if (isBlacklisted(e))
					continue;
				auto& t = transformArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["transform"] = {{"position", {t.position.x, t.position.y, t.position.z}},
								   {"rotation", {t.rotation.x, t.rotation.y, t.rotation.z}},
								   {"scale", {t.scale.x, t.scale.y, t.scale.z}}};
			}

			// CustomShape (skip UIShape entities – serialised separately)
			for (size_t i = 0; i < customShapeArray->getSize(); i++)
			{
				Entity e = customShapeArray->getEntityAtIdx(i);
				if (isBlacklisted(e))
					continue;
				if (uiShapeArray->hasData(e))
					continue;
				auto& s = customShapeArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["customShape"] = {{"distanceFieldId", s.distanceFieldId},
									 {"combination", static_cast<int>(s.combination)},
									 {"parameters", json(s.parameters)},
									 {"hasCollisions", s.hasCollisions},
									 {"groupIdx", s.groupIdx},
									 {"material", s.material},
									 {"smoothFactor", s.smoothFactor}};
			}

			// UIShape
			for (size_t i = 0; i < uiShapeArray->getSize(); i++)
			{
				Entity e = uiShapeArray->getEntityAtIdx(i);
				if (isBlacklisted(e))
					continue;
				auto& s = uiShapeArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["uiShape"] = {{"distanceFieldId", s.distanceFieldId},
								 {"combination", static_cast<int>(s.combination)},
								 {"parameters", json(s.parameters)},
								 {"groupIdx", s.groupIdx},
								 {"material", s.material},
								 {"smoothFactor", s.smoothFactor}};
			}

			// Dot
			for (size_t i = 0; i < dotArray->getSize(); i++)
			{
				Entity e = dotArray->getEntityAtIdx(i);
				if (isBlacklisted(e))
					continue;
				auto& r = dotArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["dot"] = {{"isStatic", r.isStatic}, {"materialId", r.materialId}};
			}

			// RigidBody2D – also save the simulation particle position
			for (size_t i = 0; i < rigidBodyArray->getSize(); i++)
			{
				Entity e = rigidBodyArray->getEntityAtIdx(i);
				if (isBlacklisted(e))
					continue;
				auto& rb = rigidBodyArray->getDataAtIdx(i);
				vec2 simPos = scene.m_simulation2D.getPosition(rb.simulationId);
				auto& ej = collectEntity(e);
				ej["rigidBody2D"] = {
					{"simulationId", rb.simulationId}, 
					{"physicsPosition", {simPos.x, simPos.y}},
					{"velocity", {rb.velocity.x, rb.velocity.y}},
					{"isFixed", rb.isFixed}
				};
			}

			// TextRenderer
			for (size_t i = 0; i < textArray->getSize(); i++)
			{
				Entity e = textArray->getEntityAtIdx(i);
				if (isBlacklisted(e))
					continue;
				auto& tr = textArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["textRenderer"] = {
					{"text", tr.text}, {"material", tr.material}, {"width", tr.width}, {"height", tr.height}};
			}

			// GlobalPhysicsSettings
			auto globalSettingsArray = scene.m_ecs.getComponentArray<GlobalPhysicsSettings>();
			if (globalSettingsArray)
			{
				for (size_t i = 0; i < globalSettingsArray->getSize(); i++)
				{
					Entity e = globalSettingsArray->getEntityAtIdx(i);
					if (isBlacklisted(e)) continue;
					auto& gs = globalSettingsArray->getDataAtIdx(i);
					auto& ej = collectEntity(e);
					ej["globalPhysicsSettings"] = {{"gravity", gs.gravity}, {"damping", gs.damping}};
				}
			}

			// Save entities in order of their ID
			// Entity order is very important for correct SDF operations
			// BIG TODO: when entities are added/removed, ensure this order is maintained
			// (deleted entitiy ids can be reused)
			for (Entity e = 0; e < scene.m_ecs.getEntityCount(); ++e)
			{
				if (isBlacklisted(e))
					continue;

				auto it = entityMap.find(e);
				if (it == entityMap.end())
					continue;

				entitiesJson.push_back(it->second);
			}
		}

		j["entities"] = entitiesJson;

		// Save entity tags
		{
			json tagsJson = json::array();
			for (const auto& [tagName, entity] : scene.m_tagToEntity)
			{
				if (isBlacklisted(entity))
					continue;
				tagsJson.push_back({{"name", tagName}, {"entityId", entity}});
			}
			j["tags"] = tagsJson;
		}

		// Save physics constraints directly from ECS components
		{
			json distanceConstraintsJson = json::array();
			auto distConstraintArray = scene.m_ecs.getComponentArray<DistanceConstraint>();
			if (distConstraintArray)
			{
				for (size_t i = 0; i < distConstraintArray->getSize(); i++)
				{
					Entity e = distConstraintArray->getEntityAtIdx(i);
					if (isBlacklisted(e)) continue;
					auto& dc = distConstraintArray->getDataAtIdx(i);
					distanceConstraintsJson.push_back({{"entityA", dc.entityA}, {"entityB", dc.entityB}, {"distance", dc.distance}});
				}
			}

			json springsJson = json::array();
			auto springArray = scene.m_ecs.getComponentArray<Spring>();
			if (springArray)
			{
				for (size_t i = 0; i < springArray->getSize(); i++)
				{
					Entity e = springArray->getEntityAtIdx(i);
					if (isBlacklisted(e)) continue;
					auto& sp = springArray->getDataAtIdx(i);
					springsJson.push_back({{"entityA", sp.entityA}, {"entityB", sp.entityB}, {"distance", sp.restDistance}, {"k", sp.stiffness}});
				}
			}

			j["physics"] = {{"distanceConstraints", distanceConstraintsJson}, {"springs", springsJson}};
		}

		std::ofstream outFile(filename);
		if (!outFile.is_open())
		{
			WeirdEngine::Logger::error("[SceneSerializer] Failed to open file for writing: " + filename);
			return;
		}
		outFile << j.dump(2);
		WeirdEngine::Logger::log("[SceneSerializer] Scene saved to " + filename);
	}

	void SceneSerializer::load(Scene& scene, const std::string& path, SceneSerializer::TagMap* outTags)
	{
		using json = nlohmann::json;

		std::ifstream inFile(path);
		if (!inFile.is_open())
		{
			WeirdEngine::Logger::error("[SceneSerializer] Failed to open .weird file: " + path);
			return;
		}

		json j;
		try
		{
			inFile >> j;
		}
		catch (const json::parse_error& e)
		{
			WeirdEngine::Logger::error("[SceneSerializer] JSON parse error in " + path + ": " + e.what());
			return;
		}

		// Restore camera state
		if (j.contains("camera"))
		{
			auto& camTransform = scene.m_ecs.getComponent<Transform>(scene.m_mainCamera);
			const auto& cam = j["camera"];
			if (cam.contains("position"))
			{
				camTransform.position = vec3(cam["position"][0], cam["position"][1], cam["position"][2]);
				scene.m_ecs.getComponentArray<Transform>()->setEntityDirty(scene.m_mainCamera, true);
			}
			if (cam.contains("rotation"))
				camTransform.rotation = vec3(cam["rotation"][0], cam["rotation"][1], cam["rotation"][2]);
			if (cam.contains("scale"))
				camTransform.scale = vec3(cam["scale"][0], cam["scale"][1], cam["scale"][2]);
		}

		// Map from saved simulationId → newly assigned simulationId
		std::unordered_map<int, SimulationID> simIdMap;
		// Map from saved entity id → newly created Entity
		std::unordered_map<Entity, Entity> entityIdMap;
		// Map from saved simulation id → newly created Entity
		std::unordered_map<int, Entity> simIdToEntityMap;

		// Restore entities
		if (j.contains("entities") && j["entities"].is_array())
		{
			for (const auto& ej : j["entities"])
			{
				Entity entity = scene.m_ecs.createEntity();

				// Track saved-id → new-entity mapping for tag remapping
				if (ej.contains("id"))
					entityIdMap[ej["id"].get<Entity>()] = entity;

				if (ej.contains("transform"))
				{
					auto& t = scene.m_ecs.addComponent<Transform>(entity);
					const auto& tj = ej["transform"];
					if (tj.contains("position"))
						t.position = vec3(tj["position"][0], tj["position"][1], tj["position"][2]);
					if (tj.contains("rotation"))
						t.rotation = vec3(tj["rotation"][0], tj["rotation"][1], tj["rotation"][2]);
					if (tj.contains("scale"))
						t.scale = vec3(tj["scale"][0], tj["scale"][1], tj["scale"][2]);
					scene.m_ecs.getComponentArray<Transform>()->setEntityDirty(entity, true);
				}

				if (ej.contains("customShape"))
				{
					auto& s = scene.m_ecs.addComponent<CustomShape>(entity);
					const auto& sj = ej["customShape"];
					s.distanceFieldId = static_cast<uint16_t>(sj.value("distanceFieldId", 0));
					s.combination = static_cast<CombinationType>(sj.value("combination", 0));
					s.hasCollisions = sj.value("hasCollisions", true);
					s.groupIdx = static_cast<uint16_t>(sj.value("groupIdx", 0));
					s.material = static_cast<uint16_t>(sj.value("material", 0));
					s.smoothFactor = sj.value("smoothFactor", 1.0f);
					if (sj.contains("parameters"))
					{
						for (int pi = 0; pi < (int)std::size(s.parameters) && pi < (int)sj["parameters"].size(); pi++)
							s.parameters[pi] = sj["parameters"][pi].get<float>();
					}
					scene.m_ecs.getComponentArray<CustomShape>()->setEntityDirty(entity, true);
					scene.m_2DWorldRenderContext.shapesNeedUpdate = true;
				}

				if (ej.contains("uiShape"))
				{
					auto& s = scene.m_ecs.addComponent<UIShape>(entity);
					const auto& sj = ej["uiShape"];
					s.distanceFieldId = static_cast<uint16_t>(sj.value("distanceFieldId", 0));
					s.combination = static_cast<CombinationType>(sj.value("combination", 0));
					s.groupIdx = static_cast<uint16_t>(sj.value("groupIdx", 0));
					s.material = static_cast<uint16_t>(sj.value("material", 0));
					s.smoothFactor = sj.value("smoothFactor", 10.0f);
					if (sj.contains("parameters"))
					{
						for (int pi = 0; pi < (int)std::size(s.parameters) && pi < (int)sj["parameters"].size(); pi++)
							s.parameters[pi] = sj["parameters"][pi].get<float>();
					}
					scene.m_UIRenderContext.shapesNeedUpdate = true;
				}

				if (ej.contains("dot"))
				{
					auto& r = scene.m_ecs.addComponent<Dot>(entity);
					const auto& rj = ej["dot"];
					r.isStatic = rj.value("isStatic", false);
					r.materialId = static_cast<unsigned int>(rj.value("materialId", 0));
				}

				if (ej.contains("textRenderer"))
				{
					auto& tr = scene.m_ecs.addComponent<TextRenderer>(entity);
					const auto& trj = ej["textRenderer"];
					tr.text = trj.value("text", std::string{});
					tr.material = static_cast<uint16_t>(trj.value("material", 0));
					tr.width = trj.value("width", 0.0f);
					tr.height = trj.value("height", 0.0f);
					scene.m_ecs.getComponentArray<TextRenderer>()->setEntityDirty(entity, true);
				}

				if (ej.contains("globalPhysicsSettings"))
				{
					auto& gs = scene.m_ecs.addComponent<GlobalPhysicsSettings>(entity);
					const auto& gsj = ej["globalPhysicsSettings"];
					gs.gravity = gsj.value("gravity", 0.0f);
					gs.damping = gsj.value("damping", 0.05f);
					scene.m_ecs.getComponentArray<GlobalPhysicsSettings>()->setEntityDirty(entity, true);
				}

				if (ej.contains("rigidBody2D"))
				{
					auto& rb = scene.m_ecs.addComponent<RigidBody2D>(entity);
					const auto& rbj = ej["rigidBody2D"];
					int savedSimId = rbj.value("simulationId", -1);

					if (rbj.contains("velocity"))
					{
						rb.velocity = vec2(rbj["velocity"][0].get<float>(), rbj["velocity"][1].get<float>());
					}
					if (rbj.contains("isFixed"))
					{
						rb.isFixed = rbj.value("isFixed", false);
					}
					scene.m_ecs.getComponentArray<RigidBody2D>()->setEntityDirty(entity, true); // Sync velocity and fixed state to simulation

					if (rbj.contains("physicsPosition"))
					{
						vec2 savedPos(rbj["physicsPosition"][0].get<float>(), rbj["physicsPosition"][1].get<float>());
						scene.m_simulation2D.setPosition(rb.simulationId, savedPos);
					}

					if (savedSimId >= 0)
					{
						simIdMap[savedSimId] = rb.simulationId;
						simIdToEntityMap[savedSimId] = entity;
					}
				}
			}
		}

		// Restore entity tags
		if (j.contains("tags") && j["tags"].is_array())
		{
			for (const auto& tagJ : j["tags"])
			{
				if (!tagJ.contains("name") || !tagJ.contains("entityId"))
					continue;
				std::string name = tagJ["name"].get<std::string>();
				Entity savedId = tagJ["entityId"].get<Entity>();
				if (name.empty())
					continue;
				auto it = entityIdMap.find(savedId);
				if (it != entityIdMap.end())
				{
					Entity newEntity = it->second;
					if (outTags)
					{
						// Store in the caller-provided map instead of the scene
						(*outTags)[name] = newEntity;
					}
					else
					{
						// Use scene's tag() method to keep both maps in sync
						scene.tag(newEntity, name);
					}
				}
			}
		}

		// Restore physics constraints using the entity mapping (with legacy fallback)
		if (j.contains("physics"))
		{
			const auto& phys = j["physics"];

			if (phys.contains("distanceConstraints"))
			{
				for (const auto& dcj : phys["distanceConstraints"])
				{
					Entity entityA = INVALID_ENTITY, entityB = INVALID_ENTITY;

					if (dcj.contains("A")) { // Legacy format
						int savedA = dcj.value("A", -1);
						int savedB = dcj.value("B", -1);
						if (simIdMap.find(savedA) != simIdMap.end() && simIdMap.find(savedB) != simIdMap.end()) {
							entityA = simIdToEntityMap[savedA];
							entityB = simIdToEntityMap[savedB];
						}
					}
					else if (dcj.contains("entityA")) { // Modern format
						Entity savedA = dcj.value("entityA", INVALID_ENTITY);
						Entity savedB = dcj.value("entityB", INVALID_ENTITY);
						if (entityIdMap.find(savedA) != entityIdMap.end() && entityIdMap.find(savedB) != entityIdMap.end()) {
							entityA = entityIdMap[savedA];
							entityB = entityIdMap[savedB];
						}
					}

					float dist = dcj.value("distance", 1.0f);
					float k = dcj.value("k", 1.0f);

					if (entityA != INVALID_ENTITY && entityB != INVALID_ENTITY)
					{
						if (k >= 1.0f)
						{
							Entity constraintEnt = scene.m_ecs.createEntity();
							auto& constraint = scene.m_ecs.addComponent<WeirdEngine::DistanceConstraint>(constraintEnt);
							constraint.entityA = entityA;
							constraint.entityB = entityB;
							constraint.distance = dist;
						}
						else
						{
							Entity springEnt = scene.m_ecs.createEntity();
							auto& spring = scene.m_ecs.addComponent<WeirdEngine::Spring>(springEnt);
							spring.entityA = entityA;
							spring.entityB = entityB;
							spring.stiffness = k;
							spring.restDistance = dist;
						}
					}
				}
			}

			if (phys.contains("springs"))
			{
				for (const auto& spj : phys["springs"])
				{
					Entity savedA = spj.value("entityA", INVALID_ENTITY);
					Entity savedB = spj.value("entityB", INVALID_ENTITY);

					if (entityIdMap.find(savedA) != entityIdMap.end() && entityIdMap.find(savedB) != entityIdMap.end())
					{
						Entity springEnt = scene.m_ecs.createEntity();
						auto& spring = scene.m_ecs.addComponent<WeirdEngine::Spring>(springEnt);
						spring.entityA = entityIdMap[savedA];
						spring.entityB = entityIdMap[savedB];
						spring.stiffness = spj.value("k", 1.0f);
						spring.restDistance = spj.value("distance", 1.0f);
					}
				}
			}

			if (phys.contains("gravitationalConstraints"))
			{
				for (const auto& gcj : phys["gravitationalConstraints"])
				{
					int savedA = gcj.value("A", -1);
					int savedB = gcj.value("B", -1);
					float g = gcj.value("g", 1.0f);

					auto itA = simIdMap.find(savedA);
					auto itB = simIdMap.find(savedB);
					if (itA != simIdMap.end() && itB != simIdMap.end())
					{
						scene.m_simulation2D.addGravitationalConstraint(itA->second, itB->second, g);
					}
				}
			}

			if (phys.contains("fixedObjects"))
			{
				for (const auto& fixedJ : phys["fixedObjects"])
				{
					int savedId = fixedJ.get<int>();
					auto it = simIdMap.find(savedId);
					if (it != simIdMap.end())
					{
						Entity e = simIdToEntityMap[savedId];
						if (scene.m_ecs.hasComponent<RigidBody2D>(e))
						{
							auto& rb = scene.m_ecs.getComponent<RigidBody2D>(e);
							rb.isFixed = true;
							scene.m_ecs.getComponentArray<RigidBody2D>()->setEntityDirty(e, true);
						}
						// The actual fix will happen in PhysicsSystem2D::update thanks to isDirty=true
					}
				}
			}
		}

		WeirdEngine::Logger::log("[SceneSerializer] Scene loaded from " + path);
	}

} // namespace WeirdEngine
