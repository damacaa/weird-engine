#pragma once

#include "ecs/Registry.h"
#include "ResourceManager.h"

#include "weird-engine/systems/SDFRenderSystem.h"

#include "weird-audio/AudioRingBuffer.h"
#include "weird-audio/FrictionSource.h"
#include "weird-audio/SimpleAudioRequest.h"
#include "weird-renderer/core/RenderTarget.h"
#include "weird-renderer/resources/DrawCommand.h"

#include "weird-engine/Background.h"
#include "weird-engine/Material2D.h"
#include "weird-engine/Material3D.h"
#include "weird-engine/services/ServiceProvider.h"
#include "weird-physics/PhysicsSettings.h"
#include "weird-physics/Simulation2D.h"

#include <functional>
#include <span>
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
	using CoreSystem = std::function<void(Registry&, ServiceProvider&)>;
	using EntityCollisionSystem = std::function<void(Registry&, ServiceProvider&, EntityCollisionEvent&)>;
	using EntityShapeCollisionSystem = std::function<void(Registry&, ServiceProvider&, EntityShapeCollisionEvent&)>;

	namespace WeirdAudio
	{
		class AudioEngine;
	}

	namespace WeirdRenderer
	{

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

		friend class WeirdAudio::AudioEngine;
		friend class WeirdRenderer::Renderer;
		friend void Detail::runFrame(Detail::RuntimeContext& ctx);

	public:
		// ---- Types
		/// Map from tag name (std::string) to the entity that owns it.
		using TagMap = ::WeirdEngine::TagMap;

		using RenderMode = ::WeirdEngine::RenderMode;

		using RaymarchResult = ::WeirdEngine::RaymarchResult;

		virtual ~Scene();

		// ---- Global Builtin SDF registry (engine-level, shared across scenes)
		static void registerBuiltinSDFs();
		static std::span<const std::shared_ptr<IMathExpression>> getBuiltinSDFs();
		static std::span<const std::shared_ptr<IMathExpression>> getGlobalSDFs()
		{
			return getBuiltinSDFs();
		}
		static ShapeId registerDefaultSDF(const Expr& sdf);
		static ShapeId registerDefaultSDF(std::shared_ptr<IMathExpression> sdf);

		// ---- System Dispatcher (Register systems to be called automatically)
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
		virtual void onStart(Registry& registry, ServiceProvider& services) {}
		virtual void onUpdate(Registry& registry, ServiceProvider& services) {};
		virtual void onDestroy(Registry& registry, ServiceProvider& services) {};
		virtual void onImGuiRender(Registry& registry, ServiceProvider& services) {};
		virtual void onCustomUI(Registry& registry, ServiceProvider& services) {};
		virtual void onRender(Registry& registry, ServiceProvider& services,
							  WeirdRenderer::RenderTarget& renderTarget) {};

		// ---- Main thread collision callbacks (onEntity* family). Fire after
		// the physics response has been applied; the events are read-only.
		// m_registry is safe to use here (the physics thread only ever touches
		// Simulation2D internals). See the onPhysics* callbacks below for the
		// pre-response, mutable equivalents.
		virtual void onEntityCollision(Registry& registry, ServiceProvider& services,
									   WeirdEngine::EntityCollisionEvent& event) {};
		virtual void onEntityShapeCollision(Registry& registry, ServiceProvider& services,
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
			onDestroy(m_registry, m_services);
			for (auto& sys : m_destroySystems)
			{
				sys(m_registry, m_services);
			}
		}

		// ---- Rendering pipeline (engine-driven)
		void renderExtra(WeirdRenderer::RenderTarget& renderTarget);
		void update2DWorldShader(WeirdRenderer::Shader& shader);
		void update3DWorldShader(WeirdRenderer::Shader& shader);
		void updateUIShader(WeirdRenderer::Shader& shader);
		void forceShaderRefresh();
		void get2DShapesData(vec4*& data, uint32_t& size, uint32_t& shapeCount);
		void get3DShapesData(vec4*& data, uint32_t& size, uint32_t& shapeCount);
		void getUIData(vec4*& uiData, uint32_t& size, uint32_t& shapeCount);
		void renderSettingsTab();
		void renderHierarchyTab();
		void renderPhysicsTab();
		void renderAudioTab();
		void renderCustomUI();
		void renderPhysicsStatsUI();

		// ---- Scene state access (engine-driven)
		WeirdRenderer::Camera& getCamera();
		std::vector<WeirdRenderer::Light2D>& getLights2D();
		std::vector<WeirdRenderer::Light3D>& getLights3D();
		const std::vector<WeirdRenderer::DrawCommand>& getDrawQueue() const;
		AudioRingBuffer<WeirdAudio::SimpleAudioRequest, SOUND_QUEUE_SIZE>& getAudioQueue();
		float getFrictionSound();
		const std::vector<WeirdAudio::FrictionSource>& getFrictionSources() const
		{
			return m_frictionSources;
		}
		bool isFrictionSoundOverridden() const
		{
			return m_overrideFrictionSound;
		}

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
		float getLastDelta() const
		{
			return m_lastDelta;
		}
		const Material2D* getMaterials2D() const
		{
			return m_materials2D;
		}
		const Material3D* getMaterials3D() const
		{
			return m_materials3D;
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
		Registry m_registry;
		Simulation2D m_simulation2D;
		bool m_runSimulationInThread;
		bool m_simulationIsPaused = false;

		// ---- Collision queues (physics thread pushes, main thread drains)
		std::mutex m_collisionQueueMutex;
		std::vector<PhysicsCollisionEvent> m_queuedCollisions;
		std::vector<PhysicsShapeCollisionEvent> m_queuedShapeCollisions;

		// ---- Audio, draw queue, lights
		AudioRingBuffer<WeirdAudio::SimpleAudioRequest, SOUND_QUEUE_SIZE> m_audioQueue;
		float m_frictionSoundLevelHold{0.0f};
		std::atomic<float> m_frictionSoundLevelRead{0.0f};

		// Live friction level history for the debug graph. Ring buffer: the head
		// is the next write index, which is also the oldest sample.
		static constexpr size_t FRICTION_LEVEL_HISTORY_SIZE = 256;
		float m_frictionLevelHistory[FRICTION_LEVEL_HISTORY_SIZE] = {0.0f};
		size_t m_frictionHistoryHead = 0;

		// Per-body friction aggregation for the current frame. Accumulators are
		// indexed by SimulationID and validated with a frame stamp so nothing
		// needs to be cleared per frame, even with thousands of rigidbodies.
		struct FrictionBodyAccumulator
		{
			float maxCoef = 0.0f; // max per-event friction coefficient (pre-speed)
			float speedSq = 0.0f; // body speed squared at the loudest contact
			vec3 weightedPosition{0.0f};
			float weightSum = 0.0f;
			uint32_t stamp = 0;
		};
		std::vector<FrictionBodyAccumulator> m_frictionAccumulators;
		std::vector<SimulationID> m_touchedFrictionBodies;
		std::vector<WeirdAudio::FrictionSource> m_frictionSources;
		uint32_t m_frictionFrameStamp{0};

		// Per-body impact cooldown tracking to debounce micro-jitter in resting piles.
		std::vector<float> m_bodyLastImpactTime;

		bool m_enableFrictionSound{true};
		float m_frictionSoundMultiplier{0.25f};
		// 1.0 = 100% = 1.5x the historical collision mix; used by
		// SimpleAudioRequest::makeImpact for both real and fake impacts.
		float m_collisionSoundVolume{1.0f};
		bool m_overrideFrictionSound{false};
		float m_manualFrictionLevel{0.0f};
		bool m_waveformAutoScale{true};
		float m_waveformAutoScaleRange{0.05f};
		float m_waveformManualScale{1.0f};
		std::vector<WeirdRenderer::DrawCommand> m_drawQueue;
		std::vector<WeirdRenderer::Light2D> m_lights2D;
		std::vector<WeirdRenderer::Light3D> m_lights3D;

		// ---- Serialization & visuals
		std::unordered_set<Entity> m_serializationBlacklist;
		std::string m_sceneFilePath;
		BackgroundParams m_background;
		Material2D m_materials2D[16];
		uint16_t m_material2DCount = 0;
		std::unordered_map<std::string, uint16_t> m_material2DNameToId;
		Material3D m_materials3D[16];
		uint16_t m_material3DCount = 0;
		std::unordered_map<std::string, uint16_t> m_material3DNameToId;
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
