#include "weird-audio/SdfMusicEngine.h"
#include <algorithm>
#include <cmath>

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
			m_activeVoices.clear();
			m_concussionFilterCutoff = 20000.0f;
			m_deathStage = 0;
			m_deathTimer = 0.0f;
			m_deathSilenceTimer = 0.0f;
			m_isDead = false;
			m_melodyDegree = 0;
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

		void SdfMusicEngine::sampleShapeParameters()
		{
			if (!m_currentSong || !m_currentSong->getRawShapeExpression())
			{
				m_shapeParams = ShapeMusicalParams{};
				return;
			}

			auto shape = m_currentSong->getRawShapeExpression();
			float r = m_currentSong->getSampleRadius();
			if (r <= 0.01f)
			{
				r = 20.0f;
			}

			float params[12]{};
			for (size_t i = 0; i < 8; ++i)
			{
				params[i] = m_currentSong->getParameter(i);
			}
			params[8] = 0.0f; // Static time for parameter sampling
			params[11] = 0.0f;

			auto evalAt = [&](float px, float py) -> float
			{
				params[9] = px;
				params[10] = py;
				return shape->getValue(params);
			};

			// 1. Center depth analysis: measures core thickness / hollowness at (0, 0)
			// dCenter < 0 indicates solid mass (e.g. -4px for slender text, -30px for massive star)
			// dCenter > 0 indicates hollow center
			float dCenter = evalAt(0.0f, 0.0f);
			float centerDepth = std::clamp((-dCenter + 4.0f) / 36.0f, 0.0f, 1.0f);
			m_shapeParams.tempoFactor = 0.72f + 0.56f * centerDepth; // [0.72, 1.28]

			// 2. Multi-point directional profiling across 8 compass directions:
			// Directions: 0=E, 1=NE, 2=N, 3=NW, 4=W, 5=SW, 6=S, 7=SE
			// To avoid spatial Nyquist aliasing on spoke/star shapes, each direction uses a 3-point angular kernel
			constexpr float PI = 3.14159265358979323846f;
			constexpr float deltaAngle = 0.18f; // ~10 degrees aperture
			float R[8];
			float meanR = 0.0f;

			for (int i = 0; i < 8; ++i)
			{
				float baseAngle = static_cast<float>(i) * (PI * 0.25f);
				float dMid = evalAt(r * std::cos(baseAngle), r * std::sin(baseAngle));
				float dNeg = evalAt(r * std::cos(baseAngle - deltaAngle), r * std::sin(baseAngle - deltaAngle));
				float dPos = evalAt(r * std::cos(baseAngle + deltaAngle), r * std::sin(baseAngle + deltaAngle));
				float dAvg = 0.5f * dMid + 0.25f * (dNeg + dPos);

				// Physical boundary reach estimate along this direction:
				// If dAvg < 0 (point is inside), boundary extends past r: R = r + |d| = r - dAvg
				// If dAvg > 0 (point is outside), boundary stops before r: R = r - dAvg
				R[i] = std::max(0.0f, r - dAvg);
				meanR += R[i];
			}
			meanR /= 8.0f;

			// Variance / standard deviation across all 8 directions
			float varSum = 0.0f;
			for (int i = 0; i < 8; ++i)
			{
				float diff = R[i] - meanR;
				varSum += diff * diff;
			}
			float stdDevR = std::sqrt(varSum / 8.0f);
			float relVar = std::clamp(stdDevR / 8.0f, 0.0f, 1.0f);

			// Directional asymmetries:
			// E(0) vs W(4), N(2) vs S(6), NE(1) vs SW(5), NW(3) vs SE(7)
			float asymHoriz = std::abs(R[0] - R[4]) / (R[0] + R[4] + 2.0f);
			float asymVert = std::abs(R[2] - R[6]) / (R[2] + R[6] + 2.0f);
			float asymDiag = (std::abs(R[1] - R[5]) + std::abs(R[3] - R[7])) / (R[1] + R[5] + R[3] + R[7] + 2.0f);
			float totalAsym = std::clamp((asymHoriz + asymVert + asymDiag) * 0.75f, 0.0f, 1.0f);

			// Normalized directional boundary extents (typical shapes span 3px to 32px boundary reach):
			auto normExtent = [](float reach) -> float { return std::clamp((reach - 3.0f) / 27.0f, 0.0f, 1.0f); };

			float extE = normExtent(R[0]);	// East
			float extNE = normExtent(R[1]); // North-East
			float extN = normExtent(R[2]);	// North
			float extNW = normExtent(R[3]); // North-West
			float extSW = normExtent(R[5]); // South-West
			float extS = normExtent(R[6]);	// South
			float extSE = normExtent(R[7]); // South-East

			// 3. Map to musical parameters:
			// Melody Density: horizontal extension along East (active lead vs sparse phrasing)
			m_shapeParams.melodyDensity = std::clamp(0.18f + 0.68f * extE, 0.15f, 0.90f);

			// Harmony Richness: vertical presence along North (sparse dyads vs 7th chords)
			m_shapeParams.harmonyRichness = std::clamp(0.18f + 0.68f * extN, 0.15f, 0.90f);

			// Bass Weight: downward grounded mass along South (deep foundation vs agile bass)
			m_shapeParams.bassWeight = std::clamp(0.28f + 0.60f * extS, 0.28f, 0.92f);

			// Percussion Energy: diagonal reach along NE combined with radial irregularity
			m_shapeParams.percEnergy = std::clamp(0.15f + 0.52f * extNE + 0.28f * relVar, 0.15f, 0.92f);

			// Brightness: spectral clarity and higher octaves along NW
			m_shapeParams.brightness = std::clamp(0.20f + 0.65f * extNW, 0.15f, 0.88f);

			// Syncopation: rhythmic offbeats driven by geometric asymmetry and SE reach
			m_shapeParams.syncopation = std::clamp(0.16f + 0.46f * totalAsym + 0.24f * extSE, 0.12f, 0.85f);

			// Variation: chord progression complexity and melodic jumps driven by variance and SW reach
			m_shapeParams.variation = std::clamp(0.20f + 0.45f * relVar + 0.25f * extSW, 0.15f, 0.90f);
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

			float params[12]{};
			for (size_t i = 0; i < 8; ++i)
			{
				params[i] = m_currentSong->getParameter(i);
			}
			params[8] = static_cast<float>(m_sceneTime);
			params[11] = 0.0f;

			float totalFill = 0.0f;
			float totalCurvature = 0.0f;
			float totalCurvatureSq = 0.0f;
			for (size_t i = 0; i < NUM_DOMAIN_SAMPLES && i < sampleOffsets.size(); ++i)
			{
				params[9] = sampleOffsets[i].x;
				params[10] = sampleOffsets[i].y;
				float d = shape->getValue(params);
				float fillVal = (d < 0.0f) ? 1.0f : 0.0f;
				m_domainFillSamples[i] = fillVal;
				m_domainMotionSamples[i] = 0.0f;

				float kappa = estimateSurfaceCurvature(shape, params, sampleOffsets[i], d);
				m_domainCurvatureSamples[i] = kappa;
				totalCurvature += kappa;
				totalCurvatureSq += kappa * kappa;

				totalFill += fillVal;
			}

			if (m_fillRatio < 0.0f)
			{
				m_fillRatio = totalFill / static_cast<float>(NUM_DOMAIN_SAMPLES);
			}
			m_volumeFromFill = std::clamp(0.40f + 0.60f * std::sqrt(std::max(0.0f, m_fillRatio)), 0.35f, 1.0f);

			// Surface complexity: scale-normalized standard deviation of the sampled mean curvature.
			// Constant curvature (circle/line) yields exactly 0 regardless of size.
			float avgCurvature = totalCurvature / static_cast<float>(NUM_DOMAIN_SAMPLES);
			float variance =
				std::max(0.0f, totalCurvatureSq / static_cast<float>(NUM_DOMAIN_SAMPLES) - avgCurvature * avgCurvature);
			float characteristicRadius = DOMAIN_RADIUS * std::sqrt(std::max(m_fillRatio, 1e-3f));
			m_complexityLevel = std::sqrt(variance) * characteristicRadius;
			m_complexityNorm = m_complexityLevel / (m_complexityLevel + m_complexitySaturation);
			m_tensionFromComplexity = std::pow(m_complexityNorm, COMPLEXITY_TENSION_CURVE);

			m_motionNorm = std::clamp(m_motionLevel / 100.0f, 0.0f, 1.0f);
			m_tempoFromMotion = 0.38f + 1.22f * std::pow(m_motionNorm, 1.25f);

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
			for (size_t i = 0; i < 8; ++i)
			{
				params[i] = m_currentSong->getParameter(i);
			}
			params[8] = static_cast<float>(sceneTime);
			params[11] = 0.0f;

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
			m_motionLevel += (avgMotion - m_motionLevel) * blend;

			// Surface complexity: spread of mean curvature normalized by the characteristic shape size.
			// A perfect circle has constant curvature, so its standard deviation (and complexity) is 0.
			float characteristicRadius = DOMAIN_RADIUS * std::sqrt(std::max(m_fillRatio, 1e-3f));
			float rawComplexity = curvatureStdDev * characteristicRadius;
			m_complexityLevel += (rawComplexity - m_complexityLevel) * blend;
			m_complexityNorm = m_complexityLevel / (m_complexityLevel + m_complexitySaturation);
			m_tensionFromComplexity = std::pow(m_complexityNorm, COMPLEXITY_TENSION_CURVE);

			// Motion controls overall tempo:
			// If motion == 0 (still): tempo drops to 0.38x
			// If motion is moderate: tempo is ~1.0x
			// If motion is high: tempo goes up to 1.60x
			float motionNorm = std::clamp(m_motionLevel / 100.0f, 0.0f, 1.0f);
			m_motionNorm = motionNorm;
			m_tempoFromMotion = 0.38f + 1.22f * std::pow(motionNorm, 1.25f);

			// Fill ratio controls volume:
			// Bigger shape -> higher fill ratio -> louder volume
			// Smaller shape -> lower fill ratio -> quieter volume (with audible minimum floor)
			m_volumeFromFill = std::clamp(0.40f + 0.60f * std::sqrt(m_fillRatio), 0.35f, 1.0f);
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

			static constexpr WaveType allWaves[5] = {WaveType::SoftSine, WaveType::BandlimitedSaw,
													 WaveType::PulseSquare, WaveType::FMPluck, WaveType::Wavefolder};

			uint32_t hLead = prngHash(fp.structuralHash, 101);
			uint32_t hBass = prngHash(fp.structuralHash, 203);
			uint32_t hPad = prngHash(fp.structuralHash, 307);
			uint32_t hKit = prngHash(fp.structuralHash, 409);

			// Lead selects across rich, expressive waveforms: SoftSine, FMPluck, Wavefolder, BandlimitedSaw,
			// PulseSquare
			static constexpr WaveType leadWaves[5] = {WaveType::SoftSine, WaveType::FMPluck, WaveType::Wavefolder,
													  WaveType::BandlimitedSaw, WaveType::PulseSquare};
			m_rack.lead = leadWaves[hLead % 5];

			// Bass selects across SoftSine, BandlimitedSaw, PulseSquare, FMPluck
			static constexpr WaveType bassWaves[4] = {WaveType::SoftSine, WaveType::BandlimitedSaw,
													  WaveType::PulseSquare, WaveType::FMPluck};
			m_rack.bass = bassWaves[hBass % 4];

			// Pad selects across SoftSine, Wavefolder, BandlimitedSaw, PulseSquare
			static constexpr WaveType padWaves[4] = {WaveType::SoftSine, WaveType::Wavefolder, WaveType::BandlimitedSaw,
													 WaveType::PulseSquare};
			m_rack.pad = padWaves[hPad % 4];

			// Drum kit distributes evenly across 0 (808), 1 (Acoustic), 2 (Industrial)
			m_rack.drumKit = static_cast<int>(hKit % 3);

			// Synthesis parameters derived from hash bits for rich timbre variation
			float normA = static_cast<float>(hLead & 0xFF) / 255.0f;
			float normB = static_cast<float>((hBass >> 8) & 0xFF) / 255.0f;
			float normC = static_cast<float>((hPad >> 16) & 0xFF) / 255.0f;

			m_rack.pulseWidth = 0.20f + 0.30f * normA; // [0.20, 0.50]
			m_rack.fmModIndex = 1.2f + 2.4f * normB;   // [1.20, 3.60]
			m_rack.foldDrive = 1.6f + 2.0f * normC;	   // [1.60, 3.60]
		}

		void SdfMusicEngine::resampleShape()
		{
			std::lock_guard<std::mutex> lock(m_songMutex);
			sampleShapeParameters();
			initDomainSamples();
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
				initDomainSamples();

				if (isSongEmpty(m_currentSong))
				{
					m_activeVoices.clear();
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
			return m_currentSong;
		}

		void SdfMusicEngine::queueSong(std::shared_ptr<SdfSong> nextSong)
		{
			std::lock_guard<std::mutex> lock(m_songMutex);
			m_queuedSong = std::move(nextSong);
		}

		void SdfMusicEngine::setTrackEnabled(MusicTrack track, bool enabled)
		{
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
			size_t idx = static_cast<size_t>(track);
			if (idx < m_trackPlayStates.size())
			{
				return m_trackPlayStates[idx];
			}
			return TrackPlayState::Playing;
		}

		void SdfMusicEngine::triggerPositiveFeedback(float intensity)
		{
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
			m_isDead = true;
			m_deathStage = 1; // Stage 1: Lead stops queuing new notes; active notes fade naturally
			m_deathTimer = 0.0f;
			m_deathSilenceTimer = 0.0f;
		}

		bool SdfMusicEngine::isTrackDead(MusicTrack track) const
		{
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
					std::lock_guard<std::mutex> lock(m_songMutex);
					if (m_queuedSong)
					{
						m_currentSong = std::move(m_queuedSong);
						m_queuedSong = nullptr;
						m_melodyDegree = 0;
						sampleShapeParameters();
						initDomainSamples();

						if (isSongEmpty(m_currentSong))
						{
							m_activeVoices.clear();
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
			float surgeTempoMult = 1.0f + 0.18f * m_surgeLevel;
			float dynamicTempo = baseTempo * surgeTempoMult;
			float beatSec = 60.0f / (std::max)(20.0f, dynamicTempo);

			int stepInBar = step % 16;		  // 16th note step in 4/4 bar (0..15)
			int barInSong = (step / 16) % 16; // 16-bar master loop (0..15)
			int phraseInSong = barInSong / 4; // 4-bar phrase index (0..3)
			int barInPhrase = barInSong % 4;  // Bar within 4-bar phrase (0..3)
			int bar = barInPhrase;			  // For harmonic foundation (I - IV - V)

			// Motion dictates pause frequency and note sustained duration:
			// If motion is low (still shape): notes ring out longer and pauses are frequent.
			// If motion is high (rapid moving shape): notes are shorter, active, continuous groove.
			float motionFactor = m_motionNorm;
			float noteLengthMult = 1.6f - 0.6f * motionFactor;

			// Surface complexity drives harmonic/timbral tension independently of tempo:
			// identical motion can sound calm (smooth shape) or tense (intricate/noisy shape).
			float tension = m_tensionFromComplexity;

			// -------------------------------------------------------------
			// Procedural Arrangement & Track Contrasts (16-Bar Macro Form)
			// -------------------------------------------------------------
			uint32_t arch = m_currentSong ? (m_currentSong->getFingerprint().structuralHash % 3) : 0;
			bool arrBass = true;
			bool arrPad = true;
			bool arrLead = true;
			bool arrDrums = true;

			if (arch == 0) // Groove-First: Intro -> Full Hook -> Breakdown -> Drop
			{
				if (phraseInSong == 0)
				{
					// Intro: Lead rests to establish groove
					arrLead = false;
				}
				else if (phraseInSong == 2)
				{
					// Breakdown: Drums & Bass cut out; airy chords + singing lead
					arrBass = false;
					arrDrums = false;
				}
			}
			else if (arch == 1) // Ambient-First: Atmosphere -> Build -> Climax -> Breather
			{
				if (phraseInSong == 0)
				{
					// Atmosphere: Chords + Lead only (no rhythm)
					arrBass = false;
					arrDrums = false;
				}
				else if (phraseInSong == 1)
				{
					// Build: Rhythm enters, melody takes a brief pause
					arrLead = false;
				}
				else if (phraseInSong == 3)
				{
					// Breather: Drums drop out before looping
					arrDrums = false;
				}
			}
			else // Driving Minimalist: Bass & Drums persistent, tops alternate
			{
				if (phraseInSong == 0)
				{
					arrLead = false;
				}
				else if (phraseInSong == 2)
				{
					arrPad = false;
				}
			}

			// Micro-arrangement: 1-beat cadence pause on last bar of phrase before drop/loop
			if (barInPhrase == 3 && stepInBar >= 12 && (phraseInSong == 1 || phraseInSong == 3))
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
								   (1.0f + 0.5f * tension) * (1.0f - 0.30f * m_ducking);
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
					// Pads scale tension down: dissonance/beating is much more prominent on sustained voices
					const float padTension = PAD_TENSION_SCALE * tension;

					int chordSteps[4] = {0, 4, 2, 6};
					int noteCount = 2;
					if (m_ducking > 0.40f)
					{
						// In heavy danger: grounded open fifths (root + fifth)
						chordSteps[0] = 0;
						chordSteps[1] = 4;
						noteCount = 2;
					}
					else if (m_shapeParams.harmonyRichness >= 0.65f || m_surgeLevel > 0.50f || tension > 0.80f)
					{
						noteCount = 4; // 7th chord (or forced dense cluster at extreme tension)
					}
					else if (m_shapeParams.harmonyRichness >= 0.35f || m_surgeLevel > 0.20f)
					{
						noteCount = 3; // Triad
					}

					float pans[4] = {-0.30f, 0.30f, -0.10f, 0.40f};
					float dur = beatSec * 1.8f * noteLengthMult;
					float vel = (0.28f + 0.22f * m_shapeParams.harmonyRichness) * (1.0f + 0.20f * m_surgeLevel);
					// Filter opens up with surge (+2200Hz) and complexity tension; warms and darkens with duck
					float cutoff = (700.0f + 1500.0f * m_shapeParams.brightness + 2200.0f * m_surgeLevel) *
								   (1.0f + 1.2f * padTension) * (1.0f - 0.35f * m_ducking);
					cutoff = (std::max)(210.0f, cutoff);

					for (int i = 0; i < noteCount; ++i)
					{
						int degree = bassDegree + chordSteps[i];
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

					// Complexity tension: semitone shadow voice creates slow beating against the chord
					// (anxiety drone). It intentionally ignores the scale to guarantee dissonance.
					if (tension > 0.15f)
					{
						int shadowDegree = bassDegree + chordSteps[noteCount - 1];
						int shadowOctave = (m_shapeParams.brightness > 0.75f || m_surgeLevel > 0.50f) ? 0 : -1;
						int shadowMidi = m_currentSong->getScaleDegreeMidi(shadowDegree, shadowOctave) +
										 static_cast<int>(m_positivePitchOffset);
						float shadowFreq = m_currentSong->midiToFrequency(shadowMidi) * 1.0594631f;
						float shadowVel = vel * (0.10f + 0.50f * padTension);
						playNote(shadowFreq, shadowVel, dur * 1.25f, 4, 0.35f, cutoff * (1.0f + 0.5f * padTension));
					}
				}
			}

			// -------------------------------------------------------------
			// LAYER 3: MELODIC LEAD (Expressive Motifs, Breathing Breaks, Singing Warmth)
			// -------------------------------------------------------------
			if (playLead && !isDuckingActive)
			{
				bool isQuarterBeat = (stepInBar % 4 == 0);
				bool isEighthBeat = (stepInBar % 2 == 0);

				// 1. Determine Motif Archetype for the song from fingerprint & shape
				uint32_t seed = m_currentSong->getFingerprint().structuralHash;
				int motifType = static_cast<int>((seed ^ (seed >> 8)) % 4);
				if (m_shapeParams.syncopation > 0.58f)
					motifType = 1; // Driving Funk / Syncopated
				else if (m_shapeParams.percEnergy > 0.65f || m_surgeLevel > 0.35f)
					motifType = 2; // Fast melodic runs / arps
				else if (m_shapeParams.melodyDensity < 0.32f && m_surgeLevel < 0.15f)
					motifType = 3; // Lyrical held singing melody

				// 16-step rhythmic hit masks per bar with deliberate breathing breaks:
				// Bar 0: Hook Call (first 2 beats active, beats 3-4 rest)
				// Bar 1: Response (first 2 beats active, beats 3-4 rest)
				// Bar 2: Climax (first 2.5 beats active, steps 10-15 rest)
				// Bar 3: Cadence resolution (step 0 only, steps 2-15 full 3.5 beat break)
				uint16_t motifMask = 0;
				if (motifType == 0) // Singable Hook Anthem
				{
					static constexpr uint16_t masks[4] = {
						(1 << 0) | (1 << 3) | (1 << 6),			   // Bar 0: 3 hits, steps 8..15 REST
						(1 << 0) | (1 << 4) | (1 << 6),			   // Bar 1: 3 hits, steps 8..15 REST
						(1 << 0) | (1 << 2) | (1 << 4) | (1 << 7), // Bar 2: Climax peak, steps 9..15 REST
						(1 << 0)								   // Bar 3: Cadence note, steps 2..15 REST
					};
					motifMask = masks[bar];
				}
				else if (motifType == 1) // Funk / Syncopated Groove
				{
					static constexpr uint16_t masks[4] = {(1 << 0) | (1 << 3) | (1 << 6),
														  (1 << 2) | (1 << 4) | (1 << 7),
														  (1 << 0) | (1 << 3) | (1 << 6) | (1 << 8), (1 << 0)};
					motifMask = masks[bar];
				}
				else if (motifType == 2) // Energetic Flow / Arp Runs
				{
					static constexpr uint16_t masks[4] = {(1 << 0) | (1 << 2) | (1 << 4) | (1 << 6),
														  (1 << 2) | (1 << 4) | (1 << 6),
														  (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7), (1 << 0)};
					motifMask = masks[bar];
				}
				else // Lyrical Singing Ballad
				{
					static constexpr uint16_t masks[4] = {(1 << 0) | (1 << 6), (1 << 2) | (1 << 6),
														  (1 << 0) | (1 << 4) | (1 << 8), (1 << 0)};
					motifMask = masks[bar];
				}

				// Measure-level break: Lead takes a full bar rest on Bar 1 during alternate phrases or moderate density
				bool isBarRest = (bar == 1 && (phraseInSong % 2 == 1 || m_shapeParams.melodyDensity < 0.48f) &&
								  m_surgeLevel < 0.25f);
				if (isBarRest)
				{
					motifMask = 0; // Entire bar rest: rhythm & bass groove solo
				}

				// Check if the current step is a defined motif hit
				bool isMotifHit = (motifMask & (1 << stepInBar)) != 0;

				// Thin out further if melody density is low
				if (isMotifHit && m_shapeParams.melodyDensity < 0.38f && stepInBar != 0 && m_surgeLevel < 0.10f)
				{
					if (stepHash(step, 827) > m_shapeParams.melodyDensity * 1.8f)
					{
						isMotifHit = false;
					}
				}

				if (isMotifHit)
				{
					// 2. Cohesive 4-Bar Melodic Arch
					int noteDegree = 0;
					int octaveOffset = 0; // Baseline singing register (C4-C5 range)

					if (bar == 0) // Hook Call
					{
						static constexpr int kHookNotes[16] = {0, 0, 2, 2, 4, 4, 3, 3, 2, 2, 0, 0, 0, 0, 0, 0};
						noteDegree = bassDegree + kHookNotes[stepInBar];
					}
					else if (bar == 1) // Answering Response
					{
						static constexpr int kRespNotes[16] = {3, 3, 2, 2, 1, 1, 2, 2, 1, 1, 0, 0, 0, 0, 0, 0};
						noteDegree = bassDegree + kRespNotes[stepInBar];
					}
					else if (bar == 2) // Climax (Expressive peak)
					{
						static constexpr int kClimaxNotes[16] = {4, 4, 5, 5, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 0, 0};
						noteDegree = bassDegree + kClimaxNotes[stepInBar];
						octaveOffset = 1; // 1 octave lift for climax peak
					}
					else // Bar 3: Cadence Resolution
					{
						// Complexity tension denies resolution: suspend on the subdominant instead of the tonic
						bool denyCadence = (tension > 0.60f && stepHash(step, 556) < (tension - 0.30f));
						noteDegree = denyCadence ? 3 : 0;
						octaveOffset = 0;
					}

					// Complexity tension: probabilistic high-register lift (agitated wails)
					if (stepHash(step, 555) < 0.35f * tension)
					{
						octaveOffset += 1;
					}

					// Shape-driven variation (occasional ornamental passing neighbor tone),
					// widened by complexity tension with larger jumps at extreme values
					if ((m_shapeParams.variation > 0.45f && stepHash(step, 937) < (m_shapeParams.variation - 0.35f)) ||
						(tension > 0.55f && stepHash(step, 937) < (tension - 0.35f)))
					{
						int jump = (tension > 0.75f && stepHash(step, 941) < 0.30f) ? 2 : 1;
						bool up = (jump == 2) ? (stepHash(step, 943) < 0.5f) : (stepHash(step, 941) < 0.5f);
						noteDegree += up ? jump : -jump;
					}

					int midi = m_currentSong->getScaleDegreeMidi(noteDegree, octaveOffset) +
							   static_cast<int>(m_positivePitchOffset);

					// Strict pitch ceiling and floor guard: Keep lead in the expressive, singing vocal/solo range
					// Ceiling: MIDI 81 (A5, ~880 Hz) - prevents ear-piercing shrieks and screeching
					// Floor:   MIDI 57 (A3, ~220 Hz) - prevents muddy overlap with bass
					while (midi > 81)
					{
						midi -= 12; // Octave fold down
					}
					while (midi < 57)
					{
						midi += 12; // Octave fold up
					}

					float freq = m_currentSong->midiToFrequency(midi);

					// Complexity tension: out-of-tune wobble (alternating detune up to ~1% / 17 cents)
					float wobble = (std::max)(m_detuneAmount, 0.010f * tension);
					if (wobble > 0.001f)
					{
						freq *= (1.0f - wobble * (step % 2 == 0 ? 1.0f : -1.0f));
					}

					// Note duration: strong beats and cadence notes ring long and sing;
					// other notes leave natural space before rests
					float noteBeats = 0.50f;
					if (stepInBar == 0 || (bar == 3 && stepInBar == 0))
					{
						noteBeats = 1.50f; // Long held singing note with vibrato
					}
					else if (isQuarterBeat)
					{
						noteBeats = 0.85f;
					}
					else
					{
						noteBeats = 0.50f;
					}

					// Complexity tension clips notes into ominous staccato plucks at slow tempos
					float dur = beatSec * noteBeats * noteLengthMult * (1.0f - 0.45f * tension);

					// Warm analog cutoff: smooth singing presence without harsh upper sizzle.
					// Complexity tension opens the filter into a strained, biting presence.
					float cutoff = (1800.0f + 1400.0f * m_shapeParams.brightness) * (1.0f + 0.35f * m_surgeLevel) *
								   (1.0f + 1.2f * tension);
					cutoff = std::clamp(cutoff, 1000.0f, 9000.0f);

					// Dynamic velocity with headroom for saturation drive
					float vel = (0.42f + 0.22f * m_shapeParams.melodyDensity) * (1.0f + 0.15f * m_surgeLevel);

					// Subtle alternating stereo pan for spatial motion
					float pan = (stepInBar % 2 == 0) ? -0.15f : 0.15f;

					playNote(freq, vel, dur, 0, pan, cutoff);
				}
			}

			// -------------------------------------------------------------
			// LAYER 4: PERCUSSION (Punchy Kick, Snappy Snare, Crisp Hi-Hat)
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
				// In rough ducking (> 0.30f): heavy, sluggish heartbeat kick on beat 1 (step 0) only!
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
					if (!triggerSnare && (effPercEnergy > 0.60f || m_surgeLevel > 0.35f) &&
						(stepInBar == 14 || stepInBar == 15))
					{
						triggerSnare = true; // 16th-note ghost snare before downbeat
					}
				}

				if (triggerSnare)
				{
					float snareVel =
						(0.55f + 0.25f * effPercEnergy) * (1.0f + 0.18f * m_surgeLevel) * (1.0f - 0.30f * m_ducking);
					float snareCutoff = (4500.0f + 2000.0f * m_surgeLevel) * (1.0f - 0.30f * m_ducking);
					playNote(180.0f, snareVel, 0.18f, 6, 0.0f, snareCutoff); // instrument 6 = Snare
				}

				// Hi-hat: in heavy ducking (>0.45f), drop to quarter notes; otherwise 8th notes
				bool triggerHiHat = false;
				if (m_ducking > 0.45f)
				{
					triggerHiHat = (stepInBar % 4 == 0); // Quarter-note sparse ticking
				}
				else
				{
					triggerHiHat = isEighthBeat;
					if (!triggerHiHat && (m_surgeLevel > 0.25f || effPercEnergy > 0.60f))
					{
						triggerHiHat = true; // Continuous 16th-note groove
					}
					else if (!triggerHiHat && effPercEnergy > 0.50f && (stepHash(step, 809) < 0.65f))
					{
						triggerHiHat = true;
					}
				}

				if (triggerHiHat)
				{
					bool openHat = (stepInBar % 4 == 2) && (effPercEnergy > 0.45f || m_surgeLevel > 0.20f);
					float decay = openHat ? 0.15f : 0.045f;
					float pan = (step % 2 == 0) ? 0.18f : -0.18f;
					float cutoff = (7000.0f + 3000.0f * m_shapeParams.brightness + 3000.0f * m_surgeLevel) *
								   (1.0f - 0.40f * m_ducking);
					float vel = (openHat ? 0.32f : 0.24f) * (0.6f + 0.4f * effPercEnergy) *
								(1.0f + 0.15f * m_surgeLevel) * (1.0f - 0.40f * m_ducking);

					if (!isEighthBeat)
					{
						vel *= 0.65f; // Softer ghost 16th notes
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

		void SdfMusicEngine::playNote(float freq, float amp, float durationSec, int instrument, float pan,
									  float filterCutoff)
		{
			// Lead is strictly killed during ducking
			if (instrument == 0 && (m_duckTimer > 0.0f || m_ducking > 0.01f))
				return;

			// UI feedback and Death sounds (instruments >= 8) bypass music ducking and shape fill volume scaling
			float masterAmp = 0.0f;
			if (instrument >= 8)
			{
				masterAmp = amp * m_volume;
			}
			else
			{
				float duckMult = (std::max)(0.65f, 1.0f - m_ducking * 0.22f);
				float surgeAmpBoost = 1.0f + 0.15f * m_surgeLevel;
				masterAmp = amp * m_volume * m_volumeFromFill * duckMult * surgeAmpBoost;
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

			// Complexity tension adds strained resonance to sustaining voices (lead/pad, pads gentler)
			const float tension = m_tensionFromComplexity;
			const float padTension = PAD_TENSION_SCALE * tension;
			newVoice.filterQ = (instrument == 0) ? 1.414f : 0.7071f;
			if (instrument == 0)
			{
				newVoice.filterQ += 0.9f * tension;
			}
			else if (instrument == 4)
			{
				newVoice.filterQ += 0.9f * padTension;
			}
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
					newVoice.waveParam = m_rack.fmModIndex * (1.0f + 0.8f * tension);
				else if (m_rack.lead == WaveType::PulseSquare)
					newVoice.waveParam = std::clamp(m_rack.pulseWidth - 0.15f * tension, 0.05f, 0.50f);
				else if (m_rack.lead == WaveType::Wavefolder)
					newVoice.waveParam = m_rack.foldDrive * (1.0f + 0.6f * tension);
				else
					newVoice.waveParam = 0.0f;
			}
			else if (instrument == 1) // Bass
			{
				newVoice.waveType = m_rack.bass;
				newVoice.waveParam = (m_rack.bass == WaveType::PulseSquare)
										 ? std::clamp(m_rack.pulseWidth - 0.15f * tension, 0.05f, 0.50f)
										 : 0.0f;
			}
			else if (instrument == 4) // Pad
			{
				newVoice.waveType = m_rack.pad;
				newVoice.waveParam =
					(m_rack.pad == WaveType::Wavefolder) ? m_rack.foldDrive * (1.0f + 0.6f * padTension) : 0.0f;
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

					// Singing delayed vibrato on held lead notes (gives warmth, soul, and vocal expression).
					// Complexity tension turns it into a faster, shallower nervous tremble.
					if (voice.instrument == 0 && voice.time > 0.10f)
					{
						float vibOnset = std::clamp((voice.time - 0.10f) / 0.18f, 0.0f, 1.0f);
						float vibRate = 5.4f * (1.0f + 1.6f * m_tensionFromComplexity);
						float vibDepth = 0.016f * (1.0f - 0.4f * m_tensionFromComplexity);
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
								currentFreq = 55.0f + pitchDrop;
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
								currentFreq = 44.0f + pitchDrop;
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
								currentFreq = 48.0f + pitchDrop;
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
							if (voice.drumKit == 1) // Resonant wood/acoustic snare
							{
								float bodyPitch = 140.0f + 60.0f * expf(-voice.time / 0.018f);
								currentFreq = bodyPitch;
								float bodyTone = sinf(p);
								float noise = fastNoise(voice.rngState);
								rawSample = 0.60f * bodyTone + 0.40f * noise;
							}
							else if (voice.drumKit == 2) // Industrial metallic ring snare
							{
								float bodyPitch = 100.0f + 80.0f * expf(-voice.time / 0.025f);
								currentFreq = bodyPitch;
								float bodyTone = sinf(p);
								float ring = sinf(p * 2.73f);
								float noise = fastNoise(voice.rngState);
								rawSample = 0.30f * bodyTone + 0.25f * ring + 0.45f * noise;
							}
							else // Kit 0: Standard snappy snare
							{
								float bodyPitch = 120.0f + 65.0f * expf(-voice.time / 0.020f);
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
						case 0: // Lead Melody (Analog console saturation with 2nd & 3rd harmonics)
						default:
						{
							float leadSample =
								evaluateWaveform(voice.waveType, voice.phase, voice.waveParam, voice.time, voice.decay);
							// Warm analog overdrive: 2.2x drive into tanh with odd-symmetric saturation (eliminates DC
							// bias)
							float driven = leadSample * 2.2f + 0.16f * leadSample * std::abs(leadSample);
							rawSample = std::tanh(driven) * 0.85f;
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
