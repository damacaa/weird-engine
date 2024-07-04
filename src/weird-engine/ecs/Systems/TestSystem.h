#pragma once
#include "../ECS.h"
#include "../../ResourceManager.h"
#include <random>
#include <filesystem>

namespace fs = std::filesystem;




class TestSystem : public System {
private:
	const size_t MESHES = 10;
	const size_t SHAPES = 10;

public:

	TestSystem(ECS& ecs, 
		ResourceManager& m_resourceManager, 
		InstancedRenderSystem& m_instancedRenderSystem,
		SDFRenderSystem& m_sdfRenderSystem,
		RBPhysicsSystem& m_rbPhysicsSystem)
	{
		std::string projectDir = fs::current_path().string()+"/SampleProject";

		std::string spherePath = "/Resources/Models/sphere.gltf";
		std::string cubePath = "/Resources/Models/cube.gltf";
		std::string demoPath = "/Resources/Models/Monkey/monkey.gltf";
		std::string planePath = "/Resources/Models/plane.gltf";

		// Create a random device to seed the generator
		std::random_device rdd;

		// Use the Mersenne Twister engine seeded with the random device
		std::mt19937 gen(rdd());

		// Define a uniform integer distribution from 1 to 100
		std::uniform_int_distribution<> diss(1, 100);
		

		// Spawn mesh balls
		for (size_t i = 0; i < MESHES; i++)
		{
			Entity entity = ecs.createEntity();


			float x = diss(gen) - 50;
			float y = diss(gen);
			float z = diss(gen) - 50;

			Transform t;
			t.position = 0.1f * vec3(x, y, z);
			ecs.addComponent(entity, t);


			ecs.addComponent(entity, InstancedMeshRenderer(m_resourceManager.getMeshId((projectDir +
				(i % 2 == 0 ? cubePath : spherePath)
				).c_str(), entity, true)));

			m_instancedRenderSystem.add(entity);


			ecs.addComponent(entity, RigidBody());
			m_rbPhysicsSystem.add(entity);
		}

		// Spawn shape balls
		for (size_t i = 0; i < SHAPES; i++)
		{
			float x = diss(gen) - 50;
			float y = diss(gen);
			float z = diss(gen) - 50;

			Transform t;
			t.position = 0.1f * vec3(x, y, z);


			Entity entity = ecs.createEntity();
			ecs.addComponent(entity, t);

			ecs.addComponent(entity, SDFRenderer());
			m_sdfRenderSystem.add(entity);

			ecs.addComponent(entity, RigidBody());
			m_rbPhysicsSystem.add(entity);
		}

	}
};