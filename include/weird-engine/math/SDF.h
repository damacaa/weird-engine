#pragma once

#include "weird-engine/math/MathExpressions.h"
#include <atomic>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace WeirdEngine
{
	struct Expr
	{
		std::shared_ptr<IMathExpression> node;

		Expr()
			: node(std::make_shared<FloatConstant>(0.0f))
		{
		}
		Expr(float constant)
			: node(std::make_shared<FloatConstant>(constant))
		{
		}
		Expr(std::shared_ptr<IMathExpression> n)
			: node(std::move(n))
		{
		}

		friend Expr operator+(const Expr& a, const Expr& b)
		{
			return Expr(std::make_shared<Addition>(a.node, b.node));
		}
		friend Expr operator-(const Expr& a, const Expr& b)
		{
			return Expr(std::make_shared<Subtraction>(a.node, b.node));
		}
		friend Expr operator*(const Expr& a, const Expr& b)
		{
			return Expr(std::make_shared<Multiplication>(a.node, b.node));
		}
		friend Expr operator/(const Expr& a, const Expr& b)
		{
			return Expr(std::make_shared<Division>(a.node, b.node));
		}
		friend Expr operator-(const Expr& a)
		{
			return Expr(std::make_shared<Negation>(a.node));
		}
	};

	struct Vec2Expr
	{
		Expr x, y;

		Vec2Expr()
			: x(0.0f)
			, y(0.0f)
		{
		}
		Vec2Expr(Expr x_, Expr y_)
			: x(std::move(x_))
			, y(std::move(y_))
		{
		}
		Vec2Expr(float x_, float y_)
			: x(x_)
			, y(y_)
		{
		}

		friend Vec2Expr operator+(const Vec2Expr& a, const Vec2Expr& b)
		{
			return {a.x + b.x, a.y + b.y};
		}
		friend Vec2Expr operator-(const Vec2Expr& a, const Vec2Expr& b)
		{
			return {a.x - b.x, a.y - b.y};
		}
		friend Vec2Expr operator*(const Vec2Expr& a, const Expr& s)
		{
			return {a.x * s, a.y * s};
		}
		friend Vec2Expr operator/(const Vec2Expr& a, const Expr& s)
		{
			return {a.x / s, a.y / s};
		}
	};

	inline Expr var(int index)
	{
		return Expr(std::make_shared<FloatVariable>(index));
	}
	inline Expr time()
	{
		return var(8);
	}
	inline Vec2Expr worldPoint()
	{
		return {var(9), var(10)};
	}
	inline Vec2Expr uiPoint()
	{
		return {var(11), var(12)};
	}

	inline Expr sin(const Expr& a)
	{
		return Expr(std::make_shared<Sine>(a.node));
	}
	inline Expr cos(const Expr& a)
	{
		return Expr(std::make_shared<Cosine>(a.node));
	}
	inline Expr abs(const Expr& a)
	{
		return Expr(std::make_shared<Abs>(a.node));
	}
	inline Expr sqrt(const Expr& a)
	{
		return Expr(std::make_shared<Sqrt>(a.node));
	}
	inline Expr min(const Expr& a, const Expr& b)
	{
		return Expr(std::make_shared<Min>(a.node, b.node));
	}
	inline Expr max(const Expr& a, const Expr& b)
	{
		return Expr(std::make_shared<Max>(a.node, b.node));
	}
	inline Expr clamp(const Expr& v, const Expr& lo, const Expr& hi)
	{
		return Expr(std::make_shared<Clamp>(v.node, lo.node, hi.node));
	}
	inline Expr atan2(const Expr& y, const Expr& x)
	{
		return Expr(std::make_shared<Atan2>(y.node, x.node));
	}
	inline Expr length(const Vec2Expr& p)
	{
		return Expr(std::make_shared<Length>(p.x.node, p.y.node));
	}
	inline Expr dot(const Vec2Expr& a, const Vec2Expr& b)
	{
		return a.x * b.x + a.y * b.y;
	}

	namespace SDF
	{
		inline Vec2Expr translate(const Vec2Expr& p, const Vec2Expr& offset)
		{
			return p - offset;
		}

		inline Vec2Expr rotate(const Vec2Expr& p, const Expr& angle)
		{
			Expr c = cos(angle);
			Expr s = sin(angle);
			return {c * p.x + s * p.y, -s * p.x + c * p.y};
		}

		inline Vec2Expr mirrorX(const Vec2Expr& p)
		{
			return {abs(p.x), p.y};
		}

		inline Expr sdCircle(const Vec2Expr& p, const Expr& radius)
		{
			return length(p) - radius;
		}

		inline Expr sdBox(const Vec2Expr& p, const Vec2Expr& halfSize)
		{
			Expr dx = abs(p.x) - halfSize.x;
			Expr dy = abs(p.y) - halfSize.y;
			return length({max(dx, 0.0f), max(dy, 0.0f)}) + min(max(dx, dy), 0.0f);
		}

		inline Expr sdSegment(const Vec2Expr& p, const Vec2Expr& a, const Vec2Expr& b)
		{
			Vec2Expr pa = p - a;
			Vec2Expr ba = b - a;
			Expr h = clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
			return length(pa - ba * h);
		}

		inline Expr sdLine(const Vec2Expr& p, const Vec2Expr& a, const Vec2Expr& b, const Expr& width)
		{
			return sdSegment(p, a, b) - width;
		}

		inline Expr sdfUnion(const Expr& a, const Expr& b)
		{
			return min(a, b);
		}
		inline Expr sdfSubtract(const Expr& a, const Expr& b)
		{
			return max(a, -b);
		}
		inline Expr sdfIntersect(const Expr& a, const Expr& b)
		{
			return max(a, b);
		}
		inline Expr sdfOnion(const Expr& d, const Expr& thickness)
		{
			return abs(d) - thickness;
		}
		inline Expr sdfRound(const Expr& d, const Expr& radius)
		{
			return d - radius;
		}
		inline Expr sdfErode(const Expr& d, const Expr& radius)
		{
			return d + radius;
		}

		inline Expr smoothUnion(const Expr& a, const Expr& b, const Expr& radius)
		{
			return Expr(std::make_shared<SDFSmoothAddition>(a.node, b.node, radius.node));
		}

		inline Expr smoothSubtract(const Expr& a, const Expr& b, const Expr& radius)
		{
			return Expr(std::make_shared<SDFSmoothSubtraction>(a.node, b.node, radius.node));
		}

		inline Expr sdStar(const Vec2Expr& p, const Expr& radius, const Expr& displacement, const Expr& points,
						   const Expr& speed)
		{
			Expr dist = length(p) - radius;
			Expr angle = points * atan2(p.y, p.x) - speed * time();
			return dist + displacement * sin(angle);
		}

		struct Polygon : IMathExpression
		{
		private:
			Vec2Expr m_point;
			std::vector<glm::vec2> m_vertices;
			int m_polyId;

			static std::atomic<int>& getPolyCounter()
			{
				static std::atomic<int> s_polyCount{0};
				return s_polyCount;
			}

		public:
			Polygon(Vec2Expr point, std::vector<glm::vec2> vertices)
				: m_point(std::move(point))
				, m_vertices(std::move(vertices))
			{
				m_polyId = getPolyCounter()++;
			}

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				glm::vec2 p(m_point.x.node->getValue(parameters), m_point.y.node->getValue(parameters));
				float d = glm::dot(p - m_vertices[0], p - m_vertices[0]);
				float s = 1.0f;
				int N = m_vertices.size();

				for (int i = 0, j = N - 1; i < N; j = i, i++)
				{
					glm::vec2 e = m_vertices[j] - m_vertices[i];
					glm::vec2 w = p - m_vertices[i];
					glm::vec2 b = w - e * glm::clamp(glm::dot(w, e) / glm::dot(e, e), 0.0f, 1.0f);
					d = std::min(d, glm::dot(b, b));
					glm::bvec3 c = glm::bvec3(p.y >= m_vertices[i].y, p.y<m_vertices[j].y, e.x * w.y> e.y * w.x);
					glm::bvec3 notC = glm::bvec3(!c.x, !c.y, !c.z);
					if (glm::all(c) || glm::all(notC))
						s *= -1.0f;
				}
				return s * std::sqrt(d);
			}

			[[nodiscard]]
			std::string getHelperFunctions() const override
			{
				std::string res = m_point.x.node->getHelperFunctions() + m_point.y.node->getHelperFunctions();
				res += "\nfloat sdPolygonCustom_" + std::to_string(m_polyId) + "(in vec2 p) {\n";
				res += "\tfloat d = dot(p - vec2(" + std::to_string(m_vertices[0].x) + ", " +
					   std::to_string(m_vertices[0].y) + "), p - vec2(" + std::to_string(m_vertices[0].x) + ", " +
					   std::to_string(m_vertices[0].y) + "));\n";
				res += "\tfloat s = 1.0;\n";
				for (size_t i = 0, j = m_vertices.size() - 1; i < m_vertices.size(); j = i, i++)
				{
					res += "\tvec2 e" + std::to_string(i) + " = vec2(" + std::to_string(m_vertices[j].x) + ", " +
						   std::to_string(m_vertices[j].y) + ") - vec2(" + std::to_string(m_vertices[i].x) + ", " +
						   std::to_string(m_vertices[i].y) + ");\n";
					res += "\tvec2 w" + std::to_string(i) + " = p - vec2(" + std::to_string(m_vertices[i].x) + ", " +
						   std::to_string(m_vertices[i].y) + ");\n";
					res += "\tvec2 b" + std::to_string(i) + " = w" + std::to_string(i) + " - e" + std::to_string(i) +
						   " * clamp(dot(w" + std::to_string(i) + ", e" + std::to_string(i) + ") / dot(e" +
						   std::to_string(i) + ", e" + std::to_string(i) + "), 0.0, 1.0);\n";
					res += "\td = min(d, dot(b" + std::to_string(i) + ", b" + std::to_string(i) + "));\n";
					res += "\tbvec3 c" + std::to_string(i) + " = bvec3(p.y >= " + std::to_string(m_vertices[i].y) +
						   ", p.y < " + std::to_string(m_vertices[j].y) + ", e" + std::to_string(i) + ".x * w" +
						   std::to_string(i) + ".y > e" + std::to_string(i) + ".y * w" + std::to_string(i) + ".x);\n";
					res += "\tif(all(c" + std::to_string(i) + ") || all(not(c" + std::to_string(i) + "))) s *= -1.0;\n";
				}
				res += "\treturn s * sqrt(d);\n";
				res += "}\n";
				return res;
			}

			[[nodiscard]]
			std::string print() const override
			{
				return "sdPolygonCustom_" + std::to_string(m_polyId) + "(vec2(" + m_point.x.node->print() + ", " +
					   m_point.y.node->print() + "))";
			}
		};

		inline Expr sdTerrain(const Vec2Expr& p, const std::vector<glm::vec2>& surfacePoints, float valleyRadius,
							  float baseY = -2000.0f)
		{
			if (surfacePoints.size() < 2)
				return Expr(0.0f);

			std::vector<glm::vec2> vertices;
			vertices.reserve(surfacePoints.size() + 2);

			for (size_t i = 0; i < surfacePoints.size(); ++i)
			{
				glm::vec2 pt = surfacePoints[i];
				pt.y += valleyRadius;

				if (i == 0)
					pt.x -= valleyRadius;
				else if (i == surfacePoints.size() - 1)
					pt.x += valleyRadius;

				vertices.push_back(pt);
			}

			vertices.push_back({surfacePoints.back().x + valleyRadius, baseY});
			vertices.push_back({surfacePoints.front().x - valleyRadius, baseY});

			return sdfErode(Expr(std::make_shared<Polygon>(p, std::move(vertices))), valleyRadius);
		}

		struct Triangle : IMathExpression
		{
		private:
			Vec2Expr m_p;
			Expr m_w;
			Expr m_h;
			Expr m_rotation;

		public:
			Triangle(Vec2Expr p, Expr w, Expr h, Expr rotation)
				: m_p(std::move(p))
				, m_w(std::move(w))
				, m_h(std::move(h))
				, m_rotation(std::move(rotation))
			{
			}

			static float cross(const glm::vec2& a, const glm::vec2& b)
			{
				return a.x * b.y - a.y * b.x;
			}

			static float distanceToSegment(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b)
			{
				glm::vec2 pa = p - a;
				glm::vec2 ba = b - a;
				float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
				return glm::length(pa - ba * h);
			}

			static float signedDistanceToTriangle(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b,
												  const glm::vec2& c)
			{
				float d = std::min(std::min(distanceToSegment(p, a, b), distanceToSegment(p, b, c)),
								   distanceToSegment(p, c, a));
				float c0 = cross(b - a, p - a);
				float c1 = cross(c - b, p - b);
				float c2 = cross(a - c, p - c);
				bool inside = (c0 >= 0.0f && c1 >= 0.0f && c2 >= 0.0f) || (c0 <= 0.0f && c1 <= 0.0f && c2 <= 0.0f);
				return inside ? -d : d;
			}

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				glm::vec2 p(m_p.x.node->getValue(parameters), m_p.y.node->getValue(parameters));
				float angle = m_rotation.node->getValue(parameters);
				float c = cosf(angle);
				float s = sinf(angle);

				auto rotate = [&](const glm::vec2& v) { return glm::vec2(c * v.x - s * v.y, s * v.x + c * v.y); };

				float halfWidth = m_w.node->getValue(parameters) * 0.5f;
				float height = m_h.node->getValue(parameters);

				glm::vec2 a = rotate(glm::vec2(-halfWidth, -height / 3.0f));
				glm::vec2 b = rotate(glm::vec2(halfWidth, -height / 3.0f));
				glm::vec2 c2 = rotate(glm::vec2(0.0f, 2.0f * height / 3.0f));

				return signedDistanceToTriangle(p, a, b, c2);
			}

			[[nodiscard]]
			std::string getHelperFunctions() const override
			{
				std::string base = m_p.x.node->getHelperFunctions() + m_p.y.node->getHelperFunctions() +
								   m_w.node->getHelperFunctions() + m_h.node->getHelperFunctions() +
								   m_rotation.node->getHelperFunctions();
				return base + R"(
#ifndef WEIRD_SD_TRIANGLE
#define WEIRD_SD_TRIANGLE
float sdTriangle_impl(in vec2 p, float w, float h, float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	vec2 q = vec2(c * p.x - s * p.y, s * p.x + c * p.y);

	vec2 a = vec2(-w * 0.5, -h / 3.0);
	vec2 b = vec2(w * 0.5, -h / 3.0);
	vec2 c2 = vec2(0.0, 2.0 * h / 3.0);

	vec2 pa = q - a, ba = b - a;
	float h0 = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	float d0 = length(pa - ba * h0);

	vec2 pb = q - b, cb = c2 - b;
	float h1 = clamp(dot(pb, cb) / dot(cb, cb), 0.0, 1.0);
	float d1 = length(pb - cb * h1);

	vec2 pc2 = q - c2, ac2 = a - c2;
	float h2 = clamp(dot(pc2, ac2) / dot(ac2, ac2), 0.0, 1.0);
	float d2 = length(pc2 - ac2 * h2);

	float d = min(min(d0, d1), d2);

	float cross0 = (b.x - a.x) * (q.y - a.y) - (b.y - a.y) * (q.x - a.x);
	float cross1 = (c2.x - b.x) * (q.y - b.y) - (c2.y - b.y) * (q.x - b.x);
	float cross2 = (a.x - c2.x) * (q.y - c2.y) - (a.y - c2.y) * (q.x - c2.x);
	bool inside = (cross0 >= 0.0 && cross1 >= 0.0 && cross2 >= 0.0) || (cross0 <= 0.0 && cross1 <= 0.0 && cross2 <= 0.0);

	return inside ? -d : d;
}
#endif
)";
			}

			[[nodiscard]]
			std::string print() const override
			{
				return "sdTriangle_impl(vec2(" + m_p.x.node->print() + ", " + m_p.y.node->print() + "), " +
					   m_w.node->print() + ", " + m_h.node->print() + ", " + m_rotation.node->print() + ")";
			}
		};

		inline Expr sdTriangle(const Vec2Expr& p, const Expr& w, const Expr& h, const Expr& rotation)
		{
			return Expr(std::make_shared<Triangle>(p, w, h, rotation));
		}
	} // namespace SDF
} // namespace WeirdEngine
