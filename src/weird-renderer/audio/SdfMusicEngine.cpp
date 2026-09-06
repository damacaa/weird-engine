#include "weird-renderer/audio/SdfMusicEngine.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		namespace
		{
			constexpr int NUM_DOMAIN_SAMPLES = 32;
			constexpr float DOMAIN_RADIUS = 50.0f;

			bool isSongEmpty(const std::shared_ptr<SdfSong>& song)
			{
				if (!song)
					return true;
				return song->getShapeExpression() == nullptr;
			}

			float stepHash(int step, int salt)
			{
				uint32_t h = static_cast<uint32_t>(step) * 2654435761u;
				h ^= static_cast<uint32_t>(salt) * 1013904223u;
				h = (h ^ (h >> 13)) * 1274126177u;
				h = (h ^ (h >> 16));
				return static_cast<float>(h & 0xFFFF) / 65535.0f;
			}

			float distToParam(float dist, float sharpness = 0.18f)
			{
				// dist < 0 (inside) -> v near 1.0
				// dist = 0 (boundary) -> v = 0.5
				// dist > 0 (outside) -> v near 0.0
				// sharpness is 0.18f (scaled for 10x larger domain distances)
				float v = 1.0f / (1.0f + std::exp(dist * sharpness));
				return std::clamp(v, 0.0f, 1.0f);
			}

			std::vector<glm::vec2> createDomainSamples()
			{
				std::vector<glm::vec2> pts;
				pts.reserve(NUM_DOMAIN_SAMPLES);
				constexpr float goldenAngle = 2.39996323f;
				for (int i = 0; i < NUM_DOMAIN_SAMPLES; ++i)
				{
					float r = DOMAIN_RADIUS *
							  std::sqrt((static_cast<float>(i) + 0.5f) / static_cast<float>(NUM_DOMAIN_SAMPLES));
					float theta = static_cast<float>(i) * goldenAngle;
					pts.push_back({r * std::cos(theta), r * std::sin(theta)});
				}
				return pts;
			}
		} // namespace

		SdfMusicEngine::SdfMusicEngine()
		{
			m_activeVoices.reserve(MAX_MUSIC_VOICES);
		}

		SdfMusicEngine::~SdfMusicEngine() {}

		void SdfMusicEngine::init(uint32_t sampleRate, uint32_t channels)
		{
			m_sampleRate = sampleRate;
			m_channels = channels;
			m_currentBeat = 0.0f;
			m_stepAccumulator = 0.0f;
			m_currentStep = 0;
			m_activeVoices.clear();
			m_concussionFilterCutoff = 20000.0f;
			m_deathSlowdown = 1.0f;
			m_isDead = false;
			m_melodyDegree = 0;
			m_shapeParams = ShapeMusicalParams{};
			m_motionLevel = 0.0f;
			m_fillRatio = 0.5f;
			m_tempoFromMotion = 1.0f;
			m_volumeFromFill = 0.65f;

			m_currentSong = nullptr;
			m_queuedSong = nullptr;
		}

		void SdfMusicEngine::sampleShapeParameters()
		{
			if (!m_currentSong || !m_currentSong->getShapeExpression())
			{
				m_shapeParams = ShapeMusicalParams{};
				return;
			}

			auto shape = m_currentSong->getShapeExpression();
			float r = m_currentSong->getSampleRadius();
			if (r <= 0.01f)
			{
				r = 2.0f;
			}

			glm::vec2 center = m_currentSong->getCenter();
			float k = 0.70710678f;

			float params[11];
			for (size_t i = 0; i < 8; ++i)
			{
				params[i] = m_currentSong->getParameter(i);
			}
			params[8] = 0.0f; // Static time for parameter sampling

			auto evalAt = [&](float px, float py) -> float
			{
				params[9] = center.x + px;
				params[10] = center.y + py;
				return shape->getValue(params);
			};

			float dCenter = evalAt(0.0f, 0.0f);
			float dEast = evalAt(r, 0.0f);
			float dNorth = evalAt(0.0f, r);
			float dSouth = evalAt(0.0f, -r);
			float dNE = evalAt(r * k, r * k);
			float dNW = evalAt(-r * k, r * k);
			float dSE = evalAt(r * k, -r * k);
			float dSW = evalAt(-r * k, -r * k);

			float p0 = distToParam(dCenter, 1.8f);
			float p1 = distToParam(dEast, 1.8f);
			float p2 = distToParam(dNorth, 1.8f);
			float p3 = distToParam(dSouth, 1.8f);
			float p4 = distToParam(dNE, 1.8f);
			float p5 = distToParam(dNW, 1.8f);
			float p6 = distToParam(dSE, 1.8f);
			float p7 = distToParam(dSW, 1.8f);

			m_shapeParams.tempoFactor = 0.75f + 0.50f * p0; // [0.75, 1.25]
			m_shapeParams.melodyDensity = p1;
			m_shapeParams.harmonyRichness = p2;
			m_shapeParams.bassWeight = p3;
			m_shapeParams.percEnergy = p4;
			m_shapeParams.brightness = p5;
			m_shapeParams.syncopation = p6;
			m_shapeParams.variation = p7;
		}

		void SdfMusicEngine::sampleDomainMotionAndFill(double sceneTime)
		{
			if (!m_currentSong || !m_currentSong->getShapeExpression())
				return;

			auto shape = m_currentSong->getShapeExpression();
			glm::vec2 center = m_currentSong->getCenter();

			static const auto sampleOffsets = createDomainSamples();

			float paramsT0[11];
			float paramsT1[11];
			for (size_t i = 0; i < 8; ++i)
			{
				paramsT0[i] = m_currentSong->getParameter(i);
				paramsT1[i] = paramsT0[i];
			}
			constexpr float dtSample = 0.08f;
			paramsT0[8] = static_cast<float>(sceneTime);
			paramsT1[8] = static_cast<float>(sceneTime + dtSample);

			float totalMotion = 0.0f;
			int insideCount = 0;

			for (const auto& offset : sampleOffsets)
			{
				float px = center.x + offset.x;
				float py = center.y + offset.y;

				paramsT0[9] = px;
				paramsT0[10] = py;
				float d0 = shape->getValue(paramsT0);

				paramsT1[9] = px;
				paramsT1[10] = py;
				float d1 = shape->getValue(paramsT1);

				if (d0 < 0.0f)
				{
					insideCount++;
				}

				float deltaDist = std::abs(d1 - d0);
				totalMotion += (deltaDist / dtSample);
			}

			float avgMotion = totalMotion / static_cast<float>(NUM_DOMAIN_SAMPLES);
			float currentFill = static_cast<float>(insideCount) / static_cast<float>(NUM_DOMAIN_SAMPLES);

			// Smooth with exponential moving average
			m_motionLevel += (avgMotion - m_motionLevel) * 0.25f;
			m_fillRatio += (currentFill - m_fillRatio) * 0.25f;

			// Motion controls overall tempo:
			// If motion == 0 (completely still): tempo drops to 0.38x (very slow!)
			// If motion is high (moving/spinning): tempo goes up to 1.60x (high tempo!)
			float motionNorm = std::clamp(m_motionLevel / 25.0f, 0.0f, 1.0f);
			m_tempoFromMotion = 0.38f + 1.22f * std::pow(motionNorm, 0.75f);

			// Fill ratio controls volume:
			// Bigger shape -> higher fill ratio -> louder volume
			// Smaller shape -> lower fill ratio -> quieter volume (with audible minimum floor)
			m_volumeFromFill = std::clamp(0.40f + 0.60f * std::sqrt(m_fillRatio), 0.35f, 1.0f);
		}

		void SdfMusicEngine::resampleShape()
		{
			std::lock_guard<std::mutex> lock(m_songMutex);
			sampleShapeParameters();
			sampleDomainMotionAndFill(m_sceneTime);
		}

		void SdfMusicEngine::setSong(std::shared_ptr<SdfSong> song, bool beatSynced)
		{
			std::lock_guard<std::mutex> lock(m_songMutex);
			if (!m_currentSong || !beatSynced)
			{
				m_currentSong = std::move(song);
				m_queuedSong = nullptr;
				m_currentStep = 0;
				m_stepAccumulator = 0.0f;
				m_currentBeat = 0.0f;
				m_melodyDegree = 0;
				sampleShapeParameters();
				sampleDomainMotionAndFill(m_sceneTime);

				if (isSongEmpty(m_currentSong))
				{
					m_activeVoices.clear();
				}
			}
			else
			{
				m_queuedSong = std::move(song);
			}
		}

		std::shared_ptr<SdfSong> SdfMusicEngine::getCurrentSong() const
		{
			return m_currentSong;
		}

		void SdfMusicEngine::queueSong(std::shared_ptr<SdfSong> nextSong)
		{
			std::lock_guard<std::mutex> lock(m_songMutex);
			m_queuedSong = std::move(nextSong);
		}

		void SdfMusicEngine::triggerPositiveFeedback(float intensity)
		{
			float clamped = std::clamp(intensity, 0.2f, 2.0f);
			m_positiveTimer = 1.2f * clamped;
			m_positivePitchOffset = 7.0f; // Modulate up a perfect fifth

			if (m_currentSong)
			{
				int root = m_currentSong->getRootMidi();
				float baseTempo = m_currentSong->getTempo() * m_shapeParams.tempoFactor * m_tempoFromMotion;
				float beatDuration = 60.0f / (std::max)(30.0f, baseTempo);

				playNote(m_currentSong->midiToFrequency(root + 12), 0.40f * clamped, beatDuration * 2.5f, 4, -0.4f,
						 7500.0f);
				playNote(m_currentSong->midiToFrequency(root + 16), 0.35f * clamped, beatDuration * 3.0f, 4, 0.0f,
						 8500.0f);
				playNote(m_currentSong->midiToFrequency(root + 19), 0.38f * clamped, beatDuration * 3.5f, 4, 0.4f,
						 9500.0f);
				playNote(m_currentSong->midiToFrequency(root + 24), 0.30f * clamped, beatDuration * 4.0f, 0, 0.0f,
						 11000.0f);
			}
		}

		void SdfMusicEngine::triggerNegativeFeedback(float intensity)
		{
			float clamped = std::clamp(intensity, 0.2f, 2.0f);
			m_negativeTimer = 1.0f * clamped;
			m_concussionFilterCutoff = 280.0f; // Concussive lowpass muffle (underwater effect)
			m_detuneAmount = 0.04f * clamped;

			duck(0.5f * clamped);

			if (m_currentSong)
			{
				int root = m_currentSong->getRootMidi();
				playNote(m_currentSong->midiToFrequency(root - 24), 0.55f * clamped, 0.6f, 1, 0.0f, 350.0f);
			}
		}

		void SdfMusicEngine::triggerDeath()
		{
			m_isDead = true;
			m_deathSlowdown = 1.0f;
			m_concussionFilterCutoff = 350.0f;

			if (m_currentSong)
			{
				int root = m_currentSong->getRootMidi();
				playNote(m_currentSong->midiToFrequency(root - 24), 0.65f, 4.0f, 2, 0.0f, 300.0f);
				playNote(m_currentSong->midiToFrequency(root - 18), 0.45f, 3.5f, 1, 0.0f, 400.0f); // Dissonant tritone
			}
		}

		void SdfMusicEngine::setTension(float level)
		{
			m_tension = std::clamp(level, 0.0f, 1.0f);
		}

		void SdfMusicEngine::setEnergy(float level)
		{
			m_energy = std::clamp(level, 0.0f, 1.0f);
		}

		void SdfMusicEngine::setHealth(float current, float max)
		{
			if (max > 0.001f)
			{
				m_healthRatio = std::clamp(current / max, 0.0f, 1.0f);
			}
		}

		void SdfMusicEngine::duck(float amount)
		{
			m_ducking = (std::min)(1.0f, m_ducking + amount);
		}

		void SdfMusicEngine::surge(float amount)
		{
			m_ducking = (std::max)(0.0f, m_ducking - amount * 0.5f);
		}

		float SdfMusicEngine::quantizeToSongScale(float rawFreq) const
		{
			if (!m_currentSong || rawFreq <= 20.0f)
				return rawFreq > 20.0f ? rawFreq : 440.0f;

			float midiNote = 69.0f + 12.0f * std::log2(rawFreq / 440.0f);
			int roundedMidi = static_cast<int>(std::round(midiNote));

			int minDiff = 100;
			int bestMidi = roundedMidi;
			for (int d = -10; d <= 20; ++d)
			{
				int candidateMidi = m_currentSong->getScaleDegreeMidi(d);
				int diff = std::abs(candidateMidi - roundedMidi);
				if (diff < minDiff)
				{
					minDiff = diff;
					bestMidi = candidateMidi;
				}
			}
			return m_currentSong->midiToFrequency(bestMidi);
		}

		void SdfMusicEngine::update(double deltaTime, double sceneTime)
		{
			if (!m_playing || !m_currentSong)
				return;

			float dt = static_cast<float>(deltaTime);
			m_sceneTime = sceneTime;

			// Sample domain motion & fill ratio dynamically
			sampleDomainMotionAndFill(sceneTime);

			// 1. Recover dynamic feedback parameters smoothly
			if (m_positiveTimer > 0.0f)
			{
				m_positiveTimer -= dt;
				if (m_positiveTimer <= 0.0f)
				{
					m_positivePitchOffset = 0.0f;
				}
			}

			if (m_concussionFilterCutoff < 20000.0f)
			{
				m_concussionFilterCutoff += (20000.0f - m_concussionFilterCutoff) * (std::min)(1.0f, dt * 2.5f);
			}

			if (m_detuneAmount > 0.0f)
			{
				m_detuneAmount = (std::max)(0.0f, m_detuneAmount - dt * 0.2f);
			}

			if (m_ducking > 0.0f)
			{
				m_ducking = (std::max)(0.0f, m_ducking - dt * 4.0f);
			}

			if (m_isDead)
			{
				m_deathSlowdown = (std::max)(0.0f, m_deathSlowdown - dt * 0.5f);
			}
			else
			{
				m_deathSlowdown = 1.0f;
			}

			// 2. Tempo calculations based on song base tempo, motion-derived tempo, shape tempo factor, and energy
			float baseTempo = m_currentSong->getTempo() * m_shapeParams.tempoFactor * m_tempoFromMotion;
			float dynamicTempo = baseTempo * (0.85f + 0.30f * m_energy) * m_deathSlowdown;
			if (dynamicTempo < 5.0f)
				return;

			// 16th note step duration (4 steps per beat)
			float stepDuration = (60.0f / dynamicTempo) / 4.0f;
			m_stepAccumulator += dt;

			while (m_stepAccumulator >= stepDuration)
			{
				m_stepAccumulator -= stepDuration;

				// Check for beat-synced song transition on downbeats (every 4 steps = 1 beat)
				if ((m_currentStep % 4) == 0 && m_queuedSong)
				{
					std::lock_guard<std::mutex> lock(m_songMutex);
					if (m_queuedSong)
					{
						m_currentSong = std::move(m_queuedSong);
						m_queuedSong = nullptr;
						m_melodyDegree = 0;
						sampleShapeParameters();
						sampleDomainMotionAndFill(m_sceneTime);

						if (isSongEmpty(m_currentSong))
						{
							m_activeVoices.clear();
						}
						else
						{
							int root = m_currentSong->getRootMidi();
							playNote(m_currentSong->midiToFrequency(root), 0.45f, 1.2f, 4, 0.0f, 5500.0f);
						}
					}
				}

				on16thStep(m_currentStep);
				m_currentStep = (m_currentStep + 1) % 256;
				m_currentBeat = static_cast<float>(m_currentStep) / 4.0f;
			}
		}

		void SdfMusicEngine::on16thStep(int step)
		{
			if (isSongEmpty(m_currentSong))
				return;

			evaluateShapeDrivenAtStep(step);
		}

		void SdfMusicEngine::evaluateShapeDrivenAtStep(int step)
		{
			float baseTempo = m_currentSong->getTempo() * m_shapeParams.tempoFactor * m_tempoFromMotion;
			float dynamicTempo = baseTempo * (0.85f + 0.30f * m_energy) * m_deathSlowdown;
			float beatSec = 60.0f / (std::max)(20.0f, dynamicTempo);

			int stepInBar = step % 16; // 16th note step in 4/4 bar (0..15)
			int bar = (step / 16) % 4; // Bar index in 4-bar phrase (0..3)

			// Motion dictates pause frequency and note sustained duration:
			// If motion is low (still shape): notes ring out longer and pauses are frequent.
			// If motion is high (rapid moving shape): notes are shorter, active, continuous groove.
			float motionFactor = std::clamp(m_motionLevel / 2.0f, 0.0f, 1.0f);
			float noteLengthMult = 1.6f - 0.6f * motionFactor;

			// -------------------------------------------------------------
			// Harmonic Foundation: I - IV - V progression over 4 bars
			// -------------------------------------------------------------
			int bassDegree = 0;
			if (bar == 1)
			{
				bassDegree = (m_shapeParams.variation > 0.45f) ? 3 : 0;
			}
			else if (bar == 2)
			{
				bassDegree = (m_shapeParams.variation > 0.65f) ? 5 : 3;
			}
			else if (bar == 3)
			{
				bassDegree = 4;
			}

			// -------------------------------------------------------------
			// LAYER 1: BASS LINE
			// -------------------------------------------------------------
			bool isQuarterBeat = (stepInBar % 4 == 0);
			bool isEighthBeat = (stepInBar % 2 == 0);

			bool triggerBass = isQuarterBeat;
			if (!triggerBass && isEighthBeat && m_shapeParams.syncopation > 0.35f)
			{
				if (stepHash(step, 101) < (m_shapeParams.syncopation - 0.15f) * (0.3f + 0.7f * motionFactor))
				{
					triggerBass = true;
				}
			}

			if (triggerBass)
			{
				// Bass octave is -2 (deep fundamental register: MIDI 36 = C2 = 65 Hz), solid, grounded, and rich!
				int midi = m_currentSong->getScaleDegreeMidi(bassDegree, -2) + static_cast<int>(m_positivePitchOffset);
				float freq = m_currentSong->midiToFrequency(midi);

				if (m_detuneAmount > 0.001f)
				{
					freq *= (1.0f - m_detuneAmount * (step % 2 == 0 ? 1.0f : -1.0f));
				}

				float cutoff = 350.0f + 350.0f * m_shapeParams.bassWeight;
				float dur = beatSec * 0.95f * noteLengthMult;
				float vel = 0.65f + 0.30f * m_shapeParams.bassWeight;

				playNote(freq, vel, dur, 1, 0.0f, cutoff);

				// Sub-bass doubling for powerful, tactile low end
				if (m_shapeParams.bassWeight > 0.35f)
				{
					playNote(freq * 0.5f, vel * 0.70f, dur * 1.1f, 0, 0.0f, 160.0f);
				}
			}

			// -------------------------------------------------------------
			// LAYER 2: HARMONY & CHORDS (Warm Pads / EPs)
			// -------------------------------------------------------------
			bool triggerChord = (stepInBar % 8 == 0);
			if (!triggerChord && (stepInBar % 4 == 0) && m_shapeParams.harmonyRichness > 0.50f)
			{
				triggerChord = (stepHash(step, 179) < m_shapeParams.harmonyRichness * (0.4f + 0.6f * motionFactor));
			}

			if (triggerChord)
			{
				int chordSteps[4] = {0, 4, 2, 6};
				int noteCount = 2;
				if (m_shapeParams.harmonyRichness >= 0.65f)
				{
					noteCount = 4; // 7th chord
				}
				else if (m_shapeParams.harmonyRichness >= 0.35f)
				{
					noteCount = 3; // Triad
				}

				float pans[4] = {-0.30f, 0.30f, -0.10f, 0.40f};
				float dur = beatSec * 1.8f * noteLengthMult;
				float vel = 0.28f + 0.22f * m_shapeParams.harmonyRichness;
				// Warm filter: 700 Hz to 2200 Hz, never harsh or piercing!
				float cutoff = 700.0f + 1500.0f * m_shapeParams.brightness;

				for (int i = 0; i < noteCount; ++i)
				{
					int degree = bassDegree + chordSteps[i];
					// Chords sit comfortably at octave -1 or 0 (Middle C range, ~130 Hz - 260 Hz)
					int octave = (m_shapeParams.brightness > 0.75f) ? 0 : -1;

					int midi =
						m_currentSong->getScaleDegreeMidi(degree, octave) + static_cast<int>(m_positivePitchOffset);
					float freq = m_currentSong->midiToFrequency(midi);
					playNote(freq, vel, dur, 4, pans[i], cutoff);
				}
			}

			// -------------------------------------------------------------
			// LAYER 3: MELODIC ARPEGGIO / LEAD (Warm, Singing Vocal Range)
			// -------------------------------------------------------------
			float trigProb = m_shapeParams.melodyDensity * (0.35f + 0.65f * motionFactor) * 0.65f;
			if (isEighthBeat)
			{
				trigProb += 0.22f * (0.4f + 0.6f * motionFactor);
			}
			if (isQuarterBeat)
			{
				trigProb += 0.12f;
			}

			if (stepHash(step, 233) < trigProb)
			{
				if (stepInBar == 0)
				{
					// Anchor to harmony root on downbeats
					m_melodyDegree = bassDegree + (stepHash(step, 401) > 0.5f ? 2 : 0);
				}
				else
				{
					float jump = (stepHash(step, 311) - 0.5f) * (2.0f + m_shapeParams.variation * 3.5f);
					m_melodyDegree += static_cast<int>(std::round(jump));
				}
				m_melodyDegree = std::clamp(m_melodyDegree, -2, 10);

				// Lead melody octave: 0 (Middle C = 261 Hz) or 1 (C5 = 523 Hz), musical and smooth!
				int baseOctave = (m_shapeParams.brightness > 0.60f) ? 1 : 0;
				int midi = m_currentSong->getScaleDegreeMidi(m_melodyDegree, baseOctave) +
						   static_cast<int>(m_positivePitchOffset);
				float freq = m_currentSong->midiToFrequency(midi);

				if (m_detuneAmount > 0.001f)
				{
					freq *= (1.0f - m_detuneAmount * (step % 2 == 0 ? 1.0f : -1.0f));
				}

				int inst = (m_shapeParams.brightness > 0.50f) ? 4 : 0;
				// Warm filter cutoff: 1200 Hz to 2800 Hz (soft, vocal-like)
				float cutoff = 1200.0f + 1600.0f * m_shapeParams.brightness;
				float dur = beatSec * (0.40f + 0.40f * stepHash(step, 523)) * noteLengthMult;
				float pan = stepHash(step, 617) * 1.0f - 0.5f;
				float vel = 0.35f + 0.25f * m_shapeParams.melodyDensity;

				playNote(freq, vel, dur, inst, pan, cutoff);
			}

			// -------------------------------------------------------------
			// LAYER 4: PERCUSSION (Punchy Kick, Snappy Snare, Crisp Hi-Hat)
			// -------------------------------------------------------------
			float effPercEnergy = m_shapeParams.percEnergy * (0.35f + 0.65f * motionFactor);

			// Kick drum on beats 1 and 3 (step 0 and 8), plus syncopation
			bool triggerKick = (stepInBar == 0 || stepInBar == 8);
			if (!triggerKick && (effPercEnergy > 0.45f || m_shapeParams.syncopation > 0.40f))
			{
				if (stepInBar == 6 || (effPercEnergy > 0.65f && stepInBar == 14))
				{
					triggerKick = true;
				}
			}

			if (triggerKick)
			{
				float kickVel = 0.70f + 0.25f * effPercEnergy;
				playNote(60.0f, kickVel, 0.28f, 5, 0.0f, 400.0f); // instrument 5 = Kick
			}

			// Snare / Clap on beats 2 & 4 (step 4 and 12)
			bool triggerSnare = (stepInBar == 4 || stepInBar == 12);
			if (!triggerSnare && effPercEnergy > 0.60f && stepInBar == 15)
			{
				triggerSnare = true; // 16th-note ghost snare before downbeat
			}

			if (triggerSnare)
			{
				float snareVel = 0.55f + 0.25f * effPercEnergy;
				playNote(180.0f, snareVel, 0.18f, 6, 0.0f, 4500.0f); // instrument 6 = Snare
			}

			// Hi-hat on 8th notes (steady timekeeping groove)
			if (isEighthBeat || (effPercEnergy > 0.50f && (stepHash(step, 809) < 0.65f)))
			{
				bool openHat = (stepInBar % 4 == 2) && (effPercEnergy > 0.45f);
				float decay = openHat ? 0.15f : 0.045f;
				float pan = (step % 2 == 0) ? 0.18f : -0.18f;
				float cutoff = 7000.0f + 3000.0f * m_shapeParams.brightness;
				float vel = (openHat ? 0.32f : 0.24f) * (0.6f + 0.4f * effPercEnergy);

				playNote(8000.0f, vel, decay, 7, pan, cutoff); // instrument 7 = HiHat
			}

			// Ghost percussion blip on offbeats
			if (m_shapeParams.variation > 0.50f && stepInBar >= 12 && motionFactor > 0.4f)
			{
				if (stepHash(step, 911) < m_shapeParams.variation * 0.35f)
				{
					playNote(2500.0f, 0.20f * effPercEnergy, 0.04f, 7, 0.25f, 5000.0f);
				}
			}
		}

		void SdfMusicEngine::playNote(float freq, float amp, float durationSec, int instrument, float pan,
									  float filterCutoff)
		{
			float duckMult = (std::max)(0.15f, 1.0f - m_ducking * 0.65f);
			float masterAmp = amp * m_volume * m_volumeFromFill * duckMult;
			if (masterAmp <= 0.001f)
				return;

			// Constant-power panning: [-1, +1] -> left/right gains
			float panNorm = std::clamp((pan + 1.0f) * 0.5f, 0.0f, 1.0f);
			float leftGain = std::cos(panNorm * 1.5707963f);
			float rightGain = std::sin(panNorm * 1.5707963f);

			MusicVoice newVoice;
			newVoice.frequency = freq;
			newVoice.amplitude = masterAmp;
			newVoice.decay = (std::max)(0.02f, durationSec);
			newVoice.time = 0.0f;
			newVoice.phase = 0.0f;
			newVoice.finished = false;
			newVoice.instrument = instrument;
			newVoice.leftGain = leftGain;
			newVoice.rightGain = rightGain;
			newVoice.filterCutoff = (std::min)(filterCutoff, m_concussionFilterCutoff);
			newVoice.filterState = 0.0f;

			if (m_activeVoices.size() >= MAX_MUSIC_VOICES)
			{
				size_t victimIdx = 0;
				float minAmp = 1000.0f;
				for (size_t i = 0; i < m_activeVoices.size(); ++i)
				{
					float curAmp =
						m_activeVoices[i].amplitude * expf(-m_activeVoices[i].time / m_activeVoices[i].decay);
					if (curAmp < minAmp)
					{
						minAmp = curAmp;
						victimIdx = i;
					}
				}
				if (newVoice.amplitude > minAmp * 0.5f)
				{
					m_activeVoices[victimIdx] = newVoice;
				}
				return;
			}

			m_activeVoices.push_back(newVoice);
		}

		void SdfMusicEngine::render(float* buffer, uint32_t frameCount, uint32_t channels)
		{
			if (!m_playing || frameCount == 0 || channels != 2)
				return;

			const float sampleRateF = static_cast<float>(m_sampleRate);

			for (auto& voice : m_activeVoices)
			{
				if (voice.finished)
					continue;

				// Instantaneous attack for drums (preserves punch & transient snap)
				// Smooth attack for melodic notes (prevents clicks/pops)
				const bool isPercussion =
					(voice.instrument == 5 || voice.instrument == 6 || voice.instrument == 7 || voice.instrument == 3);
				const float attackTime = isPercussion ? 0.0005f : 0.008f;

				const float effCutoff = (std::min)(voice.filterCutoff, m_concussionFilterCutoff);
				const float wc = 2.0f * static_cast<float>(M_PI) * std::clamp(effCutoff, 20.0f, 20000.0f) / sampleRateF;
				const float filterAlpha = std::clamp(wc / (wc + 1.0f), 0.001f, 1.0f);

				for (uint32_t i = 0; i < frameCount; ++i)
				{
					float env = voice.amplitude * expf(-voice.time / voice.decay);
					if (voice.time < attackTime)
					{
						env *= (voice.time / attackTime);
					}

					float rawSample = 0.0f;
					float currentFreq = voice.frequency;

					switch (voice.instrument)
					{
						case 5: // Kick Drum: Fast downward pitch drop (145 Hz -> 48 Hz) with punchy sub body
						{
							float pitchDrop = 95.0f * expf(-voice.time / 0.028f);
							currentFreq = 48.0f + pitchDrop;
							float s = sinf(voice.phase);
							rawSample = 1.25f * s - 0.25f * s * s * s; // warm soft saturation drive
							if (voice.time < 0.004f)
							{
								rawSample +=
									0.35f * ((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f);
							}
							break;
						}
						case 6: // Snare Drum: Snappy noise burst + resonant 180 Hz body tone
						{
							float bodyPitch = 120.0f + 65.0f * expf(-voice.time / 0.020f);
							currentFreq = bodyPitch;
							float bodyTone = sinf(voice.phase);
							float noise = ((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f);
							rawSample = 0.45f * bodyTone + 0.55f * noise;
							break;
						}
						case 7: // Hi-Hat: Crisp metallic high frequency burst
						{
							float noise = ((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f);
							float metallic = sinf(voice.phase * 1.37f) * sinf(voice.phase * 2.81f);
							rawSample = 0.70f * noise + 0.30f * metallic;
							break;
						}
						case 1: // Warm Bass: Rich fundamental + warm second harmonic + subtle warmth
						{
							float p = voice.phase;
							rawSample = 0.78f * sinf(p) + 0.30f * sinf(p * 2.0f) +
										0.12f * (1.0f - p / static_cast<float>(M_PI));
							break;
						}
						case 2: // Square
							rawSample = (voice.phase < static_cast<float>(M_PI)) ? 0.55f : -0.55f;
							break;
						case 4: // Warm EP / Pad (soft electric piano / lush pad chords)
						{
							float p = voice.phase;
							rawSample = 0.65f * sinf(p) + 0.25f * sinf(p * 2.0f) + 0.10f * sinf(p * 3.0f);
							break;
						}
						case 3: // Noise
							rawSample = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
							break;
						case 0: // Warm Lead (singing melodic tone)
						default:
						{
							float p = voice.phase;
							rawSample = 0.85f * sinf(p) + 0.15f * sinf(p * 2.0f);
							break;
						}
					}

					voice.filterState += filterAlpha * (rawSample - voice.filterState);
					float sample = env * voice.filterState;

					const float phaseInc = 2.0f * static_cast<float>(M_PI) * currentFreq / sampleRateF;
					voice.phase += phaseInc;
					if (voice.phase >= 2.0f * static_cast<float>(M_PI))
					{
						voice.phase -= 2.0f * static_cast<float>(M_PI);
					}
					voice.time += 1.0f / sampleRateF;

					buffer[i * 2 + 0] += sample * voice.leftGain;
					buffer[i * 2 + 1] += sample * voice.rightGain;
				}

				if (voice.time > attackTime && voice.amplitude * expf(-voice.time / voice.decay) < 0.0005f)
				{
					voice.finished = true;
				}
			}

			m_activeVoices.erase(std::remove_if(m_activeVoices.begin(), m_activeVoices.end(),
												[](const MusicVoice& v) { return v.finished; }),
								 m_activeVoices.end());
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine
