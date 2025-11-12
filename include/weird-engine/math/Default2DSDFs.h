#ifndef WEIRDSAMPLES_DEFAULT2DSDFS_H
#define WEIRDSAMPLES_DEFAULT2DSDFS_H

#include <vector>

#include "MathExpressions.h"

namespace WeirdEngine
{

	// OLD STYLE
	namespace DefaultShapes
	{
		enum Type
		{
			CIRCLE,
			BOX,
			LINE,
			RAMP,
			SINE,
			STAR,
			SIZE
		};


		struct ShapeMacro : IMathExpression
		{
		protected:
			std::vector<float> m_values;

		public:
			ShapeMacro()
			{
				// m_values = new float[16];
				m_values.resize(11);
			}

			~ShapeMacro()
			{
				// delete[] m_values;
			}

			void propagateValues(float *values)
			{
				for (int i = 0; i < 11; i++) {
					m_values[i] = values[i];
				}
			}

			virtual float getValue() const = 0;

			virtual std::string print() = 0;
		};

		struct Ramp : ShapeMacro
		{
			float getValue() const
			{
				vec2 p = vec2(m_values[9] - m_values[0], m_values[10] - m_values[1]);
				float wi = m_values[2];
				float he = m_values[3];
				float sk = m_values[4];

				glm::vec2 e(wi, sk);
				if (p.x < 0.0f) p = -p;
				glm::vec2 w = p - e;
				w.y -= std::clamp(w.y, -he, he);
				glm::vec2 d(glm::dot(w, w), -w.x);
				float s = p.y * e.x - p.x * e.y;
				if (s < 0.0f) p = -p;
				glm::vec2 v = p - glm::vec2(0.0f, he);
				v -= e * std::clamp(glm::dot(v, e) / glm::dot(e, e), -1.0f, 1.0f);
				d = glm::min(d, glm::vec2(glm::dot(v, v), wi * he - std::abs(s)));
				return std::sqrt(d.x) * std::copysign(1.0f, -d.y);
			}


			std::string print()
			{
				return "sdParallelogramVertical(p - vec2(var0, var1), var2, var3, var4)";
			}
		};

		struct Line : ShapeMacro
		{
			float getValue() const
			{
				vec2 p = vec2(m_values[9], m_values[10]);

				vec2 a = vec2(m_values[0], m_values[1]);
				vec2 b = vec2(m_values[2], m_values[3]);

				float width = m_values[4];

				vec2 pa = p - a, ba = b - a;
				float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
				return length(pa - ba * h) - width;
			}


			std::string print()
			{
				return "sdSegment(p, vec2(var0, var1), vec2(var2, var3)) - var4";
			}
		};

		inline std::vector<std::shared_ptr<IMathExpression> > getSDFS()
		{
			std::vector<std::shared_ptr<IMathExpression> > m_sdfs;
			m_sdfs.resize(DefaultShapes::SIZE);

			// Floor
			{
				// p.y - a * sinf(0.5f * p.x + u_time);

				// Define variables
				auto amplitude = std::make_shared<FloatVariable>(0);
				auto period = std::make_shared<FloatVariable>(1);
				auto speed = std::make_shared<FloatVariable>(2);
				auto offset = std::make_shared<FloatVariable>(3);

				auto time = std::make_shared<FloatVariable>(8);
				auto x = std::make_shared<FloatVariable>(9);
				auto y = std::make_shared<FloatVariable>(10);

				// Define function
				auto sineContent = std::make_shared<Addition>(std::make_shared<Multiplication>(period, x),
				                                              std::make_shared<Multiplication>(speed, time));
				std::shared_ptr<IMathExpression> waveFormula = std::make_shared<Substraction>(
					std::make_shared<Substraction>(y, offset),
					std::make_shared<Multiplication>(amplitude, std::make_shared<Sine>(sineContent)));

				// Store function
				m_sdfs[DefaultShapes::SINE] = waveFormula;
			}

			// Star
			{
				// vec2 starPosition = p - vec2(25.0f, 30.0f);
				// float infiniteShereDist = length(starPosition) - 5.0f;
				// float displacement = 5.0 * sin(5.0f * atan2f(starPosition.y, starPosition.x) - 5.0f * u_time);

				// Define variables
				auto offsetX = std::make_shared<FloatVariable>(0);
				auto offsetY = std::make_shared<FloatVariable>(1);
				auto radious = std::make_shared<FloatVariable>(2);
				auto displacementStrength = std::make_shared<FloatVariable>(3);
				auto starPoints = std::make_shared<FloatVariable>(4);
				auto speed = std::make_shared<FloatVariable>(5);

				auto time = std::make_shared<FloatVariable>(8);
				auto x = std::make_shared<FloatVariable>(9);
				auto y = std::make_shared<FloatVariable>(10);

				// Define function
				std::shared_ptr<IMathExpression> positionX = std::make_shared<Substraction>(x, offsetX);
				std::shared_ptr<IMathExpression> positionY = std::make_shared<Substraction>(y, offsetY);

				// Circle
				std::shared_ptr<IMathExpression> circleDistance = std::make_shared<Substraction>(
					std::make_shared<Length>(positionX, positionY), radious);

				std::shared_ptr<IMathExpression> angularDisplacement = std::make_shared<Multiplication>(
					starPoints, std::make_shared<Atan2>(positionY, positionX));
				std::shared_ptr<IMathExpression> animationTime = std::make_shared<Multiplication>(speed, time);

				std::shared_ptr<IMathExpression> sinContent = std::make_shared<Sine>(
					std::make_shared<Substraction>(angularDisplacement, animationTime));
				std::shared_ptr<IMathExpression> displacement = std::make_shared<Multiplication>(
					displacementStrength, sinContent);

				std::shared_ptr<IMathExpression> starDistance = std::make_shared<
					Addition>(circleDistance, displacement);

				// Store function
				m_sdfs[DefaultShapes::STAR] = starDistance;
			}

			// Circle
			{
				// vec2 starPosition = p - vec2(25.0f, 30.0f);
				// float infiniteShereDist = length(starPosition) - 5.0f;
				// float displacement = 5.0 * sin(5.0f * atan2f(starPosition.y, starPosition.x) - 5.0f * u_time);

				// Define variables
				auto offsetX = std::make_shared<FloatVariable>(0);
				auto offsetY = std::make_shared<FloatVariable>(1);
				auto radious = std::make_shared<FloatVariable>(2);
				auto displacementStrength = std::make_shared<FloatVariable>(3);
				auto starPoints = std::make_shared<FloatVariable>(4);
				auto speed = std::make_shared<FloatVariable>(5);

				auto time = std::make_shared<FloatVariable>(8);
				auto x = std::make_shared<FloatVariable>(9);
				auto y = std::make_shared<FloatVariable>(10);

				// Define function
				std::shared_ptr<IMathExpression> positionX = std::make_shared<Substraction>(x, offsetX);
				std::shared_ptr<IMathExpression> positionY = std::make_shared<Substraction>(y, offsetY);

				// Circle
				std::shared_ptr<IMathExpression> circleDistance = std::make_shared<Substraction>(
					std::make_shared<Length>(positionX, positionY), radious);

				// Store function
				m_sdfs[DefaultShapes::CIRCLE] = circleDistance;
			}

			// Box
			{
				// Define variables
				auto offsetX = std::make_shared<FloatVariable>(0);
				auto offsetY = std::make_shared<FloatVariable>(1);
				auto bX = std::make_shared<FloatVariable>(2);
				auto bY = std::make_shared<FloatVariable>(3);
				auto r = std::make_shared<FloatVariable>(4);

				auto time = std::make_shared<FloatVariable>(8);
				auto x = std::make_shared<FloatVariable>(9);
				auto y = std::make_shared<FloatVariable>(10);

				// Define function
				std::shared_ptr<IMathExpression> positionX = std::make_shared<Substraction>(x, offsetX);
				std::shared_ptr<IMathExpression> positionY = std::make_shared<Substraction>(y, offsetY);

				std::shared_ptr<IMathExpression> dX = std::make_shared<Substraction>(
					std::make_shared<Abs>(positionX), bX);
				std::shared_ptr<IMathExpression> dY = std::make_shared<Substraction>(
					std::make_shared<Abs>(positionY), bY);

				std::shared_ptr<IMathExpression> aX = std::make_shared<Max>(0.0f, dX);
				std::shared_ptr<IMathExpression> aY = std::make_shared<Max>(0.0f, dY);

				std::shared_ptr<IMathExpression> length = std::make_shared<Length>(aX, aY);

				std::shared_ptr<IMathExpression> minMax = std::make_shared<Min>(0.0f, std::make_shared<Max>(dX, dY));

				std::shared_ptr<IMathExpression> boxDistance = std::make_shared<Addition>(length, minMax);

				// Store function
				m_sdfs[DefaultShapes::BOX] = boxDistance;
			}

			Line l;
			m_sdfs[LINE] = std::make_shared<Line>(l);
			Ramp r;
			m_sdfs[RAMP] = std::make_shared<Ramp>(r);

			return m_sdfs;
		}
	}
}
#endif //WEIRDSAMPLES_DEFAULT2DSDFS_H
