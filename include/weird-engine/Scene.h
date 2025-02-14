#pragma once
#pragma once
#include "../weird-renderer/Shape.h"
#include "../weird-physics/Simulation.h"
#include "../weird-physics/Simulation2D.h"
#include "../weird-renderer/RenderPlane.h"
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

		void renderModels(WeirdRenderer::Shader& shader, WeirdRenderer::Shader& instancingShader);
		void updateCustomShapesShader(WeirdRenderer::Shader& shader);
		void renderShapes(WeirdRenderer::Shader& shader, WeirdRenderer::RenderPlane& rp);
		void update(double delta, double time);

		Scene(const Scene&) = default;			   // Deleted copy constructor
		Scene& operator=(const Scene&) = default; // Deleted copy assignment operator
		Scene(Scene&&) = default;				   // Defaulted move constructor
		Scene& operator=(Scene&&) = default;	   // Defaulted move assignment operator

		WeirdRenderer::Camera& getCamera();
		float getTime();


	protected:
		virtual void onStart() = 0;
		virtual void onUpdate() = 0;
		virtual void onRender() = 0;

		ECSManager m_ecs;
		Entity m_mainCamera;
		ResourceManager m_resourceManager;
		Simulation2D m_simulation2D;

		std::vector<std::shared_ptr<IMathExpression>> m_sdfs;

		Entity addShape(int shapeId, float* variables);

		void lookAt(Entity entity);

		SDFRenderSystem m_sdfRenderSystem;
		SDFRenderSystem2D m_sdfRenderSystem2D;
		RenderSystem m_renderSystem;
		InstancedRenderSystem m_instancedRenderSystem;
		PhysicsSystem2D m_rbPhysicsSystem2D;
		PhysicsInteractionSystem m_physicsInteractionSystem;
		PlayerMovementSystem m_playerMovementSystem;
		CameraSystem m_cameraSystem;

		bool m_debugFly = true;

	private:

		void loadScene(std::string& sceneFileContent);

		bool m_runSimulationInThread;




		std::vector<WeirdRenderer::Light> m_lights;
	};
}