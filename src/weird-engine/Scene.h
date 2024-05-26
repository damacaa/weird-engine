#pragma once
#include "../weird-renderer/Shape.h"
#include "../weird-renderer/Model.h"
#include "../weird-physics/Simulation.h"
#include "../weird-renderer/RenderPlane.h"


using namespace std;
class Scene
{
public:
	Scene();
	~Scene();
	void RenderModels(Shader& shader) const;
	void RenderShapes(Shader& shader, RenderPlane* rp) const;
	void Update(double delta, double time);

	size_t m_size = 10;
	Shape* m_data;
	vector<Model*> m_models;


	Camera* m_camera;
	vec3 m_lightPosition;
private:
	double m_simulationDelay = 0.0;
	Simulation* m_simulation;

	vector<Light> m_lights;

	void LoadScene();
};

