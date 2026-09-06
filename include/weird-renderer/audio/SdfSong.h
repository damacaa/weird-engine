#pragma once

#include "weird-engine/math/MathExpressions.h"
#include "weird-engine/math/SDF.h"
#include "weird-engine/vec.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		enum class MusicalScale
		{
			PentatonicMajor,
			PentatonicMinor,
			Major,
			NaturalMinor,
			Dorian,
			Lydian,
			Chromatic
		};

		class SdfSong
		{
		public:
			static constexpr float DOMAIN_RADIUS = 50.0f;
			static constexpr float SAMPLE_RADIUS = 20.0f;

			static glm::vec2 getDefaultCenter();
			static Vec2Expr point()
			{
				return WeirdEngine::songPoint();
			}

			SdfSong(std::string name, std::shared_ptr<IMathExpression> shapeExpression = nullptr,
					std::optional<glm::vec2> center = std::nullopt);
			SdfSong(std::string name, const Expr& shapeExpression, std::optional<glm::vec2> center = std::nullopt);
			SdfSong(std::string name, std::shared_ptr<IMathExpression> shapeExpression, float tempo,
					MusicalScale scale = MusicalScale::PentatonicMajor, int rootMidi = 60,
					float sampleRadius = SAMPLE_RADIUS);
			~SdfSong() = default;

			static std::shared_ptr<SdfSong> create(std::string name, const Expr& shapeExpression,
												   std::optional<glm::vec2> center = std::nullopt);

			static std::shared_ptr<SdfSong> create(std::string name, std::shared_ptr<IMathExpression> shapeExpression,
												   std::optional<glm::vec2> center = std::nullopt);

			// Backwards-compatible overloads for callers specifying manual musical overrides
			static std::shared_ptr<SdfSong> create(std::string name, const Expr& shapeExpression, float tempo,
												   MusicalScale scale = MusicalScale::PentatonicMajor,
												   int rootMidi = 60, float sampleRadius = SAMPLE_RADIUS);

			static std::shared_ptr<SdfSong> create(std::string name, std::shared_ptr<IMathExpression> shapeExpression,
												   float tempo, MusicalScale scale = MusicalScale::PentatonicMajor,
												   int rootMidi = 60, float sampleRadius = SAMPLE_RADIUS);

			const std::string& getName() const
			{
				return m_name;
			}

			float getTempo() const
			{
				return m_tempo;
			}
			void setTempo(float bpm)
			{
				m_tempo = bpm;
			}

			MusicalScale getScale() const
			{
				return m_scale;
			}
			void setScale(MusicalScale scale)
			{
				m_scale = scale;
			}

			int getRootMidi() const
			{
				return m_rootMidi;
			}
			void setRootMidi(int root)
			{
				m_rootMidi = root;
			}

			float getSampleRadius() const
			{
				return SAMPLE_RADIUS;
			}
			void setSampleRadius(float /*r*/) {} // sampleRadius is universal (SAMPLE_RADIUS = 2.0f)

			glm::vec2 getCenter() const
			{
				return m_center;
			}
			void setCenter(glm::vec2 center);

			Vec2Expr localPoint() const
			{
				return SDF::localPoint(m_center);
			}

			std::shared_ptr<IMathExpression> getShapeExpression() const
			{
				return m_boundedShapeExpression;
			}
			std::shared_ptr<IMathExpression> getRawShapeExpression() const
			{
				return m_rawShapeExpression;
			}
			void setShapeExpression(std::shared_ptr<IMathExpression> expr);
			void setShapeExpression(const Expr& expr)
			{
				setShapeExpression(expr.node);
			}

			// Automatically calculate tempo, scale, and base note based on the provided shape
			void calculateMusicalPropertiesFromShape();

			// Alias for compatibility with scene SDF visual registration
			std::shared_ptr<IMathExpression> getCombinedExpression() const
			{
				return m_boundedShapeExpression;
			}

			// Scale & pitch helpers
			float midiToFrequency(int midiNote) const;
			int getScaleDegreeMidi(int degreeIndex, int octaveOffset = 0) const;

			// Parameters for custom shapes (var0..var6; var7 is reserved internally for visualization scaling)
			float getParameter(size_t index) const;
			void setParameter(size_t index, float value);
			const float* getParameters() const
			{
				return m_parameters;
			}

		private:
			void applyBoundingDomain();

			std::string m_name;
			float m_tempo = 80.0f;
			MusicalScale m_scale = MusicalScale::PentatonicMajor;
			int m_rootMidi = 60; // Middle C
			glm::vec2 m_center = {0.0f, 0.0f};

			float m_parameters[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
			std::shared_ptr<IMathExpression> m_rawShapeExpression;
			std::shared_ptr<IMathExpression> m_boundedShapeExpression;
		};

	} // namespace WeirdRenderer
} // namespace WeirdEngine
