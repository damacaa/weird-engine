#pragma once

#include "weird-engine/math/MathExpressions.h"
#include "weird-renderer/core/Display.h"
#include <atomic>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace WeirdEngine
{
	inline bool getConstantVal(const std::shared_ptr<IMathExpression>& node, float& val)
	{
		if (node)
		{
			if (auto fc = dynamic_cast<FloatConstant*>(node.get()))
			{
				val = fc->getValue(nullptr);
				return true;
			}
		}
		return false;
	}

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
			float va, vb;
			bool aConst = getConstantVal(a.node, va);
			bool bConst = getConstantVal(b.node, vb);
			if (aConst && bConst)
			{
				return Expr(va + vb);
			}
			if (aConst && va == 0.0f)
			{
				return b;
			}
			if (bConst && vb == 0.0f)
			{
				return a;
			}
			return Expr(std::make_shared<Addition>(a.node, b.node));
		}
		friend Expr operator-(const Expr& a, const Expr& b)
		{
			float va, vb;
			bool aConst = getConstantVal(a.node, va);
			bool bConst = getConstantVal(b.node, vb);
			if (aConst && bConst)
			{
				return Expr(va - vb);
			}
			if (bConst && vb == 0.0f)
			{
				return a;
			}
			if (aConst && va == 0.0f)
			{
				return -b;
			}
			return Expr(std::make_shared<Subtraction>(a.node, b.node));
		}
		friend Expr operator*(const Expr& a, const Expr& b)
		{
			float va, vb;
			bool aConst = getConstantVal(a.node, va);
			bool bConst = getConstantVal(b.node, vb);
			if (aConst && bConst)
			{
				return Expr(va * vb);
			}
			if ((aConst && va == 0.0f) || (bConst && vb == 0.0f))
			{
				return Expr(0.0f);
			}
			if (aConst && va == 1.0f)
			{
				return b;
			}
			if (bConst && vb == 1.0f)
			{
				return a;
			}
			return Expr(std::make_shared<Multiplication>(a.node, b.node));
		}
		friend Expr operator/(const Expr& a, const Expr& b)
		{
			float va, vb;
			bool aConst = getConstantVal(a.node, va);
			bool bConst = getConstantVal(b.node, vb);
			if (aConst && bConst && vb != 0.0f)
			{
				return Expr(va / vb);
			}
			if (aConst && va == 0.0f)
			{
				return Expr(0.0f);
			}
			if (bConst && vb == 1.0f)
			{
				return a;
			}
			return Expr(std::make_shared<Division>(a.node, b.node));
		}
		friend Expr operator-(const Expr& a)
		{
			float va;
			if (getConstantVal(a.node, va))
			{
				return Expr(-va);
			}
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
	inline Vec2Expr localPoint(const glm::vec2& origin = {0.0f, 0.0f})
	{
		return {var(9) - origin.x, var(10) - origin.y};
	}
	inline Vec2Expr localPoint(float ox, float oy)
	{
		return localPoint(glm::vec2(ox, oy));
	}
	inline Vec2Expr songPoint()
	{
		float h = WeirdRenderer::Display::height > 0 ? static_cast<float>(WeirdRenderer::Display::height) : 800.0f;
		return localPoint(glm::vec2(70.0f, h - 70.0f));
	}
	inline Vec2Expr songPoint(const glm::vec2& center)
	{
		return localPoint(center);
	}
	inline Vec2Expr songPoint(float cx, float cy)
	{
		return localPoint(cx, cy);
	}

	inline Expr sin(const Expr& a)
	{
		float va;
		if (getConstantVal(a.node, va))
		{
			return Expr(std::sin(va));
		}
		return Expr(std::make_shared<Sine>(a.node));
	}
	inline Expr cos(const Expr& a)
	{
		float va;
		if (getConstantVal(a.node, va))
		{
			return Expr(std::cos(va));
		}
		return Expr(std::make_shared<Cosine>(a.node));
	}
	inline Expr abs(const Expr& a)
	{
		float va;
		if (getConstantVal(a.node, va))
		{
			return Expr(std::abs(va));
		}
		return Expr(std::make_shared<Abs>(a.node));
	}
	inline Expr sqrt(const Expr& a)
	{
		float va;
		if (getConstantVal(a.node, va) && va >= 0.0f)
		{
			return Expr(std::sqrt(va));
		}
		return Expr(std::make_shared<Sqrt>(a.node));
	}
	inline Expr min(const Expr& a, const Expr& b)
	{
		float va, vb;
		if (getConstantVal(a.node, va) && getConstantVal(b.node, vb))
		{
			return Expr(std::min(va, vb));
		}
		return Expr(std::make_shared<Min>(a.node, b.node));
	}
	inline Expr max(const Expr& a, const Expr& b)
	{
		float va, vb;
		if (getConstantVal(a.node, va) && getConstantVal(b.node, vb))
		{
			return Expr(std::max(va, vb));
		}
		return Expr(std::make_shared<Max>(a.node, b.node));
	}
	inline Expr clamp(const Expr& v, const Expr& lo, const Expr& hi)
	{
		float vv, vlo, vhi;
		if (getConstantVal(v.node, vv) && getConstantVal(lo.node, vlo) && getConstantVal(hi.node, vhi))
		{
			return Expr(std::clamp(vv, vlo, vhi));
		}
		return Expr(std::make_shared<Clamp>(v.node, lo.node, hi.node));
	}
	inline Expr mod(const Expr& a, const Expr& b)
	{
		float va, vb;
		if (getConstantVal(a.node, va) && getConstantVal(b.node, vb) && vb != 0.0f)
		{
			return Expr(va - vb * std::floor(va / vb));
		}
		return Expr(std::make_shared<Mod>(a.node, b.node));
	}
	inline Expr atan2(const Expr& y, const Expr& x)
	{
		float vy, vx;
		if (getConstantVal(y.node, vy) && getConstantVal(x.node, vx))
		{
			return Expr(std::atan2(vy, vx));
		}
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
		using WeirdEngine::localPoint;
		using WeirdEngine::songPoint;
		using WeirdEngine::uiPoint;
		using WeirdEngine::worldPoint;

		inline Vec2Expr translate(const Vec2Expr& p, const Vec2Expr& offset)
		{
			return p - offset;
		}

		inline Vec2Expr rotate(const Vec2Expr& p, const Expr& angle)
		{
			Expr c = cos(angle);
			Expr s = sin(angle);
			return {c * p.x - s * p.y, s * p.x + c * p.y};
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
		inline Expr sdfScale(const Expr& a, const Expr& b)
		{
			return Expr(std::make_shared<Scale>(a.node, b.node));
		}
		inline Expr scale(const Expr& a, const Expr& b)
		{
			return sdfScale(a, b);
		}

		inline Expr sdfSmoothUnion(const Expr& a, const Expr& b, const Expr& radius)
		{
			return Expr(std::make_shared<SDFSmoothAddition>(a.node, b.node, radius.node));
		}

		inline Expr sdfSmoothSubtract(const Expr& a, const Expr& b, const Expr& radius)
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

		inline Expr sdSineWave(const Vec2Expr& p, const Expr& amplitude, const Expr& frequency, const Expr& speed,
							   const Expr& offset = 0.0f)
		{
			return (p.y - offset) - amplitude * sin(frequency * p.x + speed * time());
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

			void collectHelperFunctions(std::unordered_set<std::string>& helpers) const override
			{
				if (m_point.x.node)
					m_point.x.node->collectHelperFunctions(helpers);
				if (m_point.y.node)
					m_point.y.node->collectHelperFunctions(helpers);

				std::string res = "float sdPolygonCustom_" + std::to_string(m_polyId) + "(in vec2 p) {\n";
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
				helpers.insert(res);
			}

			void getChildren(std::vector<std::shared_ptr<IMathExpression>>& out) const override
			{
				if (m_point.x.node)
					out.push_back(m_point.x.node);
				if (m_point.y.node)
					out.push_back(m_point.y.node);
			}

			[[nodiscard]]
			std::shared_ptr<IMathExpression> clone(
				const std::vector<std::shared_ptr<IMathExpression>>& c) const override
			{
				Vec2Expr pt = (c.size() >= 2) ? Vec2Expr{Expr(c[0]), Expr(c[1])} : m_point;
				return std::make_shared<Polygon>(std::move(pt), m_vertices);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "sdPolygonCustom_" + std::to_string(m_polyId) + "(vec2(" + c[0] + ", " + c[1] + "))";
			}

			[[nodiscard]]
			std::string print() const override
			{
				return printWithChildren(
					{m_point.x.node ? m_point.x.node->print() : "", m_point.y.node ? m_point.y.node->print() : ""});
			}
		};

		inline Expr sdPolygon(const Vec2Expr& p, std::vector<glm::vec2> vertices)
		{
			return Expr(std::make_shared<Polygon>(p, std::move(vertices)));
		}

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

		public:
			Triangle(Vec2Expr p, Expr w, Expr h)
				: m_p(std::move(p))
				, m_w(std::move(w))
				, m_h(std::move(h))
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
				float halfWidth = m_w.node->getValue(parameters) * 0.5f;
				float height = m_h.node->getValue(parameters);

				glm::vec2 a(-halfWidth, -height / 3.0f);
				glm::vec2 b(halfWidth, -height / 3.0f);
				glm::vec2 c(0.0f, 2.0f * height / 3.0f);

				return signedDistanceToTriangle(p, a, b, c);
			}

			void collectHelperFunctions(std::unordered_set<std::string>& helpers) const override
			{
				if (m_p.x.node)
					m_p.x.node->collectHelperFunctions(helpers);
				if (m_p.y.node)
					m_p.y.node->collectHelperFunctions(helpers);
				if (m_w.node)
					m_w.node->collectHelperFunctions(helpers);
				if (m_h.node)
					m_h.node->collectHelperFunctions(helpers);

				helpers.insert(R"(#ifndef WEIRD_SD_TRIANGLE
#define WEIRD_SD_TRIANGLE
float sdTriangle_impl(in vec2 p, float w, float h)
{
	vec2 p0 = vec2(-w * 0.5, -h / 3.0);
	vec2 p1 = vec2(w * 0.5, -h / 3.0);
	vec2 p2 = vec2(0.0, 2.0 * h / 3.0);

	vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
	vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;

	vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
	vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
	vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);

	float s = sign(e0.x * e2.y - e0.y * e2.x);
	vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
	                 vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
	                 vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));

	return -sqrt(d.x) * sign(d.y);
}
#endif)");
			}

			void getChildren(std::vector<std::shared_ptr<IMathExpression>>& out) const override
			{
				if (m_p.x.node)
					out.push_back(m_p.x.node);
				if (m_p.y.node)
					out.push_back(m_p.y.node);
				if (m_w.node)
					out.push_back(m_w.node);
				if (m_h.node)
					out.push_back(m_h.node);
			}

			[[nodiscard]]
			std::shared_ptr<IMathExpression> clone(
				const std::vector<std::shared_ptr<IMathExpression>>& c) const override
			{
				Vec2Expr pt = (c.size() >= 2) ? Vec2Expr{Expr(c[0]), Expr(c[1])} : m_p;
				Expr w = (c.size() >= 3) ? Expr(c[2]) : m_w;
				Expr h = (c.size() >= 4) ? Expr(c[3]) : m_h;
				return std::make_shared<Triangle>(std::move(pt), std::move(w), std::move(h));
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "sdTriangle_impl(vec2(" + c[0] + ", " + c[1] + "), " + c[2] + ", " + c[3] + ")";
			}

			[[nodiscard]]
			std::string print() const override
			{
				return printWithChildren({m_p.x.node ? m_p.x.node->print() : "", m_p.y.node ? m_p.y.node->print() : "",
										  m_w.node ? m_w.node->print() : "", m_h.node ? m_h.node->print() : ""});
			}
		};

		inline Expr sdTriangle(const Vec2Expr& p, const Expr& w, const Expr& h)
		{
			return Expr(std::make_shared<Triangle>(p, w, h));
		}

		struct Ramp : IMathExpression
		{
		private:
			Vec2Expr m_p;
			Expr m_w;
			Expr m_h;
			Expr m_skew;

		public:
			Ramp(Vec2Expr p, Expr w, Expr h, Expr skew)
				: m_p(std::move(p))
				, m_w(std::move(w))
				, m_h(std::move(h))
				, m_skew(std::move(skew))
			{
			}

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				glm::vec2 p(m_p.x.node->getValue(parameters), m_p.y.node->getValue(parameters));
				float wi = m_w.node->getValue(parameters);
				float he = m_h.node->getValue(parameters);
				float sk = m_skew.node->getValue(parameters);

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

			void collectHelperFunctions(std::unordered_set<std::string>& helpers) const override
			{
				if (m_p.x.node)
					m_p.x.node->collectHelperFunctions(helpers);
				if (m_p.y.node)
					m_p.y.node->collectHelperFunctions(helpers);
				if (m_w.node)
					m_w.node->collectHelperFunctions(helpers);
				if (m_h.node)
					m_h.node->collectHelperFunctions(helpers);
				if (m_skew.node)
					m_skew.node->collectHelperFunctions(helpers);

				helpers.insert(R"(#ifndef WEIRD_SD_PARALLELOGRAM
#define WEIRD_SD_PARALLELOGRAM
float sdParallelogramVertical(in vec2 p, float wi, float he, float sk)
{
	vec2 e = vec2(wi, sk);
	p = (p.x < 0.0) ? -p : p;
	vec2 w = p - e;
	w.y -= clamp(w.y, -he, he);
	vec2 d = vec2(dot(w, w), -w.x);
	float s = p.y * e.x - p.x * e.y;
	p = (s < 0.0) ? -p : p;
	vec2 v = p - vec2(0.0, he);
	v -= e * clamp(dot(v, e) / dot(e, e), -1.0, 1.0);
	d = min(d, vec2(dot(v, v), wi * he - abs(s)));
	return sqrt(d.x) * sign(-d.y);
}
#endif)");
			}

			void getChildren(std::vector<std::shared_ptr<IMathExpression>>& out) const override
			{
				if (m_p.x.node)
					out.push_back(m_p.x.node);
				if (m_p.y.node)
					out.push_back(m_p.y.node);
				if (m_w.node)
					out.push_back(m_w.node);
				if (m_h.node)
					out.push_back(m_h.node);
				if (m_skew.node)
					out.push_back(m_skew.node);
			}

			[[nodiscard]]
			std::shared_ptr<IMathExpression> clone(
				const std::vector<std::shared_ptr<IMathExpression>>& c) const override
			{
				Vec2Expr pt = (c.size() >= 2) ? Vec2Expr{Expr(c[0]), Expr(c[1])} : m_p;
				Expr w = (c.size() >= 3) ? Expr(c[2]) : m_w;
				Expr h = (c.size() >= 4) ? Expr(c[3]) : m_h;
				Expr skew = (c.size() >= 5) ? Expr(c[4]) : m_skew;
				return std::make_shared<Ramp>(std::move(pt), std::move(w), std::move(h), std::move(skew));
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "sdParallelogramVertical(vec2(" + c[0] + ", " + c[1] + "), " + c[2] + ", " + c[3] + ", " + c[4] +
					   ")";
			}

			[[nodiscard]]
			std::string print() const override
			{
				return printWithChildren({m_p.x.node ? m_p.x.node->print() : "", m_p.y.node ? m_p.y.node->print() : "",
										  m_w.node ? m_w.node->print() : "", m_h.node ? m_h.node->print() : "",
										  m_skew.node ? m_skew.node->print() : ""});
			}
		};

		inline Expr sdRamp(const Vec2Expr& p, const Expr& width, const Expr& height, const Expr& skew)
		{
			return Expr(std::make_shared<Ramp>(p, width, height, skew));
		}
	} // namespace SDF
} // namespace WeirdEngine
