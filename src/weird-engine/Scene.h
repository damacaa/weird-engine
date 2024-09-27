#pragma once
#include "../weird-renderer/Shape.h"
#include "../weird-physics/Simulation.h"
#include "../weird-physics/Simulation2D.h"
#include "../weird-renderer/RenderPlane.h"



#include "ecs/ECS.h"

#include "ResourceManager.h"

using namespace ECS;
using namespace std;

class Scene
{
public:

	Scene(const char* filePath);
	~Scene();
	void renderModels(WeirdRenderer::Shader& shader, WeirdRenderer::Shader& instancingShader);
	void renderShapes(WeirdRenderer::Shader& shader, WeirdRenderer::RenderPlane& rp);
	void update(double delta, double time);

	Scene(const Scene&) = default; // Deleted copy constructor
	Scene& operator=(const Scene&) = default; // Deleted copy assignment operator
	Scene(Scene&&) = default; // Defaulted move constructor
	Scene& operator=(Scene&&) = default; // Defaulted move assignment operator

	WeirdRenderer::Camera& getCamera();


private:
	Entity m_mainCamera;

	void loadScene(std::string sceneFileContent);

	ECSManager m_ecs;
	ResourceManager m_resourceManager;

	Simulation m_simulation;
	Simulation2D m_simulation2D;
	bool m_runSimulationInThread;

	SDFRenderSystem m_sdfRenderSystem;
	SDFRenderSystem2D m_sdfRenderSystem2D;
	RenderSystem m_renderSystem;
	InstancedRenderSystem m_instancedRenderSystem;
	RBPhysicsSystem m_rbPhysicsSystem;
	PhysicsSystem2D m_rbPhysicsSystem2D;
	PhysicsInteractionSystem m_physicsInteractionSystem;
	PlayerMovementSystem m_playerMovementSystem;
	CameraSystem m_cameraSystem;

	ProceduralSpawnerSystem m_weirdSystem;

	vector<WeirdRenderer::Light> m_lights;


};

