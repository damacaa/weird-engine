#include "weird-engine/Scene.h"

#include <imgui.h>

#include "weird-engine/Input.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/Profiler.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-physics/components/CustomShapeManager.h"
#include "weird-physics/components/CustomShapeManager.h"
#include "weird-physics/components/DistanceConstraintManager.h"
#include "weird-physics/components/RigidBodyManager.h"
#include "weird-physics/components/SpringManager.h"

#include "weird-engine/systems/SDFShaderGenerationSystem.h"
#include "weird-engine/systems/ButtonSystem.h"
#include "weird-engine/systems/CameraSystem.h"
#include "weird-engine/systems/PhysicsInteractionSystem.h"
#include "weird-engine/systems/PhysicsSystem2D.h"
#include "weird-engine/systems/PlayerMovementSystem.h"
#include "weird-engine/systems/RenderSystem.h"
#include "weird-engine/systems/SDFRenderSystem.h"
#include "weird-engine/systems/SDFRenderSystem.h"

namespace WeirdEngine
{
	static std::vector<std::shared_ptr<IMathExpression>>& getGlobalSDFsInternal()
	{
		static std::vector<std::shared_ptr<IMathExpression>> s_globalSdfs;
		return s_globalSdfs;
	}

	ShapeId Scene::registerDefaultSDF(std::shared_ptr<IMathExpression> sdf)
	{
		auto& sdfs = getGlobalSDFsInternal();
		sdfs.push_back(sdf);
		return static_cast<ShapeId>(sdfs.size() - 1);
	}

	const std::vector<std::shared_ptr<IMathExpression>>& Scene::getGlobalSDFs()
	{
		return getGlobalSDFsInternal();
	}

	Scene::Scene(const PhysicsSettings& settings)
		: m_simulation2D(MAX_ENTITIES, settings)
		, m_runSimulationInThread(true)
	{
		// Custom component managers
		std::shared_ptr<RigidBodyManager> rbManager = std::make_shared<RigidBodyManager>(m_simulation2D);
		m_ecs.registerComponent<RigidBody2D>(rbManager);

		std::shared_ptr<CustomShapeManager> shapeManager = std::make_shared<CustomShapeManager>(m_simulation2D);
		m_ecs.registerComponent<CustomShape>(shapeManager);

		std::shared_ptr<DistanceConstraintManager> distManager = std::make_shared<DistanceConstraintManager>(m_simulation2D, m_ecs);
		m_ecs.registerComponent<DistanceConstraint>(distManager);

		std::shared_ptr<SpringManager> springManager = std::make_shared<SpringManager>(m_simulation2D, m_ecs);
		m_ecs.registerComponent<Spring>(springManager);

		// Create camera
		m_mainCamera = m_ecs.createEntity();
		tag(m_mainCamera, "mainCamera");
		Transform& t = m_ecs.addComponent<Transform>(m_mainCamera);
		t.rotation = vec3(0, 0, -1.0f);
		ECS::Camera& c = m_ecs.addComponent<ECS::Camera>(m_mainCamera);

		// Shapes
		m_sdfs = Scene::getGlobalSDFs();
		m_simulation2D.setSDFs(m_sdfs);

		// Initialize simulation
		PhysicsSystem2D::init(m_ecs, m_simulation2D);

		// Start simulation if different thread
		if (m_runSimulationInThread)
		{
			m_simulation2D.startSimulationThread();
		}

		m_simulation2D.setStepCallback(&handlePhysicsStep, this);
		m_simulation2D.setCollisionCallback(&handleCollision, this);
		m_simulation2D.setShapeCollisionCallback(&handleShapeCollision, this);

		// Initialize 2D world render context
		m_2DWorldRenderContext.dotRadious = 0.5f;
		m_2DWorldRenderContext.charSpacing = 1.0f;
	}

	Scene::~Scene()
	{
		m_simulation2D.stopSimulationThread();

		// TODO: Free resources from all entities
		m_resourceManager.freeResources(0);
	}

	void Scene::start()
	{
		auto& defaultMaterial = createMaterial();
		defaultMaterial.color = vec4(1.0f);
		defaultMaterial.metallic = 0.5f;
		defaultMaterial.roughness = 0.1f;

		onCreate();

		// If a .weird file path was provided (via setSceneFilePath / registerScene),
		// restore saved scene state before the derived class's onStart() runs.
		TagMap loadedTags;
		if (!m_sceneFilePath.empty())
		{
			SceneSerializer::load(*this, m_sceneFilePath);
			loadedTags = m_tagToEntity;
		}

		onStart(m_ecs, loadedTags);

		switch (m_renderMode)
		{
			case WeirdEngine::Scene::RenderMode::RayMarching3D:
			{
				FlyMovement& fly = m_ecs.addComponent<FlyMovement>(m_mainCamera);
				break;
			}
			case WeirdEngine::Scene::RenderMode::RayMarching2D:
			case WeirdEngine::Scene::RenderMode::RayMarchingBoth:
			{
				FlyMovement2D& fly = m_ecs.addComponent<FlyMovement2D>(m_mainCamera);
				fly.targetPosition = m_ecs.getComponent<Transform>(m_mainCamera).position;
				break;
			}
			default:
				break;
		}
		
		PhysicsSystem2D::update(m_ecs, m_simulation2D);
	}

	void Scene::update(double delta, double time)
	{
		PROFILE_SCOPE("Scene Logic Update");

		if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown((Input::R)))
		{
			m_2DWorldRenderContext.shapesNeedUpdate = true;
		}

		// Update systems
		{
			if (m_debugFly)
			{
				PlayerMovementSystem::update(m_ecs, delta);
			}

			CameraSystem::update(m_ecs);
		}

		ButtonSystem::update(m_ecs, m_sdfs, getTime());

		{
			PROFILE_SCOPE("Physics synchronization");
			PhysicsSystem2D::update(m_ecs, m_simulation2D);

			if (m_debugInput)
			{
				PhysicsInteractionSystem::update(m_ecs, m_simulation2D);
			}

			m_simulation2D.update(delta);
		}

		{
			PROFILE_SCOPE("Scene logic update");

			// Process queued collisions
			std::vector<CollisionEvent> collisions;
			std::vector<ShapeCollisionEvent> shapeCollisions;
			{
				std::lock_guard<std::mutex> lock(m_collisionQueueMutex);
				collisions = std::move(m_queuedCollisions);
				shapeCollisions = std::move(m_queuedShapeCollisions);
			}
			
			for (auto& ev : collisions)
			{
				EntityCollisionEvent entityEvent{ev, getEntityForSimulationId(ev.bodyA), getEntityForSimulationId(ev.bodyB)};
				onEntityCollision(m_ecs, entityEvent);
			}

			for (auto& ev : shapeCollisions)
			{
				EntityShapeCollisionEvent entityEvent{ev, getEntityForSimulationId(ev.body)};
				onEntityShapeCollision(m_ecs, entityEvent);
			}

			onUpdate(delta, m_ecs);
		}

		{
			PROFILE_SCOPE("Render Queue update");
			RenderSystem::update(m_ecs, m_resourceManager, m_drawQueue);
		}

		m_ecs.freeRemovedComponents();
	}

	float Scene::getTime()
	{
		return m_simulation2D.getSimulationTime();
	}

	void Scene::handlePhysicsStep(void* userData)
	{
		Scene* self = static_cast<Scene*>(userData);
		self->onPhysicsStep(self->m_simulation2D);

		self->m_frictionSoundLevelRead.store(self->m_frictionSoundLevel, std::memory_order_release);
		self->m_frictionSoundLevel = 0.0f;
	}

	void Scene::handleCollision(CollisionEvent& event, void* userData)
	{
		Scene* self = static_cast<Scene*>(userData);
		self->onCollision(self->m_simulation2D, event);
		
		std::lock_guard<std::mutex> lock(self->m_collisionQueueMutex);
		self->m_queuedCollisions.push_back(event);
	}

	void Scene::handleShapeCollision(ShapeCollisionEvent& event, void* userData)
	{
		Scene* self = static_cast<Scene*>(userData);
		self->onShapeCollision(self->m_simulation2D, event);
		
		{
			std::lock_guard<std::mutex> lock(self->m_collisionQueueMutex);
			self->m_queuedShapeCollisions.push_back(event);
		}

		const float m_soundFalloff = 0.1f;
		bool spatialAudio = false;
		auto camPosition = self->getCamera().position; // Mutex?
		float speed = glm::length2(event.velocity);
		float distanceMultiplier =
			1.0f / (1.0f + (m_soundFalloff * glm::distance2(camPosition, vec3(event.position, 0.0f))));
		float frictionSample = event.friction * 0.01f * speed * (spatialAudio ? distanceMultiplier : 1.0f);

		self->m_frictionSoundLevel = std::max(frictionSample, self->m_frictionSoundLevel);

		if (event.state == CollisionState::START)
		{
			// float speedFactor = std::sqrt((std::min)(0.001f * speed, 1.0f));
			float penetrationFactor = std::sqrt((std::min)(2.0f * event.penetration, 1.0f));
			float volume = penetrationFactor;

			float freqFactor = std::abs(glm::dot(event.normal, (event.velocity)));
			freqFactor *= 0.01f;
			// freqFactor = freqFactor * freqFactor;
			float frequency = 200.0f + (freqFactor * 300.0f);

			self->m_audioQueue.push(
				WeirdRenderer::SimpleAudioRequest{volume, frequency, false, vec3(event.position, 0.0f)});
		}
	}

	// RENDER

	Scene::RenderMode Scene::getRenderMode() const
	{
		return m_renderMode;
	}

	WeirdRenderer::Camera& Scene::getCamera()
	{
		return m_ecs.getComponent<Camera>(m_mainCamera).camera;
	}

	void Scene::get2DShapesData(vec4*& data, uint32_t& size, uint32_t& customShapeCount)
	{
		PROFILE_SCOPE("Fetch World Data");
		customShapeCount = m_ecs.getComponentArray<CustomShape>()->getSize();
		SDFRenderSystem::update<Dot, CustomShape, TextRenderer>(m_ecs, m_2DWorldRenderContext, data, size);
	}

	void Scene::get3DShapesData(vec4*& data, uint32_t& size, uint32_t& customShapeCount)
	{
		PROFILE_SCOPE("Fetch 3D World Data");
		customShapeCount = m_ecs.getComponentArray<CustomShape>()->getSize();
		SDFRenderSystem::update<Dot, CustomShape, TextRenderer>(m_ecs, m_3DWorldRenderContext, data, size);
	}

	void Scene::getUIData(vec4*& uiData, uint32_t& size, uint32_t& customShapeCount)
	{
		PROFILE_SCOPE("Fetch UI Data");
		customShapeCount = m_ecs.getComponentArray<UIShape>()->getSize();
		SDFRenderSystem::update<UIDot, UIShape, UITextRenderer>(m_ecs, m_UIRenderContext, uiData, size);
	}

	void Scene::update2DWorldShader(WeirdRenderer::Shader& shader)
	{
		SDFShaderGenerationSystem::update<CustomShape>(m_ecs, m_2DWorldRenderContext, shader, m_sdfs);
	}

	void Scene::update3DWorldShader(WeirdRenderer::Shader& shader)
	{
		SDFShaderGenerationSystem::update<CustomShape>(m_ecs, m_3DWorldRenderContext, shader, m_sdfs);
	}

	void Scene::updateUIShader(WeirdRenderer::Shader& shader)
	{
		SDFShaderGenerationSystem::update<UIShape>(m_ecs, m_UIRenderContext, shader, m_sdfs);
	}

	void Scene::forceShaderRefresh()
	{
		m_2DWorldRenderContext.shapesNeedUpdate = true;
		m_3DWorldRenderContext.shapesNeedUpdate = true;
		m_UIRenderContext.shapesNeedUpdate = true;
	}

	const std::vector<WeirdRenderer::DrawCommand>& Scene::getDrawQueue() const
	{
		return m_drawQueue;
	}

	std::vector<WeirdRenderer::Light>& Scene::getLigths()
	{
		return m_lights;
	}

	void Scene::renderExtra(WeirdRenderer::RenderTarget& renderTarget)
	{
		onRender(renderTarget);
	}

	// SDFs

	ShapeId Scene::registerSDF(std::shared_ptr<IMathExpression> sdf)
	{
		m_sdfs.push_back(sdf);
		m_simulation2D.setSDFs(m_sdfs);

		return m_sdfs.size() - 1;
	}

	// AUDIO

	AudioRingBuffer<WeirdRenderer::SimpleAudioRequest, SOUND_QUEUE_SIZE>& Scene::getAudioQueue()
	{
		return m_audioQueue;
	}

	float Scene::getFrictionSound()
	{
		return m_frictionSoundLevelRead.load(std::memory_order_acquire);
	}

	void Scene::playSound(const WeirdRenderer::SimpleAudioRequest& audio)
	{
		m_audioQueue.push(audio);
	}

	// Serialization

	void Scene::tag(Entity entity, const std::string& name)
	{
		if (name.empty())
		{
			removeTag(entity);
			return;
		}

		// If the tag is already owned by another entity, remove it from that entity
		auto existingOwner = m_tagToEntity.find(name);
		if (existingOwner != m_tagToEntity.end() && existingOwner->second != entity)
		{
			m_entityToTag.erase(existingOwner->second);
		}

		// Remove any previous tag this entity had
		auto existingTag = m_entityToTag.find(entity);
		if (existingTag != m_entityToTag.end() && existingTag->second != name)
		{
			m_tagToEntity.erase(existingTag->second);
		}

		m_tagToEntity[name] = entity;
		m_entityToTag[entity] = name;
	}

	void Scene::removeTag(Entity entity)
	{
		auto it = m_entityToTag.find(entity);
		if (it == m_entityToTag.end())
			return;
		m_tagToEntity.erase(it->second);
		m_entityToTag.erase(it);
	}

	std::string Scene::getEntityTag(Entity entity) const
	{
		auto it = m_entityToTag.find(entity);
		if (it == m_entityToTag.end())
			return "";
		return it->second;
	}

	Entity Scene::getEntityByTag(const std::string& name) const
	{
		auto it = m_tagToEntity.find(name);
		if (it == m_tagToEntity.end())
			return MAX_ENTITIES;
		return it->second;
	}

	Entity Scene::getEntityForSimulationId(SimulationID simulationId)
	{
		auto rigidBodies = m_ecs.getComponentArray<RigidBody2D>();
		if (simulationId >= static_cast<SimulationID>(rigidBodies->getSize()))
			return INVALID_ENTITY;

		return rigidBodies->getEntityAtIdx(static_cast<size_t>(simulationId));
	}

	void Scene::saveScene(const std::string& filename)
	{
		SceneSerializer::save(*this, filename);
	}

	Scene::TagMap Scene::loadWeirdFile(const std::string& path, bool blacklistEntities)
	{
		TagMap loadedTags;
		Entity firstNewEntity = m_ecs.getEntityCount();
		SceneSerializer::load(*this, path, &loadedTags);
		if (blacklistEntities)
		{
			Entity lastNewEntity = m_ecs.getEntityCount();
			for (Entity entity = firstNewEntity; entity < lastNewEntity; ++entity)
				m_serializationBlacklist.insert(entity);
		}
		return loadedTags;
	}

	void Scene::loadFromWeirdFile(const std::string& path)
	{
		SceneSerializer::load(*this, path);
	}

	Scene::RaymarchResult Scene::raymarch(glm::vec2 origin, glm::vec2 direction, float epsilon, float maxDistance)
	{
		float traveled = 0.0f;
		float time = getTime();
		auto gridSnapshot = m_simulation2D.getSpatialGridSnapshot();

		if(epsilon <= 0.0f)
		{
			epsilon = 0.001f; // Default epsilon
		}

		struct GroupState
		{
			uint16_t id;
			float minDistance;
			Entity closestEntity;
		};

		for (int i = 0; i < 100; i++)
		{
			glm::vec2 p = origin + (traveled * direction);
			float d = 1000.0f;
			float minD = d;
			Entity closestEntity = INVALID_ENTITY;

			std::vector<GroupState> groups;
			groups.reserve(16);

			auto shapeArray = m_ecs.getComponentArray<CustomShape>();
			for (size_t j = 0; j < shapeArray->getSize(); j++)
			{
				auto& shape = shapeArray->getDataAtIdx(j);

				if (!shape.hasCollisions)
					continue;
					
				if (shape.distanceFieldId >= m_sdfs.size())
					continue;

				float parameters[11];
				std::copy(std::begin(shape.parameters), std::end(shape.parameters), std::begin(parameters));
				parameters[8] = time;
				parameters[9] = p.x;
				parameters[10] = p.y;

				float dist = m_sdfs[shape.distanceFieldId]->getValue(parameters);
				float currentMinDistance = d;
				Entity currentEntity = INVALID_ENTITY;

				GroupState* groupState = nullptr;
				for (auto& group : groups)
				{
					if (group.id == shape.groupIdx)
					{
						groupState = &group;
						currentMinDistance = groupState->minDistance;
						currentEntity = groupState->closestEntity;
						break;
					}
				}

				if (!groupState && shape.groupIdx != CustomShape::GLOBAL_GROUP)
				{
					groups.push_back({static_cast<uint16_t>(shape.groupIdx), 1000.0f, INVALID_ENTITY});
					groupState = &groups.back();
					currentMinDistance = groupState->minDistance;
				}

				bool closestEntityUpdated = false;
				switch (shape.combination)
				{
					case CombinationType::Addition:
						if (dist < currentMinDistance)
							closestEntityUpdated = true;
						currentMinDistance = dist;
						break;
					case CombinationType::Subtraction:
						currentMinDistance = std::max(currentMinDistance, -dist);
						break;
					case CombinationType::SmoothAddition:
						dist = fOpUnionSoft(dist, currentMinDistance, shape.smoothFactor);
						if (dist < currentMinDistance)
							closestEntityUpdated = true;
						currentMinDistance = dist;
						break;
					case CombinationType::SmoothSubtraction:
						currentMinDistance = fOpSubSoft(currentMinDistance, dist, shape.smoothFactor);
						break;
					case CombinationType::Intersection:
						currentMinDistance = std::max(currentMinDistance, dist);
						break;
				}

				if (closestEntityUpdated)
				{
					currentEntity = shapeArray->getEntityAtIdx(j);
				}

				if (shape.groupIdx == CustomShape::GLOBAL_GROUP)
				{
					d = currentMinDistance;
					if (d < minD)
					{
						minD = d;
						closestEntity = currentEntity;
					}
				}
				else
				{
					groupState->minDistance = currentMinDistance;
					if (closestEntityUpdated)
					{
						groupState->closestEntity = currentEntity;
					}
				}
			}

			for (const auto& group : groups)
			{
				if (group.minDistance < d)
				{
					d = group.minDistance;
					closestEntity = group.closestEntity;
				}
				if (d < minD)
				{
					minD = d;
				}
			}

			// Evaluate Rigidbodies using the Spatial Grid Snapshot
			if (gridSnapshot)
			{
				float minRigidbodyDist = 1000.0f;
				Entity closestRbEntity = INVALID_ENTITY;

				int gx = static_cast<int>(std::floor(p.x * gridSnapshot->invCellSize));
				int gy = static_cast<int>(std::floor(p.y * gridSnapshot->invCellSize));
				const int TABLE_SIZE = 8191; // Must match Simulation2D.cpp

				auto getHash = [](int x, int y) -> int
				{
					constexpr int p1 = 73856093;
					constexpr int p2 = 19349663;
					int hash = (x * p1) ^ (y * p2);
					hash = hash % TABLE_SIZE;
					if (hash < 0)
						hash += TABLE_SIZE;
					return hash;
				};

				for (int dx = -1; dx <= 1; dx++)
				{
					for (int dy = -1; dy <= 1; dy++)
					{
						int hash = getHash(gx + dx, gy + dy);
						int rbIndex = gridSnapshot->head[hash];

						while (rbIndex != -1)
						{
							glm::vec2 rbPos = gridSnapshot->positions[rbIndex];
							float dist = glm::length(p - rbPos) - gridSnapshot->radious;

							if (dist < minRigidbodyDist)
							{
								minRigidbodyDist = dist;
								closestRbEntity = getEntityForSimulationId(rbIndex);
							}

							rbIndex = gridSnapshot->next[rbIndex];
						}
					}
				}

				if (minRigidbodyDist < d)
				{
					d = minRigidbodyDist;
					closestEntity = closestRbEntity;
				}
				if (d < minD)
				{
					minD = d;
				}
			}

			if (d <= epsilon)
				return {traveled, closestEntity};

			traveled += std::abs(d);

			if (traveled >= maxDistance)
			{
				return {maxDistance, INVALID_ENTITY};
			}
		}

		return {traveled, INVALID_ENTITY};
	}

	// Utils

	void Scene::renderImGui()
	{
		const char* label = "Global settings";
		if (ImGui::CollapsingHeader(label))
		{

			ImGui::PushID(label);
			onImGuiRender();

			ImGui::PopID();
		}

		const char* label2 = "Hierarchy";
		if (ImGui::CollapsingHeader(label2))
		{

			ImGui::PushID(label2);

			for (Entity e = 0; e < m_ecs.getEntityCount(); ++e)
			{
				std::vector<size_t> componentIDs = m_ecs.getComponentTypes(e);

				if (componentIDs.empty())
					continue;

				std::string tag = getEntityTag(e);
				std::string label =
					tag.empty() ? ("Entity " + std::to_string(e)) : (tag + " (ID: " + std::to_string(e) + ")");

				if (ImGui::TreeNode(label.c_str()))
				{
					ImGui::TextDisabled("Attached Components:");

					for (size_t compID : componentIDs)
					{
						std::string compName = m_ecs.getComponentName(compID);
						ImGui::BulletText("%s", compName.c_str());
					}

					ImGui::TreePop();
				}
			}

			ImGui::PopID();
		}
	}

	Entity Scene::addShape(ShapeId shapeId, float* variables, uint16_t material, CombinationType combination,
						   bool hasCollision, int group)
	{
		Entity entity = m_ecs.createEntity();
		CustomShape& shape = m_ecs.addComponent<CustomShape>(entity);
		shape.distanceFieldId = shapeId;
		shape.combination = combination;
		shape.hasCollisions = hasCollision;
		shape.groupIdx = group;
		shape.material = material;
		std::copy(variables, variables + 8, shape.parameters);

		m_2DWorldRenderContext.shapesNeedUpdate = true;
		m_3DWorldRenderContext.shapesNeedUpdate = true;

		return entity;
	}

	Entity Scene::addUIShape(ShapeId shapeId, float* variables, uint16_t material, CombinationType combination,
							 int group)
	{
		Entity entity = m_ecs.createEntity();
		UIShape& shape = m_ecs.addComponent<UIShape>(entity);
		shape.distanceFieldId = shapeId;
		shape.combination = combination;
		shape.groupIdx = group;
		shape.material = material;
		std::copy(variables, variables + 8, shape.parameters);

		m_UIRenderContext.shapesNeedUpdate = true;

		return entity;
	}

	UIShape& Scene::addUIShape(ShapeId shapeId, float* variables, Entity& entity, int group)
	{
		entity = m_ecs.createEntity();
		UIShape& component = m_ecs.addComponent<UIShape>(entity);
		component.distanceFieldId = shapeId;
		component.groupIdx = group;
		component.smoothFactor = 100.0f;
		std::copy(variables, variables + 8, component.parameters);

		m_UIRenderContext.shapesNeedUpdate = true;

		return component;
	}

	Material3D& Scene::createMaterial()
	{
		if (m_materialCount >= 16)
		{
			// Max materials reached, return the last one
			return m_materials[15];
		}
		
		Material3D& mat = m_materials[m_materialCount];
		mat.id = m_materialCount;
		m_materialCount++;
		
		return mat;
	}
} // namespace WeirdEngine