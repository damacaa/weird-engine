#include "weird-engine/Scene.h"
#include "weird-engine/SceneManager.h"
#include "weird-renderer/audio/AudioEngine.h"

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

	ShapeId Scene::registerDefaultSDF(const Expr& sdf)
	{
		return registerDefaultSDF(sdf.node);
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
		// Default 2D material (slot 0)
		m_materials2D[0].id = 0;
		m_materials2D[0].name = "default";
		m_materials2D[0].color = vec4(1.0f);
		m_materials2D[0].secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		m_materials2D[0].pattern = Pattern2D::None;
		m_materials2D[0].patternScale = 1.0f;
		m_materials2D[0].emission = 0.0f;
		m_materials2D[0].edgeThickness = 0.0f;
		m_materials2D[0].edgeColor = vec4(0.0f);
		m_materials2D[0].refraction = 0.0f;
		m_material2DNameToId["default"] = 0;

		for (size_t i = 1; i < 16; ++i)
		{
			m_materials2D[i].id = static_cast<uint16_t>(i);
			m_materials2D[i].name = "";
			m_materials2D[i].color = vec4(1.0f);
			m_materials2D[i].secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
			m_materials2D[i].pattern = Pattern2D::None;
			m_materials2D[i].patternScale = 1.0f;
			m_materials2D[i].emission = 0.0f;
			m_materials2D[i].edgeThickness = 0.0f;
			m_materials2D[i].edgeColor = vec4(0.0f);
			m_materials2D[i].refraction = 0.0f;
		}

		// Default 3D material (slot 0)
		m_materials3D[0].id = 0;
		m_materials3D[0].name = "default";
		m_materials3D[0].color = vec4(1.0f);
		m_materials3D[0].secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		m_materials3D[0].metallic = 0.0f;
		m_materials3D[0].roughness = 1.0f;
		m_materials3D[0].pattern = MaterialPattern::None;
		m_materials3D[0].patternScale = 1.0f;
		m_materials3D[0].emission = 0.0f;
		m_material3DNameToId["default"] = 0;

		for (size_t i = 1; i < 16; ++i)
		{
			m_materials3D[i].id = static_cast<uint16_t>(i);
			m_materials3D[i].name = "";
			m_materials3D[i].color = vec4(1.0f);
			m_materials3D[i].secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
			m_materials3D[i].metallic = 0.0f;
			m_materials3D[i].roughness = 1.0f;
			m_materials3D[i].pattern = MaterialPattern::None;
			m_materials3D[i].patternScale = 1.0f;
			m_materials3D[i].emission = 0.0f;
		}

		m_material2DCount = 1;
		m_material3DCount = 1;
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
		onCreate(m_registry, m_services);
		for (auto& sys : m_createSystems)
		{
			sys(m_registry, m_services);
		}

		// Custom component managers
		std::shared_ptr<RigidBodyManager> rbManager = std::make_shared<RigidBodyManager>(m_simulation2D);
		m_registry.registerComponent<RigidBody2D>(rbManager);

		if (m_renderMode == RenderMode::RayMarching2D)
		{
			std::shared_ptr<CustomShapeManager> shapeManager =
				std::make_shared<CustomShapeManager>(m_simulation2D, m_2DWorldRenderContext);
			m_registry.registerComponent<CustomShape>(shapeManager);
		}
		else
		{
			std::shared_ptr<CustomShapeManager> shapeManager =
				std::make_shared<CustomShapeManager>(m_simulation2D, m_3DWorldRenderContext);
			m_registry.registerComponent<CustomShape>(shapeManager);
		}

		std::shared_ptr<DistanceConstraintManager> distManager =
			std::make_shared<DistanceConstraintManager>(m_simulation2D, m_registry);
		m_registry.registerComponent<DistanceConstraint>(distManager);

		std::shared_ptr<SpringManager> springManager = std::make_shared<SpringManager>(m_simulation2D, m_registry);
		m_registry.registerComponent<Spring>(springManager);

		std::shared_ptr<CustomUIShapeManager> uiShapeManager =
			std::make_shared<CustomUIShapeManager>(m_UIRenderContext);
		m_registry.registerComponent<UIShape>(uiShapeManager);

		// Shapes
		m_sdfs = Scene::getGlobalSDFs();
		m_simulation2D.setSDFs(m_sdfs);

		// Initialize simulation
		PhysicsSystem2D::init(m_registry, m_simulation2D);

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

		// Create camera
		m_mainCamera = m_registry.createEntity();
		m_services.tags().tag(m_mainCamera, "mainCamera");
		Transform& t = m_registry.addComponent<Transform>(m_mainCamera);
		t.rotation = vec3(0, 0, -1.0f);
		ECS::Camera& c = m_registry.addComponent<ECS::Camera>(m_mainCamera);

		// If a .weird file path was provided (via setSceneFilePath / registerScene),
		// restore saved scene state before the derived class's onStart() runs.
		if (!m_sceneFilePath.empty())
		{
			SceneSerializer::load(*this, m_sceneFilePath);
		}

		onStart(m_registry, m_services);
		for (auto& sys : m_startSystems)
		{
			sys(m_registry, m_services);
		}

		// Refresh simulation SDFs in case onStart registered custom SDFs
		m_simulation2D.setSDFs(m_sdfs);

		switch (m_renderMode)
		{
			case WeirdEngine::Scene::RenderMode::RayMarching3D:
			{
				FlyMovement& fly = m_registry.addComponent<FlyMovement>(m_mainCamera);
				break;
			}
			case WeirdEngine::Scene::RenderMode::RayMarching2D:
			case WeirdEngine::Scene::RenderMode::RayMarchingBoth:
			{
				FlyMovement2D& fly = m_registry.addComponent<FlyMovement2D>(m_mainCamera);
				fly.targetPosition = m_registry.getComponent<Transform>(m_mainCamera).position;
				break;
			}
			default:
				break;
		}

		PhysicsSystem2D::update(m_registry, m_simulation2D);
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
				PlayerMovementSystem::update(m_registry, static_cast<float>(delta));
			}

			CameraSystem::update(m_registry);
		}

		ButtonSystem::update(m_registry, m_sdfs, getTime());

		{
			PROFILE_SCOPE("Physics synchronization");
			PhysicsSystem2D::update(m_registry, m_simulation2D);

			if (m_debugInput)
			{
				PhysicsInteractionSystem::update(m_registry);
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

			auto rigidBodies = m_registry.getComponentArray<RigidBody2D>();

			for (auto& ev : collisions)
			{
				EntityCollisionEvent entityEvent{ev, getEntityForSimulationId(ev.bodyA, rigidBodies),
												 getEntityForSimulationId(ev.bodyB, rigidBodies)};
				onEntityCollision(m_registry, m_services, entityEvent);
				for (auto& sys : m_entityCollisionSystems)
				{
					sys(m_registry, m_services, entityEvent);
				}

				float relSpeed = glm::length(ev.relativeVelocity);
				float impactIntensity = std::clamp(relSpeed * 0.08f + ev.impulse * 0.04f, 0.0f, 1.0f);
				if (impactIntensity > 0.025f)
				{
					// Dynamic frequency cutoff: heavier impacts sound lower, lighter sound crisper
					float cutoff = 250.0f + 800.0f * (1.0f - impactIntensity * 0.5f);
					float vol = std::clamp(impactIntensity * 0.3f, 0.01f, 0.4f);

					WeirdRenderer::SimpleAudioRequest req;
					req.volume = vol;
					req.frequency = cutoff;
					req.spatial = true;
					req.position = vec3(ev.position, 0.0f);
					req.intensity = impactIntensity;
					req.instrument = 3; // Filtered Noise
					req.beats = 1;

					m_audioQueue.push(req);
				}
			}

			for (auto& ev : shapeCollisions)
			{
				EntityShapeCollisionEvent entityEvent{ev, getEntityForSimulationId(ev.body, rigidBodies)};
				onEntityShapeCollision(m_registry, m_services, entityEvent);
				for (auto& sys : m_entityShapeCollisionSystems)
				{
					sys(m_registry, m_services, entityEvent);
				}

				float speed = glm::length(ev.velocity);
				float frictionSample = ev.friction * 0.08f * speed;
				m_frictionSoundLevel = std::max(frictionSample, m_frictionSoundLevel);

				if (ev.state == CollisionState::START)
				{
					float normalSpeed = std::abs(glm::dot(ev.normal, ev.velocity));
					float penetrationFactor = std::sqrt((std::min)(2.0f * ev.penetration, 1.0f));
					float impactIntensity = std::clamp(normalSpeed * 0.12f + penetrationFactor * 0.3f, 0.0f, 1.0f);

					if (impactIntensity > 0.02f)
					{
						// Dynamic frequency cutoff: heavier impacts have deeper bass, lighter have crisper click
						float cutoff = 180.0f + 1100.0f * (1.0f - impactIntensity * 0.5f);
						float vol = std::clamp(impactIntensity * 0.35f, 0.01f, 0.5f);

						WeirdRenderer::SimpleAudioRequest req;
						req.volume = vol;
						req.frequency = cutoff;
						req.spatial = true;
						req.position = vec3(ev.position, 0.0f);
						req.intensity = impactIntensity;
						req.instrument = 3; // Filtered Noise
						req.beats = 1;

						m_audioQueue.push(req);
					}
				}
			}

			m_frictionSoundLevelRead.store(m_frictionSoundLevel, std::memory_order_release);
			m_frictionSoundLevel = 0.0f;
		}

		{
			PROFILE_SCOPE("OnUpdate");
			onUpdate(m_registry, m_services);
			for (auto& sys : m_updateSystems)
			{
				sys(m_registry, m_services);
			}
			m_services.audio().updateVisualization();
		}

		{
			PROFILE_SCOPE("Render Queue update");
			RenderSystem::update(m_registry, m_resourceManager, m_drawQueue, m_lights2D, m_lights3D);
		}

		m_registry.freeRemovedComponents();
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
		return m_registry.getComponent<Camera>(m_mainCamera).camera;
	}

	void Scene::get2DShapesData(vec4*& data, uint32_t& size, uint32_t& customShapeCount)
	{
		// PROFILE_SCOPE("Fetch World Data");
		customShapeCount = m_registry.getComponentArray<CustomShape>()->getSize();
		SDFRenderSystem::update<Dot, CustomShape, TextRenderer>(m_registry, m_2DWorldRenderContext, data, size);
	}

	void Scene::get3DShapesData(vec4*& data, uint32_t& size, uint32_t& customShapeCount)
	{
		// PROFILE_SCOPE("Fetch 3D World Data");
		customShapeCount = m_registry.getComponentArray<CustomShape>()->getSize();
		SDFRenderSystem::update<Dot, CustomShape, TextRenderer>(m_registry, m_3DWorldRenderContext, data, size);
	}

	void Scene::getUIData(vec4*& uiData, uint32_t& size, uint32_t& customShapeCount)
	{
		// PROFILE_SCOPE("Fetch UI Data");
		m_services.audio().updateVisualization();
		customShapeCount = m_registry.getComponentArray<UIShape>()->getSize();
		SDFRenderSystem::update<UIDot, UIShape, UITextRenderer>(m_registry, m_UIRenderContext, uiData, size);
	}

	void Scene::update2DWorldShader(WeirdRenderer::Shader& shader)
	{
		m_simulation2D.setSDFs(m_sdfs);
		SDFShaderGenerationSystem::update<CustomShape, SDFRenderSystemContext, false>(
			m_registry, m_2DWorldRenderContext, shader, m_sdfs);
	}

	void Scene::update3DWorldShader(WeirdRenderer::Shader& shader)
	{
		m_simulation2D.setSDFs(m_sdfs);
		SDFShaderGenerationSystem::update<CustomShape, SDFRenderSystemContext, true>(m_registry, m_3DWorldRenderContext,
																					 shader, m_sdfs);
	}

	void Scene::updateUIShader(WeirdRenderer::Shader& shader)
	{
		m_simulation2D.setSDFs(m_sdfs);
		SDFShaderGenerationSystem::update<UIShape, SDFRenderSystemContext, false>(m_registry, m_UIRenderContext, shader,
																				  m_sdfs);
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

	std::vector<WeirdRenderer::Light2D>& Scene::getLights2D()
	{
		return m_lights2D;
	}

	std::vector<WeirdRenderer::Light3D>& Scene::getLights3D()
	{
		return m_lights3D;
	}

	void Scene::renderExtra(WeirdRenderer::RenderTarget& renderTarget)
	{
		if (m_renderMode == RenderMode::RayMarching3D || m_renderMode == RenderMode::RayMarchingBoth)
		{
			onRender(m_registry, m_services, renderTarget);
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
		: m_registry(scene.m_registry)
		, m_time(scene.m_simulation2D, scene.m_lastDelta)
		, m_physics(scene.m_registry, scene.m_simulation2D, scene.m_sdfs)
		, m_shapes(scene.m_registry, scene.m_simulation2D, scene.m_sdfs)
		, m_render(scene.m_registry, scene.m_mainCamera, scene.m_2DWorldRenderContext, scene.m_3DWorldRenderContext,
				   scene.m_UIRenderContext, scene.m_lights2D, scene.m_lights3D, scene.m_background, scene.m_renderMode)
		, m_materials2D{scene.m_materials2D, scene.m_material2DCount, scene.m_material2DNameToId}
		, m_materials3D{scene.m_materials3D, scene.m_material3DCount, scene.m_material3DNameToId}
		, m_audio(scene.m_audioQueue, scene.m_frictionSoundLevelRead, &m_shapes, &scene.m_registry)
		, m_tags(scene.m_tagToEntity, scene.m_entityToTag)
		, m_serialization(scene, scene.m_serializationBlacklist, scene.m_sceneFilePath)
		, m_sceneControl(scene.m_isSceneComplete, scene.m_nextScene)
		, m_resources{scene.m_resourceManager, ""}
		, m_debug(scene.m_debugFly, scene.m_debugInput)
		, m_input()
	{
	}

	ShapeId ShapeService::registerDefaultSDF(const Expr& sdf)
	{
		return Scene::registerDefaultSDF(sdf);
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
		Entity firstNewEntity = scene.m_registry.getEntityCount();
		SceneSerializer::load(scene, path, &loadedTags);
		if (blacklistEntities)
		{
			Entity lastNewEntity = scene.m_registry.getEntityCount();
			for (Entity entity = firstNewEntity; entity < lastNewEntity; ++entity)
				scene.m_serializationBlacklist.insert(entity);
		}
		return loadedTags;
	}

	void AudioService::setSpatialAudioEnabled(bool enabled)
	{
		WeirdRenderer::AudioEngine::getInstance().setSpatialAudioEnabled(enabled);
	}

	bool AudioService::isSpatialAudioEnabled() const
	{
		return WeirdRenderer::AudioEngine::getInstance().isSpatialAudioEnabled();
	}

	WeirdRenderer::SdfMusicEngine& AudioService::music()
	{
		return WeirdRenderer::AudioEngine::getInstance().getMusicEngine();
	}

	WeirdRenderer::PhysicsAudioEngine& AudioService::physicsAudio()
	{
		return WeirdRenderer::AudioEngine::getInstance().getPhysicsEngine();
	}

	void AudioService::setSong(std::shared_ptr<WeirdRenderer::SdfSong> song, bool beatSynced)
	{
		SongVisualizationOptions defaultVisualOptions;
		setSong(std::move(song), defaultVisualOptions, beatSynced);
	}

	Entity AudioService::setSong(std::shared_ptr<WeirdRenderer::SdfSong> song,
								 const SongVisualizationOptions& visualOptions, bool beatSynced)
	{
		auto songPtr = song;
		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().setSong(song, beatSynced);

		if (shapes && registry && songPtr && songPtr->getShapeExpression())
		{
			if (m_visualizationEntity != INVALID_ENTITY)
			{
				registry->destroyEntity(m_visualizationEntity);
				m_visualizationEntity = INVALID_ENTITY;
			}

			ShapeId shapeId = shapes->registerSDF(songPtr->getShapeExpression());
			UIShapeConfig config;
			config.shapeId = shapeId;
			config.material = visualOptions.material;
			config.combination = visualOptions.combination;
			config.group = visualOptions.group;
			std::copy_n(songPtr->getParameters(), 8, config.variables.data);
			config.variables.data[7] = 1.0f;

			m_visualizationEntity = shapes->addUIShape(config);

			for (size_t i = 0; i < 8; ++i)
			{
				m_lastSyncedParams[i] = songPtr->getParameter(i);
			}
			m_lastSyncedParams[7] = 1.0f;

			return m_visualizationEntity;
		}

		m_visualizationEntity = INVALID_ENTITY;
		return INVALID_ENTITY;
	}

	void AudioService::setSongParameter(size_t index, float value)
	{
		if (index >= 7)
			return;

		auto curSong = WeirdRenderer::AudioEngine::getInstance().getMusicEngine().getCurrentSong();
		if (curSong)
		{
			curSong->setParameter(index, value);
		}

		m_lastSyncedParams[index] = value;

		if (m_visualizationEntity != INVALID_ENTITY && registry &&
			registry->hasComponent<UIShape>(m_visualizationEntity))
		{
			auto& ui = registry->getComponent<UIShape>(m_visualizationEntity);
			ui.parameters[index] = value;
			registry->setComponentDirty(ui);
		}

		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().resampleShape();
	}

	void AudioService::updateVisualization()
	{
		if (m_visualizationEntity != INVALID_ENTITY && registry &&
			registry->hasComponent<UIShape>(m_visualizationEntity))
		{
			auto& ui = registry->getComponent<UIShape>(m_visualizationEntity);
			float volume = WeirdRenderer::AudioEngine::getInstance().getAudioData().currentVolume;
			float scale = 1.0f + volume * 0.8f;
			if (ui.parameters[7] != scale)
			{
				ui.parameters[7] = scale;
				registry->setComponentDirty(ui);
			}
		}
	}

	void AudioService::queueSong(std::shared_ptr<WeirdRenderer::SdfSong> song)
	{
		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().queueSong(std::move(song));
	}

	void AudioService::triggerPositiveFeedback(float intensity)
	{
		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().triggerPositiveFeedback(intensity);
	}

	void AudioService::triggerNegativeFeedback(float intensity)
	{
		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().triggerNegativeFeedback(intensity);
	}

	void AudioService::triggerDeath()
	{
		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().triggerDeath();
	}

	void AudioService::setTension(float level)
	{
		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().setTension(level);
	}

	void AudioService::setEnergy(float level)
	{
		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().setEnergy(level);
	}

	void AudioService::setHealth(float current, float max)
	{
		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().setHealth(current, max);
	}

	void AudioService::surge(float amount)
	{
		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().surge(amount);
	}

	void AudioService::duck(float amount)
	{
		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().duck(amount);
	}

	void AudioService::resampleShape()
	{
		if (m_visualizationEntity != INVALID_ENTITY && registry &&
			registry->hasComponent<UIShape>(m_visualizationEntity))
		{
			auto curSong = WeirdRenderer::AudioEngine::getInstance().getMusicEngine().getCurrentSong();
			if (curSong)
			{
				auto& ui = registry->getComponent<UIShape>(m_visualizationEntity);
				for (size_t i = 0; i < 7; ++i)
				{
					if (curSong->getParameter(i) != m_lastSyncedParams[i])
					{
						ui.parameters[i] = curSong->getParameter(i);
						registry->setComponentDirty(ui);
						m_lastSyncedParams[i] = curSong->getParameter(i);
					}
					else if (ui.parameters[i] != m_lastSyncedParams[i])
					{
						curSong->setParameter(i, ui.parameters[i]);
						m_lastSyncedParams[i] = ui.parameters[i];
					}
				}
			}
		}

		WeirdRenderer::AudioEngine::getInstance().getMusicEngine().resampleShape();
	}

	float AudioService::getMotionLevel() const
	{
		return WeirdRenderer::AudioEngine::getInstance().getMusicEngine().getMotionLevel();
	}

	float AudioService::getFillRatio() const
	{
		return WeirdRenderer::AudioEngine::getInstance().getMusicEngine().getFillRatio();
	}

	float AudioService::getTempoFromMotion() const
	{
		return WeirdRenderer::AudioEngine::getInstance().getMusicEngine().getTempoFromMotion();
	}

	float AudioService::getVolumeFromFill() const
	{
		return WeirdRenderer::AudioEngine::getInstance().getMusicEngine().getVolumeFromFill();
	}

	RaymarchResult raymarchScene(Registry& registry, std::vector<std::shared_ptr<IMathExpression>>& sdfs,
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
			auto rigidBodies = registry.getComponentArray<RigidBody2D>();

			auto shapeArray = registry.getComponentArray<CustomShape>();
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

			onImGuiRender(m_registry, m_services);
			for (auto& sys : m_imguiSystems)
			{
				sys(m_registry, m_services);
			}

			ImGui::PopID();
		}

		const char* label2 = "Hierarchy";
		if (ImGui::CollapsingHeader(label2))
		{

			ImGui::PushID(label2);

			for (Entity e = 0; e < m_registry.getEntityCount(); ++e)
			{
				std::vector<size_t> componentIDs = m_registry.getComponentTypes(e);

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
						std::string compName = m_registry.getComponentName(compID);
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
} // namespace WeirdEngine