#ifndef WEIRDSAMPLES_DEFAULT2DSDFS_H
#define WEIRDSAMPLES_DEFAULT2DSDFS_H

#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <algorithm>
#include <cstdint>

#include "MathExpressions.h"
#include "StarShape.h"

namespace WeirdEngine
{
	// OLD STYLE
	namespace DefaultShapes
	{
		enum Type
		{
			CIRCLE,
			BOX,
			BOX_LINE,
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

			static constexpr uint8_t VALUES_SIZE = 11;

			static constexpr uint8_t TIME = 8;
			static constexpr uint8_t WORLD_X = 9;
			static constexpr uint8_t WORLD_Y = 10;

		public:
			ShapeMacro()
			{
				m_values.resize(VALUES_SIZE);
			}

			~ShapeMacro() override = default;

			void propagateValues(float *values) override
			{
				for (int i = 0; i < VALUES_SIZE; i++) {
					m_values[i] = values[i];
				}
			}

			[[nodiscard]] float getValue() const override = 0;

			[[nodiscard]] std::string print() const override = 0;
		};

		struct CircleNode : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> m_px;
			std::shared_ptr<IMathExpression> m_py;
			std::shared_ptr<IMathExpression> m_r;

		public:

			CircleNode(
				std::shared_ptr<IMathExpression> px,
				std::shared_ptr<IMathExpression> py,
				std::shared_ptr<IMathExpression> r)
				: m_px(std::move(px)), m_py(std::move(py)), m_r(std::move(r)) {}

			void propagateValues(float* values) override
			{
				m_px->propagateValues(values);
				m_py->propagateValues(values);
				m_r->propagateValues(values);
			}

			[[nodiscard]] float getValue() const override
			{
				vec2 p = vec2(m_px->getValue(), m_py->getValue());
				return length(p) - m_r->getValue();
			}

			[[nodiscard]] std::string print() const override
			{
				return "length(vec2(" + m_px->print() + ", " + m_py->print() + ")) - " + m_r->print();
			}
		};

		struct Circle : ShapeMacro
		{
			static constexpr uint8_t POS_X = 0;
			static constexpr uint8_t POS_Y = 1;
			static constexpr uint8_t RADIUS = 2;

			[[nodiscard]] float getValue() const override
			{
				vec2 p = vec2(m_values[WORLD_X] - m_values[POS_X], m_values[WORLD_Y] - m_values[POS_Y]);
				return length(p) - m_values[RADIUS];
			}

			[[nodiscard]] std::string print() const override
			{
				return "length(p - vec2(var" + std::to_string(POS_X) + ", var" + std::to_string(POS_Y) + ")) - var" + std::to_string(RADIUS);
			}
		};

		struct BoxNode : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> m_px;
			std::shared_ptr<IMathExpression> m_py;
			std::shared_ptr<IMathExpression> m_w;
			std::shared_ptr<IMathExpression> m_h;

		public:

			BoxNode(
				std::shared_ptr<IMathExpression> px,
				std::shared_ptr<IMathExpression> py,
				std::shared_ptr<IMathExpression> w,
				std::shared_ptr<IMathExpression> h)
				: m_px(std::move(px)), m_py(std::move(py)), m_w(std::move(w)), m_h(std::move(h)) {}

			void propagateValues(float* values) override
			{
				m_px->propagateValues(values);
				m_py->propagateValues(values);
				m_w->propagateValues(values);
				m_h->propagateValues(values);
			}

			[[nodiscard]] float getValue() const override
			{
				vec2 p = vec2(m_px->getValue(), m_py->getValue());
				vec2 b = vec2(m_w->getValue(), m_h->getValue());
				vec2 d = abs(p) - b;
				return length(max(d, vec2(0.0))) + std::min(std::max(d.x, d.y), 0.0f);
			}

			[[nodiscard]] std::string print() const override
			{
				return "sdBox(vec2(" + m_px->print() + ", " + m_py->print() + "), vec2(" + m_w->print() + ", " + m_h->print() + "))";
			}
		};

		struct Box : ShapeMacro
		{
			static constexpr uint8_t POS_X = 0;
			static constexpr uint8_t POS_Y = 1;
			static constexpr uint8_t SIZE_X = 2;
			static constexpr uint8_t SIZE_Y = 3;

			[[nodiscard]] float getValue() const override
			{
				vec2 p = vec2(m_values[WORLD_X] - m_values[POS_X], m_values[WORLD_Y] - m_values[POS_Y]);
				vec2 b = vec2(m_values[SIZE_X], m_values[SIZE_Y]);
				vec2 d = abs(p) - b;
				return length(max(d, vec2(0.0))) + std::min(std::max(d.x, d.y), 0.0f);
			}

			[[nodiscard]] std::string print() const override
			{
				return "sdBox(p - vec2(var" + std::to_string(POS_X) + ", var" + std::to_string(POS_Y) + "), vec2(var" + std::to_string(SIZE_X) + ", var" + std::to_string(SIZE_Y) + "))";
			}
		};

		struct BoxLine : ShapeMacro
		{
			static constexpr uint8_t POS_X = 0;
			static constexpr uint8_t POS_Y = 1;
			static constexpr uint8_t SIZE_X = 2;
			static constexpr uint8_t SIZE_Y = 3;
			static constexpr uint8_t THICKNESS = 4;

			[[nodiscard]] float getValue() const override
			{
				vec2 p = vec2(m_values[WORLD_X] - m_values[POS_X], m_values[WORLD_Y] - m_values[POS_Y]);
				vec2 b = vec2(m_values[SIZE_X], m_values[SIZE_Y]);
				vec2 d = abs(p) - b;
				float boxSdf = length(max(d, vec2(0.0))) + std::min(std::max(d.x, d.y), 0.0f);
				return std::abs(boxSdf) - m_values[THICKNESS];
			}

			[[nodiscard]] std::string print() const override
			{
				return "abs(sdBox(p - vec2(var" + std::to_string(POS_X) + ", var" + std::to_string(POS_Y) + "), vec2(var" + std::to_string(SIZE_X) + ", var" + std::to_string(SIZE_Y) + "))) - var" + std::to_string(THICKNESS);
			}
		};

		struct Sine : ShapeMacro
		{
			static constexpr uint8_t AMPLITUDE = 0;
			static constexpr uint8_t PERIOD = 1;
			static constexpr uint8_t SPEED = 2;
			static constexpr uint8_t OFFSET = 3;

			[[nodiscard]] float getValue() const override
			{
				float x = m_values[WORLD_X];
				float y = m_values[WORLD_Y];
				float time = m_values[TIME];

				float amplitude = m_values[AMPLITUDE];
				float period = m_values[PERIOD];
				float speed = m_values[SPEED];
				float offset = m_values[OFFSET];

				return (y - offset) - amplitude * sinf(period * x + speed * time);
			}

			[[nodiscard]] std::string print() const override
			{
				return "(p.y - var" + std::to_string(OFFSET) + ") - var" + std::to_string(AMPLITUDE) +
				       " * sin(var" + std::to_string(PERIOD) + " * p.x + var" + std::to_string(SPEED) + " * u_time)";
			}
		};


		struct Ramp : ShapeMacro
		{
			static constexpr uint8_t POS_X = 0;
			static constexpr uint8_t POS_Y = 1;
			static constexpr uint8_t WIDTH = 2;
			static constexpr uint8_t HEIGHT = 3;
			static constexpr uint8_t SKEW = 4;

			[[nodiscard]] float getValue() const override
			{
				vec2 p = vec2(m_values[WORLD_X] - m_values[POS_X], m_values[WORLD_Y] - m_values[POS_Y]);
				float wi = m_values[WIDTH];
				float he = m_values[HEIGHT];
				float sk = m_values[SKEW];

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

			[[nodiscard]] std::string print() const override
			{
				return "sdParallelogramVertical(p - vec2(var"
					+ std::to_string(POS_X) +
					", var" + std::to_string(POS_Y) + "), var" + std::to_string(WIDTH) + ", var" + std::to_string(HEIGHT)
					+ ", var" + std::to_string(SKEW) + ")";
			}
		};

		struct Line : ShapeMacro
		{
			static constexpr uint8_t POS_A_X = 0;
			static constexpr uint8_t POS_A_Y = 1;
			static constexpr uint8_t POS_B_X = 2;
			static constexpr uint8_t POS_B_Y = 3;
			static constexpr uint8_t WIDTH = 4;

			[[nodiscard]] float getValue() const override
			{
				vec2 p = vec2(m_values[WORLD_X], m_values[WORLD_Y]);

				vec2 a = vec2(m_values[POS_A_X], m_values[POS_A_Y]);
				vec2 b = vec2(m_values[POS_B_X], m_values[POS_B_Y]);

				float width = m_values[WIDTH];

				vec2 pa = p - a, ba = b - a;
				float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
				return length(pa - ba * h) - width;
			}

			[[nodiscard]] std::string print() const override
			{
				return "sdSegment(p, vec2(var" + std::to_string(POS_A_X) + ", var" + std::to_string(POS_A_Y) + "), vec2(var" + std::to_string(POS_B_X) + ", var" + std::to_string(POS_B_Y) + ")) - var" + std::to_string(WIDTH);
			}
		};

		inline std::vector<std::shared_ptr<IMathExpression> > getSDFS()
		{
			std::vector<std::shared_ptr<IMathExpression> > m_sdfs;
			m_sdfs.resize(DefaultShapes::SIZE);

			// Sine
			m_sdfs[SINE] = std::make_shared<Sine>();

			// Star
			m_sdfs[STAR] = getStarShape();

			// Circle
			m_sdfs[CIRCLE] = std::make_shared<Circle>();

			// Box
			m_sdfs[BOX] = std::make_shared<Box>();

			// Box line
			m_sdfs[BOX_LINE] = std::make_shared<BoxLine>();

			// Line
			m_sdfs[LINE] = std::make_shared<Line>();

			// Ramp
			m_sdfs[RAMP] = std::make_shared<Ramp>();

			return m_sdfs;
		}
	}
}
#endif //WEIRDSAMPLES_DEFAULT2DSDFS_H