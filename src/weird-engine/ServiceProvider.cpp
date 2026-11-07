#include "weird-audio/AudioEngine.h"
#include "weird-engine/Logger.h"
#include "weird-engine/math/SDF.h"
#include "weird-engine/Scene.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-engine/systems/PhysicsSystem2D.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/components/Spring.h"
#include "weird-physics/Simulation2D.h"
#include "weird-renderer/components/Shape.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
				   scene.m_UIRenderContext, scene.m_lights2D, scene.m_lights3D, scene.m_background, scene.m_renderMode,
				   scene.m_resetCameraReprojection)
		, m_materials2D(scene.m_materials2D, scene.m_material2DCount, scene.m_material2DNameToId)
		, m_materials3D(scene.m_materials3D, scene.m_material3DCount, scene.m_material3DNameToId)
		, m_audio(scene.m_audioQueue, scene.m_collisionSoundVolume, &m_shapes, &scene.m_registry,
				  &scene.m_serializationBlacklist)
		, m_names(scene.m_nameToEntity, scene.m_entityToName)
		, m_tags(scene.m_tagToEntities, scene.m_entityToTags)
		, m_clusters(scene, scene.m_clusters)
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

	NameMap SerializationService::loadWeirdFile(const std::string& path, bool blacklistEntities, bool restoreCamera)
	{
		NameMap loadedNames;
		Entity firstNewEntity = m_scene.m_registry.getEntityCount();
		SceneSerializer::load(m_scene, path, &loadedNames, restoreCamera);
		if (blacklistEntities)
		{
			Entity lastNewEntity = m_scene.m_registry.getEntityCount();
			for (Entity entity = firstNewEntity; entity < lastNewEntity; ++entity)
				m_scene.m_serializationBlacklist.insert(entity);
		}
		return loadedNames;
	}

	void SerializationService::deleteSceneFile()
	{
		if (!m_scene.m_snapshotFilePath.empty())
		{
			deleteSnapshotFile();
		}

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

	void SerializationService::deleteSnapshotFile()
	{
		if (m_scene.m_snapshotFilePath.empty())
			return;

		std::error_code ec;
		if (std::filesystem::remove(m_scene.m_snapshotFilePath, ec))
		{
			Logger::log("[Snapshot] Deleted snapshot file " + m_scene.m_snapshotFilePath);
		}
		else if (ec)
		{
			Logger::error("[Snapshot] Failed to delete snapshot file " + m_scene.m_snapshotFilePath + ": " +
						  ec.message());
		}
	}

	void SerializationService::setSnapshotFilePath(const std::string& path)
	{
		m_scene.m_snapshotFilePath = path;
	}

	void SerializationService::saveSnapshot(const std::string& filename)
	{
		if (!m_scene.m_registry.canSerializeSnapshot())
		{
			Logger::error("[Snapshot] Scene contains unsupported component or state types.");
			return;
		}
		m_scene.m_registry.freeRemovedComponents();
		const auto physicsSettings = m_scene.m_simulation2D.captureSnapshotSettings();

		std::filesystem::path filePath(filename);
		if (filePath.has_parent_path())
		{
			std::filesystem::create_directories(filePath.parent_path());
		}

		std::ofstream outFile(filename, std::ios::binary);
		if (!outFile.is_open())
		{
			Logger::error("[Snapshot] Failed to open file for writing: " + filename);
			return;
		}

		double simTime = m_scene.m_simulation2D.getSimulationTime();
		if (!m_scene.m_registry.serializeSnapshot(outFile, simTime))
			return;
		outFile.write(reinterpret_cast<const char*>(&physicsSettings), sizeof(physicsSettings));
		Logger::log("[Snapshot] Saved snapshot to " + filename);
	}

	bool SerializationService::loadSnapshot(const std::string& filename)
	{
		if (!m_scene.m_registry.canSerializeSnapshot())
		{
			Logger::error("[Snapshot] Scene contains unsupported component or state types.");
			return false;
		}
		std::ifstream inFile(filename, std::ios::binary);
		if (!inFile.is_open())
		{
			Logger::error("[Snapshot] Failed to open snapshot file: " + filename);
			return false;
		}

		const bool wasPaused = m_scene.m_simulation2D.isPaused();
		m_scene.m_simulation2D.pause();
		m_scene.m_simulation2D.resetBodiesAndShapes();

		double simTime = 0.0;
		if (!m_scene.m_registry.deserializeSnapshot(inFile, &simTime))
		{
			Logger::error("[Snapshot] Failed to deserialize snapshot: " + filename);
			if (!wasPaused)
				m_scene.m_simulation2D.resume();
			return false;
		}
		Simulation2D::SnapshotSettings physicsSettings;
		inFile.read(reinterpret_cast<char*>(&physicsSettings), sizeof(physicsSettings));

		m_scene.m_simulation2D.setSimulationTime(simTime);
		m_scene.m_bodyLastImpactTime.clear();

		auto rbArray = m_scene.m_registry.getComponentArray<RigidBody2D>();
		if (rbArray && rbArray->getSize() > 0)
		{
			size_t maxSimId = 0;
			for (size_t i = 0; i < rbArray->getSize(); ++i)
			{
				auto& rb = rbArray->getDataAtIdx(i);
				if (rb.simulationId.id > maxSimId)
				{
					maxSimId = rb.simulationId.id;
				}
			}
			m_scene.m_simulation2D.allocateBodiesUpTo(maxSimId + 1);
		}

		m_scene.onSnapshotLoaded(m_scene.m_registry, m_scene.m_services);
		ECS::PhysicsSystem2D::update(m_scene.m_registry, m_scene.m_simulation2D);
		m_scene.m_simulation2D.restoreSnapshotSettings(physicsSettings);

		m_scene.m_2DWorldRenderContext.shapesNeedUpdate = true;
		m_scene.m_UIRenderContext.shapesNeedUpdate = true;
		m_scene.m_3DWorldRenderContext.shapesNeedUpdate = true;

		Logger::log("[Snapshot] Loaded snapshot from " + filename);
		return true;
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

	void AudioService::playNote(float freq, float amp, float durationSec, WeirdAudio::Instrument instrument, float pan,
								float filterCutoff, float filterQ)
	{
		music().playNote(freq, amp, durationSec, instrument, pan, filterCutoff, filterQ);
	}

	void AudioService::playNote(int midiNote, float amp, float durationSec, WeirdAudio::Instrument instrument,
								float pan, float filterCutoff, float filterQ)
	{
		music().playNote(midiNote, amp, durationSec, instrument, pan, filterCutoff, filterQ);
	}

	void AudioService::setInstrumentRack(const WeirdAudio::InstrumentRack& rack)
	{
		music().setInstrumentRack(rack);
	}

	WeirdAudio::InstrumentRack AudioService::getInstrumentRack() const
	{
		return WeirdAudio::AudioEngine::getInstance().getMusicEngine().getInstrumentRack();
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
		if (WeirdAudio::AudioEngine::getInstance().getMusicEngine().getCurrentSong() != song)
		{
			WeirdAudio::AudioEngine::getInstance().getMusicEngine().setSong(song, beatSynced);
		}

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
							{
								closestEntityUpdated = true;
								currentMinDistance = dist;
							}
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

	float PhysicsService::sampleDistance(vec2 point, bool includeRigidbodies)
	{
		return sampleAt(point, includeRigidbodies).distance;
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

	glm::vec2 PhysicsService::sampleGradient(vec2 point, bool includeRigidbodies, float epsilon)
	{
		if (epsilon <= 0.0f)
		{
			epsilon = 0.005f;
		}

		SceneSampler sampler(m_registry, m_sdfs, static_cast<float>(m_simulation.getSimulationTime()),
							 includeRigidbodies ? &m_simulation : nullptr);

		float dx =
			sampler.sample(point + vec2(epsilon, 0.0f)).distance - sampler.sample(point - vec2(epsilon, 0.0f)).distance;
		float dy =
			sampler.sample(point + vec2(0.0f, epsilon)).distance - sampler.sample(point - vec2(0.0f, epsilon)).distance;

		vec2 grad(dx, dy);
		float len = glm::length(grad);
		vec2 normal = (len > 0.0001f) ? (grad / len) : vec2(0.0f, 1.0f);

		if (std::isnan(normal.x) || std::isnan(normal.y))
		{
			normal = vec2(0.0f, 1.0f);
		}

		return normal;
	}

	// =========================================================================
	// ClusterService
	// =========================================================================

	ClusterService::ClusterService(Scene& scene, std::vector<ClusterInstance>& clusters)
		: m_scene(scene)
		, m_clusters(clusters)
	{
		syncNextClusterId();
	}

	ClusterInstance& ClusterService::instantiate(const std::string& assetPath, vec2 position, float rotation,
												 const std::string& instanceName,
												 const std::vector<std::string>& instanceTags)
	{
		for (const auto& c : m_clusters)
		{
			if (c.id >= m_nextClusterId)
				m_nextClusterId = c.id + 1;
		}

		ClusterInstance instance;
		instance.id = m_nextClusterId++;
		instance.assetPath = assetPath;
		instance.position = position;
		instance.rotation = rotation;
		instance.tags = instanceTags;

		std::filesystem::path p(assetPath);
		instance.type = p.stem().string();

		if (!instanceName.empty())
			instance.name = instanceName;
		else
			instance.name = instance.type + "_" + std::to_string(instance.id);

		SceneSerializer::load(m_scene, assetPath, &instance.localNames, false, &instance.entities);

		for (Entity e : instance.entities)
		{
			for (const auto& tag : instanceTags)
			{
				m_scene.m_services.tags().addTag(e, tag);
			}
		}

		if (position != vec2(0.0f) || rotation != 0.0f)
		{
			vec2 centroid(0.0f);
			size_t count = 0;
			for (Entity e : instance.entities)
			{
				if (m_scene.m_registry.hasComponent<Transform>(e))
				{
					centroid += vec2(m_scene.m_registry.getComponent<Transform>(e).position);
					count++;
				}
			}
			if (count > 0)
				centroid /= static_cast<float>(count);

			float cosR = std::cos(rotation);
			float sinR = std::sin(rotation);

			for (Entity e : instance.entities)
			{
				if (m_scene.m_registry.hasComponent<Transform>(e))
				{
					auto& t = m_scene.m_registry.getComponent<Transform>(e);
					vec2 rel = vec2(t.position) - centroid;
					vec2 rotated = vec2(rel.x * cosR - rel.y * sinR, rel.x * sinR + rel.y * cosR);
					vec2 newWorld = centroid + rotated + position;
					t.position.x = newWorld.x;
					t.position.y = newWorld.y;
					t.rotation.z += rotation;
					m_scene.m_registry.getComponentArray<Transform>()->setEntityDirty(e, true);

					if (m_scene.m_registry.hasComponent<RigidBody2D>(e))
					{
						auto& rb = m_scene.m_registry.getComponent<RigidBody2D>(e);
						m_scene.m_simulation2D.setPosition(rb.simulationId, newWorld);
					}
				}
			}
		}

		m_clusters.push_back(std::move(instance));
		return m_clusters.back();
	}

	ClusterInstance* ClusterService::getById(uint32_t id)
	{
		for (auto& c : m_clusters)
		{
			if (c.id == id)
				return &c;
		}
		return nullptr;
	}

	const ClusterInstance* ClusterService::getById(uint32_t id) const
	{
		for (const auto& c : m_clusters)
		{
			if (c.id == id)
				return &c;
		}
		return nullptr;
	}

	ClusterInstance* ClusterService::getByName(const std::string& name)
	{
		for (auto& c : m_clusters)
		{
			if (c.name == name)
				return &c;
		}
		return nullptr;
	}

	const ClusterInstance* ClusterService::getByName(const std::string& name) const
	{
		for (const auto& c : m_clusters)
		{
			if (c.name == name)
				return &c;
		}
		return nullptr;
	}

	std::vector<ClusterInstance*> ClusterService::getByType(const std::string& type)
	{
		std::vector<ClusterInstance*> result;
		for (auto& c : m_clusters)
		{
			if (c.type == type)
				result.push_back(&c);
		}
		return result;
	}

	std::vector<const ClusterInstance*> ClusterService::getByType(const std::string& type) const
	{
		std::vector<const ClusterInstance*> result;
		for (const auto& c : m_clusters)
		{
			if (c.type == type)
				result.push_back(&c);
		}
		return result;
	}

	std::vector<ClusterInstance*> ClusterService::getByTag(const std::string& tag)
	{
		std::vector<ClusterInstance*> result;
		for (auto& c : m_clusters)
		{
			if (c.hasTag(tag))
				result.push_back(&c);
		}
		return result;
	}

	std::vector<const ClusterInstance*> ClusterService::getByTag(const std::string& tag) const
	{
		std::vector<const ClusterInstance*> result;
		for (const auto& c : m_clusters)
		{
			if (c.hasTag(tag))
				result.push_back(&c);
		}
		return result;
	}

	ClusterInstance* ClusterService::getClusterForEntity(Entity entity)
	{
		for (auto& c : m_clusters)
		{
			if (c.hasEntity(entity))
				return &c;
		}
		return nullptr;
	}

	const ClusterInstance* ClusterService::getClusterForEntity(Entity entity) const
	{
		for (const auto& c : m_clusters)
		{
			if (c.hasEntity(entity))
				return &c;
		}
		return nullptr;
	}

	void ClusterService::destroyCluster(uint32_t id)
	{
		auto it =
			std::find_if(m_clusters.begin(), m_clusters.end(), [id](const ClusterInstance& c) { return c.id == id; });
		if (it == m_clusters.end())
			return;

		for (Entity e : it->entities)
		{
			m_scene.m_services.names().removeName(e);
			m_scene.m_services.tags().removeAllTags(e);
			m_scene.m_registry.destroyEntity(e);
		}

		m_scene.m_registry.freeRemovedComponents();
		m_scene.m_2DWorldRenderContext.shapesNeedUpdate = true;

		m_clusters.erase(it);
	}

	void ClusterService::destroyCluster(ClusterInstance* cluster)
	{
		if (cluster)
			destroyCluster(cluster->id);
	}

	void ClusterService::destroyAll()
	{
		while (!m_clusters.empty())
		{
			destroyCluster(m_clusters.back().id);
		}
	}

	void ClusterService::clear()
	{
		m_clusters.clear();
		m_nextClusterId = 1;
	}

	void ClusterService::syncNextClusterId()
	{
		uint32_t maxId = 0;
		for (const auto& c : m_clusters)
		{
			if (c.id > maxId)
				maxId = c.id;
		}
		m_nextClusterId = maxId + 1;
	}

	bool ClusterService::reloadInstance(uint32_t id, const std::string& overrideAssetPath)
	{
		ClusterInstance* cluster = getById(id);
		if (!cluster)
		{
			return false;
		}

		std::string path = !overrideAssetPath.empty() ? overrideAssetPath : cluster->assetPath;
		if (path.empty())
		{
			return false;
		}

		std::filesystem::path resolvedPath(path);
		if (!std::filesystem::exists(resolvedPath))
		{
			resolvedPath =
				std::filesystem::path("examples/sample-scenes/assets") / std::filesystem::path(path).filename();
			if (!std::filesystem::exists(resolvedPath))
			{
				return false;
			}
		}

		// 1. Calculate current centroid and rotation of the cluster
		vec2 centroid(0.0f);
		size_t count = 0;
		for (Entity e : cluster->entities)
		{
			if (m_scene.m_registry.hasComponent<Transform>(e))
			{
				centroid += vec2(m_scene.m_registry.getComponent<Transform>(e).position);
				count++;
			}
		}
		if (count > 0)
		{
			centroid /= static_cast<float>(count);
		}
		else
		{
			centroid = cluster->position;
		}

		float currentRot = cluster->rotation;
		std::vector<std::string> tags = cluster->tags;

		// 2. Destroy old member entities and constraints connecting them
		std::unordered_set<Entity> oldEntitySet(cluster->entities.begin(), cluster->entities.end());

		auto springArray = m_scene.m_registry.getComponentArray<Spring>();
		if (springArray)
		{
			for (size_t i = 0; i < springArray->getSize(); ++i)
			{
				auto& sp = springArray->getDataAtIdx(i);
				if (oldEntitySet.count(sp.entityA) || oldEntitySet.count(sp.entityB))
				{
					oldEntitySet.insert(springArray->getEntityAtIdx(i));
				}
			}
		}

		auto dcArray = m_scene.m_registry.getComponentArray<DistanceConstraint>();
		if (dcArray)
		{
			for (size_t i = 0; i < dcArray->getSize(); ++i)
			{
				auto& dc = dcArray->getDataAtIdx(i);
				if (oldEntitySet.count(dc.entityA) || oldEntitySet.count(dc.entityB))
				{
					oldEntitySet.insert(dcArray->getEntityAtIdx(i));
				}
			}
		}

		for (Entity e : oldEntitySet)
		{
			m_scene.m_services.names().removeName(e);
			m_scene.m_services.tags().removeAllTags(e);
			m_scene.m_registry.destroyEntity(e);
		}
		m_scene.m_registry.freeRemovedComponents();

		// 3. Load newly modified cluster from asset file
		std::vector<Entity> newEntities;
		NameMap newLocalNames;
		SceneSerializer::load(m_scene, resolvedPath.string(), &newLocalNames, false, &newEntities);

		// 4. Apply tags
		for (Entity e : newEntities)
		{
			for (const auto& tag : tags)
			{
				m_scene.m_services.tags().addTag(e, tag);
			}
		}

		// 5. Offset and rotate new entities to (centroid, currentRot)
		vec2 assetCentroid(0.0f);
		size_t assetCount = 0;
		for (Entity e : newEntities)
		{
			if (m_scene.m_registry.hasComponent<Transform>(e))
			{
				assetCentroid += vec2(m_scene.m_registry.getComponent<Transform>(e).position);
				assetCount++;
			}
		}
		if (assetCount > 0)
		{
			assetCentroid /= static_cast<float>(assetCount);
		}

		float cosR = std::cos(currentRot);
		float sinR = std::sin(currentRot);

		for (Entity e : newEntities)
		{
			if (m_scene.m_registry.hasComponent<Transform>(e))
			{
				auto& t = m_scene.m_registry.getComponent<Transform>(e);
				vec2 rel = vec2(t.position) - assetCentroid;
				vec2 rotated = vec2(rel.x * cosR - rel.y * sinR, rel.x * sinR + rel.y * cosR);
				vec2 newWorld = centroid + rotated;
				t.position.x = newWorld.x;
				t.position.y = newWorld.y;
				t.rotation.z += currentRot;
				m_scene.m_registry.getComponentArray<Transform>()->setEntityDirty(e, true);

				if (m_scene.m_registry.hasComponent<RigidBody2D>(e))
				{
					auto& rb = m_scene.m_registry.getComponent<RigidBody2D>(e);
					m_scene.m_simulation2D.setPosition(rb.simulationId, newWorld);
				}
			}
		}

		// 6. Update cluster instance
		cluster->assetPath = path;
		cluster->position = centroid;
		cluster->rotation = currentRot;
		cluster->entities = std::move(newEntities);
		cluster->localNames = std::move(newLocalNames);
		cluster->tags = std::move(tags);

		// 7. Ensure internal springs and constraints are included in cluster entities
		auto newSpringArray = m_scene.m_registry.getComponentArray<Spring>();
		if (newSpringArray)
		{
			for (size_t i = 0; i < newSpringArray->getSize(); ++i)
			{
				Entity spEnt = newSpringArray->getEntityAtIdx(i);
				auto& sp = newSpringArray->getDataAtIdx(i);
				if (cluster->hasEntity(sp.entityA) && cluster->hasEntity(sp.entityB))
				{
					if (!cluster->hasEntity(spEnt))
					{
						cluster->entities.push_back(spEnt);
					}
				}
			}
		}

		auto newDcArray = m_scene.m_registry.getComponentArray<DistanceConstraint>();
		if (newDcArray)
		{
			for (size_t i = 0; i < newDcArray->getSize(); ++i)
			{
				Entity dcEnt = newDcArray->getEntityAtIdx(i);
				auto& dc = newDcArray->getDataAtIdx(i);
				if (cluster->hasEntity(dc.entityA) && cluster->hasEntity(dc.entityB))
				{
					if (!cluster->hasEntity(dcEnt))
					{
						cluster->entities.push_back(dcEnt);
					}
				}
			}
		}

		m_scene.m_2DWorldRenderContext.shapesNeedUpdate = true;
		return true;
	}

	int ClusterService::reloadAllInstancesOf(const std::string& assetPath)
	{
		std::filesystem::path targetP(assetPath);
		std::string targetStem = targetP.stem().string();
		std::string targetFilename = targetP.filename().string();

		int count = 0;
		for (auto& c : m_clusters)
		{
			std::filesystem::path cp(c.assetPath);
			if (cp == targetP || cp.filename().string() == targetFilename || c.type == targetStem)
			{
				if (reloadInstance(c.id, assetPath))
				{
					count++;
				}
			}
		}
		return count;
	}

	std::vector<ClusterInstance*> ClusterService::getInstancesByAsset(const std::string& assetPath)
	{
		std::filesystem::path targetP(assetPath);
		std::string targetFilename = targetP.filename().string();
		std::string targetStem = targetP.stem().string();

		std::vector<ClusterInstance*> result;
		for (auto& c : m_clusters)
		{
			std::filesystem::path cp(c.assetPath);
			if (cp == targetP || cp.filename().string() == targetFilename || c.type == targetStem)
			{
				result.push_back(&c);
			}
		}
		return result;
	}

	ClusterInstance& ClusterService::addInstance(ClusterInstance instance)
	{
		if (instance.id >= m_nextClusterId)
			m_nextClusterId = instance.id + 1;
		m_clusters.push_back(std::move(instance));
		return m_clusters.back();
	}

} // namespace WeirdEngine
