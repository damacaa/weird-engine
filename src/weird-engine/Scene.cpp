#include "weird-engine/Scene.h"
#include "weird-engine/SceneManager.h"

#ifndef WEIRD_DISABLE_IMGUI
#include <imgui.h>
#endif

#include "weird-engine/Input.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/Profiler.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-physics/components/CustomShapeManager.h"
#include "weird-physics/components/DistanceConstraintManager.h"
#include "weird-physics/components/RigidBodyManager.h"
#include "weird-physics/components/SpringManager.h"

#include "weird-engine/systems/ButtonSystem.h"
#include "weird-engine/systems/CameraSystem.h"
#include "weird-engine/systems/PhysicsInteractionSystem.h"
#include "weird-engine/systems/PhysicsSystem2D.h"
#include "weird-engine/systems/PlayerMovementSystem.h"
#include "weird-engine/systems/RenderSystem.h"
#include "weird-engine/systems/SDFRenderSystem.h"
#include "weird-engine/systems/SDFShaderGenerationSystem.h"

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

	Scene::Scene()
		: m_simulation2D(MAX_ENTITIES, SceneManager::getInstance().getPhysicsSettings())
		, m_runSimulationInThread(SceneManager::getInstance().getPhysicsSettings().runSimulationInThread)
		, m_services(*this)
	{
	}

	Scene::Scene(RenderMode mode)
		: Scene()
	{
		m_renderMode = mode;
	}

	Scene::~Scene()
	{
		m_simulation2D.stopSimulationThread();

		// TODO: Free resources from all entities
		m_resourceManager.freeResources(0);
	}

	void Scene::start()
	{
		onCreate(m_ecs, m_services);

		// Custom component managers
		std::shared_ptr<RigidBodyManager> rbManager = std::make_shared<RigidBodyManager>(m_simulation2D);
		m_ecs.registerComponent<RigidBody2D>(rbManager);

		if (m_renderMode == RenderMode::RayMarching2D)
		{
			std::shared_ptr<CustomShapeManager> shapeManager =
				std::make_shared<CustomShapeManager>(m_simulation2D, m_2DWorldRenderContext);
			m_ecs.registerComponent<CustomShape>(shapeManager);
		}
		else
		{
			std::shared_ptr<CustomShapeManager> shapeManager =
				std::make_shared<CustomShapeManager>(m_simulation2D, m_3DWorldRenderContext);
			m_ecs.registerComponent<CustomShape>(shapeManager);
		}

		std::shared_ptr<DistanceConstraintManager> distManager =
			std::make_shared<DistanceConstraintManager>(m_simulation2D, m_ecs);
		m_ecs.registerComponent<DistanceConstraint>(distManager);

		std::shared_ptr<SpringManager> springManager = std::make_shared<SpringManager>(m_simulation2D, m_ecs);
		m_ecs.registerComponent<Spring>(springManager);

		std::shared_ptr<CustomUIShapeManager> uiShapeManager =
			std::make_shared<CustomUIShapeManager>(m_UIRenderContext);
		m_ecs.registerComponent<UIShape>(uiShapeManager);

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

		auto& defaultMaterial = createMaterial();
		defaultMaterial.color = vec4(1.0f);
		defaultMaterial.metallic = 0.5f;
		defaultMaterial.roughness = 0.1f;

		// Create camera
		m_mainCamera = m_ecs.createEntity();
		m_services.tags().tag(m_mainCamera, "mainCamera");
		Transform& t = m_ecs.addComponent<Transform>(m_mainCamera);
		t.rotation = vec3(0, 0, -1.0f);
		ECS::Camera& c = m_ecs.addComponent<ECS::Camera>(m_mainCamera);

		// If a .weird file path was provided (via setSceneFilePath / registerScene),
		// restore saved scene state before the derived class's onStart() runs.
		if (!m_sceneFilePath.empty())
		{
			SceneSerializer::load(*this, m_sceneFilePath);
		}

		onStart(m_ecs, m_services);

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
		PROFILE_SCOPE("Scene Update");

		m_lastDelta = static_cast<float>(delta);

		if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown((Input::R)))
		{
			forceShaderRefresh();
		}

		// Update systems
		{
			if (m_debugFly)
			{
				PlayerMovementSystem::update(m_ecs, static_cast<float>(delta));
			}

			CameraSystem::update(m_ecs);
		}

		ButtonSystem::update(m_ecs, m_sdfs, getTime());

		{
			PROFILE_SCOPE("Physics synchronization");
			PhysicsSystem2D::update(m_ecs, m_simulation2D);

			if (m_debugInput)
			{
				PhysicsInteractionSystem::update(m_ecs);
			}

			m_simulation2D.update(delta);
		}

		{
			PROFILE_SCOPE("Collision handling");

			// Process queued collisions
			// Static vectors retain heap capacity across frames, avoiding
			// repeated allocations when thousands of collisions are generated.
			static std::vector<PhysicsCollisionEvent> collisions;
			static std::vector<PhysicsShapeCollisionEvent> shapeCollisions;
			collisions.clear();
			shapeCollisions.clear();
			{
				std::lock_guard<std::mutex> lock(m_collisionQueueMutex);
				std::swap(collisions, m_queuedCollisions);
				std::swap(shapeCollisions, m_queuedShapeCollisions);
			}

			auto rigidBodies = m_ecs.getComponentArray<RigidBody2D>();

			for (auto& ev : collisions)
			{
				EntityCollisionEvent entityEvent{ev, getEntityForSimulationId(ev.bodyA, rigidBodies),
												 getEntityForSimulationId(ev.bodyB, rigidBodies)};
				onEntityCollision(m_ecs, m_services, entityEvent);
			}

			for (auto& ev : shapeCollisions)
			{
				EntityShapeCollisionEvent entityEvent{ev, getEntityForSimulationId(ev.body, rigidBodies)};
				onEntityShapeCollision(m_ecs, m_services, entityEvent);

				const float m_soundFalloff = 0.1f;
				bool spatialAudio = false;
				auto camPosition = getCamera().position;
				float speed = glm::length2(ev.velocity);
				float distanceMultiplier =
					1.0f / (1.0f + (m_soundFalloff * glm::distance2(camPosition, vec3(ev.position, 0.0f))));
				float frictionSample = ev.friction * 0.01f * speed * (spatialAudio ? distanceMultiplier : 1.0f);

				m_frictionSoundLevel = std::max(frictionSample, m_frictionSoundLevel);

				if (ev.state == CollisionState::START)
				{
					float penetrationFactor = std::sqrt((std::min)(2.0f * ev.penetration, 1.0f));
					float volume = penetrationFactor;

					float freqFactor = std::abs(glm::dot(ev.normal, (ev.velocity)));
					freqFactor *= 0.01f;
					float frequency = 200.0f + (freqFactor * 300.0f);

					playSound(WeirdRenderer::SimpleAudioRequest{volume, frequency, false, vec3(ev.position, 0.0f)});
				}
			}

			m_frictionSoundLevelRead.store(m_frictionSoundLevel, std::memory_order_release);
			m_frictionSoundLevel = 0.0f;
		}

		{
			PROFILE_SCOPE("OnUpdate");
			onUpdate(m_ecs, m_services);
		}

		{
			PROFILE_SCOPE("Render Queue update");
			RenderSystem::update(m_ecs, m_resourceManager, m_drawQueue, m_lights);
		}

		m_ecs.freeRemovedComponents();
	}

	float Scene::getTime()
	{
		return static_cast<float>(m_simulation2D.getSimulationTime());
	}

	void Scene::handlePhysicsStep(void* userData)
	{
		Scene* self = static_cast<Scene*>(userData);
		self->onPhysicsStep(self->m_simulation2D);
	}

	void Scene::handleCollision(PhysicsCollisionEvent& event, void* userData)
	{
		Scene* self = static_cast<Scene*>(userData);
		self->onPhysicsRigidBodyCollision(self->m_simulation2D, event);

		std::lock_guard<std::mutex> lock(self->m_collisionQueueMutex);
		self->m_queuedCollisions.push_back(event);
	}

	void Scene::handleShapeCollision(PhysicsShapeCollisionEvent& event, void* userData)
	{
		Scene* self = static_cast<Scene*>(userData);
		self->onPhysicsShapeCollision(self->m_simulation2D, event);

		{
			std::lock_guard<std::mutex> lock(self->m_collisionQueueMutex);
			self->m_queuedShapeCollisions.push_back(event);
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
		// PROFILE_SCOPE("Fetch World Data");
		customShapeCount = m_ecs.getComponentArray<CustomShape>()->getSize();
		SDFRenderSystem::update<Dot, CustomShape, TextRenderer>(m_ecs, m_2DWorldRenderContext, data, size);
	}

	void Scene::get3DShapesData(vec4*& data, uint32_t& size, uint32_t& customShapeCount)
	{
		// PROFILE_SCOPE("Fetch 3D World Data");
		customShapeCount = m_ecs.getComponentArray<CustomShape>()->getSize();
		SDFRenderSystem::update<Dot, CustomShape, TextRenderer>(m_ecs, m_3DWorldRenderContext, data, size);
	}

	void Scene::getUIData(vec4*& uiData, uint32_t& size, uint32_t& customShapeCount)
	{
		// PROFILE_SCOPE("Fetch UI Data");
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

	std::vector<WeirdRenderer::Light>& Scene::getLights()
	{
		return m_lights;
	}

	void Scene::renderExtra(WeirdRenderer::RenderTarget& renderTarget)
	{
		if (m_renderMode == RenderMode::RayMarching3D || m_renderMode == RenderMode::RayMarchingBoth)
		{
			onRender(m_ecs, m_services, renderTarget);
		}
	}

	// SDFs

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

	Entity Scene::getEntityForSimulationId(SimulationID simulationId,
										   std::shared_ptr<ComponentArray<RigidBody2D>> rigidBodies)
	{
		if (simulationId >= static_cast<SimulationID>(rigidBodies->getSize()))
			return INVALID_ENTITY;

		return rigidBodies->getEntityAtIdx(static_cast<size_t>(simulationId));
	}

	void Scene::loadFromWeirdFile(const std::string& path)
	{
		SceneSerializer::load(*this, path);
	}

	// ServiceProvider

	ServiceProvider::ServiceProvider(Scene& scene)
		: m_ecs(scene.m_ecs)
		, m_time(scene.m_simulation2D, scene.m_lastDelta)
		, m_physics(scene.m_ecs, scene.m_simulation2D, scene.m_sdfs)
		, m_shapes(scene.m_ecs, scene.m_simulation2D, scene.m_sdfs)
		, m_render(scene.m_ecs, scene.m_mainCamera, scene.m_2DWorldRenderContext, scene.m_3DWorldRenderContext,
				   scene.m_UIRenderContext, scene.m_lights, scene.m_background, scene.m_renderMode)
		, m_materials(scene.m_materials, scene.m_materialCount)
		, m_audio(scene.m_audioQueue, scene.m_frictionSoundLevelRead)
		, m_tags(scene.m_tagToEntity, scene.m_entityToTag)
		, m_serialization(scene, scene.m_serializationBlacklist, scene.m_sceneFilePath)
		, m_sceneControl(scene.m_isSceneComplete, scene.m_nextScene)
		, m_resources{scene.m_resourceManager, ""}
		, m_debug(scene.m_debugFly, scene.m_debugInput)
		, m_input()
	{
	}

	ShapeId ShapeService::registerDefaultSDF(std::shared_ptr<IMathExpression> sdf)
	{
		return Scene::registerDefaultSDF(std::move(sdf));
	}

	void SerializationService::saveScene(const std::string& filename)
	{
		SceneSerializer::save(scene, filename);
	}

	TagMap SerializationService::loadWeirdFile(const std::string& path, bool blacklistEntities)
	{
		TagMap loadedTags;
		Entity firstNewEntity = scene.m_ecs.getEntityCount();
		SceneSerializer::load(scene, path, &loadedTags);
		if (blacklistEntities)
		{
			Entity lastNewEntity = scene.m_ecs.getEntityCount();
			for (Entity entity = firstNewEntity; entity < lastNewEntity; ++entity)
				scene.m_serializationBlacklist.insert(entity);
		}
		return loadedTags;
	}

	RaymarchResult raymarchScene(ECSManager& ecs, std::vector<std::shared_ptr<IMathExpression>>& sdfs,
								 Simulation2D& simulation, float time, glm::vec2 origin, glm::vec2 direction,
								 float epsilon, float maxDistance)
	{
		float traveled = 0.0f;
		auto gridSnapshot = simulation.getSpatialGridSnapshot();

		if (epsilon <= 0.0f)
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

			// Cache the rigid bodies component array to avoid repeated lookups in the ECS during the raymarching loop
			auto rigidBodies = ecs.getComponentArray<RigidBody2D>();

			auto shapeArray = ecs.getComponentArray<CustomShape>();
			for (size_t j = 0; j < shapeArray->getSize(); j++)
			{
				auto& shape = shapeArray->getDataAtIdx(j);

				if (!shape.hasCollisions)
					continue;

				if (shape.distanceFieldId >= sdfs.size())
					continue;

				float parameters[11];
				std::copy(std::begin(shape.parameters), std::end(shape.parameters), std::begin(parameters));
				parameters[8] = time;
				parameters[9] = p.x;
				parameters[10] = p.y;

				float dist = sdfs[shape.distanceFieldId]->getValue(parameters);
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

				auto entityForSimulationId = [&](SimulationID simulationId) -> Entity
				{
					if (simulationId >= static_cast<SimulationID>(rigidBodies->getSize()))
						return INVALID_ENTITY;

					return rigidBodies->getEntityAtIdx(static_cast<size_t>(simulationId));
				};

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
								closestRbEntity = entityForSimulationId(rbIndex);
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
#ifndef WEIRD_DISABLE_IMGUI
		const char* label = "Settings";
		if (ImGui::CollapsingHeader(label))
		{
			ImGui::PushID(label);

			if (ImGui::Checkbox("Pause simulation", &m_simulationIsPaused))
			{
				if (m_simulationIsPaused)
					m_simulation2D.pause();
				else
					m_simulation2D.resume();
			}

			ImGui::SeparatorText("Background");
			const char* bgTypes[] = {"Solid", "Grid", "Sky", "Custom"};
			int bgTypeIdx = static_cast<int>(m_background.type);
			if (ImGui::Combo("Type", &bgTypeIdx, bgTypes, 4))
			{
				m_background.type = static_cast<BackgroundType>(bgTypeIdx);
			}

			if (m_background.type != BackgroundType::Custom)
			{
				ImGui::ColorEdit4("Primary Color", &m_background.primaryColor[0]);
				ImGui::ColorEdit4("Secondary Color", &m_background.secondaryColor[0]);
				ImGui::DragFloat("Scale", &m_background.scale, 0.05f, 0.01f, 100.0f);
				ImGui::DragFloat("Intensity", &m_background.intensity, 0.05f, 0.0f, 10.0f);
			}

			ImGui::Separator();

			onImGuiRender(m_ecs, m_services);

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

				std::string tag = m_services.tags().getEntityTag(e);
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
#endif
	}

	void Scene::renderPhysicsStatsUI()
	{
#ifndef WEIRD_DISABLE_IMGUI
		auto simStats = m_simulation2D.getPerformanceStats();
		ImGui::Text("Step:  %.2f ms", simStats.timePerStepMs);
		ImGui::Text("  Broad Phase: %.3f ms", simStats.broadPhaseMs);
		ImGui::Text("  Narrow Phase: %.3f ms", simStats.narrowPhaseMs);
		ImGui::Text("  Shape Eval:  %.3f ms", simStats.shapeEvaluationMs);
		ImGui::Text("  Collisions:  %.3f ms", simStats.collisionEventsMs);
		ImGui::Text("  Integration: %.3f ms", simStats.integrationMs);
		ImGui::Text("Sim/Real Time: %.2fx", simStats.simulationRatio);
#endif
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