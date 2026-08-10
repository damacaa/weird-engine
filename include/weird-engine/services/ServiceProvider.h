#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "weird-engine/Background.h"
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/Material3D.h"
#include "weird-engine/ResourceManager.h"
#include "weird-engine/systems/SDFRenderSystem.h"
#include "weird-engine/vec.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/Simulation2D.h"
#include "weird-renderer/audio/AudioRingBuffer.h"
#include "weird-renderer/audio/SimpleAudioRequest.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/components/CustomShape.h"
#include "weird-renderer/scene/Light.h"

namespace WeirdEngine
{
	class Scene;

	constexpr int SOUND_QUEUE_SIZE = 16;

	/// Map from tag name (std::string) to the entity that owns it.
	using TagMap = std::unordered_map<std::string, Entity>;

	struct RaymarchResult
	{
		float distance;
		Entity entity;
	};

	enum class RenderMode
	{
		RayMarching3D,
		RayMarching2D,
		RayMarchingBoth
	};

	// Shared raymarch implementation used by both Scene::raymarch and
	// PhysicsService::raymarch. Defined in Scene.cpp.
	RaymarchResult raymarchScene(ECSManager& ecs, std::vector<std::shared_ptr<IMathExpression>>& sdfs,
								 Simulation2D& simulation, float time, glm::vec2 origin, glm::vec2 direction,
								 float epsilon, float maxDistance);

	struct TimeService
	{
		Simulation2D& simulation;
		const float& delta;

		float time() const
		{
			return static_cast<float>(simulation.getSimulationTime());
		}

		float deltaTime() const
		{
			return delta;
		}

		double fixedDeltaTime() const
		{
			return simulation.getDeltaTime();
		}
	};

	struct PhysicsService
	{
		ECSManager& ecs;
		Simulation2D& simulation;
		std::vector<std::shared_ptr<IMathExpression>>& sdfs;

		Simulation2D& sim()
		{
			return simulation;
		}

		void setGravity(float gravity)
		{
			simulation.setGravity(gravity);
		}

		void setDamping(float damping)
		{
			simulation.setDamping(damping);
		}

		void pause()
		{
			simulation.pause();
		}

		void resume()
		{
			simulation.resume();
		}

		bool isPaused() const
		{
			return simulation.isPaused();
		}

		Entity entityForSimulationId(SimulationID simulationId) const
		{
			auto rigidBodies = ecs.getComponentArray<RigidBody2D>();
			if (simulationId >= static_cast<SimulationID>(rigidBodies->getSize()))
				return INVALID_ENTITY;

			return rigidBodies->getEntityAtIdx(static_cast<size_t>(simulationId));
		}

		// Per-body user data. Set the data right after adding the RigidBody2D
		// component (read rb.simulationId from it). The pointer must be
		// heap-allocated: the simulation owns it and deletes it when the body
		// is removed or when the simulation is destroyed.
		void setUserData(SimulationID id, BodyUserData* data)
		{
			simulation.setUserData(id, data);
		}

		BodyUserData* getUserData(SimulationID id)
		{
			return simulation.getUserData(id);
		}

		template <typename T> T* getUserDataAs(SimulationID id)
		{
			return simulation.getUserDataAs<T>(id);
		}

		template <typename Fn> void forEachUserData(Fn&& fn)
		{
			simulation.forEachUserData(std::forward<Fn>(fn));
		}

		RaymarchResult raymarch(glm::vec2 origin, glm::vec2 direction, float epsilon = 0.001f,
								float maxDistance = 150.0f)
		{
			return raymarchScene(ecs, sdfs, simulation, static_cast<float>(simulation.getSimulationTime()), origin,
								 direction, epsilon, maxDistance);
		}
	};

	struct ShapeService
	{
		ECSManager& ecs;
		Simulation2D& simulation;
		std::vector<std::shared_ptr<IMathExpression>>& sdfs;

		static ShapeId registerDefaultSDF(std::shared_ptr<IMathExpression> sdf);

		ShapeId registerSDF(std::shared_ptr<IMathExpression> sdf)
		{
			sdfs.push_back(std::move(sdf));
			simulation.setSDFs(sdfs);

			return static_cast<ShapeId>(sdfs.size() - 1);
		}

		const std::vector<std::shared_ptr<IMathExpression>>& getSDFs() const
		{
			return sdfs;
		}

		Entity addShape(ShapeId shapeId, float* variables, uint16_t material,
						CombinationType combination = CombinationType::Addition, bool hasCollision = true,
						int group = 0)
		{
			Entity entity = ecs.createEntity();
			CustomShape& shape = ecs.addComponent<CustomShape>(entity);
			shape.distanceFieldId = shapeId;
			shape.combination = combination;
			shape.hasCollisions = hasCollision;
			shape.groupIdx = group;
			shape.material = material;
			std::copy(variables, variables + 8, shape.parameters);

			return entity;
		}

		Entity addShape(ShapeId shapeId, float* variables, const Material3D& material,
						CombinationType combination = CombinationType::Addition, bool hasCollision = true,
						int group = 0)
		{
			return addShape(shapeId, variables, material.id, combination, hasCollision, group);
		}

		Entity addUIShape(ShapeId shapeId, float* variables, uint16_t material,
						  CombinationType combination = CombinationType::Addition, int group = 0)
		{
			Entity entity = ecs.createEntity();
			UIShape& shape = ecs.addComponent<UIShape>(entity);
			shape.distanceFieldId = shapeId;
			shape.combination = combination;
			shape.groupIdx = group;
			shape.material = material;
			std::copy(variables, variables + 8, shape.parameters);

			return entity;
		}

		Entity addUIShape(ShapeId shapeId, float* variables, const Material3D& material,
						  CombinationType combination = CombinationType::Addition, int group = 0)
		{
			return addUIShape(shapeId, variables, material.id, combination, group);
		}

		UIShape& addUIShape(ShapeId shapeId, float* variables, Entity& entity, int group = 0)
		{
			entity = ecs.createEntity();
			UIShape& component = ecs.addComponent<UIShape>(entity);
			component.distanceFieldId = shapeId;
			component.groupIdx = group;
			component.smoothFactor = 100.0f;
			std::copy(variables, variables + 8, component.parameters);

			return component;
		}
	};

	struct RenderService
	{
		ECSManager& ecs;
		Entity& cameraEntity;
		SDFRenderSystemContext& context2D;
		SDFRenderSystemContext& context3D;
		SDFRenderSystemContext& contextUI;
		std::vector<WeirdRenderer::Light>& lights;
		BackgroundParams& background;
		RenderMode& renderMode;

		WeirdRenderer::Camera& camera()
		{
			return ecs.getComponent<ECS::Camera>(cameraEntity).camera;
		}

		Entity getCameraEntity() const
		{
			return cameraEntity;
		}

		std::vector<WeirdRenderer::Light>& getLights()
		{
			return lights;
		}

		BackgroundParams& getBackground()
		{
			return background;
		}

		const BackgroundParams& getBackground() const
		{
			return background;
		}

		RenderMode getRenderMode() const
		{
			return renderMode;
		}

		SDFRenderSystemContext& getContext2D()
		{
			return context2D;
		}

		SDFRenderSystemContext& getContext3D()
		{
			return context3D;
		}

		SDFRenderSystemContext& getContextUI()
		{
			return contextUI;
		}

		// Force the shader to regenerate the next frame. Materials are baked
		// into the shader, so changing a shape's material (or any parameter
		// that affects shader generation) requires a refresh. Use the
		// granular variants to refresh only the affected render target.
		void forceShaderRefresh2D()
		{
			context2D.shapesNeedUpdate = true;
		}

		void forceShaderRefresh3D()
		{
			context3D.shapesNeedUpdate = true;
		}

		void forceShaderRefreshUI()
		{
			contextUI.shapesNeedUpdate = true;
		}

		void forceShaderRefresh()
		{
			forceShaderRefresh2D();
			forceShaderRefresh3D();
			forceShaderRefreshUI();
		}
	};

	struct MaterialService
	{
		Material3D (&materials)[16];
		uint16_t& count;

		Material3D& createMaterial()
		{
			if (count >= 16)
			{
				// Max materials reached, return the last one
				return materials[15];
			}

			Material3D& mat = materials[count];
			mat.id = count;
			count++;

			return mat;
		}

		Material3D& getMaterial(int index)
		{
			return materials[index];
		}

		const Material3D* getMaterials() const
		{
			return materials;
		}

		uint16_t getMaterialCount() const
		{
			return count;
		}
	};

	struct AudioService
	{
		AudioRingBuffer<WeirdRenderer::SimpleAudioRequest, SOUND_QUEUE_SIZE>& queue;
		const std::atomic<float>& frictionSoundLevel;

		void playSound(const WeirdRenderer::SimpleAudioRequest& audio)
		{
			queue.push(audio);
		}

		float getFrictionSound() const
		{
			return frictionSoundLevel.load(std::memory_order_acquire);
		}

		AudioRingBuffer<WeirdRenderer::SimpleAudioRequest, SOUND_QUEUE_SIZE>& audioQueue()
		{
			return queue;
		}
	};

	struct TagService
	{
		TagMap& tagToEntity;
		std::unordered_map<Entity, std::string>& entityToTag;

		// Assign a unique tag to an entity. If the tag is already owned by
		// another entity, it is moved to this one. An empty name is treated
		// as a removal request (equivalent to calling removeTag).
		void tag(Entity entity, const std::string& name)
		{
			if (name.empty())
			{
				removeTag(entity);
				return;
			}

			// If the tag is already owned by another entity, remove it from that entity
			auto existingOwner = tagToEntity.find(name);
			if (existingOwner != tagToEntity.end() && existingOwner->second != entity)
			{
				entityToTag.erase(existingOwner->second);
			}

			// Remove any previous tag this entity had
			auto existingTag = entityToTag.find(entity);
			if (existingTag != entityToTag.end() && existingTag->second != name)
			{
				tagToEntity.erase(existingTag->second);
			}

			tagToEntity[name] = entity;
			entityToTag[entity] = name;
		}

		void removeTag(Entity entity)
		{
			auto it = entityToTag.find(entity);
			if (it == entityToTag.end())
				return;
			tagToEntity.erase(it->second);
			entityToTag.erase(it);
		}

		std::string getEntityTag(Entity entity) const
		{
			auto it = entityToTag.find(entity);
			if (it == entityToTag.end())
				return "";
			return it->second;
		}

		Entity getEntityByTag(const std::string& name) const
		{
			auto it = tagToEntity.find(name);
			if (it == tagToEntity.end())
				return MAX_ENTITIES;
			return it->second;
		}
	};

	struct SerializationService
	{
		Scene& scene;
		std::unordered_set<Entity>& blacklist;
		std::string& sceneFilePath;

		// Save the current scene state to a .weird JSON file
		void saveScene(const std::string& filename);

		// Dynamically load a .weird file and add its contents to the scene.
		// If blacklistEntities is true, all entities created by the load will be
		// excluded from future scene serialization.
		// Returns a map of tag names to their corresponding entities.
		TagMap loadWeirdFile(const std::string& path, bool blacklistEntities = false);

		void blacklistEntity(Entity entity)
		{
			blacklist.insert(entity);
		}

		// Set the path to a .weird file to load when the scene starts
		void setSceneFilePath(const std::string& path)
		{
			sceneFilePath = path;
		}
	};

	struct SceneControlService
	{
		bool& isComplete;
		std::string& nextScene;

		void goToNextScene(std::string next = "")
		{
			isComplete = true;
			nextScene = std::move(next);
		}
	};

	struct ResourceService
	{
		ResourceManager& resourceManager;

		ResourceManager& resources()
		{
			return resourceManager;
		}
	};

	struct DebugService
	{
		bool& fly;
		bool& input;

		bool debugFly() const
		{
			return fly;
		}

		void setDebugFly(bool value)
		{
			fly = value;
		}

		bool debugInput() const
		{
			return input;
		}

		void setDebugInput(bool value)
		{
			input = value;
		}
	};

	// Central access point for all non-ECS scene functionality. Systems take
	// a ServiceProvider& (plus the ECSManager&) and use it instead of reaching
	// into Scene internals. Owned by Scene, which binds it to its storage.
	class ServiceProvider
	{
	public:
		explicit ServiceProvider(Scene& scene);

		ECSManager& ecs()
		{
			return m_ecs;
		}

		TimeService& time()
		{
			return m_time;
		}

		PhysicsService& physics()
		{
			return m_physics;
		}

		ShapeService& shapes()
		{
			return m_shapes;
		}

		RenderService& render()
		{
			return m_render;
		}

		MaterialService& materials()
		{
			return m_materials;
		}

		AudioService& audio()
		{
			return m_audio;
		}

		TagService& tags()
		{
			return m_tags;
		}

		SerializationService& serialization()
		{
			return m_serialization;
		}

		SceneControlService& sceneControl()
		{
			return m_sceneControl;
		}

		ResourceService& resources()
		{
			return m_resources;
		}

		DebugService& debug()
		{
			return m_debug;
		}

	private:
		ECSManager& m_ecs;
		TimeService m_time;
		PhysicsService m_physics;
		ShapeService m_shapes;
		RenderService m_render;
		MaterialService m_materials;
		AudioService m_audio;
		TagService m_tags;
		SerializationService m_serialization;
		SceneControlService m_sceneControl;
		ResourceService m_resources;
		DebugService m_debug;
	};
} // namespace WeirdEngine
