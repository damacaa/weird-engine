#include "weird-audio/PhysicsAudioEngine.h"
#include <algorithm>
#include <cmath>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		PhysicsAudioEngine::PhysicsAudioEngine()
		{
			m_activeVoices.reserve(MAX_PHYSICS_VOICES);
		}

		PhysicsAudioEngine::~PhysicsAudioEngine() {}

		void PhysicsAudioEngine::init(uint32_t sampleRate, uint32_t channels)
		{
			m_sampleRate = sampleRate;
			m_channels = channels;
			m_activeVoices.clear();
			clearFrictionVoices();
			m_smoothedActiveCollisionVoices = 0.0f;
			m_lastCollisionMixScale = 1.0f;
			m_frictionCells.clear();
			m_occupiedFrictionCells.clear();
			m_frictionCellStamp = 0;
			m_occupiedFrictionCellCount = 0;
		}

		float PhysicsAudioEngine::generateNoiseSample()
		{
			// Xorshift32 PRNG
			m_noiseSeed ^= m_noiseSeed << 13;
			m_noiseSeed ^= m_noiseSeed >> 17;
			m_noiseSeed ^= m_noiseSeed << 5;
			float white = (static_cast<float>(m_noiseSeed) / 2147483648.0f) - 1.0f;

			// Paul Kellet's filter for pink noise approximation (-3dB/octave)
			m_pinkB0 = 0.99765f * m_pinkB0 + white * 0.0990460f;
			m_pinkB1 = 0.96300f * m_pinkB1 + white * 0.2965164f;
			m_pinkB2 = 0.57000f * m_pinkB2 + white * 1.0526913f;
			float pink = m_pinkB0 + m_pinkB1 + m_pinkB2 + white * 0.1848f;
			return std::clamp(pink * 0.22f, -1.0f, 1.0f);
		}

		float PhysicsAudioEngine::mapFrictionLevel(float level)
		{
			constexpr float MAX_FRICTION = 1.0f;
			const float normalizedFriction = (std::min)(level / MAX_FRICTION, 1.0f);

			constexpr float MIN_AUDIBLE = 0.001f;
			constexpr float MIN_COMPENSATION = 1.0f / (1.0f - MIN_AUDIBLE);
			const float adjustedAmplitude = (std::max)(normalizedFriction - MIN_AUDIBLE, 0.0f) * MIN_COMPENSATION;

			constexpr float LOW_END_BOOST_EXPONENT = 0.5f;
			constexpr float MAX_AMPLITUDE = 0.75f;
			const float initialAmplitude = MAX_AMPLITUDE * pow(adjustedAmplitude, LOW_END_BOOST_EXPONENT);

			constexpr float COMPRESSION_THRESHOLD = 0.3f;
			constexpr float COMPRESSION_RATIO = 4.0f;
			if (initialAmplitude > COMPRESSION_THRESHOLD)
			{
				float overshoot = initialAmplitude - COMPRESSION_THRESHOLD;
				return COMPRESSION_THRESHOLD + overshoot / COMPRESSION_RATIO;
			}
			return initialAmplitude;
		}

		size_t PhysicsAudioEngine::findOrInsertFrictionCell(int32_t cx, int32_t cy)
		{
			while (true)
			{
				if (m_frictionCells.empty())
				{
					m_frictionCells.assign(1024, FrictionCell{});
				}
				// Keep the load factor below 0.5 so probing stays cheap.
				if ((m_occupiedFrictionCells.size() + 1) * 2 > m_frictionCells.size())
				{
					growFrictionCellTable();
					continue;
				}

				const size_t mask = m_frictionCells.size() - 1;
				size_t index = (static_cast<uint32_t>(cx) * 73856093u ^ static_cast<uint32_t>(cy) * 19349663u) & mask;
				while (true)
				{
					FrictionCell& cell = m_frictionCells[index];
					if (cell.stamp != m_frictionCellStamp)
					{
						cell = FrictionCell{};
						cell.cx = cx;
						cell.cy = cy;
						cell.stamp = m_frictionCellStamp;
						m_occupiedFrictionCells.push_back(static_cast<uint32_t>(index));
						return index;
					}
					if (cell.cx == cx && cell.cy == cy)
					{
						return index;
					}
					index = (index + 1) & mask;
				}
			}
		}

		void PhysicsAudioEngine::growFrictionCellTable()
		{
			const size_t newCapacity = m_frictionCells.empty() ? 1024 : m_frictionCells.size() * 2;
			const size_t mask = newCapacity - 1;

			std::vector<FrictionCell> oldCells = std::move(m_frictionCells);
			m_frictionCells.assign(newCapacity, FrictionCell{});
			m_occupiedFrictionCells.clear();

			for (FrictionCell& oldCell : oldCells)
			{
				if (oldCell.stamp != m_frictionCellStamp)
					continue;

				size_t index =
					(static_cast<uint32_t>(oldCell.cx) * 73856093u ^ static_cast<uint32_t>(oldCell.cy) * 19349663u) &
					mask;
				while (m_frictionCells[index].stamp == m_frictionCellStamp)
				{
					index = (index + 1) & mask;
				}
				m_frictionCells[index] = oldCell;
				m_occupiedFrictionCells.push_back(static_cast<uint32_t>(index));
			}
		}

		void PhysicsAudioEngine::clearFrictionVoices()
		{
			for (auto& voice : m_frictionVoices)
			{
				voice = FrictionVoice{};
			}
			m_frictionFrame = 0;
			m_smoothedActiveFrictionVoices = 0.0f;
			m_lastFrictionStealCount = 0;
		}

		void PhysicsAudioEngine::setFrictionSources(std::span<const FrictionSource> sources, const vec3& listenerPos,
													const vec3& listenerForward, const vec3& listenerUp)
		{
			++m_frictionFrame;
			if (m_frictionFrame == 0) // wrapped: invalidate stale voice stamps
			{
				for (auto& voice : m_frictionVoices)
				{
					voice.lastSeenFrame = 0;
				}
				m_frictionFrame = 1;
			}

			const bool spatialEnabled = m_spatialEnabled.load(std::memory_order_acquire);
			const size_t maxVoices = m_maxFrictionVoices;
			m_lastFrictionStealCount = 0;

			// 1. Bin the per-body sources into coarse spatial cells. Power sums
			//    let many small contacts in a pile add up, while the weighted
			//    centroid keeps the cell where the friction actually is. Sources
			//    inside the blend band next to a cell edge splat into the
			//    neighboring cells (bilinear), so a body crossing a boundary
			//    hands its energy over continuously instead of in one step.
			++m_frictionCellStamp;
			if (m_frictionCellStamp == 0) // wrapped: drop every cell
			{
				m_frictionCells.clear();
				m_frictionCellStamp = 1;
			}
			m_occupiedFrictionCells.clear();

			constexpr float BLEND_BAND = FRICTION_CELL_BLEND_BAND;
			const float invCellSize = 1.0f / m_frictionCellSize;
			for (const FrictionSource& source : sources)
			{
				if (source.level <= 0.0f)
					continue;

				const float cellXf = source.position.x * invCellSize;
				const float cellYf = source.position.y * invCellSize;
				const int32_t baseX = static_cast<int32_t>(std::floor(cellXf));
				const int32_t baseY = static_cast<int32_t>(std::floor(cellYf));
				const float fx = cellXf - static_cast<float>(baseX);
				const float fy = cellYf - static_cast<float>(baseY);

				float xWeights[3] = {0.0f, 0.0f, 0.0f}; // dx = -1, 0, +1
				float yWeights[3] = {0.0f, 0.0f, 0.0f}; // dy = -1, 0, +1
				if (fx < BLEND_BAND)
				{
					xWeights[0] = 0.5f * (1.0f - fx / BLEND_BAND);
				}
				else if (fx > 1.0f - BLEND_BAND)
				{
					xWeights[2] = 0.5f * (1.0f - (1.0f - fx) / BLEND_BAND);
				}
				xWeights[1] = 1.0f - xWeights[0] - xWeights[2];
				if (fy < BLEND_BAND)
				{
					yWeights[0] = 0.5f * (1.0f - fy / BLEND_BAND);
				}
				else if (fy > 1.0f - BLEND_BAND)
				{
					yWeights[2] = 0.5f * (1.0f - (1.0f - fy) / BLEND_BAND);
				}
				yWeights[1] = 1.0f - yWeights[0] - yWeights[2];

				const float levelSq = source.level * source.level;
				for (int32_t dx = -1; dx <= 1; ++dx)
				{
					const float wx = xWeights[dx + 1];
					if (wx <= 0.0f)
						continue;
					for (int32_t dy = -1; dy <= 1; ++dy)
					{
						const float weight = wx * yWeights[dy + 1];
						if (weight <= 0.0f)
							continue;

						FrictionCell& cell = m_frictionCells[findOrInsertFrictionCell(baseX + dx, baseY + dy)];
						cell.powerSum += levelSq * weight;
						cell.weightedPosition += source.position * (source.level * weight);
						cell.weightSum += source.level * weight;
					}
				}
			}
			m_occupiedFrictionCellCount = m_occupiedFrictionCells.size();

			// 2. Pick the top cells by audibility. Ranking uses a sqrt-free
			//    proximity proxy; the exact spatial math runs only for winners.
			//    Cells that already own a voice get a bonus so cells near the
			//    selection cutoff do not flicker in and out every frame.
			constexpr float ACTIVE_CELL_RANK_BONUS = 1.15f;
			struct Candidate
			{
				int32_t cx;
				int32_t cy;
				vec3 position;
				float level;
				float rankProxy;
			};
			std::array<Candidate, MAX_FRICTION_VOICES> selected;
			size_t selectedCount = 0;

			for (const uint32_t cellIndex : m_occupiedFrictionCells)
			{
				const FrictionCell& cell = m_frictionCells[cellIndex];
				if (cell.weightSum <= 0.0f)
					continue;

				const float level = std::sqrt(cell.powerSum);
				const vec3 position = cell.weightedPosition / cell.weightSum;

				float proxy = level;
				if (spatialEnabled)
				{
					vec3 toSource = position - listenerPos;
					toSource.z = 0.0f; // planar physics space
					proxy *= SpatialAudioProcessor::distanceProxy(glm::length2(toSource));
				}

				bool hasActiveVoice = false;
				for (const FrictionVoice& voice : m_frictionVoices)
				{
					if (voice.active && voice.cellX == cell.cx && voice.cellY == cell.cy)
					{
						hasActiveVoice = true;
						break;
					}
				}
				const float rankProxy = hasActiveVoice ? proxy * ACTIVE_CELL_RANK_BONUS : proxy;

				if (selectedCount < maxVoices)
				{
					size_t i = selectedCount++;
					while (i > 0 && selected[i - 1].rankProxy < rankProxy)
					{
						selected[i] = selected[i - 1];
						--i;
					}
					selected[i] = {cell.cx, cell.cy, position, level, rankProxy};
				}
				else if (rankProxy > selected[selectedCount - 1].rankProxy)
				{
					size_t i = selectedCount - 1;
					while (i > 0 && selected[i - 1].rankProxy < rankProxy)
					{
						selected[i] = selected[i - 1];
						--i;
					}
					selected[i] = {cell.cx, cell.cy, position, level, rankProxy};
				}
			}

			// 3. Match selected cells to voices; voice identity is the cell key.
			//    Voices that lose their cell fade out through the release
			//    headroom instead of being cut. A hard reset only happens when
			//    all slots are busy: the quietest voice not claimed this frame is
			//    taken (keeping its filter state), so live candidates are safe.
			for (size_t s = 0; s < selectedCount; ++s)
			{
				const Candidate& candidate = selected[s];

				FrictionVoice* voice = nullptr;
				for (FrictionVoice& v : m_frictionVoices)
				{
					if (v.active && v.cellX == candidate.cx && v.cellY == candidate.cy)
					{
						voice = &v;
						break;
					}
				}
				if (!voice)
				{
					for (FrictionVoice& v : m_frictionVoices)
					{
						if (!v.active)
						{
							voice = &v;
							*voice = FrictionVoice{};
							break;
						}
					}
				}
				if (!voice)
				{
					size_t victim = MAX_FRICTION_VOICES;
					float quietestGain = 0.0f;
					for (size_t i = 0; i < MAX_FRICTION_VOICES; ++i)
					{
						const FrictionVoice& v = m_frictionVoices[i];
						if (!v.active)
							continue;
						// Never steal voices already claimed by a candidate in this frame
						if (v.lastSeenFrame == m_frictionFrame)
							continue;
						const float gain = v.render.smoothedGain;
						if (victim == MAX_FRICTION_VOICES || gain < quietestGain)
						{
							victim = i;
							quietestGain = gain;
						}
					}

					if (victim == MAX_FRICTION_VOICES)
					{
						continue; // all voices are busy with candidates from this frame
					}

					voice = &m_frictionVoices[victim];
					float filterState = voice->render.filterState;
					*voice = FrictionVoice{};
					voice->render.filterState = filterState;
					++m_lastFrictionStealCount;
				}

				SpatialAudioResult spatial =
					SpatialAudioProcessor::process(candidate.position, listenerPos, listenerForward, listenerUp,
												   AudioSpace::TwoDimensional, spatialEnabled);
				voice->spatial.distanceGain = spatial.distanceGain;
				voice->spatial.leftGain = spatial.leftGain;
				voice->spatial.rightGain = spatial.rightGain;
				voice->spatial.filterCutoff = spatial.filterCutoff;
				voice->cellX = candidate.cx;
				voice->cellY = candidate.cy;
				voice->position = candidate.position;
				voice->targetLevel = mapFrictionLevel(candidate.level);
				voice->active = true;
				voice->lastSeenFrame = m_frictionFrame;
				voice->silentSeconds = 0.0f;
			}

			// 4. Unseen voices release out.
			for (FrictionVoice& voice : m_frictionVoices)
			{
				if (voice.active && voice.lastSeenFrame != m_frictionFrame)
				{
					voice.targetLevel = 0.0f;
				}
			}
		}

		void PhysicsAudioEngine::setFrictionCellSize(float size)
		{
			const float clamped = std::clamp(size, MIN_FRICTION_CELL_SIZE, MAX_FRICTION_CELL_SIZE);
			if (clamped == m_frictionCellSize)
				return;
			m_frictionCellSize = clamped;
			clearFrictionVoices(); // cell keys changed; start the voices cleanly
		}

		void PhysicsAudioEngine::setFrictionLevel(float level, const vec3& sourcePos, const vec3& listenerPos,
												  const vec3& listenerForward, const vec3& listenerUp)
		{
			const FrictionSource source{0u, sourcePos, level};
			setFrictionSources(std::span<const FrictionSource>(&source, 1), listenerPos, listenerForward, listenerUp);
		}

		float PhysicsAudioEngine::getFrictionLevel() const
		{
			float maxLevel = 0.0f;
			for (const FrictionVoice& voice : m_frictionVoices)
			{
				maxLevel = (std::max)(maxLevel, voice.targetLevel);
			}
			return maxLevel;
		}

		void PhysicsAudioEngine::setMaxFrictionVoices(size_t count)
		{
			// Only the number of cells selected per frame is capped; voices
			// beyond the cap keep fading out naturally in the release headroom.
			m_maxFrictionVoices = std::clamp<size_t>(count, 1, MAX_FRICTION_VOICES);
		}

		size_t PhysicsAudioEngine::getActiveFrictionVoiceCount() const
		{
			size_t count = 0;
			for (const FrictionVoice& voice : m_frictionVoices)
			{
				if (voice.active && voice.targetLevel > 0.0f)
				{
					++count;
				}
			}
			return count;
		}

		size_t PhysicsAudioEngine::getReleasingFrictionVoiceCount() const
		{
			size_t count = 0;
			for (const FrictionVoice& voice : m_frictionVoices)
			{
				if (voice.active && voice.targetLevel <= 0.0f)
				{
					++count;
				}
			}
			return count;
		}

		void PhysicsAudioEngine::playSound(const SimpleAudioRequest& request, const vec3& listenerPos,
										   const vec3& listenerForward, const vec3& listenerUp)
		{
			float vol = request.volume * m_volume;
			if (vol <= 0.001f)
				return;

			float leftGain = 0.7071f;
			float rightGain = 0.7071f;
			float filterCutoff = request.frequency > 0.0f ? request.frequency : 20000.0f;

			if (request.spatial)
			{
				bool spatialEnabled = m_spatialEnabled.load(std::memory_order_acquire);
				SpatialAudioResult spatial = SpatialAudioProcessor::process(
					request.position, listenerPos, listenerForward, listenerUp, request.space, spatialEnabled);

				vol *= spatial.distanceGain;
				leftGain = spatial.leftGain;
				rightGain = spatial.rightGain;
				filterCutoff = (std::min)(filterCutoff, spatial.filterCutoff);
			}

			if (vol <= 0.001f)
				return;

			float decay = 0.12f * static_cast<float>((std::max)(1, request.beats));
			if (request.intensity > 0.0f && request.instrument == 3)
			{
				decay = 0.03f + 0.08f * std::clamp(request.intensity, 0.0f, 1.0f);
			}

			playVoice(request.frequency > 0.0f ? request.frequency : 180.0f, (std::min)(1.0f, vol), decay,
					  request.instrument, leftGain, rightGain, filterCutoff);
		}

		void PhysicsAudioEngine::playVoice(float freq, float amp, float decaySec, int instrument, float leftGain,
										   float rightGain, float filterCutoff)
		{
			if (!m_voicesEnabled.load(std::memory_order_acquire))
				return;

			if (amp <= 0.001f)
				return;

			PhysicsVoice newVoice;
			newVoice.frequency = freq;
			newVoice.amplitude = amp;
			newVoice.decay = (std::max)(0.01f, decaySec);
			newVoice.time = 0.0f;
			newVoice.phase = 0.0f;
			newVoice.finished = false;
			newVoice.instrument = instrument;
			newVoice.leftGain = leftGain;
			newVoice.rightGain = rightGain;
			newVoice.filterCutoff = filterCutoff;
			newVoice.filterState = 0.0f;

			if (m_activeVoices.size() >= MAX_PHYSICS_VOICES)
			{
				if (m_activeVoices.size() >= MAX_PHYSICS_BURST_VOICES)
					return; // burst headroom exhausted

				// Find the quietest voice that is neither finished nor already
				// releasing; those are safe to soft-evict.
				size_t victimIdx = m_activeVoices.size();
				float minAmp = 1000.0f;
				for (size_t i = 0; i < m_activeVoices.size(); ++i)
				{
					if (m_activeVoices[i].finished || m_activeVoices[i].releasing)
						continue;
					float curAmp =
						m_activeVoices[i].amplitude * expf(-m_activeVoices[i].time / m_activeVoices[i].decay);
					if (victimIdx == m_activeVoices.size() || curAmp < minAmp)
					{
						minAmp = curAmp;
						victimIdx = i;
					}
				}

				if (victimIdx != m_activeVoices.size())
				{
					if (newVoice.amplitude <= minAmp * 0.4f)
						return; // not loud enough to displace an audible voice

					// Soft de-click steal: let the victim fade out rapidly over 5ms
					// instead of hard-cutting its waveform mid-cycle.
					m_activeVoices[victimIdx].releasing = true;
					m_activeVoices[victimIdx].releaseRemaining = PhysicsVoice::STEAL_RELEASE_SECONDS;
				}

				// Either a victim was soft-evicted, or every voice is already
				// releasing/finished: use the burst headroom.
				m_activeVoices.push_back(newVoice);
				return;
			}

			m_activeVoices.push_back(newVoice);
		}

		void PhysicsAudioEngine::render(float* buffer, uint32_t frameCount, uint32_t channels)
		{
			if (frameCount == 0 || channels != 2)
				return;

			// 1. Render continuous friction voices. Per-voice level smoothing has
			//    a soft attack and slow release to bridge contact gaps; the mix is
			//    normalized by 1/sqrt(active voices) so the sum stays consistent.
			const float dt = static_cast<float>(frameCount) / static_cast<float>(m_sampleRate);

			size_t activeFrictionVoices = 0;
			for (const FrictionVoice& voice : m_frictionVoices)
			{
				if (voice.active && (voice.smoothedLevel > 0.0001f || voice.render.smoothedGain > 0.0001f))
				{
					++activeFrictionVoices;
				}
			}
			const float countCoef = 1.0f - expf(-dt / 0.100f);
			m_smoothedActiveFrictionVoices +=
				countCoef * (static_cast<float>(activeFrictionVoices) - m_smoothedActiveFrictionVoices);
			const float mixScale = 1.0f / std::sqrt((std::max)(1.0f, m_smoothedActiveFrictionVoices));

			for (FrictionVoice& voice : m_frictionVoices)
			{
				if (!voice.active)
					continue;
				renderFrictionVoice(voice, mixScale, dt, buffer, frameCount);
			}

			// 2. Render active physics voices. Normalize mix by 1/sqrt(active voices)
			//    so dense collisions in physics piles do not overdrive the master bus
			//    into hard tanh clipping distortion.
			size_t activeCollisionVoices = 0;
			for (const auto& voice : m_activeVoices)
			{
				if (!voice.finished)
				{
					++activeCollisionVoices;
				}
			}
			const float colCountCoef = 1.0f - expf(-dt / 0.060f);
			m_smoothedActiveCollisionVoices +=
				colCountCoef * (static_cast<float>(activeCollisionVoices) - m_smoothedActiveCollisionVoices);
			const float targetMixScale = 1.0f / std::sqrt((std::max)(1.0f, m_smoothedActiveCollisionVoices));
			const float startMixScale = m_lastCollisionMixScale;
			m_lastCollisionMixScale = targetMixScale;

			const float attackTime = 0.006f;
			const float sampleRateF = static_cast<float>(m_sampleRate);

			for (auto& voice : m_activeVoices)
			{
				if (voice.finished)
					continue;

				const float phaseInc = 2.0f * static_cast<float>(M_PI) * voice.frequency / sampleRateF;
				const float wc =
					2.0f * static_cast<float>(M_PI) * std::clamp(voice.filterCutoff, 20.0f, 20000.0f) / sampleRateF;
				const float filterAlpha = std::clamp(wc / (wc + 1.0f), 0.001f, 1.0f);

				for (uint32_t i = 0; i < frameCount; ++i)
				{
					float env = voice.amplitude * expf(-voice.time / voice.decay);
					if (voice.time < attackTime)
					{
						env *= (voice.time / attackTime);
					}
					if (voice.releasing)
					{
						env *= std::clamp(voice.releaseRemaining / PhysicsVoice::STEAL_RELEASE_SECONDS, 0.0f, 1.0f);
						voice.releaseRemaining -= 1.0f / sampleRateF;
						if (voice.releaseRemaining <= 0.0f)
						{
							voice.finished = true;
						}
					}

					float rawSample = 0.0f;
					switch (voice.instrument)
					{
						case 1: // Sawtooth
							rawSample = 1.0f - (voice.phase / static_cast<float>(M_PI));
							break;
						case 2: // Square
							rawSample = (voice.phase < static_cast<float>(M_PI)) ? 0.6f : -0.6f;
							break;
						case 4: // Modal Body Resonance (fundamental + 2nd & 3rd overtone)
							rawSample = 0.68f * sinf(voice.phase) + 0.22f * sinf(voice.phase * 2.0f) +
										0.10f * sinf(voice.phase * 3.0f);
							break;
						case 0: // Sine
							rawSample = sinf(voice.phase);
							break;
						case 3: // Filtered Noise (default for impact/foley)
						default:
							rawSample = generateNoiseSample();
							break;
					}

					// 1-Pole Lowpass Filter
					voice.filterState += filterAlpha * (rawSample - voice.filterState);
					float sample = env * voice.filterState;

					voice.phase += phaseInc;
					if (voice.phase >= 2.0f * static_cast<float>(M_PI))
					{
						voice.phase -= 2.0f * static_cast<float>(M_PI);
					}
					voice.time += 1.0f / sampleRateF;

					const float t = static_cast<float>(i) / static_cast<float>(frameCount);
					const float curMixScale = startMixScale + t * (targetMixScale - startMixScale);

					buffer[i * 2 + 0] += sample * voice.leftGain * curMixScale;
					buffer[i * 2 + 1] += sample * voice.rightGain * curMixScale;
				}

				if (voice.time > attackTime && voice.amplitude * expf(-voice.time / voice.decay) < 0.001f)
				{
					voice.finished = true;
				}
			}

			// Clean up finished voices
			m_activeVoices.erase(std::remove_if(m_activeVoices.begin(), m_activeVoices.end(),
												[](const PhysicsVoice& v) { return v.finished; }),
								 m_activeVoices.end());
		}

		void PhysicsAudioEngine::renderFrictionVoice(FrictionVoice& voice, float mixScale, float dt, float* buffer,
													 uint32_t frameCount)
		{
			// Per-voice level smoothing: soft attack, slow release to bridge the
			// frames where a body briefly has no contact event.
			const float attackCoef = 1.0f - expf(-dt / 0.035f);	 // ~35 ms attack
			const float releaseCoef = 1.0f - expf(-dt / 0.180f); // ~180 ms release
			if (voice.targetLevel > voice.smoothedLevel)
			{
				voice.smoothedLevel += (voice.targetLevel - voice.smoothedLevel) * attackCoef;
			}
			else
			{
				voice.smoothedLevel += (voice.targetLevel - voice.smoothedLevel) * releaseCoef;
			}

			const float levelGain = (std::min)(1.0f, voice.smoothedLevel * 1.10f) * m_volume * mixScale;
			const FrictionSpatialState& spatial = voice.spatial;
			FrictionVoiceState& state = voice.render;

			if (levelGain * spatial.distanceGain <= 0.0001f && state.smoothedGain <= 0.0001f)
			{
				state.smoothedGain = 0.0f;
			}
			else
			{
				const float sampleRateF = static_cast<float>(m_sampleRate);
				const float distCoef = 1.0f - expf(-1.0f / (0.120f * sampleRateF));	  // ~120 ms
				const float panCoef = 1.0f - expf(-1.0f / (0.050f * sampleRateF));	  // ~50 ms
				const float cutoffCoef = 1.0f - expf(-1.0f / (0.080f * sampleRateF)); // ~80 ms

				// Snap spatial state when the voice restarts from silence so it
				// does not sweep in from a stale source position.
				if (state.smoothedGain <= 0.0001f)
				{
					state.smoothedDistanceGain = spatial.distanceGain;
					state.smoothedLeftGain = spatial.leftGain;
					state.smoothedRightGain = spatial.rightGain;
					state.smoothedFilterCutoff = spatial.filterCutoff;
				}

				const float startGain = state.smoothedGain;

				for (uint32_t i = 0; i < frameCount; ++i)
				{
					state.smoothedDistanceGain += distCoef * (spatial.distanceGain - state.smoothedDistanceGain);
					state.smoothedLeftGain += panCoef * (spatial.leftGain - state.smoothedLeftGain);
					state.smoothedRightGain += panCoef * (spatial.rightGain - state.smoothedRightGain);
					state.smoothedFilterCutoff += cutoffCoef * (spatial.filterCutoff - state.smoothedFilterCutoff);

					const float targetGain = levelGain * state.smoothedDistanceGain;
					const float t = static_cast<float>(i) / static_cast<float>(frameCount);
					const float currentGain = startGain + t * (targetGain - startGain);

					const float wc = 2.0f * static_cast<float>(M_PI) *
									 std::clamp(state.smoothedFilterCutoff, 20.0f, 20000.0f) / sampleRateF;
					const float filterAlpha = std::clamp(wc / (wc + 1.0f), 0.001f, 1.0f);

					state.filterState += filterAlpha * (generateNoiseSample() - state.filterState);
					const float sample = state.filterState * currentGain;

					buffer[i * 2 + 0] += sample * state.smoothedLeftGain;
					buffer[i * 2 + 1] += sample * state.smoothedRightGain;
				}

				state.smoothedGain = levelGain * state.smoothedDistanceGain;
				voice.silentSeconds = 0.0f;
			}

			// Recycle the voice once it has been silent for a while.
			if (voice.targetLevel <= 0.0f && state.smoothedGain <= 0.0005f)
			{
				voice.silentSeconds += dt;
				if (voice.silentSeconds > 0.5f)
				{
					voice = FrictionVoice{};
				}
			}
		}
	} // namespace WeirdAudio
} // namespace WeirdEngine
