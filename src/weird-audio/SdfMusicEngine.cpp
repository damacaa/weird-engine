#include "weird-audio/SdfMusicEngine.h"

#include <algorithm>
#include <cmath>

#include "weird-renderer/components/Shape.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		namespace
		{
			constexpr int NUM_DOMAIN_SAMPLES = 32;
			constexpr float DOMAIN_RADIUS = 50.0f;

			// Surface complexity estimation: finite-difference stencil and convergence guards
			constexpr float CURVATURE_STENCIL = 1.0f;
			constexpr int CURVATURE_NEWTON_STEPS = 3;
			constexpr float MIN_GRADIENT = 1e-4f;

			// Complexity -> tension curve exponent: keeps the low/mid range responsive.
			// Tension intentionally follows the cooler complexity scale, so effects are milder too.
			constexpr float COMPLEXITY_TENSION_CURVE = 0.8f;

			// Pad voices react more gently to complexity tension than lead/bass (dissonance reads louder on pads)
			constexpr float PAD_TENSION_SCALE = 0.45f;

			// Dynamic effect attack: fast but smooth fade-in (seconds to ~63% of target)
			constexpr float DUCK_ATTACK_TIME = 0.10f;
			constexpr float SURGE_ATTACK_TIME = 0.06f;
			constexpr float LEAD_DUCK_FADE_RATE = 8.0f; // Full lead fade-out in ~125 ms

			// Motion EMA rate (1/s): how fast m_motionLevel chases the sampled average.
			// Slow on purpose: imperfections in the domain sampling make instant motion
			// jitter (e.g. a spinning star measures differently each rotation step), and a
			// slow average lets those readings cancel out so the level stabilizes.
			// Time constant ~5.5s to reach ~63% of a new sustained motion level.
			constexpr float MOTION_EMA_RATE = 0.18f;

			bool isSongEmpty(const std::shared_ptr<SdfSong>& song)
			{
				if (!song)
					return true;
				return song->getRawShapeExpression() == nullptr;
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
			m_loopCycle = 0;
			m_activeVoices.clear();
			m_concussionFilterCutoff = 20000.0f;
			m_deathStage = 0;
			m_deathTimer = 0.0f;
			m_deathSilenceTimer = 0.0f;
			m_isDead = false;
			m_melodyDegree = 0;
			m_melodyLastInterval = 0;
			m_shapeParams = ShapeMusicalParams{};
			m_motionLevel = 0.0f;
			m_motionNorm = 0.0f;
			m_fillRatio = -1.0f;
			m_tempoFromMotion = 1.0f;
			m_volumeFromFill = 0.65f;
			m_complexityLevel = 0.0f;
			m_complexityNorm = 0.0f;
			m_tensionFromComplexity = 0.0f;
			m_sampleIndex = 0;
			m_prevMotionDist = 0.0f;
			m_hasPrevMotionSample = false;
			m_domainFillSamples.fill(0.5f);
			m_domainMotionSamples.fill(0.0f);
			m_domainCurvatureSamples.fill(0.0f);
			m_surgeLevel = 0.0f;
			m_surgeTarget = 0.0f;
			m_surgeTimer = 0.0f;
			m_ducking = 0.0f;
			m_duckTarget = 0.0f;
			m_duckTimer = 0.0f;
			m_pendingSurgeImpact = false;
			m_surgeImpactCooldown = 0.0f;
			m_trackPlayStates.fill(TrackPlayState::Playing);

			m_currentSong = nullptr;
			m_queuedSong = nullptr;
		}

		void SdfMusicEngine::updateShapeParamsFromTraits()
		{
			if (m_overrideShapeParams)
			{
				return;
			}

			const float mass = m_visualTraits.mass;
			const float motion = m_visualTraits.motion;
			const float spikiness = m_visualTraits.spikiness;
			const float hollowness = m_visualTraits.hollowness;
			const float asymmetry = m_visualTraits.asymmetry;

			// 1. Tempo Factor: fast motion & lighter shapes push tempo factor higher
			m_shapeParams.tempoFactor = std::clamp(0.80f + 0.38f * motion + 0.10f * (1.0f - mass), 0.72f, 1.30f);

			// 2. Melody Density: spiky/lobed contours and active motion create busier lead hooks
			m_shapeParams.melodyDensity = std::clamp(0.20f + 0.44f * spikiness + 0.24f * motion, 0.16f, 0.86f);

			// 3. Harmony Richness: larger/heavier shapes play fuller chords (triads & 7ths)
			m_shapeParams.harmonyRichness =
				std::clamp(0.18f + 0.68f * mass + 0.10f * (1.0f - hollowness), 0.16f, 0.90f);

			// 4. Bass Weight: heavy, solid-core shapes bring deep sub-bass foundation
			m_shapeParams.bassWeight = std::clamp(0.24f + 0.54f * mass + 0.20f * (1.0f - hollowness), 0.22f, 0.92f);

			// 5. Percussion Energy: fast movement and sharp/spiky edges drive active drums
			m_shapeParams.percEnergy = std::clamp(0.16f + 0.52f * motion + 0.24f * spikiness, 0.15f, 0.90f);

			// 6. Brightness: smooth round shapes stay warm; spiky/sharp shapes open the filter & upper octave
			m_shapeParams.brightness = std::clamp(0.20f + 0.65f * spikiness, 0.18f, 0.88f);

			// 7. Syncopation: off-center asymmetry and hollow cutouts/rings add off-beat groove
			m_shapeParams.syncopation = std::clamp(0.14f + 0.45f * asymmetry + 0.30f * hollowness, 0.12f, 0.85f);

			// 8. Variation: intricate spiky, hollow, or asymmetric shapes unlock richer chord & melody changes
			m_shapeParams.variation =
				std::clamp(0.18f + 0.38f * spikiness + 0.24f * hollowness + 0.18f * asymmetry, 0.16f, 0.88f);
		}

		void SdfMusicEngine::sampleShapeParameters()
		{
			if (!m_currentSong || !m_currentSong->getRawShapeExpression())
			{
				m_visualTraits = ShapeVisualTraits{};
				if (!m_overrideShapeParams)
				{
					m_shapeParams = ShapeMusicalParams{};
				}
				return;
			}

			auto shape = m_currentSong->getRawShapeExpression();
			float r = m_currentSong->getSampleRadius();
			if (r <= 0.01f)
			{
				r = 20.0f;
			}

			float params[12]{};
			packSdfParameterPrefix(params, m_currentSong->getParameters(), static_cast<float>(m_sceneTime));

			auto evalAt = [&](float px, float py) -> float
			{
				params[9] = px;
				params[10] = py;
				return shape->getValue(params);
			};

			// 1. Center & Inner-Core Hollowness Analysis (Rotation-Invariant)
			// dCenter < 0 means solid interior; dCenter > 0 means hollow cutout / ring / eclipse
			float dCenter = evalAt(0.0f, 0.0f);
			constexpr float PI = 3.14159265358979323846f;
			float innerHoleScore = (dCenter > 0.0f) ? std::clamp((dCenter + 2.0f) / 14.0f, 0.0f, 1.0f) : 0.0f;
			if (dCenter <= 0.0f)
			{
				float coreSolidity = std::clamp(-dCenter / 24.0f, 0.0f, 1.0f);
				innerHoleScore = (1.0f - coreSolidity) * 0.35f;
			}

			// 2. 24-Angle Full-Circle Perimeter Profiling (Rotation-Invariant)
			// Evaluates the entire 360-degree silhouette so rotating a shape never changes its musical parameters
			constexpr int NUM_RING_ANGLES = 24;
			float R[NUM_RING_ANGLES]{};
			float meanR = 0.0f;

			for (int i = 0; i < NUM_RING_ANGLES; ++i)
			{
				float angle = static_cast<float>(i) * (2.0f * PI / static_cast<float>(NUM_RING_ANGLES));
				float d = evalAt(r * std::cos(angle), r * std::sin(angle));
				R[i] = (std::max)(0.0f, r - d);
				meanR += R[i];
			}
			meanR /= static_cast<float>(NUM_RING_ANGLES);

			// Radial variance & step-to-step angular roughness (0 for any smooth circle, high for stars/spikes)
			float varSum = 0.0f;
			float stepDiffSum = 0.0f;
			for (int i = 0; i < NUM_RING_ANGLES; ++i)
			{
				float diff = R[i] - meanR;
				varSum += diff * diff;
				int nextIdx = (i + 1) % NUM_RING_ANGLES;
				stepDiffSum += std::abs(R[i] - R[nextIdx]);
			}
			float stdDevR = std::sqrt(varSum / static_cast<float>(NUM_RING_ANGLES));
			float avgStepDiff = stepDiffSum / static_cast<float>(NUM_RING_ANGLES);

			// 180-degree opposite-pair rotational asymmetry across all 12 opposite pairs
			float asymSum = 0.0f;
			constexpr int HALF_RING = NUM_RING_ANGLES / 2;
			for (int i = 0; i < HALF_RING; ++i)
			{
				float a = R[i];
				float b = R[i + HALF_RING];
				asymSum += std::abs(a - b) / (a + b + 2.0f);
			}
			float meanAsym = asymSum / static_cast<float>(HALF_RING);

			float reachNorm = std::clamp((meanR - 4.0f) / 26.0f, 0.0f, 1.0f);
			float fillNorm = std::clamp(m_fillRatio >= 0.0f ? m_fillRatio : reachNorm, 0.0f, 1.0f);
			float contourSpikiness = std::clamp((avgStepDiff / 4.2f) * 0.65f + (stdDevR / 9.0f) * 0.35f, 0.0f, 1.0f);

			m_visualTraits.mass = std::clamp(0.60f * fillNorm + 0.40f * reachNorm, 0.0f, 1.0f);
			m_visualTraits.motion = std::clamp(m_motionNorm, 0.0f, 1.0f);
			m_visualTraits.spikiness = std::clamp((std::max)(contourSpikiness, m_complexityNorm), 0.0f, 1.0f);
			m_visualTraits.hollowness = std::clamp(innerHoleScore, 0.0f, 1.0f);
			m_visualTraits.asymmetry = std::clamp(meanAsym * 2.2f, 0.0f, 1.0f);

			updateShapeParamsFromTraits();
		}

		float SdfMusicEngine::estimateSurfaceCurvature(const std::shared_ptr<IMathExpression>& shape, float* params,
													   glm::vec2 samplePoint, float sampleDist)
		{
			constexpr float H = CURVATURE_STENCIL;

			auto evalAt = [&](float px, float py) -> float
			{
				params[9] = px;
				params[10] = py;
				return shape->getValue(params);
			};

			auto gradientAt = [&](float px, float py, float& outX, float& outY)
			{
				outX = (evalAt(px + H, py) - evalAt(px - H, py)) / (2.0f * H);
				outY = (evalAt(px, py + H) - evalAt(px, py - H)) / (2.0f * H);
			};

			// Iterative Newton projection onto the surface. A single step is exact for an
			// ideal SDF, but repeated steps keep far samples accurate for non-unit gradients.
			float px = samplePoint.x;
			float py = samplePoint.y;
			float dist = sampleDist;
			for (int i = 0; i < CURVATURE_NEWTON_STEPS; ++i)
			{
				float gx = 0.0f;
				float gy = 0.0f;
				gradientAt(px, py, gx, gy);

				float gradMag = std::sqrt(gx * gx + gy * gy);
				if (!std::isfinite(gradMag) || gradMag < MIN_GRADIENT)
					return 0.0f;

				float step = std::clamp(dist, -DOMAIN_RADIUS * 0.5f, DOMAIN_RADIUS * 0.5f);
				px -= (gx / gradMag) * step;
				py -= (gy / gradMag) * step;
				dist = evalAt(px, py);
			}

			// Mean curvature of the field: kappa = laplacian(f) / |grad f|
			// (equals 1/R everywhere on a circle of radius R, hence complexity 0)
			float fCenter = evalAt(px, py);
			float fLeft = evalAt(px - H, py);
			float fRight = evalAt(px + H, py);
			float fDown = evalAt(px, py - H);
			float fUp = evalAt(px, py + H);

			float laplacian = (fLeft + fRight + fDown + fUp - 4.0f * fCenter) / (H * H);
			float sgx = (fRight - fLeft) / (2.0f * H);
			float sgy = (fUp - fDown) / (2.0f * H);
			float surfaceGradMag = std::sqrt(sgx * sgx + sgy * sgy);
			if (!std::isfinite(laplacian) || !std::isfinite(surfaceGradMag) || surfaceGradMag < MIN_GRADIENT)
				return 0.0f;

			float kappa = laplacian / surfaceGradMag;
			return std::isfinite(kappa) ? kappa : 0.0f;
		}

		void SdfMusicEngine::initDomainSamples()
		{
			if (!m_currentSong || !m_currentSong->getRawShapeExpression())
				return;

			auto shape = m_currentSong->getRawShapeExpression();
			static const auto sampleOffsets = createDomainSamples();

			constexpr float probeDt = 0.08f;
			float params[12]{};
			float paramsNext[12]{};
			packSdfParameterPrefix(params, m_currentSong->getParameters(), static_cast<float>(m_sceneTime));
			packSdfParameterPrefix(paramsNext, m_currentSong->getParameters(),
								   static_cast<float>(m_sceneTime) + probeDt);

			float totalFill = 0.0f;
			float totalMotion = 0.0f;
			float totalCurvature = 0.0f;
			float totalCurvatureSq = 0.0f;
			for (size_t i = 0; i < NUM_DOMAIN_SAMPLES && i < sampleOffsets.size(); ++i)
			{
				params[9] = sampleOffsets[i].x;
				params[10] = sampleOffsets[i].y;
				float d = shape->getValue(params);
				float fillVal = (d < 0.0f) ? 1.0f : 0.0f;
				m_domainFillSamples[i] = fillVal;

				paramsNext[9] = sampleOffsets[i].x;
				paramsNext[10] = sampleOffsets[i].y;
				float dNext = shape->getValue(paramsNext);
				float motionRate = std::abs(dNext - d) / probeDt;
				m_domainMotionSamples[i] = motionRate;
				totalMotion += motionRate;

				float kappa = estimateSurfaceCurvature(shape, params, sampleOffsets[i], d);
				m_domainCurvatureSamples[i] = kappa;
				totalCurvature += kappa;
				totalCurvatureSq += kappa * kappa;

				totalFill += fillVal;
			}

			m_fillRatio = totalFill / static_cast<float>(NUM_DOMAIN_SAMPLES);
			m_volumeFromFill = std::clamp(0.40f + 0.60f * std::sqrt(std::max(0.0f, m_fillRatio)), 0.35f, 1.0f);

			// Surface complexity: scale-normalized standard deviation of the sampled mean curvature.
			// Constant curvature (circle/line) yields exactly 0 regardless of size.
			float avgCurvature = totalCurvature / static_cast<float>(NUM_DOMAIN_SAMPLES);
			float variance =
				std::max(0.0f, totalCurvatureSq / static_cast<float>(NUM_DOMAIN_SAMPLES) - avgCurvature * avgCurvature);
			float characteristicRadius = DOMAIN_RADIUS * std::sqrt(std::max(m_fillRatio, 1e-3f));
			m_complexityLevel = std::sqrt(variance) * characteristicRadius;
			m_complexityNorm = m_complexityLevel / (m_complexityLevel + m_complexitySaturation);
			if (!m_overrideShapeParams)
			{
				m_tensionFromComplexity = std::pow(m_complexityNorm, COMPLEXITY_TENSION_CURVE);
			}

			m_motionLevel = totalMotion / static_cast<float>(NUM_DOMAIN_SAMPLES);
			m_motionNorm = std::clamp(m_motionLevel / 100.0f, 0.0f, 1.0f);
			m_tempoFromMotion = 0.45f + 0.75f * std::pow(m_motionNorm, 1.3f);

			m_sampleIndex = 0;
			m_hasPrevMotionSample = false;
			m_prevMotionDist = 0.0f;
		}

		void SdfMusicEngine::sampleDomainMotionAndFill(double sceneTime, double deltaTime)
		{
			if (!m_currentSong || !m_currentSong->getRawShapeExpression())
				return;

			auto shape = m_currentSong->getRawShapeExpression();

			static const auto sampleOffsets = createDomainSamples();
			if (sampleOffsets.empty())
				return;

			float params[12]{};
			packSdfParameterPrefix(params, m_currentSong->getParameters(), static_cast<float>(sceneTime));

			// Sample point for current frame
			size_t currentIndex = m_sampleIndex % sampleOffsets.size();
			const auto& currentPoint = sampleOffsets[currentIndex];

			// 1. Motion evaluation:
			// If we sampled currentPoint on previous update, compare distance to calculate delta
			if (m_hasPrevMotionSample)
			{
				params[9] = currentPoint.x;
				params[10] = currentPoint.y;
				float curDist = shape->getValue(params);

				float deltaDist = std::abs(curDist - m_prevMotionDist);
				float effDt = std::clamp(static_cast<float>(deltaTime), 0.001f, 0.1f);
				float motionRate = deltaDist / effDt;

				m_domainMotionSamples[currentIndex] = motionRate;
			}

			// 2. Next position sampling:
			// Advance index to select next point and record its baseline distance for next update
			size_t nextIndex = (m_sampleIndex + 1) % sampleOffsets.size();
			const auto& nextPoint = sampleOffsets[nextIndex];

			params[9] = nextPoint.x;
			params[10] = nextPoint.y;
			float nextDist = shape->getValue(params);

			m_prevMotionDist = nextDist;
			m_hasPrevMotionSample = true;
			m_sampleIndex = nextIndex;

			// 3. Domain fill evaluation:
			// Update the sampled position's inside/outside state
			m_domainFillSamples[nextIndex] = (nextDist < 0.0f) ? 1.0f : 0.0f;

			// 4. Surface complexity evaluation:
			// Project the sampled point onto the surface and measure its mean curvature
			m_domainCurvatureSamples[nextIndex] = estimateSurfaceCurvature(shape, params, nextPoint, nextDist);

			// Running average across all domain sample points:
			// Eliminates intra-song spatial oscillation for static shapes while adapting smoothly
			float totalFill = 0.0f;
			float totalMotion = 0.0f;
			float totalCurvature = 0.0f;
			float totalCurvatureSq = 0.0f;
			for (size_t i = 0; i < NUM_DOMAIN_SAMPLES; ++i)
			{
				totalFill += m_domainFillSamples[i];
				totalMotion += m_domainMotionSamples[i];

				float kappa = m_domainCurvatureSamples[i];
				totalCurvature += kappa;
				totalCurvatureSq += kappa * kappa;
			}

			float avgFill = totalFill / static_cast<float>(NUM_DOMAIN_SAMPLES);
			float avgMotion = totalMotion / static_cast<float>(NUM_DOMAIN_SAMPLES);
			float avgCurvature = totalCurvature / static_cast<float>(NUM_DOMAIN_SAMPLES);
			float curvatureVariance =
				std::max(0.0f, totalCurvatureSq / static_cast<float>(NUM_DOMAIN_SAMPLES) - avgCurvature * avgCurvature);
			float curvatureStdDev = std::sqrt(curvatureVariance);

			// Framerate-independent exponential moving average (~0.99 old / 0.01 new blend at 60 FPS)
			float dt = std::clamp(static_cast<float>(deltaTime), 0.0001f, 0.1f);
			float blend = 1.0f - std::exp(-dt * 0.75f);

			m_fillRatio += (avgFill - m_fillRatio) * blend;
			// Motion uses a much slower EMA than the other averages (see MOTION_EMA_RATE)
			float motionBlend = 1.0f - std::exp(-dt * MOTION_EMA_RATE);
			m_motionLevel += (avgMotion - m_motionLevel) * motionBlend;

			// Surface complexity: spread of mean curvature normalized by the characteristic shape size.
			// A perfect circle has constant curvature, so its standard deviation (and complexity) is 0.
			float characteristicRadius = DOMAIN_RADIUS * std::sqrt(std::max(m_fillRatio, 1e-3f));
			float rawComplexity = curvatureStdDev * characteristicRadius;
			m_complexityLevel += (rawComplexity - m_complexityLevel) * blend;
			m_complexityNorm = m_complexityLevel / (m_complexityLevel + m_complexitySaturation);
			if (!m_overrideShapeParams)
			{
				m_tensionFromComplexity = std::pow(m_complexityNorm, COMPLEXITY_TENSION_CURVE);
			}

			// Motion controls overall tempo:
			// If motion == 0 (still): tempo drops to 0.45x
			// If motion is moderate: tempo is ~1.0x
			// If motion is high: tempo caps at ~1.2x.
			// NOTE: The cap is deliberate! Uniformly speeding every track up past ~1.2x
			// sounds like a time-stretched recording instead of energetic music. Above that,
			// energy is expressed through the lead's generative note density instead of raw BPM.
			float motionNorm = std::clamp(m_motionLevel / 100.0f, 0.0f, 1.0f);
			m_motionNorm = motionNorm;
			m_tempoFromMotion = 0.45f + 0.75f * std::pow(motionNorm, 1.3f);

			// Fill ratio controls volume:
			// Bigger shape -> higher fill ratio -> louder volume
			// Smaller shape -> lower fill ratio -> quieter volume (with audible minimum floor)
			m_volumeFromFill = std::clamp(0.40f + 0.60f * std::sqrt(m_fillRatio), 0.35f, 1.0f);

			// Keep dynamic visual traits & musical parameters smoothly synced to live motion, fill, and curvature
			m_visualTraits.motion = m_motionNorm;
			m_visualTraits.mass = std::clamp(0.60f * m_fillRatio + 0.40f * m_visualTraits.mass, 0.0f, 1.0f);
			m_visualTraits.spikiness =
				std::clamp((std::max)(m_visualTraits.spikiness * 0.85f, m_complexityNorm), 0.0f, 1.0f);
			updateShapeParamsFromTraits();
		}

		void SdfMusicEngine::selectInstrumentRack(const ASTFingerprint& fp)
		{
			m_rack = InstrumentRack{};

			// Deterministic PRNG seeded by AST structural topology hash
			auto prngHash = [](uint32_t seed, uint32_t salt) -> uint32_t
			{
				uint32_t h = seed ^ (salt * 0x9e3779b9u);
				h = (h ^ (h >> 16)) * 0x85ebca6bu;
				h = (h ^ (h >> 13)) * 0xc2b2ae35u;
				return h ^ (h >> 16);
			};

			uint32_t hLead = prngHash(fp.structuralHash, 101);
			uint32_t hBass = prngHash(fp.structuralHash, 203);
			uint32_t hPad = prngHash(fp.structuralHash, 307);
			uint32_t hKit = prngHash(fp.structuralHash, 409);

			const float spikiness = m_visualTraits.spikiness;
			const float mass = m_visualTraits.mass;

			// Match instrument timbres to what the shape looks like (Smooth vs. Groovy vs. Spiky/Sharp):
			if (spikiness < 0.32f)
			{
				// Smooth / Round shape (circles, bubbles, soft waves) -> warm, clean timbres
				static constexpr WaveType smoothLeads[2] = {WaveType::SoftSine, WaveType::FMPluck};
				static constexpr WaveType smoothBass[2] = {WaveType::SoftSine, WaveType::FMPluck};
				static constexpr WaveType smoothPads[2] = {WaveType::SoftSine, WaveType::Wavefolder};
				m_rack.lead = smoothLeads[hLead % 2];
				m_rack.bass = smoothBass[hBass % 2];
				m_rack.pad = smoothPads[hPad % 2];
				m_rack.drumKit = (mass > 0.45f) ? 0 : 1; // Deep 808 or Acoustic Punch
			}
			else if (spikiness < 0.62f)
			{
				// Medium / Contoured shape -> expressive plucks, reedy squares, and wavefolders
				static constexpr WaveType midLeads[3] = {WaveType::FMPluck, WaveType::Wavefolder,
														 WaveType::PulseSquare};
				static constexpr WaveType midBass[3] = {WaveType::SoftSine, WaveType::PulseSquare, WaveType::FMPluck};
				static constexpr WaveType midPads[3] = {WaveType::SoftSine, WaveType::Wavefolder,
														WaveType::PulseSquare};
				m_rack.lead = midLeads[hLead % 3];
				m_rack.bass = midBass[hBass % 3];
				m_rack.pad = midPads[hPad % 3];
				m_rack.drumKit = static_cast<int>(hKit % 2); // 808 or Acoustic
			}
			else
			{
				// Spiky / Sharp shape (stars, shurikens, angular cutouts) -> crisp, cutting synth timbres
				static constexpr WaveType sharpLeads[3] = {WaveType::BandlimitedSaw, WaveType::PulseSquare,
														   WaveType::FMPluck};
				static constexpr WaveType sharpBass[2] = {WaveType::BandlimitedSaw, WaveType::PulseSquare};
				static constexpr WaveType sharpPads[3] = {WaveType::Wavefolder, WaveType::BandlimitedSaw,
														  WaveType::SoftSine};
				m_rack.lead = sharpLeads[hLead % 3];
				m_rack.bass = sharpBass[hBass % 2];
				m_rack.pad = sharpPads[hPad % 3];
				m_rack.drumKit = (hKit % 2 == 0) ? 2 : 1; // Industrial / 909 or Acoustic Punch
			}

			// Synthesis parameters shaped by visual traits + hash bits
			float normA = static_cast<float>(hLead & 0xFF) / 255.0f;
			float normB = static_cast<float>((hBass >> 8) & 0xFF) / 255.0f;

			m_rack.pulseWidth = 0.22f + 0.26f * normA;					// [0.22, 0.48]
			m_rack.fmModIndex = 1.0f + 1.8f * spikiness + 0.8f * normB; // Smooth = gentle bell, Spiky = metallic bite
			m_rack.foldDrive = 1.4f + 1.6f * spikiness + 0.5f * mass;	// Smooth = warm fold, Spiky = rich harmonics
		}

		void SdfMusicEngine::resampleShape()
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			initDomainSamples();
			sampleShapeParameters();
		}

		void SdfMusicEngine::setSong(std::shared_ptr<SdfSong> song, bool beatSynced)
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			if (m_currentSong == song && song != nullptr)
			{
				return;
			}
			if (!m_currentSong || !beatSynced)
			{
				m_currentSong = std::move(song);
				m_queuedSong = nullptr;
				m_currentStep = 0;
				m_loopCycle = 0;
				m_stepAccumulator = 0.0f;
				m_currentBeat = 0.0f;
				m_melodyDegree = 0;
				m_melodyLastInterval = 0;
				m_lastLeadStep = -99;
				m_lastLeadVel = 0.0f;
				initDomainSamples();
				sampleShapeParameters();

				if (isSongEmpty(m_currentSong))
				{
					for (auto& voice : m_activeVoices)
					{
						if (!voice.finished && voice.fadeRate <= 0.0f)
						{
							voice.fadeRate = (std::max)(voice.fadeGain, 0.01f) / 0.005f;
						}
					}
					m_rack = InstrumentRack{};
				}
				else
				{
					selectInstrumentRack(m_currentSong->getFingerprint());
				}
			}
			else
			{
				m_queuedSong = std::move(song);
			}
		}

		std::shared_ptr<SdfSong> SdfMusicEngine::getCurrentSong() const
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			return m_currentSong;
		}

		void SdfMusicEngine::queueSong(std::shared_ptr<SdfSong> nextSong)
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			m_queuedSong = std::move(nextSong);
		}

		void SdfMusicEngine::setTrackEnabled(MusicTrack track, bool enabled)
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			switch (track)
			{
				case MusicTrack::Lead:
					m_tracks.lead = enabled;
					break;
				case MusicTrack::Bass:
					m_tracks.bass = enabled;
					break;
				case MusicTrack::Pad:
					m_tracks.pad = enabled;
					break;
				case MusicTrack::Drums:
					m_tracks.drums = enabled;
					break;
			}
		}

		bool SdfMusicEngine::isTrackEnabled(MusicTrack track) const
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			switch (track)
			{
				case MusicTrack::Lead:
					return m_tracks.lead;
				case MusicTrack::Bass:
					return m_tracks.bass;
				case MusicTrack::Pad:
					return m_tracks.pad;
				case MusicTrack::Drums:
					return m_tracks.drums;
			}
			return true;
		}

		TrackPlayState SdfMusicEngine::getTrackPlayState(MusicTrack track) const
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			size_t idx = static_cast<size_t>(track);
			if (idx < m_trackPlayStates.size())
			{
				return m_trackPlayStates[idx];
			}
			return TrackPlayState::Playing;
		}

		void SdfMusicEngine::triggerPositiveFeedback(float intensity)
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			float clamped = std::clamp(intensity, 0.1f, 2.0f);
			if (m_currentSong)
			{
				// Crisp, satisfying acoustic UI click in-key with the song (octave 5: MIDI 67-79, ~400-800 Hz)
				int noteMidi = m_currentSong->getRootMidi();
				while (noteMidi < 67)
					noteMidi += 12;
				while (noteMidi > 79)
					noteMidi -= 12;
				float freq = m_currentSong->midiToFrequency(noteMidi);
				playNote(freq, 0.85f * clamped, 0.075f, 8, 0.0f, 16000.0f);
			}
			else
			{
				playNote(587.33f, 0.85f * clamped, 0.075f, 8, 0.0f, 16000.0f);
			}
		}

		void SdfMusicEngine::triggerNegativeFeedback(float intensity)
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			float clamped = std::clamp(intensity, 0.1f, 2.0f);
			if (m_currentSong)
			{
				// Clear negative feedback for invalid input: short, responsive descending dissonant interval
				// Anchor pitch in mid-register (MIDI 58-70) so phone and laptop speakers reproduce it clearly
				int baseMidi = m_currentSong->getRootMidi();
				while (baseMidi < 58)
					baseMidi += 12;
				while (baseMidi > 70)
					baseMidi -= 12;

				int note1 = baseMidi;
				int note2 = baseMidi + 1; // Minor second dissonance (produces acoustic beating buzz)
				float freq1 = m_currentSong->midiToFrequency(note1);
				float freq2 = m_currentSong->midiToFrequency(note2);

				playNote(freq1, 0.65f * clamped, 0.15f, 9, -0.06f, 5000.0f);
				playNote(freq2, 0.55f * clamped, 0.15f, 9, 0.06f, 5000.0f);
			}
			else
			{
				playNote(261.63f, 0.65f * clamped, 0.15f, 9, -0.06f, 5000.0f);
				playNote(277.18f, 0.55f * clamped, 0.15f, 9, 0.06f, 5000.0f);
			}
		}

		void SdfMusicEngine::triggerDeath()
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			m_isDead = true;
			m_deathStage = 1; // Stage 1: Lead stops queuing new notes; active notes fade naturally
			m_deathTimer = 0.0f;
			m_deathSilenceTimer = 0.0f;
		}

		bool SdfMusicEngine::isTrackDead(MusicTrack track) const
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			if (!m_isDead)
				return false;

			switch (track)
			{
				case MusicTrack::Lead:
					return m_deathStage >= 1;
				case MusicTrack::Drums:
					return m_deathStage >= 2;
				case MusicTrack::Pad:
					return m_deathStage >= 3;
				case MusicTrack::Bass:
					return m_deathStage >= 4;
				default:
					return false;
			}
		}

		void SdfMusicEngine::duck(float amount)
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			m_duckTarget = (std::min)(1.0f, m_duckTarget + amount);
			// Peak hold: sustained danger hold (up to 8.0s)
			m_duckTimer = (std::min)(8.0f, (std::max)(m_duckTimer, 2.5f) + amount * 3.5f);
			m_surgeTarget = (std::max)(0.0f, m_surgeTarget - amount * 0.8f);
			m_surgeTimer = 0.0f;

			// Immediately mark Lead as Ducked
			m_trackPlayStates[static_cast<size_t>(MusicTrack::Lead)] = TrackPlayState::Ducked;

			// Fade out any ringing lead voices smoothly over ~125 ms instead of truncating
			// their decay envelope (which caused audible pops on big ducks)
			for (auto& voice : m_activeVoices)
			{
				if (voice.instrument == 0) // Lead
				{
					voice.fadeRate = (std::max)(voice.fadeRate, LEAD_DUCK_FADE_RATE);
				}
			}
		}

		void SdfMusicEngine::surge(float amount)
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			m_duckTarget = (std::max)(0.0f, m_duckTarget - amount * 0.8f);
			m_duckTimer = 0.0f;

			// If surge was low, schedule a beat-synced impact accent on the next beat downbeat
			if (m_surgeTarget < 0.25f && m_surgeImpactCooldown <= 0.0f)
			{
				m_pendingSurgeImpact = true;
			}

			m_surgeTarget = (std::min)(1.0f, m_surgeTarget + amount);
			// Peak hold: sustained hold on hits (up to 6.0s), before slow linear decay begins
			m_surgeTimer = (std::min)(6.0f, (std::max)(m_surgeTimer, 1.8f) + amount * 2.5f);
		}

		void SdfMusicEngine::resetDynamicEffects()
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			m_ducking = 0.0f;
			m_duckTarget = 0.0f;
			m_duckTimer = 0.0f;
			m_surgeLevel = 0.0f;
			m_surgeTarget = 0.0f;
			m_surgeTimer = 0.0f;
			m_pendingSurgeImpact = false;
			m_surgeImpactCooldown = 0.0f;

			m_positiveTimer = 0.0f;
			m_positivePitchOffset = 0.0f;
			m_negativeTimer = 0.0f;
			m_concussionFilterCutoff = 20000.0f;
			m_detuneAmount = 0.0f;
			m_isDead = false;
			m_deathStage = 0;
			m_deathTimer = 0.0f;
			m_deathSilenceTimer = 0.0f;

			m_trackPlayStates.fill(TrackPlayState::Playing);
		}

		float SdfMusicEngine::quantizeToSongScale(float rawFreq) const
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
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
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			if (!m_playing || !m_currentSong)
				return;

			float dt = static_cast<float>(deltaTime);
			m_sceneTime = sceneTime;

			// Sample domain motion & fill ratio dynamically
			sampleDomainMotionAndFill(sceneTime, deltaTime);

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

			if (m_surgeTimer > 0.0f)
			{
				m_surgeTimer = (std::max)(0.0f, m_surgeTimer - dt);
			}
			else if (m_surgeTarget > 0.0f)
			{
				// Smooth slow decay from 1.0 to 0.0 over ~14-15 seconds (dt * 0.07f)
				m_surgeTarget = (std::max)(0.0f, m_surgeTarget - dt * 0.07f);
			}

			if (m_duckTimer > 0.0f)
			{
				m_duckTimer = (std::max)(0.0f, m_duckTimer - dt);
			}
			else if (m_duckTarget > 0.0f)
			{
				// Smooth recovery over ~26-28 seconds
				m_duckTarget = (std::max)(0.0f, m_duckTarget - dt * 0.038f);
			}

			// Rapid-but-smooth attack: the audible levels chase their targets instead of
			// jumping, so duck/surge fade in without clicks or abrupt timbre changes
			float attackDt = std::clamp(dt, 0.0f, 0.1f);
			float duckAttack = 1.0f - std::exp(-attackDt / DUCK_ATTACK_TIME);
			float surgeAttack = 1.0f - std::exp(-attackDt / SURGE_ATTACK_TIME);
			m_ducking += (m_duckTarget - m_ducking) * duckAttack;
			m_surgeLevel += (m_surgeTarget - m_surgeLevel) * surgeAttack;
			if (m_ducking < 1e-4f)
			{
				m_ducking = 0.0f;
			}
			if (m_surgeLevel < 1e-4f)
			{
				m_surgeLevel = 0.0f;
			}

			if (m_surgeImpactCooldown > 0.0f)
			{
				m_surgeImpactCooldown = (std::max)(0.0f, m_surgeImpactCooldown - dt);
			}

			if (m_isDead)
			{
				m_deathTimer += dt;
				if (m_deathStage >= 4)
				{
					m_deathSilenceTimer += dt;
				}
				// Generous safety timer: advance stage every 1.5s if tempo is stopped or ultra slow
				if (m_deathTimer >= 1.50f && m_deathStage >= 1 && m_deathStage < 4)
				{
					m_deathStage++;
					m_deathTimer = 0.0f;
				}
			}
			else
			{
				m_deathStage = 0;
				m_deathTimer = 0.0f;
				m_deathSilenceTimer = 0.0f;
			}

			// 2. Tempo calculations based on song base tempo, motion-derived tempo, shape tempo factor,
			// surge, and duck
			float baseTempo = m_currentSong->getTempo() * m_shapeParams.tempoFactor * m_tempoFromMotion;
			float surgeTempoMult = 1.0f + 0.18f * m_surgeLevel;
			float duckTempoMult = 1.0f - 0.16f * m_ducking; // Noticeable heavy heartbeat drag (-16% max)
			float dynamicTempo = baseTempo * surgeTempoMult * duckTempoMult;
			if (dynamicTempo < 5.0f)
				return;

			// 16th note step duration (4 steps per beat)
			float stepDuration = (60.0f / dynamicTempo) / 4.0f;
			m_stepAccumulator += dt;

			while (m_stepAccumulator >= stepDuration)
			{
				m_stepAccumulator -= stepDuration;

				int stepInBar = m_currentStep % 16;
				bool isQuarterBeat = (stepInBar % 4 == 0);
				// Half-bar boundary (every 2 beats / 8 sixteenth steps):
				bool isHalfBar = (stepInBar == 0 || stepInBar == 8);

				// Beat-synchronized track death (every 2 beats / half measure):
				// Tracks stop queuing new notes sequentially, allowing all active notes to ring out and fade naturally
				// Stage 1 (on triggerDeath): Lead stops queuing new notes; active lead notes fade naturally
				// Stage 2 (2 beats later): Drums stop queuing new hits; drum tails ring out and fade naturally
				// Stage 3 (2 beats later): Pad stops queuing new chords; existing chord voices fade naturally
				// Stage 4 (2 beats later): Bass stops queuing new notes; bass notes resonate & fade away naturally
				if (m_isDead && isHalfBar && m_deathTimer > 0.35f && m_deathStage >= 1 && m_deathStage < 4)
				{
					m_deathStage++;
					m_deathTimer = 0.0f;
				}

				// Check for beat-synced song transition on downbeats (every 4 steps = 1 beat)
				if (isQuarterBeat && m_queuedSong)
				{
					std::lock_guard<std::recursive_mutex> lock(m_songMutex);
					if (m_queuedSong)
					{
						m_currentSong = std::move(m_queuedSong);
						m_queuedSong = nullptr;
						m_loopCycle = 0;
						m_melodyDegree = 0;
						m_melodyLastInterval = 0;
						m_lastLeadStep = -99;
						m_lastLeadVel = 0.0f;
						initDomainSamples();
						sampleShapeParameters();

						if (isSongEmpty(m_currentSong))
						{
							for (auto& voice : m_activeVoices)
							{
								if (!voice.finished && voice.fadeRate <= 0.0f)
								{
									voice.fadeRate = (std::max)(voice.fadeGain, 0.01f) / 0.005f;
								}
							}
							m_rack = InstrumentRack{};
						}
						else
						{
							selectInstrumentRack(m_currentSong->getFingerprint());
							int root = m_currentSong->getRootMidi();
							playNote(m_currentSong->midiToFrequency(root), 0.45f, 1.2f, 4, 0.0f, 5500.0f);
						}
					}
				}

				on16thStep(m_currentStep);
				m_currentStep = (m_currentStep + 1) % 512;
				if (m_currentStep == 0)
				{
					++m_loopCycle;
				}
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
			float surgeTempoMult = 1.0f + 0.18f * m_surgeLevel;
			float dynamicTempo = baseTempo * surgeTempoMult;
			float beatSec = 60.0f / (std::max)(20.0f, dynamicTempo);

			int stepInBar = step % 16;						 // 16th note step in 4/4 bar (0..15)
			int barInSong = (step / 16) % 32;				 // 32-bar evolving soundtrack loop (0..31)
			int phraseInSong = barInSong / 4;				 // 4-bar phrase index (0..7)
			int barInPhrase = barInSong % 4;				 // Bar within 4-bar phrase (0..3)
			int stepInPhrase = barInPhrase * 16 + stepInBar; // Step across the 4-bar phrase (0..63)
			int bar = barInPhrase;							 // Bar index within current 4-bar progression (0..3)

			// Motion dictates pause frequency and note sustained duration:
			// If motion is low (still shape): notes ring out longer and pauses are frequent.
			// If motion is high (rapid moving shape): notes are shorter, active, continuous groove.
			float motionFactor = m_motionNorm;
			float noteLengthMult = 1.6f - 0.6f * motionFactor;

			// Surface complexity is tracked for shape inspection (dissonance/tension effects removed).
			float tension = 0.0f;

			// -------------------------------------------------------------
			// Procedural Arrangement & Track Contrasts (32-Bar A/B Soundtrack Form)
			// -------------------------------------------------------------
			uint32_t arch = m_currentSong ? (m_currentSong->getFingerprint().structuralHash % 3) : 0;
			bool arrBass = true;
			bool arrPad = true;
			bool arrLead = true;
			bool arrDrums = true;

			switch (phraseInSong)
			{
				case 0: // Phrase 0 (Bars 1-4): Intro Groove (Lead rests; establishes vibe)
					arrLead = false;
					if (arch == 1)
					{
						// Ambient-First intro: floating chords only
						arrBass = false;
						arrDrums = false;
					}
					break;
				case 1: // Phrase 1 (Bars 5-8): Section A1 (Main Theme - Full Band)
					break;
				case 2: // Phrase 2 (Bars 9-12): Section A2 (Theme Answer & Variation)
					if (arch == 2)
					{
						arrPad = false; // Driving Minimalist drops pad for rhythmic space
					}
					break;
				case 3: // Phrase 3 (Bars 13-16): Groove Breather (Lead rests for 4 full bars!)
					arrLead = false;
					if (arch == 1 && barInPhrase >= 2)
					{
						arrDrums = false;
					}
					break;
				case 4: // Phrase 4 (Bars 17-20): Section B Bridge (New Chord Progression Lift)
					break;
				case 5: // Phrase 5 (Bars 21-24): Atmospheric Breakdown (Pads + Sparse Lead, No Drums)
					arrDrums = false;
					arrBass = (arch == 2);
					break;
				case 6: // Phrase 6 (Bars 25-28): Section A3 (Full Return with Cycle Variation)
					break;
				case 7: // Phrase 7 (Bars 29-32): Outro / Cool-Down (Lead rests, Drums wind down)
					arrLead = false;
					if (barInPhrase >= 2)
					{
						arrDrums = false;
					}
					break;
			}

			// Micro-arrangement: 1-beat cadence pause on last bar of key phrases before transitions
			if (barInPhrase == 3 && stepInBar >= 12 && (phraseInSong == 3 || phraseInSong == 5 || phraseInSong == 7))
			{
				arrDrums = false;
				arrBass = false;
			}

			// -------------------------------------------------------------
			// Dynamic Game Feedback Overrides (Surge & Duck)
			// -------------------------------------------------------------
			bool isDuckingActive = (m_duckTimer > 0.0f || m_ducking > 0.01f);
			bool isSurgeActive = (m_surgeTimer > 0.0f || m_surgeLevel > 0.08f);

			auto evaluateTrack = [&](MusicTrack track, bool userToggle, bool& arrGate) -> bool
			{
				TrackPlayState state;
				if (!userToggle)
				{
					state = TrackPlayState::Muted;
					arrGate = false;
				}
				else if (m_isDead && isTrackDead(track))
				{
					state = TrackPlayState::Dead;
					arrGate = false;
				}
				else if (track == MusicTrack::Lead && isDuckingActive)
				{
					state = TrackPlayState::Ducked;
					arrGate = false; // Duck ALWAYS kills lead wave completely!
				}
				else if (isSurgeActive)
				{
					state = TrackPlayState::Surged;
					arrGate = true; // Surge forces ANY paused track ON (immediate drop/climax!)
				}
				else if (!arrGate && !isDuckingActive)
				{
					state = TrackPlayState::Paused; // Paused by song structure
				}
				else
				{
					state = isDuckingActive ? TrackPlayState::Ducked : TrackPlayState::Playing;
				}
				m_trackPlayStates[static_cast<size_t>(track)] = state;
				return (state == TrackPlayState::Playing || state == TrackPlayState::Surged ||
						(state == TrackPlayState::Ducked && track != MusicTrack::Lead));
			};

			bool playBass = evaluateTrack(MusicTrack::Bass, m_tracks.bass, arrBass);
			bool playPad = evaluateTrack(MusicTrack::Pad, m_tracks.pad, arrPad);
			bool playLead = evaluateTrack(MusicTrack::Lead, m_tracks.lead, arrLead);
			bool playDrums = evaluateTrack(MusicTrack::Drums, m_tracks.drums, arrDrums);

			// -------------------------------------------------------------
			// Scale-Aware Multi-Section Harmonic Foundation (A / B / Outro Progressions)
			// -------------------------------------------------------------
			const MusicalScale activeScale = m_currentSong->getScale();
			const bool isPentatonic =
				(activeScale == MusicalScale::PentatonicMajor || activeScale == MusicalScale::PentatonicMinor);
			const int scaleSize = isPentatonic ? 5 : 7;

			// Helper: returns the scale degree for a chord tone index (0=Root, 1=5th, 2=3rd, 3=7th/6th, 4=Octave)
			auto getChordToneDegree = [&](int rootDeg, int toneIdx) -> int
			{
				static constexpr int heptatonicChord[5] = {0, 4, 2, 6, 7};
				static constexpr int pentMajorChord[5] = {0, 3, 2, 4, 5}; // Root=0, 5th=3, 3rd=2, 6th=4, Oct=5
				static constexpr int pentMinorChord[5] = {0, 3, 1, 4, 5}; // Root=0, 5th=3, b3rd=1, b7th=4, Oct=5
				int clampedIdx = std::clamp(toneIdx, 0, 4);
				if (activeScale == MusicalScale::PentatonicMajor)
					return rootDeg + pentMajorChord[clampedIdx];
				if (activeScale == MusicalScale::PentatonicMinor)
					return rootDeg + pentMinorChord[clampedIdx];
				return rootDeg + heptatonicChord[clampedIdx];
			};

			const int degI = 0;
			const int degIV = isPentatonic ? 2 : 3;
			const int degV = isPentatonic ? 3 : 4;
			const int degVi = isPentatonic ? 4 : 5;

			int bassDegree = degI;
			if (phraseInSong == 4 || phraseInSong == 5)
			{
				// Section B (Bridge & Breakdown): vi - IV - I - V (emotional harmonic lift)
				const int bridgeProg[4] = {degVi, degIV, degI, degV};
				bassDegree = bridgeProg[bar];
			}
			else if (phraseInSong == 2 || phraseInSong == 3)
			{
				// Section A2 & Breather: I - vi - IV - V (classic storytelling progression)
				const int a2Prog[4] = {degI, degVi, degIV, degV};
				bassDegree = a2Prog[bar];
			}
			else if (phraseInSong == 7)
			{
				// Outro / Turnaround: IV - V - I - I (warm resolution home)
				const int outroProg[4] = {degIV, degV, degI, degI};
				bassDegree = outroProg[bar];
			}
			else
			{
				// Section A1 & A3: I - IV - (vi or I) - V
				const int bar2Deg = (m_shapeParams.variation > 0.45f || (m_loopCycle % 2 == 1)) ? degVi : degI;
				const int a1Prog[4] = {degI, degIV, bar2Deg, degV};
				bassDegree = a1Prog[bar];
			}

			// -------------------------------------------------------------
			// Beat-Quantized Surge Impact (Plays strictly on the beat grid)
			// -------------------------------------------------------------
			bool isQuarterBeat = (stepInBar % 4 == 0);
			if (isQuarterBeat && m_pendingSurgeImpact)
			{
				m_pendingSurgeImpact = false;
				m_surgeImpactCooldown = 2.5f;
				if (playDrums)
				{
					playNote(9000.0f, 0.40f + 0.25f * m_surgeLevel, 0.35f, 7, 0.0f, 13000.0f);
				}
			}

			// -------------------------------------------------------------
			// LAYER 1: BASS LINE
			// -------------------------------------------------------------
			if (playBass)
			{
				bool isEighthBeat = (stepInBar % 2 == 0);

				bool triggerBass = isQuarterBeat;
				if (!triggerBass && isEighthBeat && (m_shapeParams.syncopation > 0.35f || m_surgeLevel > 0.25f))
				{
					if (m_surgeLevel > 0.35f ||
						stepHash(step, 101) < (m_shapeParams.syncopation - 0.15f) * (0.3f + 0.7f * motionFactor))
					{
						triggerBass = true;
					}
				}

				if (triggerBass)
				{
					// Bass octave is -2 (deep fundamental register: MIDI 36 = C2 = 65 Hz)
					// When surging high (> 0.60f), bounce an octave higher on offbeats for driving groove
					int bassOctave = (m_surgeLevel > 0.60f && !isQuarterBeat) ? -1 : -2;

					int midi = m_currentSong->getScaleDegreeMidi(bassDegree, bassOctave) +
							   static_cast<int>(m_positivePitchOffset);
					float freq = m_currentSong->midiToFrequency(midi);

					// Ducking organic pitch tension sag (-1.8% max)
					if (m_ducking > 0.05f)
					{
						freq *= (1.0f - 0.018f * m_ducking);
					}

					if (m_detuneAmount > 0.001f)
					{
						freq *= (1.0f - m_detuneAmount * (step % 2 == 0 ? 1.0f : -1.0f));
					}

					float cutoff = (420.0f + 380.0f * m_shapeParams.bassWeight) * (1.0f + 0.85f * m_surgeLevel) *
								   (1.0f - 0.30f * m_ducking);
					cutoff = (std::max)(150.0f, cutoff);
					float dur = beatSec * 1.05f * noteLengthMult * (1.0f + 0.15f * m_ducking);
					float vel = (0.72f + 0.23f * m_shapeParams.bassWeight) * (1.0f + 0.18f * m_surgeLevel) *
								(1.0f + 0.12f * m_ducking);

					playNote(freq, vel, dur, 1, 0.0f, cutoff);

					// Sub-bass doubling for warm, low foundation
					if (m_shapeParams.bassWeight > 0.20f || m_surgeLevel > 0.15f || m_ducking > 0.15f)
					{
						float subVel = vel * (0.55f + 0.25f * m_shapeParams.bassWeight + 0.22f * m_ducking);
						playNote(freq * 0.5f, subVel, dur * 1.18f, 1, 0.0f,
								 (220.0f + 80.0f * m_surgeLevel) * (1.0f - 0.20f * m_ducking));
					}
				}
			}

			// -------------------------------------------------------------
			// LAYER 2: HARMONY & CHORDS (Warm Pads / EPs)
			// -------------------------------------------------------------
			if (playPad)
			{
				bool triggerChord = (stepInBar % 8 == 0);
				if (!triggerChord && (stepInBar % 4 == 0) &&
					(m_shapeParams.harmonyRichness > 0.50f || m_surgeLevel > 0.30f))
				{
					triggerChord = (m_surgeLevel > 0.30f) ||
								   (stepHash(step, 179) < m_shapeParams.harmonyRichness * (0.4f + 0.6f * motionFactor));
				}

				if (triggerChord)
				{
					int noteCount = 2;
					if (m_ducking > 0.40f)
					{
						// In heavy danger: grounded open fifths (root + fifth)
						noteCount = 2;
					}
					else if (m_shapeParams.harmonyRichness >= 0.65f || m_surgeLevel > 0.50f)
					{
						noteCount = 4; // 7th / 6th chord
					}
					else if (m_shapeParams.harmonyRichness >= 0.35f || m_surgeLevel > 0.20f)
					{
						noteCount = 3; // Triad
					}

					float pans[4] = {-0.30f, 0.30f, -0.10f, 0.40f};
					float dur = beatSec * 1.8f * noteLengthMult;
					float vel = (0.28f + 0.22f * m_shapeParams.harmonyRichness) * (1.0f + 0.20f * m_surgeLevel);
					float cutoff = (700.0f + 1500.0f * m_shapeParams.brightness + 2200.0f * m_surgeLevel) *
								   (1.0f - 0.35f * m_ducking);
					cutoff = (std::max)(210.0f, cutoff);

					for (int i = 0; i < noteCount; ++i)
					{
						int degree = getChordToneDegree(bassDegree, i);
						// Chords sit at octave -1 or 0 (Middle C range)
						int octave = (m_shapeParams.brightness > 0.75f || m_surgeLevel > 0.50f) ? 0 : -1;

						int midi =
							m_currentSong->getScaleDegreeMidi(degree, octave) + static_cast<int>(m_positivePitchOffset);
						float freq = m_currentSong->midiToFrequency(midi);
						if (m_ducking > 0.05f)
						{
							freq *= (1.0f - 0.012f * m_ducking);
						}
						playNote(freq, vel, dur, 4, pans[i], cutoff);
					}
				}
			}

			// -------------------------------------------------------------
			// LAYER 3: MELODIC LEAD (4-Bar Spacious Story + Section B Contrast + Cycle Evolution)
			// -------------------------------------------------------------
			if (playLead && !isDuckingActive)
			{
				LeadStyle style = getEffectiveLeadStyle();
				// In Auto mode, shift to a contrasting spacious style during Section B (Bridge & Breakdown)
				if (m_leadStyle == LeadStyle::Auto)
				{
					if (phraseInSong == 5)
					{
						style = LeadStyle::LyricalSinging; // Floating, expressive long notes in Breakdown
					}
					else if (phraseInSong == 4)
					{
						style =
							(style == LeadStyle::RepeatingHook) ? LeadStyle::ChordArpeggio : LeadStyle::LyricalSinging;
					}
				}

				const uint32_t songSeed =
					m_currentSong ? (m_currentSong->getFingerprint().structuralHash & 0xFFFFu) : 1337u;
				const int sectionVariant = (phraseInSong / 2) + m_loopCycle * 3;

				bool isDownbeat = (stepInBar % 4 == 0);
				bool isEighth = (stepInBar % 2 == 0);
				bool isSyncopatedPickup = (stepInBar % 4 == 3);
				bool isCadenceStep = (barInPhrase == 3 && stepInBar == 0);

				constexpr float PI = 3.14159265358979323846f;
				// Sweep a full circle once across the entire 4-bar phrase (64 steps) instead of every 1 bar!
				// Rotate the starting angle on each section and loop cycle so the melody explores new contours.
				float sectionAngleOffset = static_cast<float>((phraseInSong / 2) + m_loopCycle) * (0.25f * PI);
				float phraseTheta = (static_cast<float>(stepInPhrase) / 64.0f) * 2.0f * PI + sectionAngleOffset;

				float sampleR = m_currentSong->getSampleRadius();
				if (sampleR <= 0.01f)
				{
					sampleR = 20.0f;
				}

				float params[12]{};
				packSdfParameterPrefix(params, m_currentSong->getParameters(), static_cast<float>(m_sceneTime));
				params[9] = sampleR * std::cos(phraseTheta);
				params[10] = sampleR * std::sin(phraseTheta);
				float dist = m_currentSong->getRawShapeExpression()->getValue(params);
				float reach = (std::max)(0.0f, sampleR - dist);
				float normReach = std::clamp((reach - 2.0f) / 25.0f, 0.0f, 1.0f);

				bool triggerLead = false;
				int noteDegree = bassDegree;
				float noteBeats = 0.50f;
				float velMult = 1.0f;

				if (style == LeadStyle::ChordArpeggio)
				{
					// Style 2: Spacious Synthwave / Groove Chord Arpeggiator
					// Walks strictly through the active chord tones with breathing space in Bars 1 and 3
					if ((barInPhrase == 1 && stepInBar >= 10) || (barInPhrase == 3 && stepInBar >= 6))
					{
						triggerLead = false; // Generous breathing gaps at half-phrase and end-of-phrase
					}
					else if (isEighth)
					{
						// 8th-note backbone with musical breath gaps
						float restChance = (stepInBar == 6 || stepInBar == 12 || stepInBar == 14) ? 0.38f : 0.12f;
						int arpStepKey = stepInPhrase + static_cast<int>(songSeed % 97u) + sectionVariant * 17;
						triggerLead = (stepHash(arpStepKey, 503) >= restChance);
					}
					else if (isSyncopatedPickup && m_surgeLevel > 0.30f)
					{
						float arp16Chance = 0.25f + 0.45f * m_surgeLevel;
						triggerLead = (stepHash(stepInPhrase + static_cast<int>(songSeed % 97u), 509) < arp16Chance);
					}

					if (triggerLead)
					{
						// Ordered low-to-high chord tones: 0=Root, 2=3rd, 1=5th, 3=7th/6th, 4=Octave
						static constexpr int orderedTones[6] = {0, 2, 1, 3, 4, 1};
						int patternType =
							static_cast<int>((songSeed + static_cast<uint32_t>(barInPhrase + sectionVariant)) % 3u);
						int slot = (stepInBar / 2) % 6;
						int toneOrderIdx = slot;
						if (patternType == 1)
						{
							toneOrderIdx = 5 - slot; // Descending arpeggio
						}
						else if (patternType == 2)
						{
							static constexpr int pendulum[8] = {0, 1, 2, 4, 3, 2, 1, 2};
							toneOrderIdx = pendulum[(stepInBar / 2) % 8];
						}

						// Shift arpeggio inversion up when the SDF contour reaches outward or in Bar 2 climb
						if ((normReach > 0.60f || barInPhrase == 2) && toneOrderIdx < 4)
						{
							toneOrderIdx += 1;
						}

						noteDegree = getChordToneDegree(bassDegree, orderedTones[toneOrderIdx]);
						if (isCadenceStep)
						{
							noteDegree = bassDegree;
							noteBeats = 1.50f;
						}
						else
						{
							noteBeats = isEighth ? 0.45f : 0.22f;
						}
						velMult = isDownbeat ? 1.0f : (isEighth ? 0.82f : 0.65f);
					}
				}
				else if (style == LeadStyle::LyricalSinging)
				{
					// Style 3: Slow, Expressive, Sustained Chord-Tone Melody (plenty of breathing space)
					if (stepInBar == 0)
					{
						triggerLead = true;
						noteBeats = (barInPhrase == 3) ? 2.40f : 1.75f;
						velMult = 1.02f;
					}
					else if (stepInBar == 6 && barInPhrase != 3 &&
							 (m_shapeParams.melodyDensity > 0.25f || m_shapeParams.syncopation > 0.30f))
					{
						triggerLead = (stepHash(barInPhrase + sectionVariant * 7 + static_cast<int>(songSeed % 53u),
												601) < 0.72f);
						noteBeats = 1.25f;
						velMult = 0.85f;
					}
					else if (stepInBar == 10 && barInPhrase == 2 && m_shapeParams.melodyDensity > 0.40f)
					{
						triggerLead = true;
						noteBeats = 1.30f;
						velMult = 0.80f;
					}

					if (triggerLead)
					{
						// Choose an expressive singing chord tone (3rd, 5th, Root, or Octave) shaped by 4-bar contour
						static constexpr int singingTones[4] = {2, 1, 0, 4}; // 3rd, 5th, Root, Octave
						int contourChoice = (static_cast<int>(std::round(normReach * 3.0f)) + barInPhrase +
											 (stepInBar / 4) + sectionVariant) %
											4;
						if (isCadenceStep)
						{
							noteDegree = (phraseInSong == 4 || phraseInSong == 5) ? bassDegree : 0;
							noteBeats = 2.40f;
						}
						else
						{
							noteDegree = getChordToneDegree(bassDegree, singingTones[contourChoice]);
						}
					}
				}
				else
				{
					// Style 1 (Default): 4-Bar Call -> Sparse Answer -> Climb -> Resolution Story
					// Bar 0: Primary Call motif (plays steps 0..13)
					// Bar 1: Short Answer (plays steps 0..7, then rests for half a bar!)
					// Bar 2: Developed Climb (new rhythm seed + higher chord inversion on steps 0..11)
					// Bar 3: Cadence Resolution on Beat 1 (step 0), then 3 full beats of silence!
					int motifStepKey = stepInBar + static_cast<int>(songSeed % 251u) * 16;
					if (barInPhrase == 1)
					{
						motifStepKey += 7 + sectionVariant * 13; // Answering variation in Bar 1
					}
					else if (barInPhrase == 2)
					{
						motifStepKey += 23 + sectionVariant * 29; // Developed variation in Bar 2 (never clones Bar 0)
					}

					float triggerChance = 0.0f;
					if (barInPhrase == 3)
					{
						// Bar 3: Only play the resolving cadence note on Beat 1 (step 0), then rest for 3 beats!
						triggerChance = (stepInBar == 0) ? 1.0f : 0.0f;
					}
					else if (barInPhrase == 1 && stepInBar >= 8)
					{
						// Second half of Bar 1: Half-bar silence so the Call/Answer can breathe!
						triggerChance = 0.0f;
					}
					else if ((barInPhrase == 0 && stepInBar >= 14) || (barInPhrase == 2 && stepInBar >= 12))
					{
						// End-of-bar breath gaps before the next bar begins
						triggerChance = 0.0f;
					}
					else if (stepInBar == 0)
					{
						triggerChance = (barInPhrase == 0) ? 0.90f : 0.75f;
					}
					else if (isDownbeat)
					{
						triggerChance = 0.42f + 0.30f * m_shapeParams.melodyDensity;
					}
					else if (isEighth)
					{
						triggerChance = 0.16f + 0.32f * m_shapeParams.melodyDensity;
					}
					else if (isSyncopatedPickup && m_surgeLevel > 0.25f)
					{
						triggerChance = 0.12f + 0.30f * m_shapeParams.syncopation;
					}

					triggerLead = isCadenceStep || (stepHash(motifStepKey, 421) < triggerChance);

					if (triggerLead)
					{
						// 4-bar melodic contour: Bar 2 climbs +1 chord tone higher for a natural story arc
						static constexpr int barArch[16] = {0, 1, 2, 1, 3, 2, 4, 3, 2, 3, 4, 2, 1, 2, 1, 0};
						int phraseClimb = (barInPhrase == 2) ? 2 : ((barInPhrase == 1) ? 1 : 0);
						int contourSteps = static_cast<int>(std::round(normReach * static_cast<float>(scaleSize)));
						int rawDegree =
							bassDegree + ((barArch[stepInBar] + contourSteps + phraseClimb) % (scaleSize + 2));

						if (isCadenceStep)
						{
							noteDegree = (phraseInSong == 4 || phraseInSong == 5) ? bassDegree : 0;
							noteBeats = 1.85f;
						}
						else if (isDownbeat)
						{
							// Every quarter-beat lands on a true chord tone (Root, 3rd, or 5th) for rock-solid harmony
							int bestDist = 999;
							int snapped = getChordToneDegree(bassDegree, 0);
							for (int oct = -1; oct <= 1; ++oct)
							{
								for (int tIdx : {0, 2, 1}) // Root, 3rd, 5th
								{
									int candidate = getChordToneDegree(bassDegree, tIdx) + oct * scaleSize;
									int d = std::abs(rawDegree - candidate);
									if (d < bestDist)
									{
										bestDist = d;
										snapped = candidate;
									}
								}
							}
							noteDegree = snapped;
							noteBeats = 0.80f;
						}
						else
						{
							// Offbeats step smoothly without jarring leaps
							int interval = rawDegree - m_melodyDegree;
							if (std::abs(interval) > 2)
							{
								rawDegree = m_melodyDegree + ((interval > 0) ? 1 : -1);
							}
							noteDegree = rawDegree;
							noteBeats = isEighth ? 0.44f : 0.22f;
						}

						m_melodyLastInterval = noteDegree - m_melodyDegree;
						m_melodyDegree = noteDegree;
						velMult = isDownbeat ? 0.96f : (isEighth ? 0.76f : 0.62f);
					}
				}

				if (triggerLead)
				{
					// Register Placement
					int octaveOffset = (m_shapeParams.brightness > 0.70f) ? 1 : 0;
					if (barInPhrase == 2 && stepInBar >= 4 && stepInBar <= 10 && style != LeadStyle::LyricalSinging &&
						m_surgeLevel > 0.25f)
					{
						octaveOffset += 1;
					}

					noteDegree = (std::max)(noteDegree, 0);

					int midi = m_currentSong->getScaleDegreeMidi(noteDegree, octaveOffset) +
							   static_cast<int>(m_positivePitchOffset);

					// Vocal Range Guard: Keep lead in a warm, pleasant mid-register (A3 to F5)
					while (midi > 77)
						midi -= 12;
					while (midi < 57)
						midi += 12;

					float freq = m_currentSong->midiToFrequency(midi);
					if (m_detuneAmount > 0.001f)
					{
						freq *= (1.0f - m_detuneAmount * (step % 2 == 0 ? 1.0f : -1.0f));
					}

					float dur = beatSec * noteBeats * noteLengthMult;
					// Warmer default filter cutoff so the Lead sits comfortably in the mix for long sessions,
					// opening up brightly when Surge is triggered!
					float cutoff = (1050.0f + 1350.0f * m_shapeParams.brightness) * (1.0f + 0.45f * m_surgeLevel);
					cutoff = std::clamp(cutoff, 800.0f, 5200.0f);

					float baseVel = (0.27f + 0.14f * m_shapeParams.melodyDensity) * (1.0f + 0.22f * m_surgeLevel);
					float vel = baseVel * velMult;
					float pan = 0.20f * std::sin(phraseTheta);

					playNote(freq, vel, dur, 0, pan, cutoff);

					// Save note for dotted-8th stereo delay echo
					m_lastLeadStep = step;
					m_lastLeadFreq = freq;
					m_lastLeadVel = vel;
					m_lastLeadDur = dur;
					m_lastLeadPan = pan;
					m_lastLeadCutoff = cutoff;
				}
				else
				{
					// Dotted-8th (3 sixteenth steps) stereo delay echo on empty steps for studio depth & space
					int stepsSinceLast =
						(step >= m_lastLeadStep) ? (step - m_lastLeadStep) : (step + 512 - m_lastLeadStep);
					if (stepsSinceLast == 3 && m_lastLeadVel > 0.05f)
					{
						float echoVel = m_lastLeadVel * 0.28f;
						float echoPan = std::clamp(-m_lastLeadPan * 1.25f, -0.45f, 0.45f);
						float echoCutoff = (std::min)(m_lastLeadCutoff * 0.60f, 1350.0f);
						playNote(m_lastLeadFreq, echoVel, m_lastLeadDur * 0.85f, 0, echoPan, echoCutoff, 0.7071f);
					}
				}
			}

			// -------------------------------------------------------------
			// LAYER 4: PERCUSSION (Punchy Kick, Snappy Snare, Relaxed Hi-Hat)
			// -------------------------------------------------------------
			if (playDrums)
			{
				bool isEighthBeat = (stepInBar % 2 == 0);
				float effPercEnergy = m_shapeParams.percEnergy * (0.35f + 0.65f * motionFactor);
				if (m_surgeLevel > 0.05f)
				{
					effPercEnergy = std::clamp(effPercEnergy + m_surgeLevel * 0.50f, 0.0f, 1.0f);
				}

				// Kick drum on beats 1 and 3 (step 0 and 8), plus syncopation.
				// Driving 4-on-the-floor kick when surge is high (> 0.45f)!
				bool triggerKick = (stepInBar == 0 || stepInBar == 8);
				if (!triggerKick && m_surgeLevel > 0.45f && (stepInBar == 4 || stepInBar == 12))
				{
					triggerKick = true; // 4-on-the-floor pulse on all 4 quarter beats!
				}
				else if (!triggerKick &&
						 (effPercEnergy > 0.45f || m_shapeParams.syncopation > 0.40f || m_surgeLevel > 0.25f))
				{
					if (stepInBar == 6 || (effPercEnergy > 0.65f && stepInBar == 14))
					{
						triggerKick = true;
					}
				}

				if (triggerKick)
				{
					float kickVel =
						(0.78f + 0.20f * effPercEnergy) * (1.0f + 0.15f * m_surgeLevel) * (1.0f + 0.12f * m_ducking);
					float kickCutoff = (500.0f + 250.0f * m_surgeLevel) * (1.0f - 0.20f * m_ducking);
					float kickPitch = (m_ducking > 0.35f) ? 48.0f : 60.0f; // Deep, heavy industrial thud
					playNote(kickPitch, kickVel, 0.34f * (1.0f + 0.20f * m_ducking), 5, 0.0f,
							 kickCutoff); // instrument 5 = Kick
				}

				// Complexity tension at low motion: slow "lub-dub" heartbeat pulse beneath the calm tempo
				if (m_motionNorm < 0.35f && tension > 0.40f && stepInBar == 2)
				{
					playNote(48.0f, (0.42f + 0.30f * tension) * (1.0f + 0.12f * m_ducking), 0.30f, 5, 0.0f, 380.0f);
				}

				// Snare / Clap on beats 2 & 4 (step 4 and 12)
				// In heavy ducking (>0.45f), switch to half-time snare on beat 4 only for tense space
				bool triggerSnare = false;
				if (m_ducking > 0.45f)
				{
					triggerSnare = (stepInBar == 12); // Half-time single snare hit
				}
				else
				{
					triggerSnare = (stepInBar == 4 || stepInBar == 12);
					if (!triggerSnare && (effPercEnergy > 0.65f || m_surgeLevel > 0.35f) &&
						(stepInBar == 15 && barInPhrase == 3))
					{
						triggerSnare = true; // Subtle ghost snare turnaround at end of 4-bar phrase
					}
				}

				if (triggerSnare)
				{
					float snareVel =
						(0.52f + 0.24f * effPercEnergy) * (1.0f + 0.18f * m_surgeLevel) * (1.0f - 0.30f * m_ducking);
					float snareCutoff = (4200.0f + 2000.0f * m_surgeLevel) * (1.0f - 0.30f * m_ducking);
					playNote(180.0f, snareVel, 0.18f, 6, 0.0f, snareCutoff); // instrument 6 = Snare
				}

				// Hi-hat: relaxed 8th-note groove by default to avoid ear fatigue; 16th-note drive on Surge
				bool triggerHiHat = false;
				if (m_ducking > 0.45f)
				{
					triggerHiHat = (stepInBar % 4 == 0); // Quarter-note sparse ticking
				}
				else
				{
					triggerHiHat = isEighthBeat;
					if (!triggerHiHat && m_surgeLevel > 0.30f)
					{
						triggerHiHat = true; // Continuous 16th-note groove during Surge!
					}
					else if (!triggerHiHat && effPercEnergy > 0.72f && (stepHash(step, 809) < 0.35f))
					{
						triggerHiHat = true; // Occasional light ghost 16th tick on very energetic shapes
					}
				}

				if (triggerHiHat)
				{
					bool openHat = (stepInBar % 4 == 2) && (effPercEnergy > 0.50f || m_surgeLevel > 0.25f);
					float decay = openHat ? 0.13f : 0.042f;
					float pan = (step % 2 == 0) ? 0.16f : -0.16f;
					float cutoff = (6200.0f + 2600.0f * m_shapeParams.brightness + 3000.0f * m_surgeLevel) *
								   (1.0f - 0.40f * m_ducking);
					float vel = (openHat ? 0.26f : 0.19f) * (0.6f + 0.4f * effPercEnergy) *
								(1.0f + 0.20f * m_surgeLevel) * (1.0f - 0.40f * m_ducking);

					if (!isEighthBeat)
					{
						vel *= 0.58f; // Softer ghost 16th notes
					}

					playNote(8000.0f, vel, decay, 7, pan, cutoff); // instrument 7 = HiHat
				}
				else if (tension > 0.50f && (stepInBar % 2 == 1) && stepHash(step, 1301) < 1.2f * (tension - 0.50f))
				{
					// Complexity tension: quiet off-grid ghost hats rub against the slow grid
					float vel = 0.12f * tension * (1.0f - 0.40f * m_ducking);
					float pan = (step % 2 == 0) ? 0.22f : -0.22f;
					playNote(8000.0f, vel, 0.035f, 7, pan, 9000.0f);
				}

				// Ghost percussion blip on offbeats
				if (m_ducking <= 0.30f && (m_shapeParams.variation > 0.50f || m_surgeLevel > 0.30f) &&
					stepInBar >= 12 && (motionFactor > 0.3f || m_surgeLevel > 0.2f))
				{
					if (stepHash(step, 911) < (m_shapeParams.variation * 0.35f + m_surgeLevel * 0.45f))
					{
						playNote(2500.0f, 0.20f * effPercEnergy * (1.0f - 0.30f * m_ducking), 0.04f, 7, 0.25f, 5000.0f);
					}
				}
			}
		}

		void SdfMusicEngine::setInstrumentWaveType(Instrument instrument, WaveType waveType)
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			switch (instrument)
			{
				case Instrument::Lead:
					m_rack.lead = waveType;
					break;
				case Instrument::Bass:
					m_rack.bass = waveType;
					break;
				case Instrument::Pad:
					m_rack.pad = waveType;
					break;
				default:
					break;
			}
		}

		WaveType SdfMusicEngine::getInstrumentWaveType(Instrument instrument) const
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			switch (instrument)
			{
				case Instrument::Lead:
					return m_rack.lead;
				case Instrument::Bass:
					return m_rack.bass;
				case Instrument::Pad:
					return m_rack.pad;
				default:
					return WaveType::SoftSine;
			}
		}

		void SdfMusicEngine::playNote(float freq, float amp, float durationSec, Instrument instrument, float pan,
									  float filterCutoff, float filterQ)
		{
			playNote(freq, amp, durationSec, static_cast<int>(instrument), pan, filterCutoff, filterQ);
		}

		void SdfMusicEngine::playNote(int midiNote, float amp, float durationSec, Instrument instrument, float pan,
									  float filterCutoff, float filterQ)
		{
			float freq = midiToFrequency(midiNote);
			playNote(freq, amp, durationSec, static_cast<int>(instrument), pan, filterCutoff, filterQ);
		}

		void SdfMusicEngine::playNote(float freq, float amp, float durationSec, int instrument, float pan,
									  float filterCutoff, float filterQ)
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			// Lead is strictly killed during ducking
			if (instrument == 0 && (m_duckTimer > 0.0f || m_ducking > 0.01f))
				return;

			// UI feedback and Death sounds (instruments >= 8) bypass music ducking and shape fill volume scaling
			float masterAmp = 0.0f;
			if (instrument >= 8)
			{
				masterAmp = amp * (m_volume > 0.01f ? m_volume : 0.65f);
			}
			else
			{
				float duckMult = (std::max)(0.65f, 1.0f - m_ducking * 0.22f);
				float surgeAmpBoost = 1.0f + 0.15f * m_surgeLevel;
				float fillMult = (m_volumeFromFill > 0.01f) ? m_volumeFromFill : 0.65f;
				float vol = (m_volume > 0.01f) ? m_volume : 0.65f;
				float trackVol = 1.0f;
				if (instrument == 0)
					trackVol = m_trackVolumes.lead;
				else if (instrument == 1)
					trackVol = m_trackVolumes.bass;
				else if (instrument == 4)
					trackVol = m_trackVolumes.pad;
				else if (instrument >= 5 && instrument <= 7)
					trackVol = m_trackVolumes.drums;
				masterAmp = amp * vol * fillMult * duckMult * surgeAmpBoost * trackVol;
			}

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
			newVoice.filterCutoff =
				(instrument >= 8) ? filterCutoff : (std::min)(filterCutoff, m_concussionFilterCutoff);
			newVoice.filterQ = (filterQ > 0.0f) ? filterQ : ((instrument == 0) ? 1.414f : 0.7071f);
			newVoice.filterS1 = 0.0f;
			newVoice.filterS2 = 0.0f;
			newVoice.rngState =
				static_cast<uint32_t>(m_currentStep * 1664525u + m_activeVoices.size() * 1013904223u + 12345u);
			newVoice.drumKit = m_rack.drumKit;

			// Assign waveform & per-voice parameter based on channel role
			if (instrument == 0) // Lead
			{
				newVoice.waveType = m_rack.lead;
				if (m_rack.lead == WaveType::FMPluck)
					newVoice.waveParam = m_rack.fmModIndex;
				else if (m_rack.lead == WaveType::PulseSquare)
					newVoice.waveParam = std::clamp(m_rack.pulseWidth, 0.05f, 0.50f);
				else if (m_rack.lead == WaveType::Wavefolder)
					newVoice.waveParam = m_rack.foldDrive;
				else
					newVoice.waveParam = 0.0f;
			}
			else if (instrument == 1) // Bass
			{
				newVoice.waveType = m_rack.bass;
				newVoice.waveParam =
					(m_rack.bass == WaveType::PulseSquare) ? std::clamp(m_rack.pulseWidth, 0.05f, 0.50f) : 0.0f;
			}
			else if (instrument == 4) // Pad
			{
				newVoice.waveType = m_rack.pad;
				newVoice.waveParam = (m_rack.pad == WaveType::Wavefolder) ? m_rack.foldDrive : 0.0f;
			}
			else
			{
				newVoice.waveType = WaveType::SoftSine;
				newVoice.waveParam = 0.0f;
			}

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
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			if (!m_playing || frameCount == 0 || channels != 2)
				return;

			const float sampleRateF = static_cast<float>(m_sampleRate);

			auto evaluateWaveform = [](WaveType type, float phaseNorm, float param, float time, float decay) -> float
			{
				const float p = phaseNorm * 2.0f * static_cast<float>(M_PI);
				switch (type)
				{
					case WaveType::BandlimitedSaw:
					{
						// 4-harmonic additive saw: bright, buzzy string character
						return 0.60f *
							   (sinf(p) - 0.5f * sinf(p * 2.0f) + 0.333f * sinf(p * 3.0f) - 0.25f * sinf(p * 4.0f));
					}
					case WaveType::PulseSquare:
					{
						// Soft-clipped pulse with smooth transition eliminating edge-clicks
						float pwOffset = (param - 0.5f) * 2.0f;
						return std::clamp((sinf(p) - pwOffset) * 8.0f, -0.60f, 0.60f);
					}
					case WaveType::FMPluck:
					{
						// 2-op FM (carrier 1x, modulator 2x) with decaying mod index
						float modEnv = param * expf(-time / (std::max)(0.05f, decay * 0.35f));
						return sinf(p + modEnv * sinf(p * 2.0f));
					}
					case WaveType::Wavefolder:
					{
						// Continuous sine-folding (drive applied to continuous sine eliminates phase wrap
						// discontinuities)
						return sinf(param * sinf(p));
					}
					case WaveType::SoftSine:
					default:
					{
						return 0.85f * sinf(p) + 0.15f * sinf(p * 2.0f);
					}
				}
			};

			for (auto& voice : m_activeVoices)
			{
				if (voice.finished)
					continue;

				// Instantaneous attack for drums and UI validation (preserves punch & transient snap)
				// Smooth attack for melodic notes (prevents clicks/pops)
				const bool isPercussion = (voice.instrument == 5 || voice.instrument == 6 || voice.instrument == 7 ||
										   voice.instrument == 3 || voice.instrument == 8 || voice.instrument == 9);
				const float attackTime = (voice.instrument == 8) ? 0.0003f : (isPercussion ? 0.0006f : 0.008f);

				const float effCutoff = (voice.instrument >= 8)
											? voice.filterCutoff
											: (std::min)(voice.filterCutoff, m_concussionFilterCutoff);
				float qVal = (voice.filterQ > 0.01f) ? voice.filterQ : 0.7071f;

				// 2-pole Topology-Preserving Transform (TPT) SVF coefficients (precalculated for non-lead / baseline)
				float clampedCutoff = std::clamp(effCutoff, 20.0f, sampleRateF * 0.45f);
				float g = std::tan(static_cast<float>(M_PI) * clampedCutoff / sampleRateF);
				float k = 1.0f / qVal;
				float a1 = 1.0f / (1.0f + g * (g + k));
				float a2 = g * a1;

				const float fadeStep = voice.fadeRate / sampleRateF;

				for (uint32_t i = 0; i < frameCount; ++i)
				{
					// Forced fade-out (ducking): linear gain ramp to avoid envelope discontinuities
					if (voice.fadeRate > 0.0f)
					{
						voice.fadeGain -= fadeStep;
						if (voice.fadeGain <= 0.0f)
						{
							voice.fadeGain = 0.0f;
							voice.finished = true;
							break;
						}
					}

					float env = voice.amplitude * expf(-voice.time / voice.decay);
					if (voice.time < attackTime)
					{
						env *= (voice.time / attackTime);
					}
					env *= voice.fadeGain;

					float rawSample = 0.0f;
					float currentFreq = voice.frequency;

					// Subtle delayed vibrato on held lead notes (gentle shimmer without sounding out of tune).
					if (voice.instrument == 0 && voice.time > 0.20f)
					{
						float vibOnset = std::clamp((voice.time - 0.20f) / 0.22f, 0.0f, 1.0f);
						float vibRate = 5.2f;
						float vibDepth = 0.0035f;
						float vib = vibDepth * vibOnset * sinf(voice.time * 2.0f * static_cast<float>(M_PI) * vibRate);
						currentFreq *= (1.0f + vib);
					}

					const float p = voice.phase * 2.0f * static_cast<float>(M_PI);

					switch (voice.instrument)
					{
						case 5: // Kick Drum (Variant influenced by drumKit)
						{
							if (voice.drumKit == 1) // Punchy acoustic kick
							{
								float pitchDrop = 145.0f * expf(-voice.time / 0.032f);
								currentFreq = (voice.frequency > 15.0f ? voice.frequency : 55.0f) + pitchDrop;
								float s = sinf(p);
								rawSample = 1.15f * s - 0.15f * s * s * s;
								if (voice.time < 0.003f)
								{
									rawSample += 0.25f * fastNoise(voice.rngState);
								}
							}
							else if (voice.drumKit == 2) // Industrial / Overdriven 909 kick
							{
								float pitchDrop = 120.0f * expf(-voice.time / 0.024f);
								currentFreq = (voice.frequency > 15.0f ? voice.frequency : 44.0f) + pitchDrop;
								float s = sinf(p);
								rawSample = std::clamp(1.6f * s - 0.6f * s * s * s, -1.0f, 1.0f);
								if (voice.time < 0.006f)
								{
									rawSample += 0.40f * fastNoise(voice.rngState);
								}
							}
							else // Kit 0: Standard 808 deep kick
							{
								float pitchDrop = 95.0f * expf(-voice.time / 0.028f);
								currentFreq = (voice.frequency > 15.0f ? voice.frequency : 48.0f) + pitchDrop;
								float s = sinf(p);
								rawSample = 1.25f * s - 0.25f * s * s * s;
								if (voice.time < 0.004f)
								{
									rawSample += 0.35f * fastNoise(voice.rngState);
								}
							}
							if (m_ducking > 0.05f)
							{
								// Subtle sub-punch during ducking
								float drive = 1.0f + 0.40f * m_ducking;
								rawSample = std::clamp(rawSample * drive - 0.05f * rawSample * rawSample * rawSample,
													   -0.95f, 0.95f);
							}
							break;
						}
						case 6: // Snare Drum (Variant influenced by drumKit)
						{
							float basePitch = (voice.frequency > 20.0f) ? voice.frequency : 120.0f;
							if (voice.drumKit == 1) // Resonant wood/acoustic snare
							{
								float bodyPitch = (voice.frequency > 20.0f ? voice.frequency : 140.0f) +
												  60.0f * expf(-voice.time / 0.018f);
								currentFreq = bodyPitch;
								float bodyTone = sinf(p);
								float noise = fastNoise(voice.rngState);
								rawSample = 0.60f * bodyTone + 0.40f * noise;
							}
							else if (voice.drumKit == 2) // Industrial metallic ring snare
							{
								float bodyPitch = (voice.frequency > 20.0f ? voice.frequency : 100.0f) +
												  80.0f * expf(-voice.time / 0.025f);
								currentFreq = bodyPitch;
								float bodyTone = sinf(p);
								float ring = sinf(p * 2.73f);
								float noise = fastNoise(voice.rngState);
								rawSample = 0.30f * bodyTone + 0.25f * ring + 0.45f * noise;
							}
							else // Kit 0: Standard snappy snare
							{
								float bodyPitch = basePitch + 65.0f * expf(-voice.time / 0.020f);
								currentFreq = bodyPitch;
								float bodyTone = sinf(p);
								float noise = fastNoise(voice.rngState);
								rawSample = 0.45f * bodyTone + 0.55f * noise;
							}
							break;
						}
						case 7: // Hi-Hat (Variant influenced by drumKit)
						{
							float noise = fastNoise(voice.rngState);
							if (voice.drumKit == 1) // Crisp tight acoustic hat
							{
								float metallic = sinf(p * 1.61f);
								rawSample = 0.75f * noise + 0.25f * metallic;
							}
							else if (voice.drumKit == 2) // Metallic industrial sizzle
							{
								float metallic = sinf(p * 1.41f) * sinf(p * 3.14f);
								rawSample = 0.55f * noise + 0.45f * metallic;
							}
							else // Kit 0: Standard FM hi-hat
							{
								float metallic = sinf(p * 1.37f) * sinf(p * 2.81f);
								rawSample = 0.70f * noise + 0.30f * metallic;
							}
							break;
						}
						case 1: // Bass (Derived from voice.waveType)
						{
							if (voice.waveType == WaveType::SoftSine)
							{
								rawSample = 0.72f * sinf(p) + 0.35f * sinf(p * 2.0f) + 0.12f * sinf(p * 3.0f) +
											0.08f * (1.0f - voice.phase * 2.0f);
							}
							else
							{
								rawSample = evaluateWaveform(voice.waveType, voice.phase, voice.waveParam, voice.time,
															 voice.decay);
							}
							if (m_ducking > 0.05f)
							{
								// Organic analog overdrive gives gritty bite without harsh fuzz
								float drive = 1.0f + 1.6f * m_ducking;
								float x = rawSample * drive;
								rawSample = std::clamp(x - 0.14f * x * x * x, -0.85f, 0.85f);
							}
							break;
						}
						case 2: // Square (Direct effect / death)
							rawSample = (voice.phase < 0.5f) ? 0.55f : -0.55f;
							break;
						case 4: // Chord Pad (Derived from voice.waveType)
						{
							if (voice.waveType == WaveType::SoftSine)
							{
								rawSample = 0.65f * sinf(p) + 0.25f * sinf(p * 2.0f) + 0.10f * sinf(p * 3.0f);
							}
							else
							{
								rawSample = evaluateWaveform(voice.waveType, voice.phase, voice.waveParam, voice.time,
															 voice.decay);
							}
							break;
						}
						case 3: // Noise
							rawSample = fastNoise(voice.rngState);
							break;
						case 8: // UI Positive Click (Crisp tactile mechanical click & resonant pop)
						{
							// Anchor body fundamental to crisp, pleasant UI register (~440 - 880 Hz, e.g. A4 to A5)
							float baseFreq = voice.frequency;
							while (baseFreq > 880.0f)
								baseFreq *= 0.5f;
							while (baseFreq < 440.0f)
								baseFreq *= 2.0f;

							// Rapid pitch drop on impact: sweeps from +1800 Hz down to baseFreq in ~3ms
							float pitchSnap = 1800.0f * expf(-voice.time / 0.0022f);
							currentFreq = baseFreq + pitchSnap;

							// Dual-action mechanical tactile transients:
							// 1. Initial contact strike at t = 0 (crisp high transient tick)
							float snap1 = expf(-voice.time / 0.0016f) *
										  (0.35f * fastNoise(voice.rngState) + 0.65f * sinf(p * 2.5f));

							// 2. Secondary leaf latch snap at t ~ 2.2ms (mechanical micro-plunger click)
							float t2 = voice.time - 0.0022f;
							float snap2 = (t2 > 0.0f) ? expf(-t2 / 0.0012f) *
															(0.40f * fastNoise(voice.rngState) + 0.60f * cosf(p * 3.5f))
													  : 0.0f;

							// 3. Resonant acoustic wooden/marimba pop (rings out smoothly with voice decay)
							float body = sinf(p) + 0.28f * sinf(p * 2.0f) + 0.10f * sinf(p * 3.0f);

							// Blend: sharp initial mechanical transients + rich resonant acoustic pop
							float raw = 1.35f * snap1 + 1.10f * snap2 + 0.95f * body;
							rawSample = std::tanh(raw * 1.25f) * 0.90f;
							break;
						}
						case 9: // UI Negative Error (Clear invalid input rejection)
						{
							// Descending pitch envelope (~22% drop over duration) gives clear downward / rejection cue
							float dropFactor = (std::max)(0.5f, 1.0f - 0.22f * (voice.time / voice.decay));
							currentFreq = voice.frequency * dropFactor;

							float s = sinf(p);

							// Asymmetric saw with rich harmonics cutting through small speakers
							float saw = s - 0.45f * sinf(p * 2.0f) + 0.30f * sinf(p * 3.0f) - 0.18f * sinf(p * 4.0f);
							// Clipped pulse edge for classic buzzer bite
							float pulse = (s > 0.04f ? 0.70f : -0.70f);
							float roughTone = 0.50f * saw + 0.50f * pulse;

							// Granular grit: wave-synced noise layer for tactile crunchy error rasp
							float noise = fastNoise(voice.rngState);
							float grit = 0.25f * noise * fabsf(s);

							rawSample = std::clamp((roughTone + grit) * 1.30f, -0.92f, 0.92f);
							break;
						}
						case 0: // Lead Melody (Clean, warm saturation preserving each waveform's unique character)
						default:
						{
							float leadSample =
								evaluateWaveform(voice.waveType, voice.phase, voice.waveParam, voice.time, voice.decay);
							float driven = leadSample * 1.15f + 0.06f * leadSample * std::abs(leadSample);
							rawSample = std::tanh(driven) * 0.90f;
							break;
						}
					}

					// Dynamic filter envelope on lead notes (crisp pluck attack & dynamic Q bite)
					float gLocal = g;
					float a1Local = a1;
					float a2Local = a2;
					if (voice.instrument == 0)
					{
						float fEnv = expf(-voice.time / (std::max)(0.035f, voice.decay * 0.35f));
						float dynCutoff =
							std::clamp((effCutoff * 0.70f) + (effCutoff * 0.65f * fEnv), 20.0f, sampleRateF * 0.45f);
						float dynQ = qVal + 0.75f * fEnv; // Pluck increases resonance bite on attack
						gLocal = std::tan(static_cast<float>(M_PI) * dynCutoff / sampleRateF);
						float kLocal = 1.0f / dynQ;
						a1Local = 1.0f / (1.0f + gLocal * (gLocal + kLocal));
						a2Local = gLocal * a1Local;
					}

					// 2-pole Topology-Preserving Transform (TPT) State Variable Filter (12 dB/octave)
					float v0 = rawSample;
					float v1 = a1Local * voice.filterS1 + a2Local * (v0 - voice.filterS2);
					float v2 = voice.filterS2 + gLocal * v1; // 12 dB/oct resonant low-pass output
					voice.filterS1 = 2.0f * v1 - voice.filterS1;
					voice.filterS2 = 2.0f * v2 - voice.filterS2;

					// Denormal flush
					if (std::abs(voice.filterS1) < 1e-15f)
						voice.filterS1 = 0.0f;
					if (std::abs(voice.filterS2) < 1e-15f)
						voice.filterS2 = 0.0f;

					float sample = env * v2;

					if (m_ducking > 0.05f && voice.instrument < 8)
					{
						// Smooth tape-style warmth and saturation on output
						float roughDrive = 1.0f + 0.45f * m_ducking;
						sample = std::tanh(sample * roughDrive) / std::sqrt(roughDrive);
					}

					if (m_isDead && m_deathStage >= 4)
					{
						// Smooth master natural fade-out over ~2 seconds for any remaining tails
						float fade = expf(-m_deathSilenceTimer / 1.5f);
						sample *= fade;
					}

					const float phaseInc = currentFreq / sampleRateF;
					voice.phase += phaseInc;
					if (voice.phase >= 1.0f)
					{
						voice.phase -= 1.0f;
					}
					voice.time += 1.0f / sampleRateF;

					buffer[i * 2 + 0] += sample * voice.leftGain;
					buffer[i * 2 + 1] += sample * voice.rightGain;
				}

				if (voice.time > attackTime && (voice.amplitude * expf(-voice.time / voice.decay) < 0.0005f ||
												(m_isDead && m_deathStage >= 4 && m_deathSilenceTimer > 5.0f)))
				{
					voice.finished = true;
				}
			}

			m_activeVoices.erase(std::remove_if(m_activeVoices.begin(), m_activeVoices.end(),
												[](const MusicVoice& v) { return v.finished; }),
								 m_activeVoices.end());
		}

		const char* SdfMusicEngine::getWaveTypeName(WaveType waveType)
		{
			switch (waveType)
			{
				case WaveType::SoftSine:
					return "Soft Sine (Warm)";
				case WaveType::BandlimitedSaw:
					return "Bandlimited Saw (Bright)";
				case WaveType::PulseSquare:
					return "Pulse Square (Reedy)";
				case WaveType::FMPluck:
					return "FM Pluck (Bell/Metallic)";
				case WaveType::Wavefolder:
					return "Wavefolder (Buchla/Evolving)";
				default:
					return "Unknown";
			}
		}

		const char* SdfMusicEngine::getDrumKitName(int kit)
		{
			switch (kit)
			{
				case 1:
					return "Acoustic Punch";
				case 2:
					return "Industrial / 909";
				case 0:
				default:
					return "Deep 808 Electronic";
			}
		}

		const char* SdfMusicEngine::getLeadStyleName(LeadStyle style)
		{
			switch (style)
			{
				case LeadStyle::Auto:
					return "Auto (From Shape)";
				case LeadStyle::RepeatingHook:
					return "Repeating Hook (Call & Response)";
				case LeadStyle::ChordArpeggio:
					return "Chord Arpeggio (Synthwave)";
				case LeadStyle::LyricalSinging:
					return "Lyrical Singing (Slow/Ambient)";
				default:
					return "Unknown";
			}
		}

		LeadStyle SdfMusicEngine::getEffectiveLeadStyle() const
		{
			std::lock_guard<std::recursive_mutex> lock(m_songMutex);
			if (m_leadStyle != LeadStyle::Auto)
			{
				return m_leadStyle;
			}
			if (m_motionNorm < 0.22f && m_shapeParams.melodyDensity < 0.45f)
			{
				return LeadStyle::LyricalSinging;
			}
			if (m_currentSong &&
				((m_currentSong->getFingerprint().structuralHash % 3u) == 1u || m_shapeParams.syncopation > 0.55f))
			{
				return LeadStyle::ChordArpeggio;
			}
			return LeadStyle::RepeatingHook;
		}

		float SdfMusicEngine::getTempo() const
		{
			if (!m_currentSong)
				return 120.0f;

			float baseTempo = m_currentSong->getTempo() * m_shapeParams.tempoFactor * m_tempoFromMotion;
			float surgeTempoMult = 1.0f + 0.18f * m_surgeLevel;
			float duckTempoMult = 1.0f - 0.16f * m_ducking;
			float dynamicTempo = baseTempo * surgeTempoMult * duckTempoMult;
			return (std::max)(5.0f, dynamicTempo);
		}

		float SdfMusicEngine::getTimeBetweenBeats() const
		{
			return 60.0f / getTempo();
		}

	} // namespace WeirdAudio
} // namespace WeirdEngine
