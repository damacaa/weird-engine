#pragma once

#include <weird-engine.h>
#include <weird-engine/math/Default2DSDFs.h>

#include <weird-engine/math/Default3DSDFs.h>

using namespace WeirdEngine;

struct PlayerPhysics : public Component
{
	float speed = 60.0f;
	float damping = 3.0f;
	vec3 velocity = vec3(0.0f);
};

// Cornell Box
class MicroWorld : public Scene
{
public:
	MicroWorld(const PhysicsSettings& settings)
		: Scene(settings) {};

private:

    Entity m_target;
	Entity m_player;
    Entity m_floor;

	// Inherited via Scene
	void onStart() override
	{
		m_renderMode = RenderMode::RayMarching3D;
		// m_debugFly = true;



		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(0.0f, 3.0f, 0.0f);

            m_target = entity;
		}
		
		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(0.0f, 3.0f, 0.0f);

			auto& mat = createMaterial();
			mat.color = vec4(1.0f);
			mat.metallic = 1.0f;
			mat.roughness = 0.0f;

			auto& sdf = m_ecs.addComponent<Dot>(entity);
			sdf.materialId = mat.id;

			auto& physics = m_ecs.addComponent<PlayerPhysics>(entity);

            m_player = entity;
		}

		{
			auto& floorMaterial = createMaterial();
			floorMaterial.color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
			floorMaterial.metallic = 0.0f;
			floorMaterial.roughness = 0.3f;
			floorMaterial.pattern = MaterialPattern::Waves;
			floorMaterial.patternScale = 0.1f;
			floorMaterial.secondaryColor = floorMaterial.color * 0.8f;

			float vars[8] = {0, 3, 0.1};
			Entity floor = addShape(DefaultShapes3D::PERLIN_PLANE, vars, floorMaterial, CombinationType::Addition, false);
			
		}

		{
			auto& floorMaterial = createMaterial();
			floorMaterial.color = vec4(0.9f, 1.0f, 1.0f, 1.0f);
			floorMaterial.metallic = 0.5f;
			floorMaterial.roughness = 0.3f;

			float vars[8] = {0.0f};
			Entity floor = addShape(DefaultShapes3D::PLANE, vars, floorMaterial, CombinationType::Addition, false);
			
		}

        {
			// float vars[8] = {0,0,0, 5, 50, 5};
			// Entity floor = addShape(DefaultShapes3D::BOX, vars, 0, CombinationType::Intersection, false);
			float vars[8] = {0,0,0, 7.5f, 50, 5};
			Entity floor = addShape(DefaultShapes3D::CYLINDER, vars, 0, CombinationType::Intersection, false);
            m_floor = floor;
		}

		{
			auto& floorMaterial = createMaterial();
			floorMaterial.color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
			floorMaterial.metallic = 0.0f;
			floorMaterial.roughness = 1.0f;
			
			float vars[8] = {3.0f};
			Entity floor = addShape(DefaultShapes3D::PLANE, vars, floorMaterial, CombinationType::Addition, false);
			
		}
        
		getLigths().push_back(
			Light{0, glm::vec3(0.0f, 0.0f, 0.0f), 0, normalize(glm::vec3(0.0f, 1.2f, 1.0f)), glm::vec4(1.0f, 1.0f, 1.0f, 0.5f)});

		// getLigths().push_back(
		// 	Light{1, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.35f, 0.45f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)});

		// getLigths().push_back(
		// 	Light{2, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 2.0f, 10.0f)});

		auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
		cameraTransform.position = vec3(12, -1, 12);
		cameraTransform.rotation.x = -0.95f;
	}

	void onUpdate(float delta) override
	{
		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}

        auto& targetTransform = m_ecs.getComponent<Transform>(m_target);
		vec3 playerInput = vec3(0.0f);

        if (Input::GetKey(Input::W))
        {
			playerInput.z = -1.0f;
		}
        else if (Input::GetKey(Input::S))
        {
            playerInput.z = 1.0f;
        }
        
        if (Input::GetKey(Input::A))
        {
            playerInput.x = -1.0f;
        }
        else if (Input::GetKey(Input::D))
        {
            playerInput.x = 1.0f;
        }

		if(playerInput != vec3(0.0f))
		{
			playerInput = glm::normalize(playerInput);
		}
		
		float angle = -45.0f;
		float angleRad = glm::radians(angle);
		playerInput = vec3(sin(angleRad) * -playerInput.x + cos(angleRad) * playerInput.z, 0.0f, cos(angleRad) * -playerInput.x - sin(angleRad) * playerInput.z);

		targetTransform.position += playerInput * 5.0f * delta;

		auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);

		const float CAMERA_DISTANCE = 15.0f;

        cameraTransform.position.x = targetTransform.position.x + CAMERA_DISTANCE;
        cameraTransform.position.y = targetTransform.position.y + CAMERA_DISTANCE;
        cameraTransform.position.z = targetTransform.position.z + CAMERA_DISTANCE;

        cameraTransform.rotation.x = -0.58f;
        cameraTransform.rotation.y = -0.58f;
        cameraTransform.rotation.z = -0.58f;
        

        auto& floorShape = m_ecs.getComponent<CustomShape>(m_floor);
        floorShape.parameters[0] = targetTransform.position.x;
        floorShape.parameters[1] = targetTransform.position.y;
        floorShape.parameters[2] = targetTransform.position.z;

        floorShape.isDirty = true;


		auto& playerTransform = m_ecs.getComponent<Transform>(m_player);
		auto& playerPhysics = m_ecs.getComponent<PlayerPhysics>(m_player);
		playerPhysics.velocity += (targetTransform.position - playerTransform.position) * playerPhysics.speed * delta;
		playerPhysics.velocity -= playerPhysics.velocity * playerPhysics.damping * delta; // damping

		playerTransform.position += playerPhysics.velocity * delta;


		// getLigths()[0].position.x = cameraTransform.position.x;
		// getLigths()[0].position.y = cameraTransform.position.y;
		// getLigths()[0].position.z = cameraTransform.position.z;

		// getLigths()[0].rotation.x = -cameraTransform.rotation.x;
		// getLigths()[0].rotation.y = -cameraTransform.rotation.y;
		// getLigths()[0].rotation.z = -cameraTransform.rotation.z;
	}
};
