#pragma once
#include "../weird-renderer/Shape.h"
#include "../weird-renderer/Model.h"
#include "../weird-physics/Simulation.h"
#include "../weird-renderer/RenderPlane.h"



#include "ecs/ECS.h"

#include "ResourceManager.h"

using namespace std;
class Scene
{
public:
	Scene();
	~Scene();
	void RenderModels(Shader& shader) ;
	void RenderShapes(Shader& shader, RenderPlane* rp) const;
	void Update(double delta, double time);

	ResourceManager m_resourceManager;


	size_t m_size = 10;
	Shape* m_data;
	vector<Model*> m_models;


	Camera* m_camera;
	vec3 m_lightPosition;


private:

	ECS m_ecs;
	std::vector<Entity> m_entities;

	double m_simulationDelay = 0.0;
	Simulation* m_simulation;

	vector<Light> m_lights;

	std::shared_ptr<MovementSystem> movementSystem;
	std::shared_ptr<RenderSystem> m_renderSystem;
	std::shared_ptr<InstancedRenderSystem> m_instancedRenderSystem;

	void LoadScene();
};

