#include "weird-audio/AudioEngine.h"
#include "weird-engine/components/Transform.h"
#include "weird-engine/Logger.h"
#include "weird-engine/math/SDF.h"
#include "weird-engine/Scene.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-engine/systems/PhysicsSystem2D.h"
#include "weird-engine/systems/Shape2DPacking.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/components/Spring.h"
#include "weird-physics/Simulation2D.h"
#include "weird-renderer/components/SDFRenderer.h"
#include "weird-renderer/components/Shape.h"
#include "weird-renderer/components/TextRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

namespace WeirdEngine
{
	ServiceProvider::ServiceProvider(Scene& scene)
		: m_registry(scene.m_registry)
		, m_time(scene.m_simulation2D, scene.m_lastDelta)
		, m_physics(scene.m_registry, scene.m_simulation2D, scene.m_sdfs)
		, m_shapes(scene.m_registry, scene.m_simulation2D, scene.m_sdfs, nullptr, &scene.m_sdfDefinitionsVersion,
				   &scene.m_sdfBounds)
		, m_render(scene.m_registry, scene.m_mainCamera, scene.m_2DWorldRenderContext, scene.m_3DWorldRenderContext,
				   scene.m_UIRenderContext, scene.m_lights2D, scene.m_lights3D, scene.m_background, scene.m_renderMode,
				   scene.m_resetCameraReprojection, scene.m_worldSmoothFactorOverride, scene.m_worldSmoothFactorDirty)
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
		m_shapes.setResourceService(&m_resources);
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
		for (auto& sys : m_scene.m_snapshotLoadedSystems)
		{
			sys(m_scene.m_registry, m_scene.m_services);
		}
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
		config.layer = visualOptions.layer;
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
		// Evaluates the scene SDF (shape combination, layer, and order logic) plus optional
		// rigidbodies (spatial grid snapshot) at a single world point. One instance
		// per query; caches the component arrays and reuses the layer scratch buffer
		// across raymarch steps.
		class SceneSampler
		{
		public:
			SceneSampler(Registry& registry, std::vector<std::shared_ptr<IMathExpression>>& sdfs, float time,
						 Simulation2D* simulation, int layerFilter = -1)
				: m_sdfs(sdfs)
				, m_time(time)
				, m_layerFilter(layerFilter)
				, m_shapes(registry.getComponentArray<Shape>())
				, m_rigidBodies(registry.getComponentArray<RigidBody2D>())
			{
				if (simulation)
				{
					m_gridSnapshot = simulation->getSpatialGridSnapshot();
				}
				if (m_shapes)
				{
					const size_t count = m_shapes->getSize();
					m_orderedShapeIndices.resize(count);
					for (size_t i = 0; i < count; ++i)
					{
						m_orderedShapeIndices[i] = i;
					}
					std::stable_sort(m_orderedShapeIndices.begin(), m_orderedShapeIndices.end(),
									 [&](size_t a, size_t b)
									 {
										 const auto& sa = m_shapes->getDataAtIdx(a);
										 const auto& sb = m_shapes->getDataAtIdx(b);
										 if (sa.layer != sb.layer)
										 {
											 return sa.layer < sb.layer;
										 }
										 return sa.order < sb.order;
									 });
				}
			}

			RaymarchResult sample(glm::vec2 point)
			{
				float d = 1000.0f;
				Entity closestEntity = INVALID_ENTITY;

				int currentLayer = -1;
				float layerMinDistance = 1000.0f;
				Entity layerClosestEntity = INVALID_ENTITY;

				auto flushLayer = [&]()
				{
					if (currentLayer != -1 && currentLayer != Shape::GLOBAL_LAYER)
					{
						if (layerMinDistance < d)
						{
							d = layerMinDistance;
							closestEntity = layerClosestEntity;
						}
					}
				};

				for (size_t idx : m_orderedShapeIndices)
				{
					auto& shape = m_shapes->getDataAtIdx(idx);

					if (!shape.hasCollisions)
						continue;

					if (m_layerFilter >= 0 && shape.layer != static_cast<uint16_t>(m_layerFilter) &&
						shape.layer != Shape::GLOBAL_LAYER)
						continue;

					if (shape.distanceFieldId >= DefaultShapes::COUNT_2D && shape.distanceFieldId >= m_sdfs.size())
						continue;

					const int layer = shape.layer;
					if (layer != currentLayer)
					{
						flushLayer();
						currentLayer = layer;
						layerMinDistance = 1000.0f;
						layerClosestEntity = INVALID_ENTITY;
					}

					float dist = Shape2D::evaluate(shape, m_sdfs, point, m_time,
												   WeirdAudio::AudioEngine::getInstance().getAudioVolume());

					const bool isGlobal = (shape.layer == Shape::GLOBAL_LAYER);
					float& targetDist = isGlobal ? d : layerMinDistance;
					Entity& targetEntity = isGlobal ? closestEntity : layerClosestEntity;

					bool closestEntityUpdated = false;
					switch (shape.combination)
					{
						case CombinationType::Addition:
							if (dist < targetDist)
							{
								closestEntityUpdated = true;
								targetDist = dist;
							}
							break;
						case CombinationType::Subtraction:
							targetDist = std::max(targetDist, -dist);
							break;
						case CombinationType::SmoothAddition:
							dist = fOpUnionSoft(dist, targetDist, shape.smoothFactor);
							if (dist < targetDist)
								closestEntityUpdated = true;
							targetDist = dist;
							break;
						case CombinationType::SmoothSubtraction:
							targetDist = fOpSubSoft(targetDist, dist, shape.smoothFactor);
							break;
						case CombinationType::Intersection:
							targetDist = std::max(targetDist, dist);
							break;
					}

					if (closestEntityUpdated)
					{
						targetEntity = m_shapes->getEntityAtIdx(idx);
					}
				}

				flushLayer();

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
				}

				return {d, closestEntity};
			}

		private:
			std::vector<std::shared_ptr<IMathExpression>>& m_sdfs;
			float m_time;
			int m_layerFilter = -1;
			std::shared_ptr<ComponentArray<Shape>> m_shapes;
			std::shared_ptr<ComponentArray<RigidBody2D>> m_rigidBodies;
			std::shared_ptr<SpatialGridSnapshot> m_gridSnapshot;
			std::vector<size_t> m_orderedShapeIndices;
		};
	} // namespace

	RaymarchResult PhysicsService::sampleAt(vec2 point, bool includeRigidbodies, int layerFilter)
	{
		SceneSampler sampler(m_registry, m_sdfs, static_cast<float>(m_simulation.getSimulationTime()),
							 includeRigidbodies ? &m_simulation : nullptr, layerFilter);
		return sampler.sample(point);
	}

	float PhysicsService::sampleDistance(vec2 point, bool includeRigidbodies, int layerFilter)
	{
		return sampleAt(point, includeRigidbodies, layerFilter).distance;
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

	glm::vec2 PhysicsService::sampleGradient(vec2 point, bool includeRigidbodies, float epsilon, int layerFilter)
	{
		if (epsilon <= 0.0f)
		{
			epsilon = 0.005f;
		}

		SceneSampler sampler(m_registry, m_sdfs, static_cast<float>(m_simulation.getSimulationTime()),
							 includeRigidbodies ? &m_simulation : nullptr, layerFilter);

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

		const std::string resolvedPath = m_scene.m_services.resources().resolveAssetPath(assetPath);

		ClusterInstance instance;
		instance.id = m_nextClusterId++;
		instance.assetPath = resolvedPath;
		instance.position = position;
		instance.rotation = rotation;
		instance.tags = instanceTags;
		instance.hasOverrideMetadata = true;

		std::filesystem::path p(resolvedPath);
		instance.type = p.stem().string();

		if (!instanceName.empty())
			instance.name = instanceName;
		else
			instance.name = instance.type + "_" + std::to_string(instance.id);

		SceneSerializer::load(m_scene, resolvedPath, &instance.localNames, false, &instance.entities,
							  &instance.sourceEntityIds);

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

	namespace
	{
		struct OldClusterEntitySnapshot
		{
			Entity oldEntity = INVALID_ENTITY;
			Entity sourceEntityId = INVALID_ENTITY;
			std::string localName;
			std::string globalName;
			std::vector<std::string> customTags;
			uint32_t overrideMask = ClusterOverrideFlag::None;
			bool hasTransform = false;
			Transform transform{};
			vec2 localPosUnrotated{0.0f};
			bool hasRigidBody = false;
			RigidBody2D rigidBody{};
			bool hasDot = false;
			Dot dot{};
			bool hasShape = false;
			Shape shape{};
			bool hasText = false;
			TextRenderer text{};
		};

		struct ExternalConstraintRef
		{
			Entity constraintEntity = INVALID_ENTITY;
			bool isSpring = true;
			bool clusterOnA = true;
			Entity oldClusterEntity = INVALID_ENTITY;
		};

		vec2 rotate2D(vec2 v, float angleRad)
		{
			float c = std::cos(angleRad);
			float s = std::sin(angleRad);
			return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
		}

		// Minimum-cost rectangular Hungarian solver (rows <= cols).
		std::vector<int> solveMinCostAssignment(const std::vector<std::vector<double>>& cost)
		{
			const int n = static_cast<int>(cost.size());
			if (n == 0)
				return {};
			const int m = static_cast<int>(cost[0].size());

			constexpr double kInf = std::numeric_limits<double>::infinity();
			std::vector<double> u(n + 1, 0.0);
			std::vector<double> v(m + 1, 0.0);
			std::vector<int> p(m + 1, 0);
			std::vector<int> way(m + 1, 0);

			for (int i = 1; i <= n; ++i)
			{
				p[0] = i;
				int j0 = 0;
				std::vector<double> minv(m + 1, kInf);
				std::vector<bool> used(m + 1, false);
				do
				{
					used[j0] = true;
					int i0 = p[j0];
					double delta = kInf;
					int j1 = 0;
					for (int j = 1; j <= m; ++j)
					{
						if (used[j])
							continue;
						double cur = cost[i0 - 1][j - 1] - u[i0] - v[j];
						if (cur < minv[j])
						{
							minv[j] = cur;
							way[j] = j0;
						}
						if (minv[j] < delta)
						{
							delta = minv[j];
							j1 = j;
						}
					}
					for (int j = 0; j <= m; ++j)
					{
						if (used[j])
						{
							u[p[j]] += delta;
							v[j] -= delta;
						}
						else
						{
							minv[j] -= delta;
						}
					}
					j0 = j1;
				} while (p[j0] != 0);

				do
				{
					int j1 = way[j0];
					p[j0] = p[j1];
					j0 = j1;
				} while (j0 != 0);
			}

			std::vector<int> assignment(n, -1);
			for (int j = 1; j <= m; ++j)
			{
				if (p[j] > 0)
					assignment[p[j] - 1] = j - 1;
			}
			return assignment;
		}

		std::vector<OldClusterEntitySnapshot> captureClusterSnapshots(const ClusterInstance& cluster,
																	  Registry& registry, ServiceProvider& services,
																	  vec2 centroid, float currentRot,
																	  float& outMaxRadius)
		{
			std::unordered_map<Entity, std::string> localNameByEntity;
			for (const auto& [name, ent] : cluster.localNames)
			{
				localNameByEntity[ent] = name;
			}

			std::vector<OldClusterEntitySnapshot> snapshots;
			outMaxRadius = 0.0f;

			for (Entity e : cluster.entities)
			{
				const bool hasTransform = registry.hasComponent<Transform>(e);
				const bool hasRigidBody = registry.hasComponent<RigidBody2D>(e);
				const bool hasDot = registry.hasComponent<Dot>(e);
				const bool hasShape = registry.hasComponent<Shape>(e);
				const bool hasText = registry.hasComponent<TextRenderer>(e);
				if (!hasTransform && !hasRigidBody && !hasDot && !hasShape && !hasText)
					continue;

				OldClusterEntitySnapshot snap;
				snap.oldEntity = e;
				snap.sourceEntityId = cluster.getSourceEntityId(e);
				if (auto it = localNameByEntity.find(e); it != localNameByEntity.end())
					snap.localName = it->second;
				snap.globalName = services.names().getName(e);
				for (const auto& tag : services.tags().getTags(e))
				{
					if (!cluster.hasTag(tag))
						snap.customTags.push_back(tag);
				}
				snap.overrideMask = cluster.getOverrides(e);

				if (hasTransform)
				{
					snap.hasTransform = true;
					snap.transform = registry.getComponent<Transform>(e);
					vec2 relWorld = vec2(snap.transform.position) - centroid;
					outMaxRadius = std::max(outMaxRadius, glm::length(relWorld));
					snap.localPosUnrotated = rotate2D(relWorld, -currentRot);
				}
				if (hasRigidBody)
				{
					snap.hasRigidBody = true;
					snap.rigidBody = registry.getComponent<RigidBody2D>(e);
				}
				if (hasDot)
				{
					snap.hasDot = true;
					snap.dot = registry.getComponent<Dot>(e);
				}
				if (hasShape)
				{
					snap.hasShape = true;
					snap.shape = registry.getComponent<Shape>(e);
				}
				if (hasText)
				{
					snap.hasText = true;
					snap.text = registry.getComponent<TextRenderer>(e);
				}

				snapshots.push_back(std::move(snap));
			}
			return snapshots;
		}

		std::unordered_map<Entity, Entity> matchReloadedClusterMembers(
			const std::vector<OldClusterEntitySnapshot>& oldSnaps, const std::vector<Entity>& newEntities,
			const std::unordered_map<Entity, Entity>& newSourceIds, const NameMap& newLocalNames, Registry& registry,
			float oldMaxRadius)
		{
			std::unordered_map<Entity, Entity> oldToNew;

			std::vector<Entity> newMembers;
			newMembers.reserve(newEntities.size());
			std::unordered_map<Entity, size_t> newIdxByEntity;
			float newMaxRadius = 0.0f;
			vec2 newCentroid(0.0f);
			size_t newPosCount = 0;

			for (Entity e : newEntities)
			{
				const bool isMember = registry.hasComponent<Transform>(e) || registry.hasComponent<RigidBody2D>(e) ||
									  registry.hasComponent<Dot>(e) || registry.hasComponent<Shape>(e) ||
									  registry.hasComponent<TextRenderer>(e);
				if (!isMember)
					continue;
				newIdxByEntity[e] = newMembers.size();
				newMembers.push_back(e);
				if (registry.hasComponent<Transform>(e))
				{
					newCentroid += vec2(registry.getComponent<Transform>(e).position);
					++newPosCount;
				}
			}
			if (newPosCount > 0)
				newCentroid /= static_cast<float>(newPosCount);
			for (Entity e : newMembers)
			{
				if (registry.hasComponent<Transform>(e))
				{
					newMaxRadius = std::max(
						newMaxRadius, glm::length(vec2(registry.getComponent<Transform>(e).position) - newCentroid));
				}
			}

			std::vector<bool> oldMatched(oldSnaps.size(), false);
			std::vector<bool> newMatched(newMembers.size(), false);

			// 1. Exact template entity ID match (when available)
			std::unordered_map<Entity, Entity> newBySourceId;
			for (const auto& [newEnt, srcId] : newSourceIds)
			{
				if (srcId != INVALID_ENTITY && newIdxByEntity.count(newEnt))
					newBySourceId[srcId] = newEnt;
			}
			for (size_t i = 0; i < oldSnaps.size(); ++i)
			{
				if (oldSnaps[i].sourceEntityId == INVALID_ENTITY)
					continue;
				auto it = newBySourceId.find(oldSnaps[i].sourceEntityId);
				if (it != newBySourceId.end())
				{
					size_t j = newIdxByEntity[it->second];
					if (!newMatched[j])
					{
						oldMatched[i] = true;
						newMatched[j] = true;
						oldToNew[oldSnaps[i].oldEntity] = it->second;
					}
				}
			}

			// 2. Local name match ("head", "hand", etc.)
			for (size_t i = 0; i < oldSnaps.size(); ++i)
			{
				if (oldMatched[i] || oldSnaps[i].localName.empty())
					continue;
				auto it = newLocalNames.find(oldSnaps[i].localName);
				if (it == newLocalNames.end())
					continue;
				auto idxIt = newIdxByEntity.find(it->second);
				if (idxIt != newIdxByEntity.end() && !newMatched[idxIt->second])
				{
					oldMatched[i] = true;
					newMatched[idxIt->second] = true;
					oldToNew[oldSnaps[i].oldEntity] = it->second;
				}
			}

			// 3. Optimal bipartite matching by spatial proximity + component signature for remaining members
			std::vector<size_t> unmatchedOld;
			std::vector<size_t> unmatchedNew;
			for (size_t i = 0; i < oldSnaps.size(); ++i)
			{
				if (!oldMatched[i])
					unmatchedOld.push_back(i);
			}
			for (size_t j = 0; j < newMembers.size(); ++j)
			{
				if (!newMatched[j])
					unmatchedNew.push_back(j);
			}

			if (!unmatchedOld.empty() && !unmatchedNew.empty())
			{
				const float clusterDiameter = 2.0f * std::max(oldMaxRadius, newMaxRadius);
				const float maxMatchDist = std::max(6.0f, clusterDiameter * 2.0f);
				const double rejectThreshold = static_cast<double>(maxMatchDist) + 200.0;
				const int rows = static_cast<int>(unmatchedOld.size());
				const int realCols = static_cast<int>(unmatchedNew.size());
				const int totalCols = realCols + rows; // Extra dummy columns allow leaving deleted entities unmatched

				std::vector<std::vector<double>> cost(rows, std::vector<double>(totalCols, rejectThreshold));
				for (int r = 0; r < rows; ++r)
				{
					const auto& oldSnap = oldSnaps[unmatchedOld[r]];
					for (int c = 0; c < realCols; ++c)
					{
						Entity newEnt = newMembers[unmatchedNew[c]];
						double spatialDist = 0.0;
						if (oldSnap.hasTransform && registry.hasComponent<Transform>(newEnt))
						{
							spatialDist = glm::distance(vec2(oldSnap.transform.position),
														vec2(registry.getComponent<Transform>(newEnt).position));
						}
						if (spatialDist > static_cast<double>(maxMatchDist))
						{
							cost[r][c] = rejectThreshold + 1000.0;
							continue;
						}

						double penalty = 0.0;
						if (oldSnap.hasRigidBody != registry.hasComponent<RigidBody2D>(newEnt))
							penalty += 100.0;
						if (oldSnap.hasDot != registry.hasComponent<Dot>(newEnt))
							penalty += 100.0;
						if (oldSnap.hasShape != registry.hasComponent<Shape>(newEnt))
							penalty += 100.0;
						if (oldSnap.hasText != registry.hasComponent<TextRenderer>(newEnt))
							penalty += 100.0;
						if (oldSnap.hasDot && registry.hasComponent<Dot>(newEnt) &&
							oldSnap.dot.materialId != registry.getComponent<Dot>(newEnt).materialId)
						{
							penalty += 1.5;
						}

						cost[r][c] = std::min(rejectThreshold - 1.0, spatialDist + penalty);
					}
				}

				const std::vector<int> assignment = solveMinCostAssignment(cost);
				for (int r = 0; r < rows; ++r)
				{
					const int c = assignment[r];
					if (c >= 0 && c < realCols && cost[r][c] < rejectThreshold)
					{
						oldToNew[oldSnaps[unmatchedOld[r]].oldEntity] = newMembers[unmatchedNew[c]];
					}
				}
			}

			return oldToNew;
		}

		uint32_t inferLegacyOverrides(const OldClusterEntitySnapshot& oldSnap, Entity newEnt, Registry& registry)
		{
			uint32_t mask = ClusterOverrideFlag::None;

			if (oldSnap.hasRigidBody && registry.hasComponent<RigidBody2D>(newEnt))
			{
				const auto& newRb = registry.getComponent<RigidBody2D>(newEnt);
				const BodyType oldEffType = oldSnap.rigidBody.isFixed ? BodyType::Fixed : oldSnap.rigidBody.type;
				const BodyType newEffType = newRb.isFixed ? BodyType::Fixed : newRb.type;

				if (oldEffType != BodyType::Dynamic && oldEffType != newEffType)
					mask |= ClusterOverrideFlag::BodyType;
				if (std::abs(oldSnap.rigidBody.mass - 1.0f) > 1e-4f &&
					std::abs(oldSnap.rigidBody.mass - newRb.mass) > 1e-4f)
					mask |= ClusterOverrideFlag::Mass;
				if (!oldSnap.rigidBody.enableCollision && newRb.enableCollision)
					mask |= ClusterOverrideFlag::EnableCollision;
				if (!oldSnap.rigidBody.enableShapeCollision && newRb.enableShapeCollision)
					mask |= ClusterOverrideFlag::EnableShapeCollision;
			}

			if (oldSnap.hasDot && registry.hasComponent<Dot>(newEnt))
			{
				const auto& newDot = registry.getComponent<Dot>(newEnt);
				if (oldSnap.dot.isStatic && !newDot.isStatic)
					mask |= ClusterOverrideFlag::DotIsStatic;
			}

			if (!oldSnap.globalName.empty())
				mask |= ClusterOverrideFlag::GlobalName;
			if (!oldSnap.customTags.empty())
				mask |= ClusterOverrideFlag::CustomTags;

			// Legacy scenes with manually posed Fixed/Kinematic vertices (such as posed arms) preserve their offset
			if ((mask & ClusterOverrideFlag::BodyType) != 0 && oldSnap.hasTransform &&
				registry.hasComponent<Transform>(newEnt))
			{
				const vec2 oldPos(oldSnap.transform.position);
				const vec2 newPos(registry.getComponent<Transform>(newEnt).position);
				if (glm::distance(oldPos, newPos) > 0.25f)
					mask |= ClusterOverrideFlag::Position;
			}

			return mask;
		}

		void applyEntityOverrides(const OldClusterEntitySnapshot& oldSnap, Entity newEnt, uint32_t mask, vec2 centroid,
								  float currentRot, Registry& registry, Simulation2D& simulation2D,
								  ServiceProvider& services, bool& outShapesNeedUpdate)
		{
			if (mask == ClusterOverrideFlag::None)
				return;

			if (oldSnap.hasTransform && registry.hasComponent<Transform>(newEnt))
			{
				auto& t = registry.getComponent<Transform>(newEnt);
				bool dirtyTransform = false;
				if (mask & ClusterOverrideFlag::Position)
				{
					vec2 worldPos = centroid + rotate2D(oldSnap.localPosUnrotated, currentRot);
					t.position = vec3(worldPos.x, worldPos.y, oldSnap.transform.position.z);
					if (registry.hasComponent<RigidBody2D>(newEnt))
					{
						simulation2D.setPosition(registry.getComponent<RigidBody2D>(newEnt).simulationId, worldPos);
					}
					dirtyTransform = true;
				}
				if (mask & ClusterOverrideFlag::Rotation)
				{
					t.rotation = oldSnap.transform.rotation;
					dirtyTransform = true;
				}
				if (mask & ClusterOverrideFlag::Scale)
				{
					t.scale = oldSnap.transform.scale;
					dirtyTransform = true;
				}
				if (dirtyTransform)
				{
					registry.getComponentArray<Transform>()->setEntityDirty(newEnt, true);
				}
			}

			if (oldSnap.hasRigidBody && registry.hasComponent<RigidBody2D>(newEnt))
			{
				auto& rb = registry.getComponent<RigidBody2D>(newEnt);
				bool dirtyRb = false;
				if (mask & ClusterOverrideFlag::BodyType)
				{
					rb.type = oldSnap.rigidBody.isFixed ? BodyType::Fixed : oldSnap.rigidBody.type;
					rb.isFixed = (rb.type == BodyType::Fixed);
					simulation2D.setBodyType(rb.simulationId, rb.type);
					dirtyRb = true;
				}
				if (mask & ClusterOverrideFlag::Mass)
				{
					rb.mass = oldSnap.rigidBody.mass;
					simulation2D.setMass(rb.simulationId, rb.mass);
					dirtyRb = true;
				}
				if (mask & ClusterOverrideFlag::EnableCollision)
				{
					rb.enableCollision = oldSnap.rigidBody.enableCollision;
					simulation2D.setCollisionEnabled(rb.simulationId, rb.enableCollision);
					dirtyRb = true;
				}
				if (mask & ClusterOverrideFlag::EnableShapeCollision)
				{
					rb.enableShapeCollision = oldSnap.rigidBody.enableShapeCollision;
					simulation2D.setShapeCollisionEnabled(rb.simulationId, rb.enableShapeCollision);
					dirtyRb = true;
				}
				if (dirtyRb)
				{
					registry.getComponentArray<RigidBody2D>()->setEntityDirty(newEnt, true);
				}
			}

			if (oldSnap.hasDot && registry.hasComponent<Dot>(newEnt))
			{
				auto& dot = registry.getComponent<Dot>(newEnt);
				bool dirtyDot = false;
				if (mask & ClusterOverrideFlag::DotIsStatic)
				{
					dot.isStatic = oldSnap.dot.isStatic;
					dirtyDot = true;
				}
				if (mask & ClusterOverrideFlag::DotMaterial)
				{
					dot.materialId = oldSnap.dot.materialId;
					dirtyDot = true;
				}
				if (dirtyDot)
				{
					registry.getComponentArray<Dot>()->setEntityDirty(newEnt, true);
					outShapesNeedUpdate = true;
				}
			}

			if (oldSnap.hasShape && registry.hasComponent<Shape>(newEnt))
			{
				auto& s = registry.getComponent<Shape>(newEnt);
				bool dirtyShape = false;
				if (mask & ClusterOverrideFlag::ShapeMaterial)
				{
					s.material = oldSnap.shape.material;
					dirtyShape = true;
				}
				if (mask & ClusterOverrideFlag::ShapeHasCollisions)
				{
					s.hasCollisions = oldSnap.shape.hasCollisions;
					dirtyShape = true;
				}
				if (mask & ClusterOverrideFlag::ShapeCombination)
				{
					s.combination = oldSnap.shape.combination;
					dirtyShape = true;
				}
				if (mask & ClusterOverrideFlag::ShapeSmoothFactor)
				{
					s.smoothFactor = oldSnap.shape.smoothFactor;
					dirtyShape = true;
				}
				if (mask & ClusterOverrideFlag::ShapeLayer)
				{
					s.layer = oldSnap.shape.layer;
					dirtyShape = true;
				}
				if (mask & ClusterOverrideFlag::ShapeOrder)
				{
					s.order = oldSnap.shape.order;
					dirtyShape = true;
				}
				if (mask & ClusterOverrideFlag::ShapeParameters)
				{
					std::copy(std::begin(oldSnap.shape.parameters), std::end(oldSnap.shape.parameters),
							  std::begin(s.parameters));
					dirtyShape = true;
				}
				if (dirtyShape)
				{
					registry.getComponentArray<Shape>()->setEntityDirty(newEnt, true);
					outShapesNeedUpdate = true;
				}
			}

			if (oldSnap.hasText && registry.hasComponent<TextRenderer>(newEnt))
			{
				auto& tr = registry.getComponent<TextRenderer>(newEnt);
				bool dirtyText = false;
				if (mask & ClusterOverrideFlag::TextMaterial)
				{
					tr.material = oldSnap.text.material;
					dirtyText = true;
				}
				if (mask & ClusterOverrideFlag::TextContent)
				{
					tr.text = oldSnap.text.text;
					dirtyText = true;
				}
				if (mask & ClusterOverrideFlag::TextSize)
				{
					tr.width = oldSnap.text.width;
					tr.height = oldSnap.text.height;
					dirtyText = true;
				}
				if (dirtyText)
				{
					registry.getComponentArray<TextRenderer>()->setEntityDirty(newEnt, true);
					outShapesNeedUpdate = true;
				}
			}

			if ((mask & ClusterOverrideFlag::GlobalName) && !oldSnap.globalName.empty())
			{
				services.names().setName(newEnt, oldSnap.globalName);
			}

			if (mask & ClusterOverrideFlag::CustomTags)
			{
				for (const auto& tag : oldSnap.customTags)
				{
					services.tags().addTag(newEnt, tag);
				}
			}
		}
	} // namespace

	void ClusterService::markEntityOverride(Entity entity, uint32_t flags)
	{
		if (entity == INVALID_ENTITY || flags == ClusterOverrideFlag::None)
			return;
		if (auto* cluster = getClusterForEntity(entity))
		{
			cluster->markOverride(entity, flags);
		}
	}

	void ClusterService::clearEntityOverride(Entity entity, uint32_t flags)
	{
		if (entity == INVALID_ENTITY || flags == ClusterOverrideFlag::None)
			return;
		if (auto* cluster = getClusterForEntity(entity))
		{
			cluster->clearOverride(entity, flags);
		}
	}

	bool ClusterService::reloadInstance(uint32_t id, const std::string& overrideAssetPath,
										std::unordered_map<Entity, Entity>* outOldToNewEntities)
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

		std::filesystem::path resolvedPath(m_scene.m_services.resources().resolveAssetPath(path));
		if (!std::filesystem::exists(resolvedPath))
		{
			return false;
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
		const bool hadOverrideMetadata = cluster->hasOverrideMetadata;

		// Capture member state & scene overrides before tearing down old entities
		float oldMaxRadius = 0.0f;
		std::vector<OldClusterEntitySnapshot> oldSnapshots = captureClusterSnapshots(
			*cluster, m_scene.m_registry, m_scene.m_services, centroid, currentRot, oldMaxRadius);

		// 2. Partition old entities and constraints: destroy internal constraints, preserve external ones
		std::unordered_set<Entity> oldEntitySet(cluster->entities.begin(), cluster->entities.end());
		std::vector<ExternalConstraintRef> externalConstraints;

		auto springArray = m_scene.m_registry.getComponentArray<Spring>();
		if (springArray)
		{
			for (size_t i = 0; i < springArray->getSize(); ++i)
			{
				Entity spEnt = springArray->getEntityAtIdx(i);
				auto& sp = springArray->getDataAtIdx(i);
				const bool inA = oldEntitySet.count(sp.entityA) > 0;
				const bool inB = oldEntitySet.count(sp.entityB) > 0;
				if (inA && inB)
				{
					oldEntitySet.insert(spEnt);
				}
				else if (inA || inB)
				{
					externalConstraints.push_back({spEnt, true, inA, inA ? sp.entityA : sp.entityB});
				}
			}
		}

		auto dcArray = m_scene.m_registry.getComponentArray<DistanceConstraint>();
		if (dcArray)
		{
			for (size_t i = 0; i < dcArray->getSize(); ++i)
			{
				Entity dcEnt = dcArray->getEntityAtIdx(i);
				auto& dc = dcArray->getDataAtIdx(i);
				const bool inA = oldEntitySet.count(dc.entityA) > 0;
				const bool inB = oldEntitySet.count(dc.entityB) > 0;
				if (inA && inB)
				{
					oldEntitySet.insert(dcEnt);
				}
				else if (inA || inB)
				{
					externalConstraints.push_back({dcEnt, false, inA, inA ? dc.entityA : dc.entityB});
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

		// Re-fetch cluster pointer after potential vector operations
		cluster = getById(id);

		// 3. Load newly modified cluster from asset file
		std::vector<Entity> newEntities;
		NameMap newLocalNames;
		std::unordered_map<Entity, Entity> newSourceEntityIds;
		SceneSerializer::load(m_scene, resolvedPath.string(), &newLocalNames, false, &newEntities, &newSourceEntityIds);

		// 4. Apply cluster tags
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

		for (Entity e : newEntities)
		{
			if (m_scene.m_registry.hasComponent<Transform>(e))
			{
				auto& t = m_scene.m_registry.getComponent<Transform>(e);
				vec2 rel = vec2(t.position) - assetCentroid;
				vec2 newWorld = centroid + rotate2D(rel, currentRot);
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

		// 6. Match old cluster members to new members and re-apply scene overrides
		std::unordered_map<Entity, Entity> oldToNew = matchReloadedClusterMembers(
			oldSnapshots, newEntities, newSourceEntityIds, newLocalNames, m_scene.m_registry, oldMaxRadius);

		std::unordered_map<Entity, uint32_t> newOverrides;
		bool shapesNeedUpdate = true;
		for (const auto& oldSnap : oldSnapshots)
		{
			auto mapIt = oldToNew.find(oldSnap.oldEntity);
			if (mapIt == oldToNew.end())
				continue;

			Entity newEnt = mapIt->second;
			uint32_t mask = oldSnap.overrideMask;
			if (!hadOverrideMetadata)
			{
				mask |= inferLegacyOverrides(oldSnap, newEnt, m_scene.m_registry);
			}
			if (mask != ClusterOverrideFlag::None)
			{
				applyEntityOverrides(oldSnap, newEnt, mask, centroid, currentRot, m_scene.m_registry,
									 m_scene.m_simulation2D, m_scene.m_services, shapesNeedUpdate);
				newOverrides[newEnt] = mask;
			}
		}

		// 7. Rebind or clean up external constraints that connected old cluster entities to the rest of the scene
		for (const auto& ext : externalConstraints)
		{
			auto mapIt = oldToNew.find(ext.oldClusterEntity);
			if (mapIt == oldToNew.end())
			{
				m_scene.m_services.names().removeName(ext.constraintEntity);
				m_scene.m_services.tags().removeAllTags(ext.constraintEntity);
				m_scene.m_registry.destroyEntity(ext.constraintEntity);
				continue;
			}

			Entity newClusterEnt = mapIt->second;
			if (ext.isSpring && m_scene.m_registry.hasComponent<Spring>(ext.constraintEntity))
			{
				auto& sp = m_scene.m_registry.getComponent<Spring>(ext.constraintEntity);
				if (ext.clusterOnA)
					sp.entityA = newClusterEnt;
				else
					sp.entityB = newClusterEnt;
				m_scene.m_registry.getComponentArray<Spring>()->setEntityDirty(ext.constraintEntity, true);
			}
			else if (!ext.isSpring && m_scene.m_registry.hasComponent<DistanceConstraint>(ext.constraintEntity))
			{
				auto& dc = m_scene.m_registry.getComponent<DistanceConstraint>(ext.constraintEntity);
				if (ext.clusterOnA)
					dc.entityA = newClusterEnt;
				else
					dc.entityB = newClusterEnt;
				m_scene.m_registry.getComponentArray<DistanceConstraint>()->setEntityDirty(ext.constraintEntity, true);
			}
		}
		m_scene.m_registry.freeRemovedComponents();

		// 8. Update cluster instance
		cluster->assetPath = path;
		cluster->position = centroid;
		cluster->rotation = currentRot;
		cluster->entities = std::move(newEntities);
		cluster->localNames = std::move(newLocalNames);
		cluster->tags = std::move(tags);
		cluster->sourceEntityIds = std::move(newSourceEntityIds);
		cluster->entityOverrides = std::move(newOverrides);
		cluster->hasOverrideMetadata = true;

		// 9. Ensure internal springs and constraints are included in cluster entities
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

		if (outOldToNewEntities)
		{
			*outOldToNewEntities = std::move(oldToNew);
		}

		m_scene.m_2DWorldRenderContext.shapesNeedUpdate = shapesNeedUpdate;
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

	float ShapeService::evaluateShape2D(const Shape& shape, vec2 p, float time, float audioVolume) const
	{
		return Shape2D::evaluate(shape, m_sdfs, p, time, audioVolume);
	}

	float ShapeService::evaluateUIShape2D(const UIShape& shape, vec2 p, float time, float audioVolume) const
	{
		return Shape2D::evaluate(shape, m_sdfs, p, time, audioVolume);
	}

} // namespace WeirdEngine
