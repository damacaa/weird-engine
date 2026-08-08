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

	constexpr int SOUND_QUEUE_SIZE = 16;

	// Forward declaration – full definition in SceneSerializer.h
	class SceneSerializer;

	class Scene
	{
		friend class SceneSerializer;

	public:
		/// Map from tag name (std::string) to the entity that owns it.
		using TagMap = std::unordered_map<std::string, Entity>;

		Scene();
		virtual ~Scene();
		void start();

		void renderExtra(WeirdRenderer::RenderTarget& renderTarget);

		void update2DWorldShader(WeirdRenderer::Shader& shader);
		void update3DWorldShader(WeirdRenderer::Shader& shader);
		void updateUIShader(WeirdRenderer::Shader& shader);
		void forceShaderRefresh();
		void update(double delta, double time);

		void get2DShapesData(vec4*& data, uint32_t& size, uint32_t& customShapeCount);
		void get3DShapesData(vec4*& data, uint32_t& size, uint32_t& customShapeCount);
		void getUIData(vec4*& uiData, uint32_t& size, uint32_t& customShapeCount);

		WeirdRenderer::Camera& getCamera();
		std::vector<WeirdRenderer::Light>& getLights();

		Simulation2D& getSimulation2D()
		{
			return m_simulation2D;
		}

		Material3D& createMaterial();

		Material3D& getMaterial(int index)
		{
			return m_materials[index];
		}
		const Material3D* getMaterials() const
		{
			return m_materials;
		}

		BackgroundParams& getBackground()
		{
			return m_background;
		}
		const BackgroundParams& getBackground() const
		{
			return m_background;
		}

		float getTime();

		enum class RenderMode
		{
			RayMarching3D,
			RayMarching2D,
			RayMarchingBoth
		};

		RenderMode getRenderMode() const;

		float getFrictionSound();
		const std::vector<WeirdRenderer::DrawCommand>& getDrawQueue() const;
		AudioRingBuffer<WeirdRenderer::SimpleAudioRequest, SOUND_QUEUE_SIZE>& getAudioQueue();

		bool isSceneComplete() const
		{
			return m_isSceneComplete;
		};
		std::string getNextScene() const
		{
			return m_nextScene;
		};

		static ShapeId registerDefaultSDF(std::shared_ptr<IMathExpression> sdf);
		static const std::vector<std::shared_ptr<IMathExpression>>& getGlobalSDFs();

		ShapeId registerSDF(std::shared_ptr<IMathExpression> sdf);

		// Set the path to a .weird file to load when the scene starts
		void setSceneFilePath(const std::string& path)
		{
			m_sceneFilePath = path;
		}

		struct RaymarchResult
		{
			float distance;
			Entity entity;
		};

		// Physics queries
		RaymarchResult raymarch(glm::vec2 origin, glm::vec2 direction, float epsilon = 0.001f,
								float maxDistance = 150.0f);

		// Called by SceneManager right before this scene is destroyed during
		// a scene transition. Runs on the main thread; the physics thread may
		// still be stepping, so keep the same thread rules as the callbacks below.
		virtual void onDestroy() {};

		void renderImGui();
		void renderPhysicsStatsUI();

	protected:
		virtual void onCreate() {};
		virtual void onStart(ECSManager& ecs, const TagMap& tags)
		{
			onStart(ecs);
		}
		virtual void onStart(ECSManager& ecs) {}
		virtual void onUpdate(float delta, ECSManager& ecs) = 0;
		virtual void onRender(WeirdRenderer::RenderTarget& renderTarget) {};
		virtual void onImGuiRender() {};

		// Physics thread callbacks. Fire on the physics thread mid-step; no
		// ECS access here. Use them for simulation-coupled logic only (contact
		// tuning, immediate impulses). Everything else belongs in the main
		// thread callbacks below.
		virtual void onPhysicsStep(Simulation2D& simulation) {};
		virtual void onCollision(Simulation2D& simulation, WeirdEngine::CollisionEvent& event) {};
		virtual void onShapeCollision(Simulation2D& simulation, WeirdEngine::ShapeCollisionEvent& event) {};

		// Main thread callbacks (m_ecs is safe to use here; the physics
		// thread only ever touches Simulation2D internals, never the ECS)
		virtual void onEntityCollision(ECSManager& ecs, WeirdEngine::EntityCollisionEvent& event) {};
		virtual void onEntityShapeCollision(ECSManager& ecs, WeirdEngine::EntityShapeCollisionEvent& event) {};

		void setSceneComplete(std::string nextScene = "")
		{
			m_isSceneComplete = true;
			m_nextScene = nextScene;
		};

		Entity m_mainCamera;
		ResourceManager m_resourceManager;

		Material3D m_materials[16];
		uint16_t m_materialCount = 0;

		std::vector<std::shared_ptr<IMathExpression>> m_sdfs;

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

		void lookAt(Entity entity);

		// Entities in this set will be skipped during scene serialization
		std::unordered_set<Entity> m_serializationBlacklist;
		void blacklistEntity(Entity e)
		{
			m_serializationBlacklist.insert(e);
		}

		// Tag management
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

		SDFRenderSystemContext m_2DWorldRenderContext;
		SDFRenderSystemContext m_3DWorldRenderContext;
		SDFRenderSystemContext m_UIRenderContext;
		// Resolve a physics SimulationID to the owning entity.
		Entity getEntityForSimulationId(SimulationID simulationId,
										std::shared_ptr<ComponentArray<RigidBody2D>> rigidBodies);

		bool m_debugFly = false;
		bool m_debugInput = false;

		RenderMode m_renderMode = RenderMode::RayMarching2D;

		void playSound(const WeirdRenderer::SimpleAudioRequest& audio);

		// Save the current scene state to a .weird JSON file
		void saveScene(const std::string& filename);

		// Dynamically load a .weird file and add its contents to the scene.
		// If blacklistEntities is true, all entities created by the load will be
		// excluded from future scene serialization.
		// Returns a map of tag names to their corresponding entities.
		TagMap loadWeirdFile(const std::string& path, bool blacklistEntities = false);

		// Path to a .weird file to load when the scene starts (set via setSceneFilePath or registerScene)
		std::string m_sceneFilePath;

		BackgroundParams m_background;

	private:
		// Load scene state from a .weird JSON file
		void loadFromWeirdFile(const std::string& path);

		bool m_runSimulationInThread;

		AudioRingBuffer<WeirdRenderer::SimpleAudioRequest, SOUND_QUEUE_SIZE> m_audioQueue;
		float m_frictionSoundLevel{0.0f};
		std::atomic<float> m_frictionSoundLevelRead{0.0f};

		std::vector<WeirdRenderer::DrawCommand> m_drawQueue;
		std::vector<WeirdRenderer::Light> m_lights;

		static void handlePhysicsStep(void* userData);
		static void handleCollision(CollisionEvent& event, void* userData);
		static void handleShapeCollision(ShapeCollisionEvent& event, void* userData);

		std::string m_nextScene;
		bool m_isSceneComplete = false;

		ECSManager m_ecs;
		Simulation2D m_simulation2D;
		bool m_simulationIsPaused = false;

		std::mutex m_collisionQueueMutex;
		std::vector<CollisionEvent> m_queuedCollisions;
		std::vector<ShapeCollisionEvent> m_queuedShapeCollisions;

		// Entity tag storage (bidirectional maps kept in sync)
		TagMap m_tagToEntity;
		std::unordered_map<Entity, std::string> m_entityToTag;
	};
	class Scene2D : public Scene
	{
	public:
		Scene2D()
		{
			m_renderMode = RenderMode::RayMarching2D;
		}
	};

	class Scene3D : public Scene
	{
	public:
		Scene3D()
		{
			m_renderMode = RenderMode::RayMarching3D;
		}
	};

	class SceneBoth : public Scene
	{
	public:
		SceneBoth()
		{
			m_renderMode = RenderMode::RayMarchingBoth;
		}
	};
} // namespace WeirdEngine
