#include "services/SceneOperations.h"

#include "weird-engine.h"
#include "weird-engine/components/Transform.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/components/Spring.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/components/SDFRenderer.h"
#include "weird-renderer/components/Shape.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <imgui.h>
#include <json/json.h>

namespace WeirdEngine::Editor
{
	// =====================================================================
	// Scene File I/O
	// =====================================================================

	void SceneOperations::newScene()
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

		// 4. Reset physics bodies, particles, and shapes
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
		m_ctx.selectedEntity = INVALID_ENTITY;
		m_ctx.selectedClusterId = 0;
		m_ctx.selectedGroupId = 0;
		m_ctx.selectedEntities.clear();
		m_ctx.selectedClusterIds.clear();
		m_ctx.groups.clear();
		m_ctx.nextGroupId = 1;
		m_ctx.isDragging = false;
		m_ctx.dragEntityStartPositions.clear();
		m_ctx.dragClusterStartPositions.clear();
		m_ctx.currentScenePath.clear();
		m_ctx.isSceneDirty = false;
		Logger::log("[SceneEditor] Cleared scene.");
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
		std::string defaultPath = m_ctx.project.isOpen() ? m_ctx.project.getAssetsPath().string() : "";
		SDL_ShowSaveFileDialog(onSaveSceneCallback, this, nullptr, filters, 1, "scene.weirdscene");
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
		newScene();
		if (m_ctx.scene)
		{
			SceneSerializer::load(reinterpret_cast<Scene&>(*m_ctx.scene), path);
		}
		syncAllConstraintsClusterMembership();
		m_ctx.currentScenePath = path;
		m_ctx.isSceneDirty = false;

		// Check if any cluster instance's asset file on disk has been modified since the scene was saved
		std::error_code ec;
		auto sceneWriteTime = std::filesystem::last_write_time(path, ec);
		std::vector<uint32_t> clustersToAutoReload;
		for (const auto& c : m_ctx.services->clusters().getAll())
		{
			if (!c.assetPath.empty())
			{
				std::filesystem::path resolved = resolveClusterPath(c.assetPath);
				if (std::filesystem::exists(resolved, ec))
				{
					auto assetWriteTime = std::filesystem::last_write_time(resolved, ec);
					std::string normPath = ShapeService::normalizePath(resolved.string());
					m_ctx.clusterAssetWriteTimes[normPath] = assetWriteTime;
					if (!ec && (assetWriteTime > sceneWriteTime))
					{
						clustersToAutoReload.push_back(c.id);
					}
				}
			}
		}
		for (uint32_t cId : clustersToAutoReload)
		{
			reloadCluster(cId);
		}

		// Restore editor groups from scene file
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

		Logger::log("[SceneEditor] Loaded scene: " + path);
	}

	void SceneOperations::saveScene(const std::string& path)
	{
		std::string finalPath = path;
		if (!finalPath.ends_with(".weirdscene"))
		{
			finalPath += ".weirdscene";
		}

		m_ctx.services->serialization().saveScene(finalPath);
		m_ctx.currentScenePath = finalPath;
		m_ctx.isSceneDirty = false;
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

		Logger::log("[SceneEditor] Saved scene to " + finalPath);
	}

	void SceneOperations::pollFileActions()
	{
		std::string loadPath;
		std::string savePath;
		{
			std::lock_guard<std::mutex> lock(m_ctx.fileMutex);
			loadPath = std::move(m_ctx.pendingLoadScenePath);
			savePath = std::move(m_ctx.pendingSaveScenePath);
			m_ctx.pendingLoadScenePath.clear();
			m_ctx.pendingSaveScenePath.clear();
		}

		if (!loadPath.empty())
		{
			loadScene(loadPath);
		}
		if (!savePath.empty())
		{
			saveScene(savePath);
		}
	}

	void SDLCALL SceneOperations::onOpenSceneCallback(void* userdata, const char* const* filelist, int /*filter*/)
	{
		auto* self = static_cast<SceneOperations*>(userdata);
		if (self && filelist && *filelist)
		{
			std::lock_guard<std::mutex> lock(self->m_ctx.fileMutex);
			self->m_ctx.pendingLoadScenePath = *filelist;
		}
	}

	void SDLCALL SceneOperations::onSaveSceneCallback(void* userdata, const char* const* filelist, int /*filter*/)
	{
		auto* self = static_cast<SceneOperations*>(userdata);
		if (self && filelist && *filelist)
		{
			std::lock_guard<std::mutex> lock(self->m_ctx.fileMutex);
			self->m_ctx.pendingSaveScenePath = *filelist;
		}
	}

	// =====================================================================
	// Spawning
	// =====================================================================

	Entity SceneOperations::spawnDefaultShape(uint16_t shapeType, vec2 pos)
	{
		Entity e = m_ctx.registry->createEntity();
		auto& t = m_ctx.registry->addComponent<Transform>(e);
		t.position = vec3(pos.x, pos.y, 0.0f);

		auto& s = m_ctx.registry->addComponent<Shape>(e);
		s.distanceFieldId = shapeType;
		s.combination = CombinationType::Addition;
		s.hasCollisions = true;
		s.material = 1;
		s.smoothFactor = 1.0f;

		// Defaults for common shapes
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
		else if (shapeType == DefaultShapes::RoundBox)
		{
			s.parameters[DefaultShapes::RoundBox::PosX] = pos.x;
			s.parameters[DefaultShapes::RoundBox::PosY] = pos.y;
			s.parameters[DefaultShapes::RoundBox::SizeX] = 2.0f;
			s.parameters[DefaultShapes::RoundBox::SizeY] = 1.0f;
			s.parameters[DefaultShapes::RoundBox::Radius] = 0.3f;
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
			s.parameters[0] = pos.x;
			s.parameters[1] = pos.y;
			s.parameters[2] = 1.5f;
			s.parameters[3] = 0.5f;
			s.parameters[4] = 5.0f;
			s.parameters[5] = 0.0f;
		}
		else
		{
			s.parameters[0] = pos.x;
			s.parameters[1] = pos.y;
		}

		m_ctx.registry->setComponentDirty(s);
		m_ctx.worldRenderContext->shapesNeedUpdate = true;

		m_ctx.selectedEntity = e;
		m_ctx.selectedClusterId = 0;
		m_ctx.isSceneDirty = true;
		return e;
	}

	Entity SceneOperations::spawnCustomShape(const std::string& assetPath, vec2 pos)
	{
		ShapeId id = m_ctx.services->shapes().loadCustomShape(assetPath);
		Entity e = m_ctx.registry->createEntity();
		auto& t = m_ctx.registry->addComponent<Transform>(e);
		t.position = vec3(pos.x, pos.y, 0.0f);

		auto& s = m_ctx.registry->addComponent<Shape>(e);
		s.distanceFieldId = id;
		s.combination = CombinationType::Addition;
		s.hasCollisions = true;
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

		m_ctx.selectedEntity = e;
		m_ctx.selectedClusterId = 0;
		m_ctx.isSceneDirty = true;
		return e;
	}

	void SceneOperations::spawnCluster(const std::string& assetPath, vec2 pos)
	{
		auto& cluster = m_ctx.services->clusters().instantiate(assetPath, pos);
		syncAllConstraintsClusterMembership();
		m_ctx.selectedClusterId = cluster.id;
		m_ctx.selectedEntity = INVALID_ENTITY;
		m_ctx.isSceneDirty = true;

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
		if (s.distanceFieldId == DefaultShapes::Line)
		{
			s.parameters[DefaultShapes::Line::StartX] += delta.x;
			s.parameters[DefaultShapes::Line::StartY] += delta.y;
			s.parameters[DefaultShapes::Line::EndX] += delta.x;
			s.parameters[DefaultShapes::Line::EndY] += delta.y;
			return;
		}

		const auto* customData = m_ctx.services->shapes().getCustomShapeData(s.distanceFieldId);
		if (customData)
		{
			int posXIdx = -1;
			int posYIdx = -1;
			for (int p = 0; p < 8; ++p)
			{
				std::string nameLower = customData->parameterNames[p];
				std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
				if (posXIdx == -1 &&
					(nameLower == "posx" || nameLower == "pos x" || nameLower == "pos_x" || nameLower == "x"))
				{
					posXIdx = p;
				}
				else if (posYIdx == -1 &&
						 (nameLower == "posy" || nameLower == "pos y" || nameLower == "pos_y" || nameLower == "y"))
				{
					posYIdx = p;
				}
			}
			if (posXIdx != -1 && posYIdx != -1)
			{
				s.parameters[posXIdx] += delta.x;
				s.parameters[posYIdx] += delta.y;
				return;
			}
		}

		// Default fallback for default shapes (and custom shapes using parameters 0 and 1 for position)
		s.parameters[0] += delta.x;
		s.parameters[1] += delta.y;
	}

	vec2 SceneOperations::getShapePosition(const Shape& s, Entity e) const
	{
		if (s.distanceFieldId == DefaultShapes::Line)
		{
			return vec2((s.parameters[DefaultShapes::Line::StartX] + s.parameters[DefaultShapes::Line::EndX]) * 0.5f,
						(s.parameters[DefaultShapes::Line::StartY] + s.parameters[DefaultShapes::Line::EndY]) * 0.5f);
		}

		const auto* customData = m_ctx.services->shapes().getCustomShapeData(s.distanceFieldId);
		if (customData)
		{
			int posXIdx = -1;
			int posYIdx = -1;
			for (int p = 0; p < 8; ++p)
			{
				std::string nameLower = customData->parameterNames[p];
				std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
				if (posXIdx == -1 &&
					(nameLower == "posx" || nameLower == "pos x" || nameLower == "pos_x" || nameLower == "x"))
				{
					posXIdx = p;
				}
				else if (posYIdx == -1 &&
						 (nameLower == "posy" || nameLower == "pos y" || nameLower == "pos_y" || nameLower == "y"))
				{
					posYIdx = p;
				}
			}
			if (posXIdx != -1 && posYIdx != -1)
			{
				return vec2(s.parameters[posXIdx], s.parameters[posYIdx]);
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
		data.initialPosition = cluster.position;
		data.initialRotation = cluster.rotation;

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
		}

		m_ctx.isSceneDirty = true;
	}

	void SceneOperations::resetClusterToInitialPosition(ClusterInstance& cluster)
	{
		auto& restData = getOrCreateClusterRestData(cluster);
		cluster.rotation = restData.initialRotation;
		resetClusterShapeAtPosition(cluster, restData.initialPosition);
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

		// Clear selection if it targeted this cluster's old entities
		for (Entity e : cluster->entities)
		{
			if (m_ctx.selectedEntity == e)
			{
				m_ctx.selectedEntity = INVALID_ENTITY;
			}
			m_ctx.selectedEntities.erase(e);
		}

		std::string resolvedStr = resolved.string();
		if (!m_ctx.services->clusters().reloadInstance(clusterId, resolvedStr))
		{
			return false;
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

		for (uint32_t cId : clustersToDelete)
		{
			removeClusterFromAllGroups(cId);
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
					if (!entitiesToDelete.count(spEnt))
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
					if (!entitiesToDelete.count(dcEnt))
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
			if (e == camEntity)
				continue; // Do not delete main camera

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
		deselectAll();
		m_ctx.isSceneDirty = true;
	}

	void SceneOperations::duplicateSelected()
	{
		if (m_ctx.selectedEntity != INVALID_ENTITY && m_ctx.registry->hasComponent<Shape>(m_ctx.selectedEntity))
		{
			auto& origShape = m_ctx.registry->getComponent<Shape>(m_ctx.selectedEntity);
			auto& origTrans = m_ctx.registry->getComponent<Transform>(m_ctx.selectedEntity);

			Entity dup = m_ctx.registry->createEntity();
			auto& newTrans = m_ctx.registry->addComponent<Transform>(dup);
			newTrans = origTrans;
			newTrans.position.x += 1.0f;
			newTrans.position.y += 1.0f;

			auto& newShape = m_ctx.registry->addComponent<Shape>(dup);
			newShape = origShape;
			offsetShapeParameters(newShape, vec2(1.0f, 1.0f));
			m_ctx.registry->setComponentDirty(newShape);
			m_ctx.worldRenderContext->shapesNeedUpdate = true;

			if (m_ctx.registry->hasComponent<RigidBody2D>(m_ctx.selectedEntity))
			{
				auto& origRb = m_ctx.registry->getComponent<RigidBody2D>(m_ctx.selectedEntity);
				auto& newRb = m_ctx.registry->addComponent<RigidBody2D>(dup);
				newRb.type = origRb.type;
				newRb.mass = origRb.mass;
				newRb.enableCollision = origRb.enableCollision;
				newRb.enableShapeCollision = origRb.enableShapeCollision;
			}

			m_ctx.selectedEntity = dup;
			m_ctx.isSceneDirty = true;
		}
	}

	// =====================================================================
	// Picking & Selection Helpers
	// =====================================================================

	void SceneOperations::pickAtWorld(vec2 worldPos)
	{
		bool isCtrl = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
		bool pickIndividual = m_ctx.selectIndividualClusterEntities || ImGui::GetIO().KeyAlt;

		// 1. Check clusters (find closest cluster entity if any)
		float bestClusterDist = 1.0f;
		Entity bestClusterMember = INVALID_ENTITY;
		uint32_t bestClusterId = 0;

		for (const auto& cluster : m_ctx.services->clusters().getAll())
		{
			for (Entity member : cluster.entities)
			{
				float memberDist = 999.0f;
				if (m_ctx.registry->hasComponent<Shape>(member))
				{
					const auto& s = m_ctx.registry->getComponent<Shape>(member);
					const auto& sdfs = m_ctx.services->shapes().getSDFs();
					if (s.distanceFieldId < sdfs.size() && sdfs[s.distanceFieldId])
					{
						float params[12];
						packSdfParameters(params, s.parameters, m_ctx.services->time().time(), worldPos, 0.0f);
						float dist = sdfs[s.distanceFieldId]->getValue(params);
						if (dist <= 0.2f)
						{
							memberDist = std::max(0.0f, dist);
						}
					}
				}
				if (m_ctx.registry->hasComponent<Transform>(member))
				{
					vec2 memberPos(m_ctx.registry->getComponent<Transform>(member).position);
					float d = glm::distance(worldPos, memberPos);
					if (d < 1.0f && d < memberDist)
					{
						memberDist = d;
					}
				}

				if (memberDist < bestClusterDist)
				{
					bestClusterDist = memberDist;
					bestClusterMember = member;
					bestClusterId = cluster.id;
				}
			}
		}

		if (bestClusterMember != INVALID_ENTITY)
		{
			if (pickIndividual)
			{
				if (!isCtrl && m_ctx.isEntitySelected(bestClusterMember))
				{
					m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestClusterMember)};
					m_ctx.hasShiftAnchor = true;
					return; // Part of current selection, keep for dragging
				}
				selectEntity(bestClusterMember, isCtrl);
				m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestClusterMember)};
				m_ctx.hasShiftAnchor = true;
				return;
			}
			else
			{
				if (!isCtrl && m_ctx.isClusterSelected(bestClusterId))
				{
					m_ctx.shiftAnchorItem = {HierarchyItemType::Cluster, bestClusterId};
					m_ctx.hasShiftAnchor = true;
					return; // Part of current selection, keep for dragging
				}
				selectCluster(bestClusterId, isCtrl);
				m_ctx.shiftAnchorItem = {HierarchyItemType::Cluster, bestClusterId};
				m_ctx.hasShiftAnchor = true;
				return;
			}
		}

		// 2. Check standalone shapes using exact Signed Distance Field evaluation
		auto shapeArray = m_ctx.registry->getComponentArray<Shape>();
		if (shapeArray && shapeArray->getSize() > 0)
		{
			const auto& sdfs = m_ctx.services->shapes().getSDFs();
			float bestDist = 0.5f; // Boundary tolerance for clicking near outlines/lines
			Entity bestEntity = INVALID_ENTITY;

			// Iterate in reverse (topmost / last-drawn shapes first)
			for (int i = static_cast<int>(shapeArray->getSize()) - 1; i >= 0; --i)
			{
				Entity e = shapeArray->getEntityAtIdx(i);

				// Skip entities belonging to a cluster
				if (m_ctx.services->clusters().getClusterForEntity(e) != nullptr)
				{
					continue;
				}

				const auto& s = shapeArray->getDataAtIdx(i);
				if (s.distanceFieldId < sdfs.size() && sdfs[s.distanceFieldId])
				{
					float params[12];
					packSdfParameters(params, s.parameters, m_ctx.services->time().time(), worldPos, 0.0f);
					float dist = sdfs[s.distanceFieldId]->getValue(params);

					if (dist <= 0.0f)
					{
						// Direct click inside the shape! Topmost shape wins immediately.
						if (!isCtrl && m_ctx.isEntitySelected(e))
						{
							m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(e)};
							m_ctx.hasShiftAnchor = true;
							return; // Part of current selection, keep for dragging
						}
						selectEntity(e, isCtrl);
						m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(e)};
						m_ctx.hasShiftAnchor = true;
						return;
					}
					else if (dist < bestDist)
					{
						bestDist = dist;
						bestEntity = e;
					}
				}
				else if (m_ctx.registry->hasComponent<Transform>(e))
				{
					vec2 pos(m_ctx.registry->getComponent<Transform>(e).position);
					float dist = glm::distance(worldPos, pos);
					if (dist < 1.5f && dist < bestDist)
					{
						bestDist = dist;
						bestEntity = e;
					}
				}
			}

			if (bestEntity != INVALID_ENTITY)
			{
				if (!isCtrl && m_ctx.isEntitySelected(bestEntity))
				{
					m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestEntity)};
					m_ctx.hasShiftAnchor = true;
					return; // Part of current selection, keep for dragging
				}
				selectEntity(bestEntity, isCtrl);
				m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestEntity)};
				m_ctx.hasShiftAnchor = true;
				return;
			}
		}

		// 3. Fallback: check other entities with Transform (e.g. dots, camera markers)
		auto transformArray = m_ctx.registry->getComponentArray<Transform>();
		if (transformArray)
		{
			Entity camEntity = m_ctx.services->render().getCameraEntity();
			float bestDist = 1.0f;
			Entity bestEntity = INVALID_ENTITY;

			for (size_t i = 0; i < transformArray->getSize(); ++i)
			{
				Entity e = transformArray->getEntityAtIdx(i);
				if (e == camEntity || m_ctx.services->clusters().getClusterForEntity(e) != nullptr)
				{
					continue;
				}

				vec2 pos(transformArray->getDataAtIdx(i).position);
				float dist = glm::distance(worldPos, pos);
				if (dist < bestDist)
				{
					bestDist = dist;
					bestEntity = e;
				}
			}

			if (bestEntity != INVALID_ENTITY)
			{
				if (!isCtrl && m_ctx.isEntitySelected(bestEntity))
				{
					m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestEntity)};
					m_ctx.hasShiftAnchor = true;
					return; // Part of current selection, keep for dragging
				}
				selectEntity(bestEntity, isCtrl);
				m_ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestEntity)};
				m_ctx.hasShiftAnchor = true;
				return;
			}
		}

		// If clicked empty space, deselect (unless Ctrl is held)
		if (!isCtrl)
		{
			deselectAll();
		}
	}

	void SceneOperations::selectEntity(Entity e, bool multiSelect)
	{
		if (multiSelect)
		{
			m_ctx.selectedGroupId = 0;
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
		if (multiSelect)
		{
			m_ctx.selectedGroupId = 0;
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
		m_ctx.isSimulating = !m_ctx.isSimulating;
		if (m_ctx.isSimulating)
		{
			// Take temporary snapshot before physics simulation
			m_ctx.services->serialization().saveSnapshot("temp_editor.weirdsnap");
			m_ctx.hasSnapshot = true;
			m_ctx.simulation2D->setGravity(m_ctx.playGravity);
			m_ctx.simulation2D->setDamping(m_ctx.playDamping);
			Logger::log("[SceneEditor] Simulation started (play mode).");
		}
		else
		{
			m_ctx.simulation2D->setGravity(0.0f);
			m_ctx.simulation2D->setDamping(1.0f);
			// Revert to snapshot on pause
			if (m_ctx.hasSnapshot)
			{
				m_ctx.services->serialization().loadSnapshot("temp_editor.weirdsnap");
				Logger::log("[SceneEditor] Simulation paused (design mode, reverted to snapshot).");
			}
		}
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

} // namespace WeirdEngine::Editor
