#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <array>

#include "weird-engine/vec.h"
#include "CompiledMathExpressions.h"
#include "MathExpressions.h"
#include "StarShape.h"

namespace WeirdEngine::Primitives
{
    static constexpr uint8_t WORLD_X = 9;
	static constexpr uint8_t WORLD_Y = 10;

        struct Circle : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> m_px;
			std::shared_ptr<IMathExpression> m_py;
			std::shared_ptr<IMathExpression> m_r;
			std::shared_ptr<IMathExpression> m_time;
			std::shared_ptr<IMathExpression> m_worldX;
			std::shared_ptr<IMathExpression> m_worldY;

		public:
			static constexpr uint8_t POS_X = 0;
			static constexpr uint8_t POS_Y = 1;
			static constexpr uint8_t RADIUS = 2;

			// Common
			static constexpr uint8_t TIME = 8;
			static constexpr uint8_t WORLD_X = 9;
			static constexpr uint8_t WORLD_Y = 10;

			Circle(std::shared_ptr<IMathExpression> px, std::shared_ptr<IMathExpression> py,
					   std::shared_ptr<IMathExpression> r)
				: m_px(std::move(px))
				, m_py(std::move(py))
				, m_r(std::move(r))
			{
				// Can I reuse these?
				m_time = std::make_shared<FloatVariable>(TIME);
				m_worldX = std::make_shared<FloatVariable>(WORLD_X);
				m_worldY = std::make_shared<FloatVariable>(WORLD_Y);
			}

			void propagateValues(float* values) override
			{
				m_px->propagateValues(values);
				m_py->propagateValues(values);
				m_r->propagateValues(values);

				m_time->propagateValues(values);
				m_worldX->propagateValues(values);
				m_worldY->propagateValues(values);
			}

			[[nodiscard]]
			float getValue() const override
			{
				vec2 p = vec2(m_worldX->getValue() - m_px->getValue(), m_worldY->getValue() - m_py->getValue());
				return length(p) - m_r->getValue();
			}

			[[nodiscard]]
			std::string print() const override
			{
				return "(length(vec2(" + m_worldX->print() + " - " + m_px->print() + ", " + m_worldY->print() + " - " +
					   m_py->print() + ")) - " + m_r->print() + ")";
			}
		};

		struct Box : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> m_px;
			std::shared_ptr<IMathExpression> m_py;
			std::shared_ptr<IMathExpression> m_w;
			std::shared_ptr<IMathExpression> m_h;
			std::shared_ptr<IMathExpression> m_worldX;
			std::shared_ptr<IMathExpression> m_worldY;

		public:
			static constexpr uint8_t POS_X = 0;
			static constexpr uint8_t POS_Y = 1;
			static constexpr uint8_t SIZE_X = 2;
			static constexpr uint8_t SIZE_Y = 3;

			static constexpr uint8_t WORLD_X = 9;
			static constexpr uint8_t WORLD_Y = 10;

			Box(std::shared_ptr<IMathExpression> px, std::shared_ptr<IMathExpression> py,
				std::shared_ptr<IMathExpression> w, std::shared_ptr<IMathExpression> h)
				: m_px(std::move(px))
				, m_py(std::move(py))
				, m_w(std::move(w))
				, m_h(std::move(h))
			{
				m_worldX = std::make_shared<FloatVariable>(WORLD_X);
				m_worldY = std::make_shared<FloatVariable>(WORLD_Y);
			}

			void propagateValues(float* values) override
			{
				m_px->propagateValues(values);
				m_py->propagateValues(values);
				m_w->propagateValues(values);
				m_h->propagateValues(values);

				m_worldX->propagateValues(values);
				m_worldY->propagateValues(values);
			}

			[[nodiscard]]
			float getValue() const override
			{
				vec2 p = vec2(m_worldX->getValue() - m_px->getValue(), m_worldY->getValue() - m_py->getValue());
				vec2 b = vec2(m_w->getValue(), m_h->getValue());
				vec2 d = abs(p) - b;
				return length(max(d, vec2(0.0))) + std::min(std::max(d.x, d.y), 0.0f);
			}

			[[nodiscard]]
			std::string print() const override
			{
				return "sdBox(vec2(" + m_worldX->print() + " - " + m_px->print() + ", " + m_worldY->print() + " - " +
					   m_py->print() + "), vec2(" + m_w->print() + ", " + m_h->print() + "))";
			}
		};

		struct BoxLine : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> m_px;
			std::shared_ptr<IMathExpression> m_py;
			std::shared_ptr<IMathExpression> m_w;
			std::shared_ptr<IMathExpression> m_h;
			std::shared_ptr<IMathExpression> m_thickness;
			std::shared_ptr<IMathExpression> m_worldX;
			std::shared_ptr<IMathExpression> m_worldY;

		public:
			static constexpr uint8_t POS_X = 0;
			static constexpr uint8_t POS_Y = 1;
			static constexpr uint8_t SIZE_X = 2;
			static constexpr uint8_t SIZE_Y = 3;
			static constexpr uint8_t THICKNESS = 4;

			static constexpr uint8_t WORLD_X = 9;
			static constexpr uint8_t WORLD_Y = 10;

			BoxLine(std::shared_ptr<IMathExpression> px, std::shared_ptr<IMathExpression> py,
					std::shared_ptr<IMathExpression> w, std::shared_ptr<IMathExpression> h,
					std::shared_ptr<IMathExpression> thickness)
				: m_px(std::move(px))
				, m_py(std::move(py))
				, m_w(std::move(w))
				, m_h(std::move(h))
				, m_thickness(std::move(thickness))
			{
				m_worldX = std::make_shared<FloatVariable>(WORLD_X);
				m_worldY = std::make_shared<FloatVariable>(WORLD_Y);
			}

			void propagateValues(float* values) override
			{
				m_px->propagateValues(values);
				m_py->propagateValues(values);
				m_w->propagateValues(values);
				m_h->propagateValues(values);
				m_thickness->propagateValues(values);

				m_worldX->propagateValues(values);
				m_worldY->propagateValues(values);
			}

			[[nodiscard]]
			float getValue() const override
			{
				vec2 p = vec2(m_worldX->getValue() - m_px->getValue(), m_worldY->getValue() - m_py->getValue());
				vec2 b = vec2(m_w->getValue(), m_h->getValue());
				vec2 d = abs(p) - b;
				float boxSdf = length(max(d, vec2(0.0))) + std::min(std::max(d.x, d.y), 0.0f);
				return std::abs(boxSdf) - m_thickness->getValue();
			}

			[[nodiscard]]
			std::string print() const override
			{
				return "abs(sdBox(vec2(" + m_worldX->print() + " - " + m_px->print() + ", " + m_worldY->print() +
					   " - " + m_py->print() + "), vec2(" + m_w->print() + ", " + m_h->print() + "))) - " +
					   m_thickness->print();
			}
		};

		struct SineWave : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> m_amplitude;
			std::shared_ptr<IMathExpression> m_period;
			std::shared_ptr<IMathExpression> m_speed;
			std::shared_ptr<IMathExpression> m_offset;
			std::shared_ptr<IMathExpression> m_time;
			std::shared_ptr<IMathExpression> m_worldX;
			std::shared_ptr<IMathExpression> m_worldY;

		public:
			static constexpr uint8_t AMPLITUDE = 0;
			static constexpr uint8_t PERIOD = 1;
			static constexpr uint8_t SPEED = 2;
			static constexpr uint8_t OFFSET = 3;

			static constexpr uint8_t TIME = 8;
			static constexpr uint8_t WORLD_X = 9;
			static constexpr uint8_t WORLD_Y = 10;

			SineWave(std::shared_ptr<IMathExpression> amplitude, std::shared_ptr<IMathExpression> period,
				 std::shared_ptr<IMathExpression> speed, std::shared_ptr<IMathExpression> offset)
				: m_amplitude(std::move(amplitude))
				, m_period(std::move(period))
				, m_speed(std::move(speed))
				, m_offset(std::move(offset))
			{
				m_time = std::make_shared<FloatVariable>(TIME);
				m_worldX = std::make_shared<FloatVariable>(WORLD_X);
				m_worldY = std::make_shared<FloatVariable>(WORLD_Y);
			}

			void propagateValues(float* values) override
			{
				m_amplitude->propagateValues(values);
				m_period->propagateValues(values);
				m_speed->propagateValues(values);
				m_offset->propagateValues(values);

				m_time->propagateValues(values);
				m_worldX->propagateValues(values);
				m_worldY->propagateValues(values);
			}

			[[nodiscard]]
			float getValue() const override
			{
				return (m_worldY->getValue() - m_offset->getValue()) -
					   m_amplitude->getValue() * sinf(m_period->getValue() * m_worldX->getValue() +
												 m_speed->getValue() * m_time->getValue());
			}

			[[nodiscard]]
			std::string print() const override
			{
				return "(" + m_worldY->print() + " - " + m_offset->print() + ") - " + m_amplitude->print() +
					   " * sin(" + m_period->print() + " * " + m_worldX->print() + " + " + m_speed->print() + " * " +
					   m_time->print() + ")";
			}
		};

		struct Ramp : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> m_px;
			std::shared_ptr<IMathExpression> m_py;
			std::shared_ptr<IMathExpression> m_w;
			std::shared_ptr<IMathExpression> m_h;
			std::shared_ptr<IMathExpression> m_skew;
			std::shared_ptr<IMathExpression> m_worldX;
			std::shared_ptr<IMathExpression> m_worldY;

		public:
			static constexpr uint8_t POS_X = 0;
			static constexpr uint8_t POS_Y = 1;
			static constexpr uint8_t WIDTH = 2;
			static constexpr uint8_t HEIGHT = 3;
			static constexpr uint8_t SKEW = 4;

			static constexpr uint8_t WORLD_X = 9;
			static constexpr uint8_t WORLD_Y = 10;

			Ramp(std::shared_ptr<IMathExpression> px, std::shared_ptr<IMathExpression> py,
				 std::shared_ptr<IMathExpression> width, std::shared_ptr<IMathExpression> height,
				 std::shared_ptr<IMathExpression> skew)
				: m_px(std::move(px))
				, m_py(std::move(py))
				, m_w(std::move(width))
				, m_h(std::move(height))
				, m_skew(std::move(skew))
			{
				m_worldX = std::make_shared<FloatVariable>(WORLD_X);
				m_worldY = std::make_shared<FloatVariable>(WORLD_Y);
			}

			void propagateValues(float* values) override
			{
				m_px->propagateValues(values);
				m_py->propagateValues(values);
				m_w->propagateValues(values);
				m_h->propagateValues(values);
				m_skew->propagateValues(values);

				m_worldX->propagateValues(values);
				m_worldY->propagateValues(values);
			}

			[[nodiscard]]
			float getValue() const override
			{
				vec2 p = vec2(m_worldX->getValue() - m_px->getValue(), m_worldY->getValue() - m_py->getValue());
				float wi = m_w->getValue();
				float he = m_h->getValue();
				float sk = m_skew->getValue();

				glm::vec2 e(wi, sk);
				if (p.x < 0.0f)
					p = -p;
				glm::vec2 w = p - e;
				w.y -= std::clamp(w.y, -he, he);
				glm::vec2 d(glm::dot(w, w), -w.x);
				float s = p.y * e.x - p.x * e.y;
				if (s < 0.0f)
					p = -p;
				glm::vec2 v = p - glm::vec2(0.0f, he);
				v -= e * std::clamp(glm::dot(v, e) / glm::dot(e, e), -1.0f, 1.0f);
				d = glm::min(d, glm::vec2(glm::dot(v, v), wi * he - std::abs(s)));
				return std::sqrt(d.x) * std::copysign(1.0f, -d.y);
			}

			[[nodiscard]]
			std::string print() const override
			{
				return "sdParallelogramVertical(vec2(" + m_worldX->print() + " - " + m_px->print() + ", " +
					   m_worldY->print() + " - " + m_py->print() + "), " + m_w->print() + ", " + m_h->print() +
					   ", " + m_skew->print() + ")";
			}
		};

		struct Triangle : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> m_px;
			std::shared_ptr<IMathExpression> m_py;
			std::shared_ptr<IMathExpression> m_w;
			std::shared_ptr<IMathExpression> m_h;
			std::shared_ptr<IMathExpression> m_rotation;
			std::shared_ptr<IMathExpression> m_worldX;
			std::shared_ptr<IMathExpression> m_worldY;

		public:
			static constexpr uint8_t POS_X = 0;
			static constexpr uint8_t POS_Y = 1;
			static constexpr uint8_t SIZE_X = 2;
			static constexpr uint8_t SIZE_Y = 3;
			static constexpr uint8_t ROTATION = 4;

			static constexpr uint8_t WORLD_X = 9;
			static constexpr uint8_t WORLD_Y = 10;

			Triangle(std::shared_ptr<IMathExpression> px, std::shared_ptr<IMathExpression> py,
				 std::shared_ptr<IMathExpression> w, std::shared_ptr<IMathExpression> h,
				 std::shared_ptr<IMathExpression> rotation)
				: m_px(std::move(px))
				, m_py(std::move(py))
				, m_w(std::move(w))
				, m_h(std::move(h))
				, m_rotation(std::move(rotation))
			{
				m_worldX = std::make_shared<FloatVariable>(WORLD_X);
				m_worldY = std::make_shared<FloatVariable>(WORLD_Y);
			}

			void propagateValues(float* values) override
			{
				m_px->propagateValues(values);
				m_py->propagateValues(values);
				m_w->propagateValues(values);
				m_h->propagateValues(values);
				m_rotation->propagateValues(values);

				m_worldX->propagateValues(values);
				m_worldY->propagateValues(values);
			}

			static float cross(const vec2& a, const vec2& b)
			{
				return a.x * b.y - a.y * b.x;
			}

			static float distanceToSegment(const vec2& p, const vec2& a, const vec2& b)
			{
				vec2 pa = p - a;
				vec2 ba = b - a;
				float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
				return length(pa - ba * h);
			}

			static float signedDistanceToTriangle(const vec2& p, const vec2& a, const vec2& b, const vec2& c)
			{
				float d = std::min(std::min(distanceToSegment(p, a, b), distanceToSegment(p, b, c)),
							 distanceToSegment(p, c, a));

				float c0 = cross(b - a, p - a);
				float c1 = cross(c - b, p - b);
				float c2 = cross(a - c, p - c);

				bool inside = (c0 >= 0.0f && c1 >= 0.0f && c2 >= 0.0f) ||
						(c0 <= 0.0f && c1 <= 0.0f && c2 <= 0.0f);

				return inside ? -d : d;
			}

			[[nodiscard]]
			float getValue() const override
			{
				vec2 p = vec2(m_worldX->getValue() - m_px->getValue(), m_worldY->getValue() - m_py->getValue());
				float angle = m_rotation->getValue();
				float c = cosf(angle);
				float s = sinf(angle);

				auto rotate = [&](const vec2& v) {
					return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
				};

				float halfWidth = m_w->getValue() * 0.5f;
				float height = m_h->getValue();

				vec2 a = rotate(vec2(-halfWidth, -height / 3.0f));
				vec2 b = rotate(vec2(halfWidth, -height / 3.0f));
				vec2 c2 = rotate(vec2(0.0f, 2.0f * height / 3.0f));

				return signedDistanceToTriangle(p, a, b, c2);
			}

			[[nodiscard]]
			std::string print() const override
			{
				return "sdTriangle(vec2(" + m_worldX->print() + " - " + m_px->print() + ", " +
					   m_worldY->print() + " - " + m_py->print() + "), " + m_w->print() + ", " +
					   m_h->print() + ", " + m_rotation->print() + ")";
			}
		};

		struct Line : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> m_ax;
			std::shared_ptr<IMathExpression> m_ay;
			std::shared_ptr<IMathExpression> m_bx;
			std::shared_ptr<IMathExpression> m_by;
			std::shared_ptr<IMathExpression> m_width;
			std::shared_ptr<IMathExpression> m_worldX;
			std::shared_ptr<IMathExpression> m_worldY;

		public:
			static constexpr uint8_t POS_A_X = 0;
			static constexpr uint8_t POS_A_Y = 1;
			static constexpr uint8_t POS_B_X = 2;
			static constexpr uint8_t POS_B_Y = 3;
			static constexpr uint8_t WIDTH = 4;

			static constexpr uint8_t WORLD_X = 9;
			static constexpr uint8_t WORLD_Y = 10;

			Line(std::shared_ptr<IMathExpression> ax, std::shared_ptr<IMathExpression> ay,
				 std::shared_ptr<IMathExpression> bx, std::shared_ptr<IMathExpression> by,
				 std::shared_ptr<IMathExpression> width)
				: m_ax(std::move(ax))
				, m_ay(std::move(ay))
				, m_bx(std::move(bx))
				, m_by(std::move(by))
				, m_width(std::move(width))
			{
				m_worldX = std::make_shared<FloatVariable>(WORLD_X);
				m_worldY = std::make_shared<FloatVariable>(WORLD_Y);
			}

			void propagateValues(float* values) override
			{
				m_ax->propagateValues(values);
				m_ay->propagateValues(values);
				m_bx->propagateValues(values);
				m_by->propagateValues(values);
				m_width->propagateValues(values);

				m_worldX->propagateValues(values);
				m_worldY->propagateValues(values);
			}

			[[nodiscard]]
			float getValue() const override
			{
				vec2 p = vec2(m_worldX->getValue(), m_worldY->getValue());

				vec2 a = vec2(m_ax->getValue(), m_ay->getValue());
				vec2 b = vec2(m_bx->getValue(), m_by->getValue());

				float width = m_width->getValue();

				vec2 pa = p - a, ba = b - a;
				float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
				return length(pa - ba * h) - width;
			}

			[[nodiscard]]
			std::string print() const override
			{
				return "sdSegment(vec2(" + m_worldX->print() + ", " + m_worldY->print() + "), vec2(" +
					   m_ax->print() + ", " + m_ay->print() + "), vec2(" + m_bx->print() + ", " + m_by->print() +
					   ")) - " + m_width->print();
			}
		};
} // namespace WeirdEngine