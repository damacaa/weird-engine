#pragma once

#include "ecs/ECS.h"
#include "ResourceManager.h"

#include "weird-engine/systems/SDFRenderSystem2D.h"

#include "weird-renderer/audio/AudioRingBuffer.h"
#include "weird-renderer/audio/SimpleAudioRequest.h"
#include "weird-renderer/core/RenderTarget.h"
#include "weird-renderer/resources/DrawCommand.h"

#include "weird-physics/PhysicsSettings.h"
#include "weird-physics/Simulation2D.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace WeirdEngine
{
	using namespace ECS;

	constexpr int SOUND_QUEUE_SIZE = 16;

	// Forward declaration – full definition in SceneSerializer.h
	class SceneSerializer;

	class Scene
	{
		friend class SceneSerializer;

	public:
		/// Map from tag name (std::string) to the entity that owns it.
		using TagMap = std::unordered_map<std::string, Entity>;

		Scene(const PhysicsSettings& settings);
		virtual ~Scene();
		void start();

		void renderModels(WeirdRenderer::RenderTarget& renderTarget, WeirdRenderer::Shader& shader,
						  WeirdRenderer::Shader& instancingShader);

		void update2DWorldShader(WeirdRenderer::Shader& shader);
		void updateUIShader(WeirdRenderer::Shader& shader);
		void forceShaderRefresh();
		void update(double delta, double time);

		void get2DShapesData(vec4*& data, uint32_t& size, uint32_t& customShapeCount);
		void getUIData(vec4*& uiData, uint32_t& size, uint32_t& customShapeCount);

		WeirdRenderer::Camera& getCamera();
		std::vector<WeirdRenderer::Light>& getLigths();

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

		ShapeId registerSDF(std::shared_ptr<IMathExpression> sdf);

		// Set the path to a .weird file to load when the scene starts
		void setSceneFilePath(const std::string& path)
		{
			m_sceneFilePath = path;
		}

	protected:
		virtual void onCreate() {};
		// Override onStart(tags) to receive the map of tags→entities when the
		// scene is loaded from a .weird file.  Override onStart() (no parameter)
		// for scenes that don't need the tag map.  The default implementation of
		// the tags overload simply calls onStart(), so you only need one.
		virtual void onStart(const TagMap& tags)
		{
			onStart();
		}
		virtual void onStart() {}
		virtual void onUpdate(float delta) = 0;
		virtual void onRender(WeirdRenderer::RenderTarget& renderTarget) {};
		virtual void onPhysicsStep() {};
		virtual void onCollision(WeirdEngine::CollisionEvent& event) {};
		virtual void onShapeCollision(WeirdEngine::ShapeCollisionEvent& event) {};
		virtual void onDestroy() {};

		void setSceneComplete(std::string nextScene = "")
		{
			m_isSceneComplete = true;
			m_nextScene = nextScene;
		};

		ECSManager m_ecs;
		Entity m_mainCamera;
		ResourceManager m_resourceManager;
		Simulation2D m_simulation2D;

		std::vector<std::shared_ptr<IMathExpression>> m_sdfs;

		Entity addShape(ShapeId shapeId, float* variables, uint16_t material,
						CombinationType combination = CombinationType::Addition, bool hasCollision = true,
						int group = 0);
		Entity addUIShape(ShapeId shapeId, float* variables, uint16_t material,
						  CombinationType combination = CombinationType::Addition, int group = 0);
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

		SDFRenderSystem2DContext m_2DWorldRenderContext;
		SDFRenderSystem2DContext m_UIRenderContext;

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

		// Entity tag storage (bidirectional maps kept in sync)
		TagMap m_tagToEntity;
		std::unordered_map<Entity, std::string> m_entityToTag;
	};
} // namespace WeirdEngine
