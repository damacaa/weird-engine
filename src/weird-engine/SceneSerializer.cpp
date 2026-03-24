#include "weird-engine/SceneSerializer.h"
#include "weird-engine/Scene.h"

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <json/json.h>

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
			j["camera"]["position"] = { camTransform.position.x, camTransform.position.y, camTransform.position.z };
			j["camera"]["rotation"] = { camTransform.rotation.x, camTransform.rotation.y, camTransform.rotation.z };
			j["camera"]["scale"]    = { camTransform.scale.x,    camTransform.scale.y,    camTransform.scale.z };
		}

		// Save entities (skip the main camera entity)
		json entitiesJson = json::array();

		{
			auto transformArray   = scene.m_ecs.getComponentArray<Transform>();
			auto customShapeArray = scene.m_ecs.getComponentArray<CustomShape>();
			auto uiShapeArray     = scene.m_ecs.getComponentArray<UIShape>();
			auto sdfRendererArray = scene.m_ecs.getComponentArray<SDFRenderer>();
			auto rigidBodyArray   = scene.m_ecs.getComponentArray<RigidBody2D>();
			auto textArray        = scene.m_ecs.getComponentArray<TextRenderer>();

			std::unordered_map<Entity, json> entityMap;

			auto collectEntity = [&](Entity e) -> json& {
				if (entityMap.find(e) == entityMap.end())
				{
					entityMap[e] = json::object();
					entityMap[e]["id"] = e;
				}
				return entityMap[e];
			};

			// Transform (skip camera)
			for (size_t i = 0; i < transformArray->getSize(); i++)
			{
				Entity e = transformArray->getEntityAtIdx(i);
				if (e == scene.m_mainCamera) continue;
				auto& t = transformArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["transform"] = {
					{"position", { t.position.x, t.position.y, t.position.z }},
					{"rotation", { t.rotation.x, t.rotation.y, t.rotation.z }},
					{"scale",    { t.scale.x,    t.scale.y,    t.scale.z    }}
				};
			}

			// CustomShape (skip UIShape entities – serialised separately)
			for (size_t i = 0; i < customShapeArray->getSize(); i++)
			{
				Entity e = customShapeArray->getEntityAtIdx(i);
				if (e == scene.m_mainCamera) continue;
				if (uiShapeArray->hasData(e)) continue;
				auto& s = customShapeArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["customShape"] = {
					{"distanceFieldId", s.distanceFieldId},
					{"combination",     static_cast<int>(s.combination)},
					{"parameters",      json(s.parameters)},
					{"hasCollisions",   s.hasCollisions},
					{"groupIdx",        s.groupIdx},
					{"material",        s.material},
					{"smoothFactor",    s.smoothFactor}
				};
			}

			// UIShape
			for (size_t i = 0; i < uiShapeArray->getSize(); i++)
			{
				Entity e = uiShapeArray->getEntityAtIdx(i);
				if (e == scene.m_mainCamera) continue;
				auto& s = uiShapeArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["uiShape"] = {
					{"distanceFieldId", s.distanceFieldId},
					{"combination",     static_cast<int>(s.combination)},
					{"parameters",      json(s.parameters)},
					{"hasCollisions",   s.hasCollisions},
					{"groupIdx",        s.groupIdx},
					{"material",        s.material},
					{"smoothFactor",    s.smoothFactor}
				};
			}

			// SDFRenderer
			for (size_t i = 0; i < sdfRendererArray->getSize(); i++)
			{
				Entity e = sdfRendererArray->getEntityAtIdx(i);
				if (e == scene.m_mainCamera) continue;
				auto& r = sdfRendererArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["sdfRenderer"] = {
					{"isStatic",   r.isStatic},
					{"materialId", r.materialId}
				};
			}

			// RigidBody2D – also save the simulation particle position
			for (size_t i = 0; i < rigidBodyArray->getSize(); i++)
			{
				Entity e = rigidBodyArray->getEntityAtIdx(i);
				if (e == scene.m_mainCamera) continue;
				auto& rb = rigidBodyArray->getDataAtIdx(i);
				vec2 simPos = scene.m_simulation2D.getPosition(rb.simulationId);
				auto& ej = collectEntity(e);
				ej["rigidBody2D"] = {
					{"simulationId",    rb.simulationId},
					{"physicsPosition", { simPos.x, simPos.y }}
				};
			}

			// TextRenderer
			for (size_t i = 0; i < textArray->getSize(); i++)
			{
				Entity e = textArray->getEntityAtIdx(i);
				if (e == scene.m_mainCamera) continue;
				auto& tr = textArray->getDataAtIdx(i);
				auto& ej = collectEntity(e);
				ej["textRenderer"] = {
					{"text",     tr.text},
					{"material", tr.material},
					{"width",    tr.width},
					{"height",   tr.height}
				};
			}

			for (auto& [id, ej] : entityMap)
				entitiesJson.push_back(ej);
		}

		j["entities"] = entitiesJson;

		// Save physics constraints
		{
			json distanceConstraintsJson = json::array();
			for (const auto& dc : scene.m_simulation2D.getDistanceConstraints())
			{
				distanceConstraintsJson.push_back({
					{"A", dc.A}, {"B", dc.B}, {"distance", dc.Distance}, {"k", dc.K}
				});
			}

			json gravitationalConstraintsJson = json::array();
			for (const auto& gc : scene.m_simulation2D.getGravitationalConstraints())
			{
				gravitationalConstraintsJson.push_back({
					{"A", gc.A}, {"B", gc.B}, {"g", gc.g}
				});
			}

			json fixedObjectsJson = json::array();
			for (SimulationID fid : scene.m_simulation2D.getFixedObjects())
				fixedObjectsJson.push_back(fid);

			j["physics"] = {
				{"distanceConstraints",      distanceConstraintsJson},
				{"gravitationalConstraints", gravitationalConstraintsJson},
				{"fixedObjects",             fixedObjectsJson}
			};
		}

		std::ofstream outFile(filename);
		if (!outFile.is_open())
		{
			std::cerr << "[SceneSerializer] Failed to open file for writing: " << filename << "\n";
			return;
		}
		outFile << j.dump(2);
		std::cout << "[SceneSerializer] Scene saved to " << filename << "\n";
	}

	void SceneSerializer::load(Scene& scene, const std::string& path)
	{
		using json = nlohmann::json;

		std::ifstream inFile(path);
		if (!inFile.is_open())
		{
			std::cerr << "[SceneSerializer] Failed to open .weird file: " << path << "\n";
			return;
		}

		json j;
		try
		{
			inFile >> j;
		}
		catch (const json::parse_error& e)
		{
			std::cerr << "[SceneSerializer] JSON parse error in " << path << ": " << e.what() << "\n";
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
				camTransform.isDirty = true;
			}
			if (cam.contains("rotation"))
				camTransform.rotation = vec3(cam["rotation"][0], cam["rotation"][1], cam["rotation"][2]);
			if (cam.contains("scale"))
				camTransform.scale = vec3(cam["scale"][0], cam["scale"][1], cam["scale"][2]);
		}

		// Map from saved simulationId → newly assigned simulationId
		std::unordered_map<int, SimulationID> simIdMap;

		// Restore entities
		if (j.contains("entities") && j["entities"].is_array())
		{
			for (const auto& ej : j["entities"])
			{
				Entity entity = scene.m_ecs.createEntity();

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
					t.isDirty = true;
				}

				if (ej.contains("customShape"))
				{
					auto& s = scene.m_ecs.addComponent<CustomShape>(entity);
					const auto& sj = ej["customShape"];
					s.distanceFieldId = static_cast<uint16_t>(sj.value("distanceFieldId", 0));
					s.combination     = static_cast<CombinationType>(sj.value("combination", 0));
					s.hasCollisions   = sj.value("hasCollisions", true);
					s.groupIdx        = static_cast<uint16_t>(sj.value("groupIdx", 0));
					s.material        = static_cast<uint16_t>(sj.value("material", 0));
					s.smoothFactor    = sj.value("smoothFactor", 1.0f);
					if (sj.contains("parameters"))
					{
						for (int pi = 0; pi < (int)std::size(s.parameters) && pi < (int)sj["parameters"].size(); pi++)
							s.parameters[pi] = sj["parameters"][pi].get<float>();
					}
					s.isDirty = true;
					scene.m_sdfRenderSystem2D.shaderNeedsUpdate() = true;
				}

				if (ej.contains("uiShape"))
				{
					auto& s = scene.m_ecs.addComponent<UIShape>(entity);
					const auto& sj = ej["uiShape"];
					s.distanceFieldId = static_cast<uint16_t>(sj.value("distanceFieldId", 0));
					s.combination     = static_cast<CombinationType>(sj.value("combination", 0));
					s.hasCollisions   = sj.value("hasCollisions", false);
					s.groupIdx        = static_cast<uint16_t>(sj.value("groupIdx", 0));
					s.material        = static_cast<uint16_t>(sj.value("material", 0));
					s.smoothFactor    = sj.value("smoothFactor", 10.0f);
					if (sj.contains("parameters"))
					{
						for (int pi = 0; pi < (int)std::size(s.parameters) && pi < (int)sj["parameters"].size(); pi++)
							s.parameters[pi] = sj["parameters"][pi].get<float>();
					}
					s.isDirty = true;
					scene.m_UIRenderSystem.shaderNeedsUpdate() = true;
				}

				if (ej.contains("sdfRenderer"))
				{
					auto& r = scene.m_ecs.addComponent<SDFRenderer>(entity);
					const auto& rj = ej["sdfRenderer"];
					r.isStatic   = rj.value("isStatic", false);
					r.materialId = static_cast<unsigned int>(rj.value("materialId", 0));
				}

				if (ej.contains("textRenderer"))
				{
					auto& tr = scene.m_ecs.addComponent<TextRenderer>(entity);
					const auto& trj = ej["textRenderer"];
					tr.text     = trj.value("text", std::string{});
					tr.material = static_cast<uint16_t>(trj.value("material", 0));
					tr.width    = trj.value("width", 0.0f);
					tr.height   = trj.value("height", 0.0f);
					tr.dirty    = true;
				}

				if (ej.contains("rigidBody2D"))
				{
					auto& rb = scene.m_ecs.addComponent<RigidBody2D>(entity);
					const auto& rbj = ej["rigidBody2D"];
					int savedSimId = rbj.value("simulationId", -1);

					if (rbj.contains("physicsPosition"))
					{
						vec2 savedPos(rbj["physicsPosition"][0].get<float>(),
						              rbj["physicsPosition"][1].get<float>());
						scene.m_simulation2D.setPosition(rb.simulationId, savedPos);
					}

					if (savedSimId >= 0)
						simIdMap[savedSimId] = rb.simulationId;
				}
			}
		}

		// Restore physics constraints using the simulationId mapping
		if (j.contains("physics"))
		{
			const auto& phys = j["physics"];

			if (phys.contains("distanceConstraints"))
			{
				for (const auto& dcj : phys["distanceConstraints"])
				{
					int savedA = dcj.value("A", -1);
					int savedB = dcj.value("B", -1);
					float dist = dcj.value("distance", 1.0f);
					float k    = dcj.value("k", 1.0f);

					auto itA = simIdMap.find(savedA);
					auto itB = simIdMap.find(savedB);
					if (itA != simIdMap.end() && itB != simIdMap.end())
					{
						scene.m_simulation2D.addRawDistanceConstraint(
							static_cast<int>(itA->second),
							static_cast<int>(itB->second),
							dist, k);
					}
				}
			}

			if (phys.contains("gravitationalConstraints"))
			{
				for (const auto& gcj : phys["gravitationalConstraints"])
				{
					int savedA = gcj.value("A", -1);
					int savedB = gcj.value("B", -1);
					float g    = gcj.value("g", 1.0f);

					auto itA = simIdMap.find(savedA);
					auto itB = simIdMap.find(savedB);
					if (itA != simIdMap.end() && itB != simIdMap.end())
					{
						scene.m_simulation2D.addGravitationalConstraint(
							itA->second, itB->second, g);
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
						scene.m_simulation2D.fix(it->second);
				}
			}
		}

		std::cout << "[SceneSerializer] Scene loaded from " << path << "\n";
	}

} // namespace WeirdEngine
