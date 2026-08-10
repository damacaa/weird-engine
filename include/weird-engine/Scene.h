#pragma once

#include "ecs/ECS.h"
#include "ResourceManager.h"

#include "weird-engine/systems/SDFRenderSystem.h"

#include "weird-renderer/audio/AudioRingBuffer.h"
#include "weird-renderer/audio/SimpleAudioRequest.h"
#include "weird-renderer/core/RenderTarget.h"
#include "weird-renderer/resources/DrawCommand.h"

#include "weird-engine/Background.h"
#include "weird-engine/Material3D.h"
#include "weird-engine/services/ServiceProvider.h"
#include "weird-physics/PhysicsSettings.h"
#include "weird-physics/Simulation2D.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace WeirdEngine
{
	using namespace ECS;

	struct EntityCollisionEvent
	{
		// Raw event data from the physics thread. Read-only: the physics
		// response has already been applied by the time this is dispatched.
		const CollisionEvent& raw;
		Entity entityA;
		Entity entityB;
	};

	struct EntityShapeCollisionEvent
	{
		// Raw event data from the physics thread. Read-only: the physics
		// response has already been applied by the time this is dispatched.
		const ShapeCollisionEvent& raw;
		Entity entity;
	};

	// Forward declaration – full definition in SceneSerializer.h
	class SceneSerializer;

	class Scene
	{
		// Serialization and the service provider reach into the scene's
		// private state (storage lives here; the provider is a facade).
		friend class SceneSerializer;
		friend class ServiceProvider;
		friend struct SerializationService;

	public:
		// ---- Types
		/// Map from tag name (std::string) to the entity that owns it.
		using TagMap = ::WeirdEngine::TagMap;

		using RenderMode = ::WeirdEngine::RenderMode;

		using RaymarchResult = ::WeirdEngine::RaymarchResult;

		// ---- Lifecycle (engine-driven)
		Scene();
		virtual ~Scene();
		void start();

		// Called by the engine once per frame with the variable frame delta.
		void update(double delta, double time);

		// Called by SceneManager right before this scene is destroyed during
		// a scene transition. Runs on the main thread; the physics thread may
		// still be stepping, so keep the same thread rules as the callbacks.
		void destroy()
		{
			onDestroy(m_ecs, m_services);
		}

		// ---- Rendering pipeline (engine-driven)
		void renderExtra(WeirdRenderer::RenderTarget& renderTarget);
		void update2DWorldShader(WeirdRenderer::Shader& shader);
		void update3DWorldShader(WeirdRenderer::Shader& shader);
		void updateUIShader(WeirdRenderer::Shader& shader);
		void forceShaderRefresh();
		void get2DShapesData(vec4*& data, uint32_t& size, uint32_t& customShapeCount);
		void get3DShapesData(vec4*& data, uint32_t& size, uint32_t& customShapeCount);
		void getUIData(vec4*& uiData, uint32_t& size, uint32_t& customShapeCount);
		void renderImGui();
		void renderPhysicsStatsUI();

		// ---- Scene state access (engine-driven)
		WeirdRenderer::Camera& getCamera();
		std::vector<WeirdRenderer::Light>& getLights();
		const std::vector<WeirdRenderer::DrawCommand>& getDrawQueue() const;
		AudioRingBuffer<WeirdRenderer::SimpleAudioRequest, SOUND_QUEUE_SIZE>& getAudioQueue();
		float getFrictionSound();

		BackgroundParams& getBackground()
		{
			return m_background;
		}

		const BackgroundParams& getBackground() const
		{
			return m_background;
		}

		RenderMode getRenderMode() const;
		float getTime();
		Material3D& createMaterial();
		Material3D& getMaterial(int index)
		{
			return m_materials[index];
		}
		const Material3D* getMaterials() const
		{
			return m_materials;
		}

		// ---- Scene control
		bool isSceneComplete() const
		{
			return m_isSceneComplete;
		};
		std::string getNextScene() const
		{
			return m_nextScene;
		};

		// Set the path to a .weird file to load when the scene starts
		void setSceneFilePath(const std::string& path)
		{
			m_sceneFilePath = path;
		}

		// ---- Global SDF registry (engine-level, shared across scenes)
		static ShapeId registerDefaultSDF(std::shared_ptr<IMathExpression> sdf);
		static const std::vector<std::shared_ptr<IMathExpression>>& getGlobalSDFs();

	protected:
		// Internal constructor: sets the render mode for Scene2D/Scene3D/SceneBoth.
		Scene(RenderMode mode);

		// ---- Lifecycle callbacks
		virtual void onCreate(ECSManager& ecs, ServiceProvider& services) {};
		virtual void onStart(ECSManager& ecs, ServiceProvider& services) {}
		virtual void onUpdate(ECSManager& ecs, ServiceProvider& services) {};
		virtual void onDestroy(ECSManager& ecs, ServiceProvider& services) {};
		virtual void onRender(WeirdRenderer::RenderTarget& renderTarget) {};
		virtual void onImGuiRender(ECSManager& ecs, ServiceProvider& services) {};

		// ---- Main thread collision callbacks (onEntity* family). Fire after
		// the physics response has been applied; the events are read-only.
		// m_ecs is safe to use here (the physics thread only ever touches
		// Simulation2D internals). See the onPhysics* callbacks below for the
		// pre-response, mutable equivalents.
		virtual void onEntityCollision(ECSManager& ecs, ServiceProvider& services,
									   WeirdEngine::EntityCollisionEvent& event) {};
		virtual void onEntityShapeCollision(ECSManager& ecs, ServiceProvider& services,
											WeirdEngine::EntityShapeCollisionEvent& event) {};

		// ---- Physics thread callbacks (onPhysics* family). Fire on the
		// physics thread mid-step; no ECS access here. The collision events
		// are pre-response and mutable: tune the event fields (friction,
		// absorption), queue impulses, or call fix() to alter the solver's
		// behavior. Everything else belongs in the onEntity* main-thread
		// callbacks above (post-response, read-only).
		virtual void onPhysicsStep(Simulation2D& simulation) {};
		virtual void onPhysicsRigidBodyCollision(Simulation2D& simulation, WeirdEngine::CollisionEvent& event) {};
		virtual void onPhysicsShapeCollision(Simulation2D& simulation, WeirdEngine::ShapeCollisionEvent& event) {};

		// ---- Scene control helper
		void goToNextScene(std::string nextScene = "")
		{
			m_isSceneComplete = true;
			m_nextScene = nextScene;
		};

		// Deprecated alias of goToNextScene.
		void setSceneComplete(std::string nextScene = "")
		{
			goToNextScene(nextScene);
		};

		// ---- Shape creation & SDF registration
		ShapeId registerSDF(std::shared_ptr<IMathExpression> sdf);
		Entity addShape(ShapeId shapeId, float* variables, uint16_t material,
						CombinationType combination = CombinationType::Addition, bool hasCollision = true,
						int group = 0);
		Entity addShape(ShapeId shapeId, float* variables, const Material3D& material,
						CombinationType combination = CombinationType::Addition, bool hasCollision = true,
						int group = 0)
		{
			return addShape(shapeId, variables, material.id, combination, hasCollision, group);
		}

		Entity addUIShape(ShapeId shapeId, float* variables, uint16_t material,
						  CombinationType combination = CombinationType::Addition, int group = 0);
		Entity addUIShape(ShapeId shapeId, float* variables, const Material3D& material,
						  CombinationType combination = CombinationType::Addition, int group = 0)
		{
			return addUIShape(shapeId, variables, material.id, combination, group);
		}
		UIShape& addUIShape(ShapeId shapeId, float* variables, Entity& entity, int group = 0);

		// ---- Entity helpers
		// Assign a unique tag to an entity. If the tag is already owned by
		// another entity, it is moved to this one.  An empty name is treated
		// as a removal request (equivalent to calling removeTag).
		void tag(Entity entity, const std::string& name);
		// Remove any tag currently assigned to an entity.
		void removeTag(Entity entity);
		// Return the tag of an entity, or "" if none.
		std::string getEntityTag(Entity entity) const;
		// Return the entity that owns a tag, or MAX_ENTITIES if none.
		Entity getEntityByTag(const std::string& name) const;

		// Entities in this set will be skipped during scene serialization
		void blacklistEntity(Entity e)
		{
			m_serializationBlacklist.insert(e);
		}

		// ---- Persistence & physics queries
		// Save the current scene state to a .weird JSON file
		void saveScene(const std::string& filename);

		// Dynamically load a .weird file and add its contents to the scene.
		// If blacklistEntities is true, all entities created by the load will be
		// excluded from future scene serialization.
		// Returns a map of tag names to their corresponding entities.
		TagMap loadWeirdFile(const std::string& path, bool blacklistEntities = false);

		RaymarchResult raymarch(glm::vec2 origin, glm::vec2 direction, float epsilon = 0.001f,
								float maxDistance = 150.0f);

		// ---- Shared state (available to derived scenes)
		Entity m_mainCamera;
		ResourceManager m_resourceManager;
		std::vector<std::shared_ptr<IMathExpression>> m_sdfs;
		bool m_debugFly = false;
		bool m_debugInput = false;

	private:
		// ---- Internal helpers
		static void handlePhysicsStep(void* userData);
		static void handleCollision(CollisionEvent& event, void* userData);
		static void handleShapeCollision(ShapeCollisionEvent& event, void* userData);
		// Load scene state from a .weird JSON file
		void loadFromWeirdFile(const std::string& path);
		void playSound(const WeirdRenderer::SimpleAudioRequest& audio);
		// Resolve a physics SimulationID to the owning entity.
		Entity getEntityForSimulationId(SimulationID simulationId,
										std::shared_ptr<ComponentArray<RigidBody2D>> rigidBodies);

		// ---- Simulation
		ECSManager m_ecs;
		Simulation2D m_simulation2D;
		bool m_runSimulationInThread;
		bool m_simulationIsPaused = false;

		// ---- Collision queues (physics thread pushes, main thread drains)
		std::mutex m_collisionQueueMutex;
		std::vector<CollisionEvent> m_queuedCollisions;
		std::vector<ShapeCollisionEvent> m_queuedShapeCollisions;

		// ---- Audio, draw queue, lights
		AudioRingBuffer<WeirdRenderer::SimpleAudioRequest, SOUND_QUEUE_SIZE> m_audioQueue;
		float m_frictionSoundLevel{0.0f};
		std::atomic<float> m_frictionSoundLevelRead{0.0f};
		std::vector<WeirdRenderer::DrawCommand> m_drawQueue;
		std::vector<WeirdRenderer::Light> m_lights;

		// ---- Serialization & visuals
		std::unordered_set<Entity> m_serializationBlacklist;
		std::string m_sceneFilePath;
		BackgroundParams m_background;
		Material3D m_materials[16];
		uint16_t m_materialCount = 0;
		SDFRenderSystemContext m_2DWorldRenderContext;
		SDFRenderSystemContext m_3DWorldRenderContext;
		SDFRenderSystemContext m_UIRenderContext;
		RenderMode m_renderMode = RenderMode::RayMarching2D;

		// ---- Scene control state
		std::string m_nextScene;
		bool m_isSceneComplete = false;

		// ---- Entity tag storage (bidirectional maps kept in sync)
		TagMap m_tagToEntity;
		std::unordered_map<Entity, std::string> m_entityToTag;

		// ---- Service provider binding (must be the last declared member: it
		// holds references to the members above).
		float m_lastDelta = 0.0f;
		ServiceProvider m_services;
	};
	class Scene2D : public Scene
	{
	public:
		Scene2D()
			: Scene(RenderMode::RayMarching2D)
		{
		}
	};

	class Scene3D : public Scene
	{
	public:
		Scene3D()
			: Scene(RenderMode::RayMarching3D)
		{
		}
	};

	class SceneBoth : public Scene
	{
	public:
		SceneBoth()
			: Scene(RenderMode::RayMarchingBoth)
		{
		}
	};
} // namespace WeirdEngine
