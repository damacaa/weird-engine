#pragma once

#include "weird-engine/math/MathExpressions.h"
#include "weird-renderer/core/Display.h"
#include <cmath>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace WeirdEngine
{
	using namespace detail;

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

	// =========================================================================
	// Expr - Scalar AST Expression Wrapper
	// =========================================================================

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

	// =========================================================================
	// Vec2Expr - 2D Vector AST Expression Wrapper
	// =========================================================================

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

		Vec2Expr(const glm::vec2& v)
			: x(v.x)
			, y(v.y)
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

	// =========================================================================
	// System Variables & Evaluation Coordinates
	// =========================================================================

	namespace SystemParams
	{
		constexpr uint8_t TIME = 8;
		constexpr uint8_t POINT_X = 9;
		constexpr uint8_t POINT_Y = 10;
		constexpr uint8_t AUDIO_VOLUME = 11;
	} // namespace SystemParams

	inline Expr var(int index)
	{
		return Expr(std::make_shared<FloatVariable>(index));
	}

	inline Expr time()
	{
		return var(SystemParams::TIME);
	}

	inline Expr audioVolume()
	{
		return var(SystemParams::AUDIO_VOLUME);
	}

	inline Vec2Expr point()
	{
		return {var(SystemParams::POINT_X), var(SystemParams::POINT_Y)};
	}

	inline Vec2Expr samplePoint()
	{
		return point();
	}

	// =========================================================================
	// Scalar Math Functions with Constant Folding
	// =========================================================================

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

	inline Expr sign(const Expr& a)
	{
		float va;
		if (getConstantVal(a.node, va))
		{
			return Expr((va > 0.0f) ? 1.0f : ((va < 0.0f) ? -1.0f : 0.0f));
		}

		return Expr(std::make_shared<Sign>(a.node));
	}

	inline Expr step(const Expr& edge, const Expr& x)
	{
		float ve, vx;
		if (getConstantVal(edge.node, ve) && getConstantVal(x.node, vx))
		{
			return Expr((vx >= ve) ? 1.0f : 0.0f);
		}

		return Expr(std::make_shared<Step>(edge.node, x.node));
	}

	// =========================================================================
	// Vector Math Functions
	// =========================================================================

	inline Expr length(const Vec2Expr& p)
	{
		float vx, vy;
		if (getConstantVal(p.x.node, vx) && getConstantVal(p.y.node, vy))
		{
			return Expr(std::hypot(vx, vy));
		}

		return Expr(std::make_shared<Length>(p.x.node, p.y.node));
	}

	inline Expr dot(const Vec2Expr& a, const Vec2Expr& b)
	{
		return a.x * b.x + a.y * b.y;
	}

	// =========================================================================
	// SDF Primitives, Transforms, and CSG Combinations
	// =========================================================================

	namespace SDF
	{
		using WeirdEngine::point;
		using WeirdEngine::samplePoint;

		// --- Transforms ---

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

		// --- CSG & Domain Operations ---

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
			return a * b;
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

		// --- 2D Primitives ---

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

		inline Expr sdTriangle(const Vec2Expr& p, const Expr& w, const Expr& h)
		{
			Vec2Expr p0 = {-w * 0.5f, -h / 3.0f};
			Vec2Expr p1 = {w * 0.5f, -h / 3.0f};
			Vec2Expr p2 = {0.0f, 2.0f * h / 3.0f};

			Vec2Expr e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
			Vec2Expr v0 = p - p0, v1 = p - p1, v2 = p - p2;

			Vec2Expr pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0f, 1.0f);
			Vec2Expr pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0f, 1.0f);
			Vec2Expr pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0f, 1.0f);

			Expr s = sign(e0.x * e2.y - e0.y * e2.x);

			Expr dx = min(min(dot(pq0, pq0), dot(pq1, pq1)), dot(pq2, pq2));
			Expr dy = min(min(s * (v0.x * e0.y - v0.y * e0.x), s * (v1.x * e1.y - v1.y * e1.x)),
						  s * (v2.x * e2.y - v2.y * e2.x));

			return -sqrt(dx) * sign(dy);
		}

		inline Expr sdRamp(const Vec2Expr& pIn, const Expr& wi, const Expr& he, const Expr& sk)
		{
			Vec2Expr e = {wi, sk};
			Expr condX = step(0.0f, pIn.x) * 2.0f - 1.0f;
			Vec2Expr p = pIn * condX;

			Vec2Expr w = p - e;
			Expr wy = w.y - clamp(w.y, -he, he);
			Vec2Expr wClamped = {w.x, wy};

			Expr dx0 = dot(wClamped, wClamped);
			Expr dy0 = -w.x;

			Expr s = p.y * e.x - p.x * e.y;
			Expr condS = step(0.0f, s) * 2.0f - 1.0f;
			p = p * condS;

			Vec2Expr v = p - Vec2Expr{0.0f, he};
			Expr dotEE = dot(e, e);
			Expr h = clamp(dot(v, e) / dotEE, -1.0f, 1.0f);
			v = v - e * h;

			Expr dx1 = dot(v, v);
			Expr dy1 = wi * he - abs(s);

			Expr dx = min(dx0, dx1);
			Expr dy = min(dy0, dy1);

			return sqrt(dx) * sign(-dy);
		}

		inline Expr sdPolygon(const Vec2Expr& p, const std::vector<glm::vec2>& vertices)
		{
			const size_t N = vertices.size();
			if (N < 3)
			{
				return Expr(0.0f);
			}

			Expr d;
			Expr s = 1.0f;

			for (size_t i = 0, j = N - 1; i < N; j = i, i++)
			{
				Vec2Expr vi = {vertices[i].x, vertices[i].y};
				Vec2Expr vj = {vertices[j].x, vertices[j].y};
				Vec2Expr e = vj - vi;
				Vec2Expr w = p - vi;

				Expr dotEE = dot(e, e);
				Expr h = clamp(dot(w, e) / dotEE, 0.0f, 1.0f);
				Vec2Expr b = w - e * h;
				Expr edgeDistSq = dot(b, b);

				if (i == 0)
				{
					d = edgeDistSq;
				}
				else
				{
					d = min(d, edgeDistSq);
				}

				// Ray-crossing condition:
				// c.x = (p.y >= vi.y)
				// c.y = (p.y < vj.y)
				// c.z = (e.x * w.y > e.y * w.x)
				// flip when all(c) || all(!c)
				Expr cx = step(vi.y, p.y);
				Expr cy = 1.0f - step(vj.y, p.y);
				Expr cz = step(0.0f, e.x * w.y - e.y * w.x);

				Expr allC = cx * cy * cz;
				Expr allNotC = (1.0f - cx) * (1.0f - cy) * (1.0f - cz);
				Expr flip = allC + allNotC;

				s = s * (1.0f - 2.0f * flip);
			}

			return s * sqrt(d);
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

			return sdfErode(sdPolygon(p, vertices), valleyRadius);
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

	} // namespace SDF
} // namespace WeirdEngine
