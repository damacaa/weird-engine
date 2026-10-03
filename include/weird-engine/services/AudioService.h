#pragma once

#include <cstddef>
#include <memory>
#include <unordered_set>

#include "weird-audio/AudioRingBuffer.h"
#include "weird-audio/SimpleAudioRequest.h"
#include "weird-engine/ecs/Registry.h"
#include "weird-engine/services/ShapeTypes.h"
#include "weird-engine/vec.h"

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		class SdfMusicEngine;
		class SdfSong;
	} // namespace WeirdAudio

	class ShapeService;

	constexpr int SOUND_QUEUE_SIZE = 64;

	enum class SongVisualizationMode
	{
		None,
		UI,
		World,
	};

	struct SongVisualizationOptions
	{
		SongVisualizationMode mode = SongVisualizationMode::None;
		ShapeMaterial material = -1;
		CombinationType combination = CombinationType::Addition;
		int group = 0;
	};

	class AudioService
	{
	public:
		AudioService(AudioRingBuffer<WeirdAudio::SimpleAudioRequest, SOUND_QUEUE_SIZE>& queue,
					 float& collisionSoundVolume, ShapeService* shapes = nullptr, Registry* registry = nullptr,
					 std::unordered_set<Entity>* serializationBlacklist = nullptr)
			: m_queue(queue)
			, m_collisionSoundVolume(collisionSoundVolume)
			, m_shapes(shapes)
			, m_registry(registry)
			, m_serializationBlacklist(serializationBlacklist)
		{
		}

		void playSound(const WeirdAudio::SimpleAudioRequest& audio)
		{
			m_queue.push(audio);
		}

		// Trigger a one-shot fake collision impact at a world position. Uses the
		// exact same synthesis and volume multiplier as real engine collisions.
		// intensity: 0 (light click) to 1 (heavy thud).
		void playCollisionSound(
			const vec3& position, float intensity = 0.5f,
			WeirdAudio::SimpleAudioRequest::ImpactType type = WeirdAudio::SimpleAudioRequest::ImpactType::Shape)
		{
			m_queue.push(WeirdAudio::SimpleAudioRequest::makeImpact(position, intensity, type, m_collisionSoundVolume));
		}

		// Spatial Audio
		void setSpatialAudioEnabled(bool enabled);
		bool isSpatialAudioEnabled() const;

		// Subsystems
		WeirdAudio::SdfMusicEngine& music();

		// Song Management (beat-synced)
		void setSong(std::shared_ptr<WeirdAudio::SdfSong> song, bool beatSynced = true);
		Entity setSong(std::shared_ptr<WeirdAudio::SdfSong> song, const SongVisualizationOptions& visualOptions,
					   bool beatSynced = true);
		void queueSong(std::shared_ptr<WeirdAudio::SdfSong> song);

		// Visualization Parameter Sync
		void setSongParameter(size_t index, float value);

		// Motion & Domain Fill Inspection
		float getMotionLevel() const;
		float getMotionNorm() const;
		float getFillRatio() const;
		float getTempoFromMotion() const;
		float getVolumeFromFill() const;
		float getComplexityLevel() const;
		float getComplexityNorm() const;
		float getTension() const;
		float getTempo() const;
		float getTimeBetweenBeats() const;
		float getAudioVolume() const;

		// Re-sample procedural shape parameters (call after updating shape variables in real time)
		void resampleShape();

		// Real-time Dynamic Feedback
		void triggerPositiveFeedback(float intensity = 1.0f);
		void triggerNegativeFeedback(float intensity = 1.0f);
		void triggerDeath();
		void surge(float amount = 0.5f);
		void duck(float amount = 0.5f);
		void resetDynamicEffects();

	private:
		Entity createUIVisualization(const std::shared_ptr<WeirdAudio::SdfSong>& song,
									 const SongVisualizationOptions& visualOptions);

		AudioRingBuffer<WeirdAudio::SimpleAudioRequest, SOUND_QUEUE_SIZE>& m_queue;
		float& m_collisionSoundVolume; // 1.0 = 100%, shared with Scene's real impacts
		ShapeService* m_shapes = nullptr;
		Registry* m_registry = nullptr;
		std::unordered_set<Entity>* m_serializationBlacklist = nullptr;
		Entity m_visualizationEntity = INVALID_ENTITY;
		float m_lastSyncedParams[8] = {0.0f};
	};
} // namespace WeirdEngine
