#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "weird-audio/AudioRingBuffer.h"
#include "weird-audio/SimpleAudioRequest.h"
#include "weird-engine/Assert.h"
#include "weird-engine/Background.h"
#include "weird-engine/ecs/Registry.h"
#include "weird-engine/Input.h"
#include "weird-engine/Material2D.h"
#include "weird-engine/Material3D.h"
#include "weird-engine/math/SDF.h"
#include "weird-engine/ResourceManager.h"
#include "weird-engine/systems/SDFRenderSystem.h"
#include "weird-engine/Utils.h"
#include "weird-engine/vec.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/Simulation2D.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/components/CustomShape.h"
#include "weird-renderer/core/Display.h"
#include "weird-renderer/scene/Light.h"

namespace WeirdEngine
{
	class Scene;

	namespace WeirdAudio
	{
		class AudioEngine;
		class PhysicsAudioEngine;
		class SdfMusicEngine;
		class SdfSong;
	} // namespace WeirdAudio

	constexpr int SOUND_QUEUE_SIZE = 64;

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
	RaymarchResult raymarchScene(Registry& registry, std::vector<std::shared_ptr<IMathExpression>>& sdfs,
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
		Registry& registry;
		Simulation2D& simulation;
		std::vector<std::shared_ptr<IMathExpression>>& sdfs;

		// Simulation2D& sim()
		// {
		// 	return simulation;
		// }

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
			auto rigidBodies = registry.getComponentArray<RigidBody2D>();
			if (simulationId >= static_cast<SimulationID>(rigidBodies->getSize()))
				return INVALID_ENTITY;

			return rigidBodies->getEntityAtIdx(static_cast<size_t>(simulationId));
		}

		// Per-body user data. Set the data right after adding the RigidBody2D
		// component (read rb.simulationId from it). Ownership is transferred to
		// the simulation (e.g. std::make_unique<CharacterData>()): it deletes
		// the data when the body is removed or when the simulation is
		// destroyed. Do not retain the pointer after the call; query it back
		// through getUserData()/getUserDataAs<T>().
		void setUserData(SimulationID id, std::unique_ptr<BodyUserData> data)
		{
			simulation.setUserData(id, std::move(data));
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
			return raymarchScene(registry, sdfs, simulation, static_cast<float>(simulation.getSimulationTime()), origin,
								 direction, epsilon, maxDistance);
		}
	};

	struct ShapeParamValue
	{
		size_t index = 0;
		float value = 0.0f;
	};

	struct ShapeVariables
	{
		float data[8]{};

		constexpr ShapeVariables() = default;

		constexpr ShapeVariables(std::initializer_list<float> list)
		{
			size_t i = 0;
			for (float v : list)
			{
				if (i >= 8)
					break;
				data[i++] = v;
			}
		}

		constexpr ShapeVariables(std::initializer_list<ShapeParamValue> indexedList)
		{
			for (const auto& pv : indexedList)
			{
				if (pv.index < 8)
				{
					data[pv.index] = pv.value;
				}
			}
		}

		template <size_t N> constexpr ShapeVariables(const float (&arr)[N])
		{
			const size_t count = std::min<size_t>(N, 8);
			for (size_t i = 0; i < count; ++i)
				data[i] = arr[i];
		}

		constexpr ShapeVariables(std::span<const float> s)
		{
			const size_t count = std::min<size_t>(s.size(), 8);
			for (size_t i = 0; i < count; ++i)
				data[i] = s[i];
		}

		constexpr ShapeVariables(const float* ptr, size_t n = 8)
		{
			const size_t count = std::min<size_t>(n, 8);
			for (size_t i = 0; i < count; ++i)
				data[i] = ptr[i];
		}
	};

	struct ShapeMaterial
	{
		uint16_t id = 0;

		constexpr ShapeMaterial() = default;
		constexpr ShapeMaterial(uint16_t matId)
			: id(matId)
		{
		}
		constexpr ShapeMaterial(int matId)
			: id(static_cast<uint16_t>(matId))
		{
		}
		ShapeMaterial(const Material2D& mat)
			: id(mat.id)
		{
		}
		ShapeMaterial(const Material3D& mat)
			: id(mat.id)
		{
		}
		constexpr ShapeMaterial(Material2DHandle handle)
			: id(handle.id)
		{
		}
		constexpr ShapeMaterial(Material3DHandle handle)
			: id(handle.id)
		{
		}
		constexpr operator uint16_t() const
		{
			return id;
		}
	};

	struct ShapeConfig
	{
		ShapeId shapeId = 0;
		ShapeVariables variables{};
		ShapeMaterial material = 0;
		CombinationType combination = CombinationType::Addition;
		bool hasCollision = true;
		int group = 0;
	};

	struct UIShapeConfig
	{
		ShapeId shapeId = 0;
		ShapeVariables variables{};
		ShapeMaterial material = 0;
		CombinationType combination = CombinationType::Addition;
		int group = 0;
	};

	struct SongVisualizationOptions
	{
		ShapeMaterial material = -1;
		CombinationType combination = CombinationType::Addition;
		int group = 0;
	};

	struct ShapeService
	{
		Registry& registry;
		Simulation2D& simulation;
		std::vector<std::shared_ptr<IMathExpression>>& sdfs;

		static ShapeId registerDefaultSDF(const Expr& sdf);
		static ShapeId registerDefaultSDF(std::shared_ptr<IMathExpression> sdf);

		ShapeId registerSDF(const Expr& expr)
		{
			return registerSDF(expr.node);
		}

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

		Entity addShape(const ShapeConfig& config)
		{
			Entity entity = registry.createEntity();
			CustomShape& shape = registry.addComponent<CustomShape>(entity);
			shape.distanceFieldId = config.shapeId;
			shape.combination = config.combination;
			shape.hasCollisions = config.hasCollision;
			shape.groupIdx = config.group;
			shape.material = config.material.id;

			std::copy_n(config.variables.data, 8, shape.parameters);

			return entity;
		}

		Entity addUIShape(const UIShapeConfig& config)
		{
			Entity entity = registry.createEntity();
			UIShape& shape = registry.addComponent<UIShape>(entity);
			shape.distanceFieldId = config.shapeId;
			shape.combination = config.combination;
			shape.groupIdx = config.group;
			shape.material = config.material.id;

			std::copy_n(config.variables.data, 8, shape.parameters);

			return entity;
		}
	};

	struct RenderService
	{
		Registry& registry;
		Entity& cameraEntity;
		SDFRenderSystemContext& context2D;
		SDFRenderSystemContext& context3D;
		SDFRenderSystemContext& contextUI;
		std::vector<WeirdRenderer::Light2D>& lights2D;
		std::vector<WeirdRenderer::Light3D>& lights3D;
		BackgroundParams& background;
		RenderMode& renderMode;

		WeirdRenderer::Camera& camera()
		{
			return registry.getComponent<ECS::Camera>(cameraEntity).camera;
		}

		Entity getCameraEntity() const
		{
			return cameraEntity;
		}

		std::vector<WeirdRenderer::Light2D>& getLights2D()
		{
			return lights2D;
		}

		std::vector<WeirdRenderer::Light3D>& getLights3D()
		{
			return lights3D;
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

	struct Material2DService
	{
		Material2D (&materials)[16];
		uint16_t& count;
		std::unordered_map<std::string, uint16_t>& nameToId;

		Material2D& createMaterial(const std::string& name = "")
		{
			if (!name.empty())
			{
				auto it = nameToId.find(name);
				if (it != nameToId.end())
				{
					return materials[it->second];
				}
			}

			// If slots below 16 are available, use them, otherwise use the last slot
			uint16_t slot = count < 16 ? count++ : 15;
			Material2D& mat = materials[slot];
			mat.id = slot;
			mat.name = name;
			if (!name.empty())
			{
				nameToId[name] = slot;
			}
			return mat;
		}

		Material2D& getOrCreate(const std::string& name, std::function<void(Material2D&)> init = nullptr)
		{
			auto it = nameToId.find(name);
			if (it != nameToId.end())
			{
				return materials[it->second];
			}
			Material2D& mat = createMaterial(name);
			if (init)
			{
				init(mat);
			}
			return mat;
		}

		Material2D& get(const std::string& name)
		{
			auto it = nameToId.find(name);
			if (it != nameToId.end())
			{
				return materials[it->second];
			}
			return materials[0];
		}

		const Material2D& get(const std::string& name) const
		{
			auto it = nameToId.find(name);
			if (it != nameToId.end())
			{
				return materials[it->second];
			}
			return materials[0];
		}

		Material2DHandle getHandle(const std::string& name) const
		{
			auto it = nameToId.find(name);
			if (it != nameToId.end())
			{
				return Material2DHandle(it->second);
			}
			return Material2DHandle(0);
		}

		Material2D& get(uint16_t id)
		{
			return materials[id < 16 ? id : 15];
		}

		const Material2D& get(uint16_t id) const
		{
			return materials[id < 16 ? id : 15];
		}

		Material2D& getMaterial(int index)
		{
			return get(static_cast<uint16_t>(index));
		}

		const Material2D* getMaterials() const
		{
			return materials;
		}

		uint16_t getMaterialCount() const
		{
			return count;
		}

		bool has(const std::string& name) const
		{
			return nameToId.find(name) != nameToId.end();
		}
	};

	struct Material3DService
	{
		Material3D (&materials)[16];
		uint16_t& count;
		std::unordered_map<std::string, uint16_t>& nameToId;

		Material3D& createMaterial(const std::string& name = "")
		{
			if (!name.empty())
			{
				auto it = nameToId.find(name);
				if (it != nameToId.end())
				{
					return materials[it->second];
				}
			}

			uint16_t slot = count < 16 ? count++ : 15;
			Material3D& mat = materials[slot];
			mat.id = slot;
			mat.name = name;
			if (!name.empty())
			{
				nameToId[name] = slot;
			}
			return mat;
		}

		Material3D& getOrCreate(const std::string& name, std::function<void(Material3D&)> init = nullptr)
		{
			auto it = nameToId.find(name);
			if (it != nameToId.end())
			{
				return materials[it->second];
			}
			Material3D& mat = createMaterial(name);
			if (init)
			{
				init(mat);
			}
			return mat;
		}

		Material3D& get(const std::string& name)
		{
			auto it = nameToId.find(name);
			if (it != nameToId.end())
			{
				return materials[it->second];
			}
			return materials[0];
		}

		const Material3D& get(const std::string& name) const
		{
			auto it = nameToId.find(name);
			if (it != nameToId.end())
			{
				return materials[it->second];
			}
			return materials[0];
		}

		Material3DHandle getHandle(const std::string& name) const
		{
			auto it = nameToId.find(name);
			if (it != nameToId.end())
			{
				return Material3DHandle(it->second);
			}
			return Material3DHandle(0);
		}

		Material3D& get(uint16_t id)
		{
			return materials[id < 16 ? id : 15];
		}

		const Material3D& get(uint16_t id) const
		{
			return materials[id < 16 ? id : 15];
		}

		Material3D& getMaterial(int index)
		{
			return get(static_cast<uint16_t>(index));
		}

		const Material3D* getMaterials() const
		{
			return materials;
		}

		uint16_t getMaterialCount() const
		{
			return count;
		}

		bool has(const std::string& name) const
		{
			return nameToId.find(name) != nameToId.end();
		}
	};

	struct AudioService
	{
		AudioRingBuffer<WeirdAudio::SimpleAudioRequest, SOUND_QUEUE_SIZE>& queue;
		std::atomic<float>& frictionSoundLevel;
		ShapeService* shapes = nullptr;
		Registry* registry = nullptr;
		Entity m_visualizationEntity = INVALID_ENTITY;
		float m_lastSyncedParams[8] = {0.0f};

		AudioService(AudioRingBuffer<WeirdAudio::SimpleAudioRequest, SOUND_QUEUE_SIZE>& q, std::atomic<float>& fsl,
					 ShapeService* s = nullptr, Registry* r = nullptr)
			: queue(q)
			, frictionSoundLevel(fsl)
			, shapes(s)
			, registry(r)
		{
		}

		void playSound(const WeirdAudio::SimpleAudioRequest& audio)
		{
			queue.push(audio);
		}

		void playPhysicsSound(const WeirdAudio::SimpleAudioRequest& audio)
		{
			queue.push(audio);
		}

		float getFrictionSound() const
		{
			return frictionSoundLevel.load(std::memory_order_acquire);
		}

		void setFrictionSound(float level)
		{
			frictionSoundLevel.store(level, std::memory_order_release);
		}

		AudioRingBuffer<WeirdAudio::SimpleAudioRequest, SOUND_QUEUE_SIZE>& audioQueue()
		{
			return queue;
		}

		// Spatial Audio
		void setSpatialAudioEnabled(bool enabled);
		bool isSpatialAudioEnabled() const;

		// Subsystems
		WeirdAudio::SdfMusicEngine& music();
		WeirdAudio::PhysicsAudioEngine& physicsAudio();

		// Song Management (beat-synced)
		void setSong(std::shared_ptr<WeirdAudio::SdfSong> song, bool beatSynced = true);
		Entity setSong(std::shared_ptr<WeirdAudio::SdfSong> song, const SongVisualizationOptions& visualOptions,
					   bool beatSynced = true);
		void queueSong(std::shared_ptr<WeirdAudio::SdfSong> song);

		// Visualization Entity Inspection & Parameter Sync
		Entity getVisualizationEntity() const
		{
			return m_visualizationEntity;
		}
		void setSongParameter(size_t index, float value);
		void updateVisualization();

		// Motion & Domain Fill Inspection
		float getMotionLevel() const;
		float getMotionNorm() const;
		float getFillRatio() const;
		float getTempoFromMotion() const;
		float getVolumeFromFill() const;
		float getTempo() const;
		float getTimeBetweenBeats() const;

		// Re-sample procedural shape parameters (call after updating shape variables in real time)
		void resampleShape();

		// Real-time Dynamic Feedback
		void triggerPositiveFeedback(float intensity = 1.0f);
		void triggerNegativeFeedback(float intensity = 1.0f);
		void triggerDeath();
		void surge(float amount = 0.5f);
		void duck(float amount = 0.5f);
		void resetDynamicEffects();
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

	struct InputService
	{
		bool getKey(Input::KeyCode key) const
		{
			return Input::GetKey(key);
		}
		bool getKeyDown(Input::KeyCode key) const
		{
			return Input::GetKeyDown(key);
		}
		bool getKeyUp(Input::KeyCode key) const
		{
			return Input::GetKeyUp(key);
		}

		float getMouseX() const
		{
			return Input::GetMouseX();
		}
		float getMouseY() const
		{
			return Input::GetMouseY();
		}
		float getMouseDeltaX() const
		{
			return Input::GetMouseDeltaX();
		}
		float getMouseDeltaY() const
		{
			return Input::GetMouseDeltaY();
		}
		float getMouseDeltaXRaw() const
		{
			return Input::GetMouseDeltaXRaw();
		}
		float getMouseDeltaYRaw() const
		{
			return Input::GetMouseDeltaYRaw();
		}
		bool getMouseButton(Input::MouseButton button) const
		{
			return Input::GetMouseButton(button);
		}
		bool getMouseButtonDown(Input::MouseButton button) const
		{
			return Input::GetMouseButtonDown(button);
		}
		bool getMouseButtonUp(Input::MouseButton button) const
		{
			return Input::GetMouseButtonUp(button);
		}
		void setMousePosition(float x, float y)
		{
			Input::SetMousePosition(x, y);
		}
		void showMouse()
		{
			Input::ShowMouse();
		}
		void hideMouse()
		{
			Input::HideMouse();
		}
		bool isUIClick() const
		{
			return Input::isUIClick();
		}
		void flagUIClick()
		{
			Input::flagUIClick();
		}

		bool getGamepadButton(Input::GamepadButton button) const
		{
			return Input::GetGamepadButton(button);
		}
		bool getGamepadButtonDown(Input::GamepadButton button) const
		{
			return Input::GetGamepadButtonDown(button);
		}
		bool getGamepadButtonUp(Input::GamepadButton button) const
		{
			return Input::GetGamepadButtonUp(button);
		}
		float getGamepadAxis(Input::GamepadAxis axis) const
		{
			return Input::GetGamepadAxis(axis);
		}

		void suppressMouseInput()
		{
			Input::suppressMouseInput();
		}
		void suppressKeyboardInput()
		{
			Input::suppressKeyboardInput();
		}
	};

	struct ResourceService
	{
		ResourceManager& resourceManager;
		std::string assetsBasePath;

		ResourceManager& resources()
		{
			return resourceManager;
		}

		void setAssetsBasePath(const std::string& path)
		{
			assetsBasePath = path;
		}

		std::string assetPath(const std::string& relative) const
		{
			return assetsBasePath + relative;
		}

		MeshID getMeshId(const std::string& path, Entity entity, bool instancing = false)
		{
			return resourceManager.getMeshId(assetPath(path).c_str(), entity, instancing);
		}

		std::string readTextFile(const std::string& path) const
		{
			return get_file_contents(path.c_str());
		}

		void writeTextFile(const std::string& path, const std::string& content) const
		{
			saveToFile(path.c_str(), content);
		}

		bool fileExists(const std::string& path) const
		{
			return checkIfFileExists(path.c_str());
		}

		void ensureDirectory(const std::string& path) const
		{
			if (!std::filesystem::exists(path))
			{
				std::filesystem::create_directory(path);
			}
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
	// a ServiceProvider& (plus the Registry&) and use it instead of reaching
	// into Scene internals. Owned by Scene, which binds it to its storage.
	class ServiceProvider
	{
	public:
		explicit ServiceProvider(Scene& scene);

		Registry& registry()
		{
			return m_registry;
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

		Material2DService& materials2D()
		{
			return m_materials2D;
		}

		Material3DService& materials3D()
		{
			return m_materials3D;
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

		InputService& input()
		{
			return m_input;
		}

	private:
		Registry& m_registry;
		TimeService m_time;
		PhysicsService m_physics;
		ShapeService m_shapes;
		RenderService m_render;
		Material2DService m_materials2D;
		Material3DService m_materials3D;
		AudioService m_audio;
		TagService m_tags;
		SerializationService m_serialization;
		SceneControlService m_sceneControl;
		ResourceService m_resources;
		DebugService m_debug;
		InputService m_input;
	};
} // namespace WeirdEngine
