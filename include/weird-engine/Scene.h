#pragma once
#include "../weird-renderer/Shape.h"
#include "../weird-physics/Simulation.h"
#include "../weird-physics/Simulation2D.h"
#include "../weird-renderer/RenderTarget.h"
#include "../weird-renderer/Shape.h"

#include "ecs/ECS.h"

#include "ResourceManager.h"

namespace WeirdEngine
{
	using namespace ECS;

	class Scene
	{
	public:
		Scene();
		~Scene();
		void start();

		void renderModels(WeirdRenderer::RenderTarget& renderTarget, WeirdRenderer::Shader& shader, WeirdRenderer::Shader& instancingShader);

		void updateRayMarchingShader(WeirdRenderer::Shader& shader);
		void update(double delta, double time);

		void get2DShapesData(WeirdRenderer::Dot2D*& data, uint32_t& size);

		Scene(const Scene&) = default;						 // Deleted copy constructor
		Scene& operator=(const Scene&) = default; // Deleted copy assignment operator
		Scene(Scene&&) = default;								 // Defaulted move constructor
		Scene& operator=(Scene&&) = default;			 // Defaulted move assignment operator

		WeirdRenderer::Camera& getCamera();
		std::vector<WeirdRenderer::Light>& getLigths();

		float getTime();

		void fillShapeDataBuffer(WeirdRenderer::Dot2D*& data, uint32_t& size);

		enum class RenderMode
		{
			Simple3D,
			RayMarching3D,
			RayMarching2D,
			RayMarchingBoth
		};

		RenderMode getRenderMode() const;

	protected:
		virtual void onCreate() {};
		virtual void onStart() = 0;
		virtual void onUpdate(float delta) = 0;
		virtual void onRender(WeirdRenderer::RenderTarget& renderTarget) {};
		virtual void onCollision(WeirdEngine::CollisionEvent& event) {};
		virtual void onDestroy() {};

		ECSManager m_ecs;
		Entity m_mainCamera;
		ResourceManager m_resourceManager;
		Simulation2D m_simulation2D;

		std::vector<std::shared_ptr<IMathExpression>> m_sdfs;

		Entity addShape(int shapeId, float* variables);
		Entity addScreenSpaceShape(int shapeId, float* variables);

		void lookAt(Entity entity);

		SDFRenderSystem m_sdfRenderSystem;
		SDFRenderSystem2D m_sdfRenderSystem2D;
		RenderSystem m_renderSystem;
		InstancedRenderSystem m_instancedRenderSystem;
		PhysicsSystem2D m_rbPhysicsSystem2D;
		PlayerMovementSystem m_playerMovementSystem;
		CameraSystem m_cameraSystem;

		PhysicsInteractionSystem m_physicsInteractionSystem;

		bool m_debugFly = true;
		bool m_debugInput = false;

		RenderMode m_renderMode = RenderMode::RayMarching2D;

		int m_charWidth;
		int m_charHeight;

		std::vector<std::vector<vec2>> m_letters;

		void print(const std::string& text);
		void loadFont(const char* imagePath, int charWidth, int charHeight, const char* characters);

	private:
		// Char lookup table
		std::array<int, 256> m_CharLookUpTable{};

		// Lookup function
		int getIndex(char c)
		{
			return m_CharLookUpTable[static_cast<unsigned char>(c)];
		}

		void loadScene(std::string& sceneFileContent);

		bool m_runSimulationInThread;

		void updateCustomShapesShader(WeirdRenderer::Shader& shader);

		std::vector<WeirdRenderer::Light> m_lights;

		static void handleCollision(CollisionEvent& event, void* userData);
	};
}