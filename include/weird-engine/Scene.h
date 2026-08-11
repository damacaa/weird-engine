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

#include <functional>
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
		const PhysicsCollisionEvent& raw;
		Entity entityA;
		Entity entityB;
	};

	struct EntityShapeCollisionEvent
	{
		// Raw event data from the physics thread. Read-only: the physics
		// response has already been applied by the time this is dispatched.
		const PhysicsShapeCollisionEvent& raw;
		Entity entity;
	};

	// Forward declaration – full definition in SceneSerializer.h
	class SceneSerializer;
	class SceneManager;

	// ---- System Signatures ----
	using CoreSystem = std::function<void(ECSManager&, ServiceProvider&)>;
	using EntityCollisionSystem = std::function<void(ECSManager&, ServiceProvider&, EntityCollisionEvent&)>;
	using EntityShapeCollisionSystem = std::function<void(ECSManager&, ServiceProvider&, EntityShapeCollisionEvent&)>;

	namespace WeirdRenderer
	{
		class AudioEngine;
		class Renderer;
		class MeshRenderPipeline;
	} // namespace WeirdRenderer

	namespace Detail
	{
		struct RuntimeContext;
		void runFrame(RuntimeContext& ctx);
	} // namespace Detail

	class Scene
	{
		// Serialization and the service provider reach into the scene's
		// private state (storage lives here; the provider is a facade).
		friend class SceneManager;
		friend class SceneSerializer;
		friend class ServiceProvider;
		friend struct SerializationService;

		friend class WeirdRenderer::AudioEngine;
		friend class WeirdRenderer::Renderer;
		friend void Detail::runFrame(Detail::RuntimeContext& ctx);

	public:
		// ---- Types
		/// Map from tag name (std::string) to the entity that owns it.
		using TagMap = ::WeirdEngine::TagMap;

		using RenderMode = ::WeirdEngine::RenderMode;

		using RaymarchResult = ::WeirdEngine::RaymarchResult;

		virtual ~Scene();

		// ---- Global SDF registry (engine-level, shared across scenes)
		static ShapeId registerDefaultSDF(std::shared_ptr<IMathExpression> sdf);
		static const std::vector<std::shared_ptr<IMathExpression>>& getGlobalSDFs();

		// ---- System Dispatcher (Register systems to be called automatically)
		void addCreateSystem(CoreSystem system)
		{
			m_createSystems.push_back(std::move(system));
		}
		void addStartSystem(CoreSystem system)
		{
			m_startSystems.push_back(std::move(system));
		}
		void addUpdateSystem(CoreSystem system)
		{
			m_updateSystems.push_back(std::move(system));
		}
		void addDestroySystem(CoreSystem system)
		{
			m_destroySystems.push_back(std::move(system));
		}
		void addImGuiRenderSystem(CoreSystem system)
		{
			m_imguiSystems.push_back(std::move(system));
		}
		void addEntityCollisionSystem(EntityCollisionSystem system)
		{
			m_entityCollisionSystems.push_back(std::move(system));
		}
		void addEntityShapeCollisionSystem(EntityShapeCollisionSystem system)
		{
			m_entityShapeCollisionSystems.push_back(std::move(system));
		}

	protected:
		// Internal constructors: sets the render mode for Scene2D/Scene3D/SceneBoth.
		Scene();
		Scene(RenderMode mode);

		// ---- Lifecycle callbacks
		virtual void onCreate(ECSManager& ecs, ServiceProvider& services) {};
		virtual void onStart(ECSManager& ecs, ServiceProvider& services) {}
		virtual void onUpdate(ECSManager& ecs, ServiceProvider& services) {};
		virtual void onDestroy(ECSManager& ecs, ServiceProvider& services) {};
		virtual void onImGuiRender(ECSManager& ecs, ServiceProvider& services) {};
		virtual void onRender(ECSManager& ecs, ServiceProvider& services, WeirdRenderer::RenderTarget& renderTarget) {};

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
		virtual void onPhysicsRigidBodyCollision(Simulation2D& simulation, WeirdEngine::PhysicsCollisionEvent& event) {
		};
		virtual void onPhysicsShapeCollision(Simulation2D& simulation, WeirdEngine::PhysicsShapeCollisionEvent& event) {
		};

	private:
		// ---- Lifecycle (engine-driven)
		void start();
		void update(double delta, double time);
		void destroy()
		{
			onDestroy(m_ecs, m_services);
			for (auto& sys : m_destroySystems)
			{
				sys(m_ecs, m_services);
			}
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

		// ---- Internal helpers
		static void handlePhysicsStep(void* userData);
		static void handleCollision(PhysicsCollisionEvent& event, void* userData);
		static void handleShapeCollision(PhysicsShapeCollisionEvent& event, void* userData);
		// Load scene state from a .weird JSON file
		void loadFromWeirdFile(const std::string& path);
		void playSound(const WeirdRenderer::SimpleAudioRequest& audio);
		// Resolve a physics SimulationID to the owning entity.
		Entity getEntityForSimulationId(SimulationID simulationId,
										std::shared_ptr<ComponentArray<RigidBody2D>> rigidBodies);

		ServiceProvider m_services;

		// ---- Shared state (managed via ServiceProvider)
		Entity m_mainCamera;
		ResourceManager m_resourceManager;
		std::vector<std::shared_ptr<IMathExpression>> m_sdfs;
		bool m_debugFly = false;
		bool m_debugInput = false;

		// ---- Simulation
		ECSManager m_ecs;
		Simulation2D m_simulation2D;
		bool m_runSimulationInThread;
		bool m_simulationIsPaused = false;

		// ---- Collision queues (physics thread pushes, main thread drains)
		std::mutex m_collisionQueueMutex;
		std::vector<PhysicsCollisionEvent> m_queuedCollisions;
		std::vector<PhysicsShapeCollisionEvent> m_queuedShapeCollisions;

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

		// ---- Registered Systems
		std::vector<CoreSystem> m_createSystems;
		std::vector<CoreSystem> m_startSystems;
		std::vector<CoreSystem> m_updateSystems;
		std::vector<CoreSystem> m_destroySystems;
		std::vector<CoreSystem> m_imguiSystems;
		std::vector<EntityCollisionSystem> m_entityCollisionSystems;
		std::vector<EntityShapeCollisionSystem> m_entityShapeCollisionSystems;

		float m_lastDelta = 0.0f;
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
