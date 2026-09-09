#include "weird-renderer/audio/SdfSong.h"
#include "weird-engine/Assert.h"
#include "weird-renderer/core/Display.h"
#include <algorithm>
#include <cmath>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		glm::vec2 SdfSong::getDefaultCenter()
		{
			float h = Display::height > 0 ? static_cast<float>(Display::height) : 800.0f;
			return glm::vec2(70.0f, h - 70.0f);
		}

		SdfSong::SdfSong(std::string name, std::shared_ptr<IMathExpression> shapeExpression,
						 std::optional<glm::vec2> center)
			: m_name(std::move(name))
			, m_center(center.value_or(getDefaultCenter()))
			, m_rawShapeExpression(std::move(shapeExpression))
		{
			applyBoundingDomain();
			calculateMusicalPropertiesFromShape();
		}

		SdfSong::SdfSong(std::string name, const Expr& shapeExpression, std::optional<glm::vec2> center)
			: SdfSong(std::move(name), shapeExpression.node, center)
		{
		}

		SdfSong::SdfSong(std::string name, std::shared_ptr<IMathExpression> shapeExpression, float tempo,
						 MusicalScale scale, int rootMidi, float /*sampleRadius*/)
			: m_name(std::move(name))
			, m_center(getDefaultCenter())
			, m_tempo(tempo)
			, m_scale(scale)
			, m_rootMidi(rootMidi)
			, m_rawShapeExpression(std::move(shapeExpression))
		{
			applyBoundingDomain();
		}

		void SdfSong::setCenter(glm::vec2 center)
		{
			m_center = center;
			applyBoundingDomain();
			calculateMusicalPropertiesFromShape();
		}

		void SdfSong::setShapeExpression(std::shared_ptr<IMathExpression> expr)
		{
			m_rawShapeExpression = std::move(expr);
			applyBoundingDomain();
			calculateMusicalPropertiesFromShape();
		}

		void SdfSong::applyBoundingDomain()
		{
			if (!m_rawShapeExpression)
			{
				m_boundedShapeExpression = nullptr;
				return;
			}

			// Bounding domain: Always intersect the provided shape with a sphere/circle of radius 50.0 centered at song
			// center, applying the internal scale node on parameter 7 for visualization
			Expr scaleFactor = WeirdEngine::max(var(7), Expr(0.001f));
			Expr localX = (var(9) - m_center.x) / scaleFactor;
			Expr localY = (var(10) - m_center.y) / scaleFactor;

			auto transformedRaw =
				transformAST(m_rawShapeExpression,
							 [&](const std::shared_ptr<IMathExpression>& node) -> std::shared_ptr<IMathExpression>
							 {
								 auto varNode = std::dynamic_pointer_cast<FloatVariable>(node);
								 if (varNode)
								 {
									 if (varNode->getOffset() == 9)
										 return localX.node;
									 if (varNode->getOffset() == 10)
										 return localY.node;
								 }
								 return node;
							 });

			Expr rawTransformed(transformedRaw);
			Expr scaled = SDF::sdfScale(rawTransformed, scaleFactor);
			Expr domain = SDF::sdCircle(point() - Vec2Expr(m_center), DOMAIN_RADIUS);
			m_boundedShapeExpression = SDF::sdfIntersect(scaled, domain).node;
		}

		std::shared_ptr<SdfSong> SdfSong::create(std::string name, const Expr& shapeExpression,
												 std::optional<glm::vec2> center)
		{
			return std::make_shared<SdfSong>(std::move(name), shapeExpression.node, center);
		}

		std::shared_ptr<SdfSong> SdfSong::create(std::string name, std::shared_ptr<IMathExpression> shapeExpression,
												 std::optional<glm::vec2> center)
		{
			return std::make_shared<SdfSong>(std::move(name), std::move(shapeExpression), center);
		}

		std::shared_ptr<SdfSong> SdfSong::create(std::string name, const Expr& shapeExpression, float tempo,
												 MusicalScale scale, int rootMidi, float sampleRadius)
		{
			return std::make_shared<SdfSong>(std::move(name), shapeExpression.node, tempo, scale, rootMidi,
											 sampleRadius);
		}

		std::shared_ptr<SdfSong> SdfSong::create(std::string name, std::shared_ptr<IMathExpression> shapeExpression,
												 float tempo, MusicalScale scale, int rootMidi, float sampleRadius)
		{
			return std::make_shared<SdfSong>(std::move(name), std::move(shapeExpression), tempo, scale, rootMidi,
											 sampleRadius);
		}

		float SdfSong::getParameter(size_t index) const
		{
			return index < 8 ? m_parameters[index] : 0.0f;
		}

		void SdfSong::setParameter(size_t index, float value)
		{
			WEIRD_ASSERT(index < 7, "The last parameter (index 7) is reserved for procedural song visualization "
									"scaling and cannot be used.");
			if (index < 8)
			{
				m_parameters[index] = value;
				calculateMusicalPropertiesFromShape();
			}
		}

		void SdfSong::calculateMusicalPropertiesFromShape()
		{
			if (!m_rawShapeExpression)
			{
				m_tempo = 80.0f;
				m_scale = MusicalScale::PentatonicMajor;
				m_rootMidi = 60;
				return;
			}

			float paramsT0[11];
			float paramsT1[11];
			for (size_t i = 0; i < 8; ++i)
			{
				paramsT0[i] = m_parameters[i];
				paramsT1[i] = m_parameters[i];
			}

			constexpr float dt = 0.08f;
			paramsT0[8] = 0.0f;
			paramsT1[8] = dt;

			auto evalRaw = [&](float px, float py, const float* p) -> float
			{
				float localP[11];
				std::copy_n(p, 11, localP);
				localP[9] = px;
				localP[10] = py;
				return m_rawShapeExpression->getValue(localP);
			};

			// 1. Evaluate motion across domain sample points to calculate tempo
			// If the shape is completely still, tempo is very slow (40-50 BPM).
			// If the shape has high motion/spinning, tempo rises (90-135 BPM).
			constexpr int sampleCount = 16;
			float totalMotion = 0.0f;
			float insideCount = 0.0f;
			float distSum = 0.0f;

			for (int i = 0; i < sampleCount; ++i)
			{
				float angle = (2.0f * 3.14159265f * static_cast<float>(i)) / static_cast<float>(sampleCount);
				float radius =
					10.0f + 30.0f * (static_cast<float>((i * 7) % sampleCount) / static_cast<float>(sampleCount));
				float px = std::cos(angle) * radius;
				float py = std::sin(angle) * radius;

				float d0 = evalRaw(px, py, paramsT0);
				float d1 = evalRaw(px, py, paramsT1);

				totalMotion += std::abs(d1 - d0) / dt;
				distSum += d0;
				if (d0 < 0.0f)
				{
					insideCount += 1.0f;
				}
			}

			float avgMotion = totalMotion / static_cast<float>(sampleCount);
			float fillRatio = insideCount / static_cast<float>(sampleCount);

			float motionNorm = std::clamp(avgMotion / 30.0f, 0.0f, 1.0f);
			m_tempo = std::clamp(45.0f + 85.0f * std::pow(motionNorm, 0.7f), 40.0f, 150.0f);

			// 2. Sample 8 compass points at universal SAMPLE_RADIUS to compute symmetry, variance, and base note
			float dCenter = evalRaw(0.0f, 0.0f, paramsT0);
			float r = SAMPLE_RADIUS;
			float k = 0.70710678f;
			float dE = evalRaw(r, 0.0f, paramsT0);
			float dW = evalRaw(-r, 0.0f, paramsT0);
			float dN = evalRaw(0.0f, r, paramsT0);
			float dS = evalRaw(0.0f, -r, paramsT0);
			float dNE = evalRaw(r * k, r * k, paramsT0);
			float dNW = evalRaw(-r * k, r * k, paramsT0);
			float dSE = evalRaw(r * k, -r * k, paramsT0);
			float dSW = evalRaw(-r * k, -r * k, paramsT0);

			float dSamples[8] = {dE, dNE, dN, dNW, dW, dSW, dS, dSE};
			float meanD = 0.0f;
			for (float d : dSamples)
			{
				meanD += d;
			}
			meanD /= 8.0f;

			float variance = 0.0f;
			for (float d : dSamples)
			{
				float diff = d - meanD;
				variance += diff * diff;
			}
			variance /= 8.0f;

			float asymmetry = std::abs(dE - dW) + std::abs(dN - dS) + std::abs(dNE - dSW) + std::abs(dNW - dSE);
			float radialAlternation = std::abs((dE + dW + dN + dS) - (dNE + dNW + dSE + dSW));

			// 3. Base note (root MIDI): calculated based on center distance, fill ratio, and mean distance
			// Maps to a solid, grounded key around Middle C (MIDI 55 [G3] to 67 [G4])
			// Distance is normalized by 0.1f to maintain exact resulting note with 10x shape scale
			float rootMetric = std::abs((dCenter * 0.1f) * 2.3f + (meanD * 0.1f) * 1.7f + fillRatio * 7.0f);
			int semitone = static_cast<int>(std::floor(rootMetric)) % 12;
			m_rootMidi = 55 + semitone;

			// 4. Musical Scale: normalized geometric thresholds maintain exact musical scale selection
			float normVariance = variance / 100.0f;
			float normAsymmetry = asymmetry / 10.0f;
			float normAlternation = radialAlternation / 10.0f;
			float normCenter = dCenter / 10.0f;

			if (normVariance < 0.15f && normAsymmetry < 0.25f)
			{
				m_scale = MusicalScale::PentatonicMajor;
			}
			else if (normAlternation > 0.8f)
			{
				m_scale = MusicalScale::PentatonicMinor;
			}
			else if (normAsymmetry < 0.6f && normCenter < 0.0f)
			{
				m_scale = MusicalScale::Major;
			}
			else if (normCenter > 0.0f && normVariance > 0.5f)
			{
				m_scale = MusicalScale::Lydian;
			}
			else if (normAsymmetry > 1.2f)
			{
				m_scale = MusicalScale::NaturalMinor;
			}
			else
			{
				m_scale = MusicalScale::Dorian;
			}
		}

		float SdfSong::midiToFrequency(int midiNote) const
		{
			return 440.0f * std::pow(2.0f, (static_cast<float>(midiNote) - 69.0f) / 12.0f);
		}

		int SdfSong::getScaleDegreeMidi(int degreeIndex, int octaveOffset) const
		{
			static const int pentMajor[] = {0, 2, 4, 7, 9};
			static const int pentMinor[] = {0, 3, 5, 7, 10};
			static const int major[] = {0, 2, 4, 5, 7, 9, 11};
			static const int minor[] = {0, 2, 3, 5, 7, 8, 10};
			static const int dorian[] = {0, 2, 3, 5, 7, 9, 10};
			static const int lydian[] = {0, 2, 4, 6, 7, 9, 11};
			static const int chromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

			const int* intervals = pentMajor;
			int scaleSize = 5;

			switch (m_scale)
			{
				case MusicalScale::PentatonicMinor:
					intervals = pentMinor;
					scaleSize = 5;
					break;
				case MusicalScale::Major:
					intervals = major;
					scaleSize = 7;
					break;
				case MusicalScale::NaturalMinor:
					intervals = minor;
					scaleSize = 7;
					break;
				case MusicalScale::Dorian:
					intervals = dorian;
					scaleSize = 7;
					break;
				case MusicalScale::Lydian:
					intervals = lydian;
					scaleSize = 7;
					break;
				case MusicalScale::Chromatic:
					intervals = chromatic;
					scaleSize = 12;
					break;
				case MusicalScale::PentatonicMajor:
				default:
					intervals = pentMajor;
					scaleSize = 5;
					break;
			}

			int octave = degreeIndex / scaleSize + octaveOffset;
			int degree = degreeIndex % scaleSize;
			if (degree < 0)
			{
				degree += scaleSize;
				octave -= 1;
			}

			return m_rootMidi + octave * 12 + intervals[degree];
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine
