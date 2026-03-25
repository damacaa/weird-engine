#pragma once

#include "ecs/ECS.h"
#include "ResourceManager.h"
#include "weird-engine/math/Default2DSDFs.h"

#include "weird-renderer/core/RenderTarget.h"
#include "weird-renderer/resources/DrawCommand.h"
#include "weird-renderer/scene/Shape.h"

#include "weird-physics/Simulation2D.h"
#include "weird-renderer/audio/AudioRingBuffer.h"
#include "weird-renderer/audio/SimpleAudioRequest.h"

#include "weird-physics/PhysicsSettings.h"

#include <string>
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
		Scene(const PhysicsSettings& settings);
		~Scene();
		void start();

		void renderModels(WeirdRenderer::RenderTarget& renderTarget, WeirdRenderer::Shader& shader,
						  WeirdRenderer::Shader& instancingShader);

		void updateRayMarchingShader(WeirdRenderer::Shader& shader);
		void updateUIShader(WeirdRenderer::Shader& shader);
		void forceShaderRefresh();
		void update(double delta, double time);

		void get2DShapesData(WeirdRenderer::Dot2D*& data, uint32_t& size, uint32_t& customShapeCount);
		void getUIData(WeirdRenderer::Dot2D*& uiData, uint32_t& size, uint32_t& customShapeCount);

		Scene(const Scene&) = default;			  // Deleted copy constructor
		Scene& operator=(const Scene&) = default; // Deleted copy assignment operator
		Scene(Scene&&) = default;				  // Defaulted move constructor
		Scene& operator=(Scene&&) = default;	  // Defaulted move assignment operator

		WeirdRenderer::Camera& getCamera();
		std::vector<WeirdRenderer::Light>& getLigths();

		float getTime();

		void fillShapeDataBuffer(WeirdRenderer::Dot2D*& data, uint32_t& size);

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
		void setSceneFilePath(const std::string& path) { m_sceneFilePath = path; }

	protected:
		virtual void onCreate() {};
		virtual void onStart() = 0;
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
		void blacklistEntity(Entity e) { m_serializationBlacklist.insert(e); }

		SDFRenderSystem m_sdfRenderSystem;
		SDFRenderSystem2D<Dot, CustomShape, TextRenderer> m_sdfRenderSystem2D;
		SDFRenderSystem2D<UIDot, UIShape, UITextRenderer> m_UIRenderSystem;
		RenderSystem m_renderSystem;
		InstancedRenderSystem m_instancedRenderSystem;
		PhysicsSystem2D m_rbPhysicsSystem2D;
		PlayerMovementSystem m_playerMovementSystem;
		CameraSystem m_cameraSystem;

		PhysicsInteractionSystem m_physicsInteractionSystem;

		bool m_debugFly = false;
		bool m_debugInput = false;

		RenderMode m_renderMode = RenderMode::RayMarching2D;

		void playSound(const WeirdRenderer::SimpleAudioRequest& audio);

		// Save the current scene state to a .weird JSON file
		void saveScene(const std::string& filename);

		// Path to a .weird file to load when the scene starts (set via setSceneFilePath or registerScene)
		std::string m_sceneFilePath;

	private:
		void loadScene(std::string& sceneFileContent);

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
	};
} // namespace WeirdEngine
