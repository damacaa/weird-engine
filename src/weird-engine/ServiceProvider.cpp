#include "weird-audio/AudioEngine.h"
#include "weird-engine/Logger.h"
#include "weird-engine/math/SDF.h"
#include "weird-engine/Scene.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/Simulation2D.h"
#include "weird-renderer/components/Shape.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

namespace WeirdEngine
{
	ServiceProvider::ServiceProvider(Scene& scene)
		: m_registry(scene.m_registry)
		, m_time(scene.m_simulation2D, scene.m_lastDelta)
		, m_physics(scene.m_registry, scene.m_simulation2D, scene.m_sdfs)
		, m_shapes(scene.m_registry, scene.m_simulation2D, scene.m_sdfs)
		, m_render(scene.m_registry, scene.m_mainCamera, scene.m_2DWorldRenderContext, scene.m_3DWorldRenderContext,
				   scene.m_UIRenderContext, scene.m_lights2D, scene.m_lights3D, scene.m_background, scene.m_renderMode)
		, m_materials2D(scene.m_materials2D, scene.m_material2DCount, scene.m_material2DNameToId)
		, m_materials3D(scene.m_materials3D, scene.m_material3DCount, scene.m_material3DNameToId)
		, m_audio(scene.m_audioQueue, scene.m_collisionSoundVolume, &m_shapes, &scene.m_registry,
				  &scene.m_serializationBlacklist)
		, m_tags(scene.m_tagToEntity, scene.m_entityToTag)
		, m_serialization(scene, scene.m_serializationBlacklist, scene.m_sceneFilePath)
		, m_sceneControl(scene.m_isSceneComplete, scene.m_nextScene)
		, m_resources(scene.m_resourceManager, "")
		, m_debug(scene.m_debugFly, scene.m_debugInput)
		, m_input()
		, m_random()
	{
	}

	ShapeId ShapeService::registerDefaultSDF(const Expr& sdf)
	{
		return Scene::registerDefaultSDF(sdf);
	}

	ShapeId ShapeService::registerDefaultSDF(std::shared_ptr<IMathExpression> sdf)
	{
		return Scene::registerDefaultSDF(std::move(sdf));
	}

	void SerializationService::saveScene(const std::string& filename)
	{
		SceneSerializer::save(m_scene, filename);
	}

	TagMap SerializationService::loadWeirdFile(const std::string& path, bool blacklistEntities)
	{
		TagMap loadedTags;
		Entity firstNewEntity = m_scene.m_registry.getEntityCount();
		SceneSerializer::load(m_scene, path, &loadedTags);
		if (blacklistEntities)
		{
			Entity lastNewEntity = m_scene.m_registry.getEntityCount();
			for (Entity entity = firstNewEntity; entity < lastNewEntity; ++entity)
				m_scene.m_serializationBlacklist.insert(entity);
		}
		return loadedTags;
	}

	void SerializationService::deleteSceneFile()
	{
		if (m_sceneFilePath.empty())
			return;

		std::error_code ec;
		if (std::filesystem::remove(m_sceneFilePath, ec))
		{
			Logger::log("[SceneSerializer] Deleted scene file " + m_sceneFilePath);
		}
		else if (ec)
		{
			Logger::error("[SceneSerializer] Failed to delete scene file " + m_sceneFilePath + ": " + ec.message());
		}
	}

	void AudioService::setSpatialAudioEnabled(bool enabled)
	{
		WeirdAudio::AudioEngine::getInstance().setSpatialAudioEnabled(enabled);
	}

	bool AudioService::isSpatialAudioEnabled() const
	{
		return WeirdAudio::AudioEngine::getInstance().isSpatialAudioEnabled();
	}

	WeirdAudio::SdfMusicEngine& AudioService::music()
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine();
	}

	void AudioService::setSong(std::shared_ptr<WeirdAudio::SdfSong> song, bool beatSynced)
	{
		SongVisualizationOptions defaultVisualOptions;
		setSong(std::move(song), defaultVisualOptions, beatSynced);
	}

	Entity AudioService::setSong(std::shared_ptr<WeirdAudio::SdfSong> song,
								 const SongVisualizationOptions& visualOptions, bool beatSynced)
	{
		auto songPtr = song;
		WeirdAudio::AudioEngine::getInstance().getMusicEngine().setSong(song, beatSynced);

		if (m_visualizationEntity != INVALID_ENTITY && m_registry)
		{
			if (m_serializationBlacklist)
			{
				m_serializationBlacklist->erase(m_visualizationEntity);
			}
			m_registry->destroyEntity(m_visualizationEntity);
			m_visualizationEntity = INVALID_ENTITY;
		}

		switch (visualOptions.mode)
		{
			case SongVisualizationMode::None:
				break;
			case SongVisualizationMode::UI:
				if (m_shapes && m_registry && songPtr && songPtr->getShapeExpression())
				{
					return createUIVisualization(songPtr, visualOptions);
				}
				break;
			case SongVisualizationMode::World:
				// TODO(world songs): play a song anchored to a world-space SDF shape.
				// No Transform is involved: the shape's position is baked into the registered SDF
				// expression (world coordinates), exactly like UI mode bakes the screen anchor.
				// The listener is the cameraEntity. Per frame:
				//   1. Register the song shape as a world Shape (ShapeService::addShape) so it
				//      renders through the 2D/3D world pipelines instead of the UI pipeline.
				//   2. Sample that SDF at the cameraEntity position to obtain the distance: XY
				//      distance with z = 0 in 2D scenes, full 3D distance in 3D scenes.
				//   3. Map the distance to a gain that fades out as the listener moves away
				//      (falloff radius/curve fields would extend SongVisualizationOptions).
				//   4. Push the gain as a scalar into SdfMusicEngine, which stays ECS-agnostic.
				// AudioService will need the camera entity wired in (similar to how RenderService
				// exposes cameraEntity) to evaluate the listener position each frame.
				break;
		}

		return INVALID_ENTITY;
	}

	Entity AudioService::createUIVisualization(const std::shared_ptr<WeirdAudio::SdfSong>& song,
											   const SongVisualizationOptions& visualOptions)
	{
		ShapeId shapeId = m_shapes->registerSDF(song->getShapeExpression());
		UIShapeConfig config;
		config.shapeId = shapeId;
		config.material = visualOptions.material;
		config.combination = visualOptions.combination;
		config.group = visualOptions.group;
		std::copy_n(song->getParameters(), 8, config.variables.data);

		m_visualizationEntity = m_shapes->addUIShape(config);

		if (m_serializationBlacklist)
		{
			m_serializationBlacklist->insert(m_visualizationEntity);
		}

		for (size_t i = 0; i < 8; ++i)
		{
			m_lastSyncedParams[i] = song->getParameter(i);
		}

		return m_visualizationEntity;
	}

	void AudioService::setSongParameter(size_t index, float value)
	{
		if (index >= 8)
			return;

		auto curSong = WeirdAudio::AudioEngine::getInstance().getMusicEngine().getCurrentSong();
		if (curSong)
		{
			curSong->setParameter(index, value);
		}

		m_lastSyncedParams[index] = value;

		if (m_visualizationEntity != INVALID_ENTITY && m_registry &&
			m_registry->hasComponent<UIShape>(m_visualizationEntity))
		{
			auto& ui = m_registry->getComponent<UIShape>(m_visualizationEntity);
			ui.parameters[index] = value;
			m_registry->setComponentDirty(ui);
		}

		WeirdAudio::AudioEngine::getInstance().getMusicEngine().resampleShape();
	}

	void AudioService::queueSong(std::shared_ptr<WeirdAudio::SdfSong> song)
	{
		WeirdAudio::AudioEngine::getInstance().getMusicEngine().queueSong(std::move(song));
	}

	void AudioService::triggerPositiveFeedback(float intensity)
	{
		WeirdAudio::AudioEngine::getInstance().getMusicEngine().triggerPositiveFeedback(intensity);
	}

	void AudioService::triggerNegativeFeedback(float intensity)
	{
		WeirdAudio::AudioEngine::getInstance().getMusicEngine().triggerNegativeFeedback(intensity);
	}

	void AudioService::triggerDeath()
	{
		WeirdAudio::AudioEngine::getInstance().getMusicEngine().triggerDeath();
	}

	void AudioService::surge(float amount)
	{
		WeirdAudio::AudioEngine::getInstance().getMusicEngine().surge(amount);
	}

	void AudioService::duck(float amount)
	{
		WeirdAudio::AudioEngine::getInstance().getMusicEngine().duck(amount);
	}

	void AudioService::resetDynamicEffects()
	{
		WeirdAudio::AudioEngine::getInstance().getMusicEngine().resetDynamicEffects();
	}

	void AudioService::resampleShape()
	{
		if (m_visualizationEntity != INVALID_ENTITY && m_registry &&
			m_registry->hasComponent<UIShape>(m_visualizationEntity))
		{
			auto curSong = WeirdAudio::AudioEngine::getInstance().getMusicEngine().getCurrentSong();
			if (curSong)
			{
				auto& ui = m_registry->getComponent<UIShape>(m_visualizationEntity);
				for (size_t i = 0; i < 8; ++i)
				{
					if (curSong->getParameter(i) != m_lastSyncedParams[i])
					{
						ui.parameters[i] = curSong->getParameter(i);
						m_registry->setComponentDirty(ui);
						m_lastSyncedParams[i] = curSong->getParameter(i);
					}
					else if (ui.parameters[i] != m_lastSyncedParams[i])
					{
						curSong->setParameter(i, ui.parameters[i]);
						m_lastSyncedParams[i] = ui.parameters[i];
					}
				}
			}
		}

		WeirdAudio::AudioEngine::getInstance().getMusicEngine().resampleShape();
	}

	float AudioService::getMotionLevel() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getMotionLevel();
	}

	float AudioService::getMotionNorm() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getMotionNorm();
	}

	float AudioService::getFillRatio() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getFillRatio();
	}

	float AudioService::getTempoFromMotion() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getTempoFromMotion();
	}

	float AudioService::getVolumeFromFill() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getVolumeFromFill();
	}

	float AudioService::getComplexityLevel() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getComplexityLevel();
	}

	float AudioService::getComplexityNorm() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getComplexityNorm();
	}

	float AudioService::getTension() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getTension();
	}

	float AudioService::getTempo() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getTempo();
	}

	float AudioService::getTimeBetweenBeats() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getTimeBetweenBeats();
	}

	float AudioService::getAudioVolume() const
	{
		return WeirdAudio::AudioEngine::getInstance().getAudioVolume();
	}

	namespace
	{
		// Evaluates the scene SDF (shape combination and group logic) plus optional
		// rigidbodies (spatial grid snapshot) at a single world point. One instance
		// per query; caches the component arrays and reuses the group scratch buffer
		// across raymarch steps.
		class SceneSampler
		{
		public:
			SceneSampler(Registry& registry, std::vector<std::shared_ptr<IMathExpression>>& sdfs, float time,
						 Simulation2D* simulation)
				: m_sdfs(sdfs)
				, m_time(time)
				, m_shapes(registry.getComponentArray<Shape>())
				, m_rigidBodies(registry.getComponentArray<RigidBody2D>())
			{
				if (simulation)
				{
					m_gridSnapshot = simulation->getSpatialGridSnapshot();
				}
			}

			RaymarchResult sample(glm::vec2 point)
			{
				m_groups.clear();

				float d = 1000.0f;
				float minD = d;
				Entity closestEntity = INVALID_ENTITY;

				for (size_t j = 0; j < m_shapes->getSize(); j++)
				{
					auto& shape = m_shapes->getDataAtIdx(j);

					if (!shape.hasCollisions)
						continue;

					if (shape.distanceFieldId >= m_sdfs.size())
						continue;

					float parameters[12];
					packSdfParameters(parameters, shape.parameters, m_time, point,
									  WeirdAudio::AudioEngine::getInstance().getAudioVolume());

					float dist = m_sdfs[shape.distanceFieldId]->getValue(parameters);
					float currentMinDistance = d;
					Entity currentEntity = INVALID_ENTITY;

					GroupState* groupState = nullptr;
					for (auto& group : m_groups)
					{
						if (group.id == shape.groupIdx)
						{
							groupState = &group;
							currentMinDistance = groupState->minDistance;
							currentEntity = groupState->closestEntity;
							break;
						}
					}

					if (!groupState && shape.groupIdx != Shape::GLOBAL_GROUP)
					{
						m_groups.push_back({static_cast<uint16_t>(shape.groupIdx), 1000.0f, INVALID_ENTITY});
						groupState = &m_groups.back();
						currentMinDistance = groupState->minDistance;
					}

					bool closestEntityUpdated = false;
					switch (shape.combination)
					{
						case CombinationType::Addition:
							if (dist < currentMinDistance)
								closestEntityUpdated = true;
							currentMinDistance = dist;
							break;
						case CombinationType::Subtraction:
							currentMinDistance = std::max(currentMinDistance, -dist);
							break;
						case CombinationType::SmoothAddition:
							dist = fOpUnionSoft(dist, currentMinDistance, shape.smoothFactor);
							if (dist < currentMinDistance)
								closestEntityUpdated = true;
							currentMinDistance = dist;
							break;
						case CombinationType::SmoothSubtraction:
							currentMinDistance = fOpSubSoft(currentMinDistance, dist, shape.smoothFactor);
							break;
						case CombinationType::Intersection:
							currentMinDistance = std::max(currentMinDistance, dist);
							break;
					}

					if (closestEntityUpdated)
					{
						currentEntity = m_shapes->getEntityAtIdx(j);
					}

					if (shape.groupIdx == Shape::GLOBAL_GROUP)
					{
						d = currentMinDistance;
						if (d < minD)
						{
							minD = d;
							closestEntity = currentEntity;
						}
					}
					else
					{
						groupState->minDistance = currentMinDistance;
						if (closestEntityUpdated)
						{
							groupState->closestEntity = currentEntity;
						}
					}
				}

				for (const auto& group : m_groups)
				{
					if (group.minDistance < d)
					{
						d = group.minDistance;
						closestEntity = group.closestEntity;
					}
					if (d < minD)
					{
						minD = d;
					}
				}

				// Evaluate Rigidbodies using the Spatial Grid Snapshot
				if (m_gridSnapshot)
				{
					float minRigidbodyDist = 1000.0f;
					Entity closestRbEntity = INVALID_ENTITY;

					auto entityForSimulationId = [&](SimulationID simulationId) -> Entity
					{
						if (simulationId >= static_cast<SimulationID>(m_rigidBodies->getSize()))
							return INVALID_ENTITY;

						return m_rigidBodies->getEntityAtIdx(static_cast<size_t>(simulationId));
					};

					int gx = static_cast<int>(std::floor(point.x * m_gridSnapshot->invCellSize));
					int gy = static_cast<int>(std::floor(point.y * m_gridSnapshot->invCellSize));
					const int TABLE_SIZE = 8191; // Must match Simulation2D.cpp

					auto getHash = [](int x, int y) -> int
					{
						constexpr int p1 = 73856093;
						constexpr int p2 = 19349663;
						int hash = (x * p1) ^ (y * p2);
						hash = hash % TABLE_SIZE;
						if (hash < 0)
							hash += TABLE_SIZE;
						return hash;
					};

					for (int dx = -1; dx <= 1; dx++)
					{
						for (int dy = -1; dy <= 1; dy++)
						{
							int hash = getHash(gx + dx, gy + dy);
							int rbIndex = m_gridSnapshot->head[hash];

							while (rbIndex != -1)
							{
								glm::vec2 rbPos = m_gridSnapshot->positions[rbIndex];
								float dist = glm::length(point - rbPos) - m_gridSnapshot->radious;

								if (dist < minRigidbodyDist)
								{
									minRigidbodyDist = dist;
									closestRbEntity = entityForSimulationId(rbIndex);
								}

								rbIndex = m_gridSnapshot->next[rbIndex];
							}
						}
					}

					if (minRigidbodyDist < d)
					{
						d = minRigidbodyDist;
						closestEntity = closestRbEntity;
					}
					if (d < minD)
					{
						minD = d;
					}
				}

				return {d, closestEntity};
			}

		private:
			struct GroupState
			{
				uint16_t id;
				float minDistance;
				Entity closestEntity;
			};

			std::vector<std::shared_ptr<IMathExpression>>& m_sdfs;
			float m_time;
			std::shared_ptr<ComponentArray<Shape>> m_shapes;
			std::shared_ptr<ComponentArray<RigidBody2D>> m_rigidBodies;
			std::shared_ptr<SpatialGridSnapshot> m_gridSnapshot;
			std::vector<GroupState> m_groups;
		};
	} // namespace

	RaymarchResult PhysicsService::sampleAt(vec2 point, bool includeRigidbodies)
	{
		SceneSampler sampler(m_registry, m_sdfs, static_cast<float>(m_simulation.getSimulationTime()),
							 includeRigidbodies ? &m_simulation : nullptr);
		return sampler.sample(point);
	}

	RaymarchResult PhysicsService::raymarch(glm::vec2 origin, glm::vec2 direction, float epsilon, float maxDistance,
											bool includeRigidbodies)
	{
		if (epsilon <= 0.0f)
		{
			epsilon = 0.001f; // Default epsilon
		}

		SceneSampler sampler(m_registry, m_sdfs, static_cast<float>(m_simulation.getSimulationTime()),
							 includeRigidbodies ? &m_simulation : nullptr);

		float traveled = 0.0f;
		for (int i = 0; i < 100; i++)
		{
			glm::vec2 p = origin + (traveled * direction);
			RaymarchResult result = sampler.sample(p);

			if (result.distance <= epsilon)
				return {traveled, result.entity};

			traveled += std::abs(result.distance);

			if (traveled >= maxDistance)
			{
				return {maxDistance, INVALID_ENTITY};
			}
		}

		return {traveled, INVALID_ENTITY};
	}
} // namespace WeirdEngine
