#include "weird-audio/SdfSong.h"
#include "weird-engine/Assert.h"
#include "weird-renderer/core/Display.h"
#include <algorithm>
#include <cmath>

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		glm::vec2 SdfSong::getDefaultCenter()
		{
			float h = WeirdRenderer::Display::height > 0 ? static_cast<float>(WeirdRenderer::Display::height) : 800.0f;
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
			// center, applying the dynamic audio volume for visualization pulsating
			Expr scaleFactor = WeirdEngine::max(Expr(1.0f) + audioVolume() * Expr(0.8f), Expr(0.001f));
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

			float paramsT0[12]{};
			float paramsT1[12]{};
			for (size_t i = 0; i < 8; ++i)
			{
				paramsT0[i] = m_parameters[i];
				paramsT1[i] = m_parameters[i];
			}

			constexpr float dt = 0.08f;
			paramsT0[8] = 0.0f;
			paramsT1[8] = dt;
			paramsT0[11] = 0.0f;
			paramsT1[11] = 0.0f;

			auto evalRaw = [&](float px, float py, const float* p) -> float
			{
				float localP[12];
				std::copy_n(p, 12, localP);
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

			// 2. Rotation-invariant 24-angle perimeter profiling to compute spikiness, asymmetry, and hollowness
			float dCenter = evalRaw(0.0f, 0.0f, paramsT0);
			float r = SAMPLE_RADIUS;
			constexpr int NUM_RING_ANGLES = 24;
			constexpr float PI = 3.14159265358979323846f;
			float dSamples[NUM_RING_ANGLES]{};
			float meanD = 0.0f;

			for (int i = 0; i < NUM_RING_ANGLES; ++i)
			{
				float angle = static_cast<float>(i) * (2.0f * PI / static_cast<float>(NUM_RING_ANGLES));
				dSamples[i] = evalRaw(r * std::cos(angle), r * std::sin(angle), paramsT0);
				meanD += dSamples[i];
			}
			meanD /= static_cast<float>(NUM_RING_ANGLES);

			float variance = 0.0f;
			float stepDiffSum = 0.0f;
			for (int i = 0; i < NUM_RING_ANGLES; ++i)
			{
				float diff = dSamples[i] - meanD;
				variance += diff * diff;
				int nextIdx = (i + 1) % NUM_RING_ANGLES;
				stepDiffSum += std::abs(dSamples[i] - dSamples[nextIdx]);
			}
			variance /= static_cast<float>(NUM_RING_ANGLES);
			float avgStepDiff = stepDiffSum / static_cast<float>(NUM_RING_ANGLES);

			float asymSum = 0.0f;
			constexpr int HALF_RING = NUM_RING_ANGLES / 2;
			for (int i = 0; i < HALF_RING; ++i)
			{
				asymSum += std::abs(dSamples[i] - dSamples[i + HALF_RING]);
			}
			float meanAsym = asymSum / static_cast<float>(HALF_RING);

			// 3. Base note (root MIDI): calculated from rotation-invariant center depth, fill ratio, and mean distance
			// Maps to a solid, grounded key around Middle C (MIDI 55 [G3] to 66 [F#4])
			float rootMetric = std::abs((dCenter * 0.1f) * 2.3f + (meanD * 0.1f) * 1.7f + fillRatio * 7.0f);
			int semitone = static_cast<int>(std::floor(rootMetric)) % 12;
			m_rootMidi = 55 + semitone;

			// 4. Musical Scale: driven by rotation-invariant visual traits (Smooth vs Spiky, Solid vs Hollow, Symmetric
			// vs Asymmetric)
			float normVariance = variance / 100.0f;
			float normAsymmetry = meanAsym / 3.5f;
			float normSpikiness = avgStepDiff / 3.2f;
			bool isHollowCenter = (dCenter > 0.0f);

			if (normVariance < 0.15f && normAsymmetry < 0.25f && !isHollowCenter)
			{
				// Smooth, solid, symmetric shape (e.g. circle, soft rounded blob) -> warm, happy Pentatonic Major
				m_scale = MusicalScale::PentatonicMajor;
			}
			else if (normSpikiness > 0.75f && !isHollowCenter)
			{
				// Spiky star or gear with solid core -> cool, driving Pentatonic Minor
				m_scale = MusicalScale::PentatonicMinor;
			}
			else if (normAsymmetry < 0.55f && !isHollowCenter)
			{
				// Balanced solid shape -> bright Major
				m_scale = MusicalScale::Major;
			}
			else if (isHollowCenter && normVariance > 0.45f)
			{
				// Hollow ring/cutout with varied contour -> dreamy Sci-Fi Lydian or groovy Dorian
				m_scale = (normSpikiness > 0.65f) ? MusicalScale::Dorian : MusicalScale::Lydian;
			}
			else if (normAsymmetry > 1.1f || (isHollowCenter && normAsymmetry > 0.6f))
			{
				// Strongly off-center / eclipsed shape -> atmospheric Natural Minor
				m_scale = MusicalScale::NaturalMinor;
			}
			else
			{
				m_scale = MusicalScale::Dorian;
			}

			computeASTFingerprint();
		}

		void SdfSong::computeASTFingerprint()
		{
			m_fingerprint = ASTFingerprint{};
			if (!m_rawShapeExpression)
			{
				return;
			}

			using namespace WeirdEngine::detail;

			struct TraversalNode
			{
				std::shared_ptr<IMathExpression> node;
				int depth;
				int childIndex;
			};

			std::vector<TraversalNode> stack;
			stack.push_back({m_rawShapeExpression, 0, 0});

			std::unordered_set<const IMathExpression*> visited;

			int nonTrivialCount = 0;
			int maxDepth = 0;
			int branchCount = 0;

			uint32_t hash = 2166136261u; // FNV-1a 32-bit offset basis

			while (!stack.empty())
			{
				auto current = stack.back();
				stack.pop_back();

				if (!current.node)
				{
					continue;
				}

				if (current.depth > maxDepth)
				{
					maxDepth = current.depth;
				}

				if (current.node->isTrivial())
				{
					// Skip trivial leaves (FloatConstant, FloatVariable) - parameter immunity!
					continue;
				}

				// Prevent exponential processing if subtrees are shared (DAG)
				const IMathExpression* rawPtr = current.node.get();
				if (visited.find(rawPtr) != visited.end())
				{
					continue;
				}
				visited.insert(rawPtr);

				nonTrivialCount++;

				// Pure topological hashing: depth, child index, and visit order
				hash ^= static_cast<uint32_t>(current.depth * 37 + current.childIndex * 13 + nonTrivialCount);
				hash *= 16777619u;

				std::vector<std::shared_ptr<IMathExpression>> children;
				current.node->getChildren(children);

				if (children.size() >= 2)
				{
					branchCount++;
				}

				for (size_t i = 0; i < children.size(); ++i)
				{
					stack.push_back({children[i], current.depth + 1, static_cast<int>(i)});
				}
			}

			m_fingerprint.structuralHash = hash;
			m_fingerprint.nodeCount = nonTrivialCount;
			m_fingerprint.maxDepth = maxDepth;
			m_fingerprint.branchCount = branchCount;
		}

		float SdfSong::midiToFrequency(int midiNote)
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

		const char* SdfSong::getScaleName(MusicalScale scale)
		{
			switch (scale)
			{
				case MusicalScale::PentatonicMajor:
					return "Pentatonic Major";
				case MusicalScale::PentatonicMinor:
					return "Pentatonic Minor";
				case MusicalScale::Major:
					return "Major";
				case MusicalScale::NaturalMinor:
					return "Natural Minor";
				case MusicalScale::Dorian:
					return "Dorian";
				case MusicalScale::Lydian:
					return "Lydian";
				case MusicalScale::Chromatic:
					return "Chromatic";
				default:
					return "Unknown";
			}
		}

		std::string SdfSong::midiToNoteString(int midi)
		{
			if (midi < 0 || midi > 127)
				return "N/A";
			const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
			int note = midi % 12;
			int octave = (midi / 12) - 1;
			return std::string(noteNames[note]) + std::to_string(octave);
		}

	} // namespace WeirdAudio
} // namespace WeirdEngine
