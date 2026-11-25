#pragma once

#include "weird-engine/math/MathExpressions.h"
#include "weird-renderer/core/Display.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include <vector>

namespace WeirdEngine
{
	using namespace detail;

	struct Bounds2DExpr;
	struct PointFrame2D;
	struct Expr;

	inline std::shared_ptr<Bounds2DExpr> scaleBoundsMetric(const std::shared_ptr<Bounds2DExpr>& bounds,
														   const Expr& factor);

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
		std::shared_ptr<Bounds2DExpr> bounds;

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

		Expr(std::shared_ptr<IMathExpression> n, std::shared_ptr<Bounds2DExpr> b)
			: node(std::move(n))
			, bounds(std::move(b))
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

			Expr res;
			if (aConst && bConst)
			{
				res = Expr(va * vb);
			}
			else if ((aConst && va == 0.0f) || (bConst && vb == 0.0f))
			{
				res = Expr(0.0f);
			}
			else if (aConst && va == 1.0f)
			{
				res = b;
			}
			else if (bConst && vb == 1.0f)
			{
				res = a;
			}
			else
			{
				res = Expr(std::make_shared<Multiplication>(a.node, b.node));
			}

			// Scaling a bounded SDF by a non-negative scalar preserves its zero-contour AABB
			if (a.bounds && !b.bounds && !(bConst && vb <= 0.0f))
			{
				res.bounds = scaleBoundsMetric(a.bounds, b);
			}
			else if (b.bounds && !a.bounds && !(aConst && va <= 0.0f))
			{
				res.bounds = scaleBoundsMetric(b.bounds, a);
			}

			return res;
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
		std::shared_ptr<PointFrame2D> frame;

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
	};

	// Tracks the forward affine mapping from a transformed 2D sample coordinate `q` back to world space `p`:
	//   p = origin + q.x * axisX + q.y * axisY
	// Optional `parent` with `mirrorXBeforeParent` supports exact bounds across mirrorX().
	struct PointFrame2D
	{
		Vec2Expr origin{0.0f, 0.0f};
		Vec2Expr axisX{1.0f, 0.0f};
		Vec2Expr axisY{0.0f, 1.0f};
		std::shared_ptr<PointFrame2D> parent;
		bool mirrorXBeforeParent = false;

		static std::shared_ptr<PointFrame2D> identity()
		{
			return std::make_shared<PointFrame2D>();
		}
	};

	// =========================================================================
	// Bounds2DExpr - Symbolic 2D World Bounding Box Expression
	// =========================================================================
	// Uses the same var(0..7), time(), audioVolume() AST expressions as SDFs.
	// Supports half-bounded axes via +/-infinity and an unbounded fallback.
	struct Bounds2DExpr
	{
		Vec2Expr min{0.0f, 0.0f};
		Vec2Expr max{0.0f, 0.0f};
		Expr metricScale{1.0f}; // World distance per 1.0 unit of SDF value (1.0 in world space)
		bool isUnbounded = true;

		static constexpr float kNegInf = -std::numeric_limits<float>::infinity();
		static constexpr float kPosInf = std::numeric_limits<float>::infinity();

		Bounds2DExpr() = default;

		Bounds2DExpr(Vec2Expr minPt, Vec2Expr maxPt, Expr scale = Expr(1.0f))
			: min(std::move(minPt))
			, max(std::move(maxPt))
			, metricScale(std::move(scale))
			, isUnbounded(false)
		{
		}

		static Bounds2DExpr unbounded()
		{
			Bounds2DExpr b;
			b.min = Vec2Expr(kNegInf, kNegInf);
			b.max = Vec2Expr(kPosInf, kPosInf);
			b.isUnbounded = true;
			return b;
		}

		void evaluate(const float* params12, glm::vec2& outMin, glm::vec2& outMax) const
		{
			if (isUnbounded || !min.x.node || !min.y.node || !max.x.node || !max.y.node)
			{
				outMin = glm::vec2(kNegInf, kNegInf);
				outMax = glm::vec2(kPosInf, kPosInf);
				return;
			}

			const float minX = min.x.node->getValue(params12);
			const float minY = min.y.node->getValue(params12);
			const float maxX = max.x.node->getValue(params12);
			const float maxY = max.y.node->getValue(params12);

			if (std::isnan(minX) || std::isnan(minY) || std::isnan(maxX) || std::isnan(maxY))
			{
				outMin = glm::vec2(kNegInf, kNegInf);
				outMax = glm::vec2(kPosInf, kPosInf);
				return;
			}

			outMin = glm::vec2(minX, minY);
			outMax = glm::vec2(maxX, maxY);
		}
	};

	inline std::shared_ptr<Bounds2DExpr> scaleBoundsMetric(const std::shared_ptr<Bounds2DExpr>& bounds,
														   const Expr& factor)
	{
		if (!bounds || bounds->isUnbounded)
		{
			return bounds;
		}
		float vf = 0.0f;
		if (getConstantVal(factor.node, vf) && vf == 1.0f)
		{
			return bounds;
		}
		auto scaled = std::make_shared<Bounds2DExpr>(*bounds);
		auto absFactor = std::make_shared<Abs>(factor.node);
		auto safeFactor = std::make_shared<Max>(absFactor, std::make_shared<FloatConstant>(1e-6f));
		scaled->metricScale = scaled->metricScale / Expr(std::move(safeFactor));
		return scaled;
	}

	inline Vec2Expr operator+(const Vec2Expr& a, const Vec2Expr& b)
	{
		Vec2Expr res(a.x + b.x, a.y + b.y);
		if (a.frame && !b.frame)
		{
			auto f = std::make_shared<PointFrame2D>(*a.frame);
			f->origin = Vec2Expr(f->origin.x - (b.x * f->axisX.x + b.y * f->axisY.x),
								 f->origin.y - (b.x * f->axisX.y + b.y * f->axisY.y));
			res.frame = std::move(f);
		}
		else if (!a.frame && b.frame)
		{
			auto f = std::make_shared<PointFrame2D>(*b.frame);
			f->origin = Vec2Expr(f->origin.x - (a.x * f->axisX.x + a.y * f->axisY.x),
								 f->origin.y - (a.x * f->axisX.y + a.y * f->axisY.y));
			res.frame = std::move(f);
		}
		return res;
	}

	inline Vec2Expr operator-(const Vec2Expr& a, const Vec2Expr& b)
	{
		Vec2Expr res(a.x - b.x, a.y - b.y);
		if (a.frame && !b.frame)
		{
			auto f = std::make_shared<PointFrame2D>(*a.frame);
			f->origin = Vec2Expr(f->origin.x + (b.x * f->axisX.x + b.y * f->axisY.x),
								 f->origin.y + (b.x * f->axisX.y + b.y * f->axisY.y));
			res.frame = std::move(f);
		}
		else if (!a.frame && b.frame)
		{
			auto f = std::make_shared<PointFrame2D>(*b.frame);
			f->origin = Vec2Expr(f->origin.x + (a.x * f->axisX.x + a.y * f->axisY.x),
								 f->origin.y + (a.x * f->axisX.y + a.y * f->axisY.y));
			f->axisX = Vec2Expr(-f->axisX.x, -f->axisX.y);
			f->axisY = Vec2Expr(-f->axisY.x, -f->axisY.y);
			res.frame = std::move(f);
		}
		return res;
	}

	inline Vec2Expr operator*(const Vec2Expr& a, const Expr& s)
	{
		Vec2Expr res(a.x * s, a.y * s);
		float vs = 0.0f;
		if (a.frame && !(getConstantVal(s.node, vs) && vs == 0.0f))
		{
			auto f = std::make_shared<PointFrame2D>(*a.frame);
			f->axisX = Vec2Expr(f->axisX.x / s, f->axisX.y / s);
			f->axisY = Vec2Expr(f->axisY.x / s, f->axisY.y / s);
			res.frame = std::move(f);
		}
		return res;
	}

	inline Vec2Expr operator/(const Vec2Expr& a, const Expr& s)
	{
		Vec2Expr res(a.x / s, a.y / s);
		float vs = 0.0f;
		if (a.frame && !(getConstantVal(s.node, vs) && vs == 0.0f))
		{
			auto f = std::make_shared<PointFrame2D>(*a.frame);
			f->axisX = Vec2Expr(f->axisX.x * s, f->axisX.y * s);
			f->axisY = Vec2Expr(f->axisY.x * s, f->axisY.y * s);
			res.frame = std::move(f);
		}
		return res;
	}

	// =========================================================================
	// Vec3Expr - 3D Vector AST Expression Wrapper
	// =========================================================================

	struct Vec3Expr
	{
		Expr x, y, z;

		Vec3Expr()
			: x(0.0f)
			, y(0.0f)
			, z(0.0f)
		{
		}

		Vec3Expr(Expr x_, Expr y_, Expr z_)
			: x(std::move(x_))
			, y(std::move(y_))
			, z(std::move(z_))
		{
		}

		Vec3Expr(float x_, float y_, float z_)
			: x(x_)
			, y(y_)
			, z(z_)
		{
		}

		Vec3Expr(const glm::vec3& v)
			: x(v.x)
			, y(v.y)
			, z(v.z)
		{
		}

		friend Vec3Expr operator+(const Vec3Expr& a, const Vec3Expr& b)
		{
			return {a.x + b.x, a.y + b.y, a.z + b.z};
		}

		friend Vec3Expr operator-(const Vec3Expr& a, const Vec3Expr& b)
		{
			return {a.x - b.x, a.y - b.y, a.z - b.z};
		}

		friend Vec3Expr operator*(const Vec3Expr& a, const Expr& s)
		{
			return {a.x * s, a.y * s, a.z * s};
		}

		friend Vec3Expr operator*(const Expr& s, const Vec3Expr& a)
		{
			return {a.x * s, a.y * s, a.z * s};
		}

		friend Vec3Expr operator/(const Vec3Expr& a, const Expr& s)
		{
			return {a.x / s, a.y / s, a.z / s};
		}

		friend Vec3Expr operator-(const Vec3Expr& a)
		{
			return {-a.x, -a.y, -a.z};
		}

		Vec2Expr xy() const
		{
			return {x, y};
		}

		Vec2Expr xz() const
		{
			return {x, z};
		}

		Vec2Expr yz() const
		{
			return {y, z};
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
		constexpr uint8_t POINT_Z = 11; // 3D evaluation reuses slot 11 for Z (skipping audio volume)
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
		Vec2Expr p(var(SystemParams::POINT_X), var(SystemParams::POINT_Y));
		p.frame = PointFrame2D::identity();
		return p;
	}

	inline Vec3Expr point3D()
	{
		return {var(SystemParams::POINT_X), var(SystemParams::POINT_Y), var(SystemParams::POINT_Z)};
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

	inline Expr ternary(const Expr& cond, const Expr& a, const Expr& b)
	{
		float vc; //, va, vb;
		if (getConstantVal(cond.node, vc))
		{
			int icond = static_cast<int>(vc);
			if (icond >= 1)
			{
				return a;
			}
			else
			{
				return b;
			}
		}

		return Expr(std::make_shared<Ternary>(cond.node, a.node, b.node));
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

	inline Expr length(const Vec3Expr& p)
	{
		float vx, vy, vz;
		if (getConstantVal(p.x.node, vx) && getConstantVal(p.y.node, vy) && getConstantVal(p.z.node, vz))
		{
			return Expr(std::hypot(vx, vy, vz));
		}

		return Expr(std::make_shared<Length3D>(p.x.node, p.y.node, p.z.node));
	}

	inline Expr dot(const Vec3Expr& a, const Vec3Expr& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	inline Vec3Expr abs(const Vec3Expr& p)
	{
		return {abs(p.x), abs(p.y), abs(p.z)};
	}

	inline Vec3Expr max(const Vec3Expr& a, const Vec3Expr& b)
	{
		return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
	}

	inline Vec3Expr max(const Vec3Expr& a, const Expr& b)
	{
		return {max(a.x, b), max(a.y, b), max(a.z, b)};
	}

	inline Vec3Expr min(const Vec3Expr& a, const Vec3Expr& b)
	{
		return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)};
	}

	inline Vec3Expr min(const Vec3Expr& a, const Expr& b)
	{
		return {min(a.x, b), min(a.y, b), min(a.z, b)};
	}

	inline Expr vmax(const Vec3Expr& v)
	{
		return max(max(v.x, v.y), v.z);
	}

	inline Expr vmin(const Vec3Expr& v)
	{
		return min(min(v.x, v.y), v.z);
	}

	// =========================================================================
	// SDF Primitives, Transforms, and CSG Combinations
	// =========================================================================

	namespace SDF
	{
		using WeirdEngine::Bounds2DExpr;
		using WeirdEngine::point;
		using WeirdEngine::point3D;
		using WeirdEngine::samplePoint;
		using WeirdEngine::var;

		// --- Symbolic 2D Bounding Box Builders & Combinators ---

		inline Bounds2DExpr boundsUnbounded()
		{
			return Bounds2DExpr::unbounded();
		}

		inline Bounds2DExpr boundsMinMax(const Vec2Expr& minPt, const Vec2Expr& maxPt)
		{
			return Bounds2DExpr(minPt, maxPt);
		}

		inline Bounds2DExpr boundsBox(const Vec2Expr& center, const Vec2Expr& halfSize)
		{
			Expr hx = abs(halfSize.x);
			Expr hy = abs(halfSize.y);
			return Bounds2DExpr({center.x - hx, center.y - hy}, {center.x + hx, center.y + hy});
		}

		inline Bounds2DExpr boundsCircle(const Vec2Expr& center, const Expr& radius)
		{
			Expr r = max(radius, 0.0f);
			return Bounds2DExpr({center.x - r, center.y - r}, {center.x + r, center.y + r});
		}

		inline Bounds2DExpr boundsRotatedBox(const Vec2Expr& center, const Vec2Expr& halfSize, const Expr& angle)
		{
			Expr c = abs(cos(angle));
			Expr s = abs(sin(angle));
			Expr hx = abs(halfSize.x);
			Expr hy = abs(halfSize.y);
			Expr rx = c * hx + s * hy;
			Expr ry = s * hx + c * hy;
			return Bounds2DExpr({center.x - rx, center.y - ry}, {center.x + rx, center.y + ry});
		}

		inline Bounds2DExpr boundsSegment(const Vec2Expr& a, const Vec2Expr& b, const Expr& width)
		{
			Expr w = max(width, 0.0f);
			return Bounds2DExpr({min(a.x, b.x) - w, min(a.y, b.y) - w}, {max(a.x, b.x) + w, max(a.y, b.y) + w});
		}

		inline Bounds2DExpr boundsHalfPlaneBelow(const Expr& maxY)
		{
			return Bounds2DExpr({Bounds2DExpr::kNegInf, Bounds2DExpr::kNegInf}, {Bounds2DExpr::kPosInf, maxY});
		}

		inline Bounds2DExpr boundsHalfPlaneAbove(const Expr& minY)
		{
			return Bounds2DExpr({Bounds2DExpr::kNegInf, minY}, {Bounds2DExpr::kPosInf, Bounds2DExpr::kPosInf});
		}

		inline Bounds2DExpr boundsExpand(const Bounds2DExpr& b, const Expr& margin)
		{
			if (b.isUnbounded)
			{
				return b;
			}
			Expr m = max(margin, 0.0f);
			auto shiftMin = [&](const Expr& e) -> Expr
			{
				float v = 0.0f;
				if (getConstantVal(e.node, v) && !std::isfinite(v))
				{
					return e;
				}
				return e - m;
			};
			auto shiftMax = [&](const Expr& e) -> Expr
			{
				float v = 0.0f;
				if (getConstantVal(e.node, v) && !std::isfinite(v))
				{
					return e;
				}
				return e + m;
			};
			return Bounds2DExpr({shiftMin(b.min.x), shiftMin(b.min.y)}, {shiftMax(b.max.x), shiftMax(b.max.y)},
								b.metricScale);
		}

		inline Bounds2DExpr boundsTranslate(const Bounds2DExpr& b, const Vec2Expr& offset)
		{
			if (b.isUnbounded)
			{
				return b;
			}
			auto shift = [](const Expr& e, const Expr& delta) -> Expr
			{
				float v = 0.0f;
				if (getConstantVal(e.node, v) && !std::isfinite(v))
				{
					return e;
				}
				return e + delta;
			};
			return Bounds2DExpr({shift(b.min.x, offset.x), shift(b.min.y, offset.y)},
								{shift(b.max.x, offset.x), shift(b.max.y, offset.y)}, b.metricScale);
		}

		inline Bounds2DExpr boundsRotate(const Bounds2DExpr& b, const Vec2Expr& pivot, const Expr& angle)
		{
			if (b.isUnbounded)
			{
				return b;
			}
			float v = 0.0f;
			if ((getConstantVal(b.min.x.node, v) && !std::isfinite(v)) ||
				(getConstantVal(b.min.y.node, v) && !std::isfinite(v)) ||
				(getConstantVal(b.max.x.node, v) && !std::isfinite(v)) ||
				(getConstantVal(b.max.y.node, v) && !std::isfinite(v)))
			{
				return boundsUnbounded();
			}
			Vec2Expr center((b.min.x + b.max.x) * 0.5f - pivot.x, (b.min.y + b.max.y) * 0.5f - pivot.y);
			Vec2Expr halfSize(max((b.max.x - b.min.x) * 0.5f, 0.0f), max((b.max.y - b.min.y) * 0.5f, 0.0f));
			Expr c = cos(angle);
			Expr s = sin(angle);
			Vec2Expr rotCenter(pivot.x + c * center.x - s * center.y, pivot.y + s * center.x + c * center.y);
			Expr ac = abs(c);
			Expr as = abs(s);
			Expr rx = ac * halfSize.x + as * halfSize.y;
			Expr ry = as * halfSize.x + ac * halfSize.y;
			return Bounds2DExpr({rotCenter.x - rx, rotCenter.y - ry}, {rotCenter.x + rx, rotCenter.y + ry},
								b.metricScale);
		}

		inline Bounds2DExpr boundsUnion(const Bounds2DExpr& a, const Bounds2DExpr& b)
		{
			if (a.isUnbounded || b.isUnbounded)
			{
				return boundsUnbounded();
			}
			return Bounds2DExpr({min(a.min.x, b.min.x), min(a.min.y, b.min.y)},
								{max(a.max.x, b.max.x), max(a.max.y, b.max.y)}, max(a.metricScale, b.metricScale));
		}

		inline Bounds2DExpr boundsIntersect(const Bounds2DExpr& a, const Bounds2DExpr& b)
		{
			if (a.isUnbounded)
			{
				return b;
			}
			if (b.isUnbounded)
			{
				return a;
			}
			return Bounds2DExpr({max(a.min.x, b.min.x), max(a.min.y, b.min.y)},
								{min(a.max.x, b.max.x), min(a.max.y, b.max.y)}, max(a.metricScale, b.metricScale));
		}

		namespace bounds_detail
		{
			inline bool isNonFiniteConstant(const Expr& e)
			{
				float v = 0.0f;
				return getConstantVal(e.node, v) && !std::isfinite(v);
			}

			inline bool isAxisAlignedIdentityAxes(const PointFrame2D& f)
			{
				float xx = 0.0f, xy = 0.0f, yx = 0.0f, yy = 0.0f;
				return getConstantVal(f.axisX.x.node, xx) && getConstantVal(f.axisX.y.node, xy) &&
					   getConstantVal(f.axisY.x.node, yx) && getConstantVal(f.axisY.y.node, yy) && xx == 1.0f &&
					   xy == 0.0f && yx == 0.0f && yy == 1.0f;
			}

			// Maps a primitive's local AABB through the coordinate PointFrame2D chain into world space.
			inline std::shared_ptr<Bounds2DExpr> transformLocalBounds(const std::shared_ptr<PointFrame2D>& frame,
																	  const Bounds2DExpr& local)
			{
				if (!frame || local.isUnbounded)
				{
					return nullptr;
				}

				const PointFrame2D& f = *frame;
				const bool hasInf = isNonFiniteConstant(local.min.x) || isNonFiniteConstant(local.min.y) ||
									isNonFiniteConstant(local.max.x) || isNonFiniteConstant(local.max.y);

				Bounds2DExpr mapped;
				if (hasInf)
				{
					if (!isAxisAlignedIdentityAxes(f))
					{
						return nullptr;
					}
					mapped = boundsTranslate(local, f.origin);
				}
				else
				{
					Vec2Expr lc((local.min.x + local.max.x) * 0.5f, (local.min.y + local.max.y) * 0.5f);
					Vec2Expr lh(max((local.max.x - local.min.x) * 0.5f, 0.0f),
								max((local.max.y - local.min.y) * 0.5f, 0.0f));

					Vec2Expr wc(f.origin.x + f.axisX.x * lc.x + f.axisY.x * lc.y,
								f.origin.y + f.axisX.y * lc.x + f.axisY.y * lc.y);
					Expr whx = abs(f.axisX.x) * lh.x + abs(f.axisY.x) * lh.y;
					Expr why = abs(f.axisX.y) * lh.x + abs(f.axisY.y) * lh.y;

					Expr scaleX = length(f.axisX);
					Expr scaleY = length(f.axisY);
					Expr frameScale = max(scaleX, scaleY);
					mapped = Bounds2DExpr({wc.x - whx, wc.y - why}, {wc.x + whx, wc.y + why},
										  local.metricScale * frameScale);
				}

				if (f.parent)
				{
					if (f.mirrorXBeforeParent)
					{
						if (isNonFiniteConstant(mapped.min.x) || isNonFiniteConstant(mapped.max.x))
						{
							mapped.min.x = Expr(Bounds2DExpr::kNegInf);
							mapped.max.x = Expr(Bounds2DExpr::kPosInf);
						}
						else
						{
							Expr symX = max(abs(mapped.min.x), abs(mapped.max.x));
							mapped.min.x = -symX;
							mapped.max.x = symX;
						}
					}
					return transformLocalBounds(f.parent, mapped);
				}

				return std::make_shared<Bounds2DExpr>(std::move(mapped));
			}
		} // namespace bounds_detail

		// --- Transforms ---

		inline Vec2Expr translate(const Vec2Expr& p, const Vec2Expr& offset)
		{
			return p - offset;
		}

		inline Vec3Expr translate(const Vec3Expr& p, const Vec3Expr& offset)
		{
			return p - offset;
		}

		inline Vec2Expr rotate(const Vec2Expr& p, const Expr& angle)
		{
			Expr c = cos(angle);
			Expr s = sin(angle);
			Vec2Expr res(c * p.x - s * p.y, s * p.x + c * p.y);
			if (p.frame)
			{
				auto f = std::make_shared<PointFrame2D>(*p.frame);
				Vec2Expr oldX = f->axisX;
				Vec2Expr oldY = f->axisY;
				f->axisX = Vec2Expr(oldX.x * c - oldY.x * s, oldX.y * c - oldY.y * s);
				f->axisY = Vec2Expr(oldX.x * s + oldY.x * c, oldX.y * s + oldY.y * c);
				res.frame = std::move(f);
			}
			return res;
		}

		inline Vec3Expr rotateX(const Vec3Expr& p, const Expr& angle)
		{
			Expr c = cos(angle);
			Expr s = sin(angle);
			return {p.x, c * p.y - s * p.z, s * p.y + c * p.z};
		}

		inline Vec3Expr rotateY(const Vec3Expr& p, const Expr& angle)
		{
			Expr c = cos(angle);
			Expr s = sin(angle);
			return {c * p.x + s * p.z, p.y, -s * p.x + c * p.z};
		}

		inline Vec3Expr rotateZ(const Vec3Expr& p, const Expr& angle)
		{
			Expr c = cos(angle);
			Expr s = sin(angle);
			return {c * p.x - s * p.y, s * p.x + c * p.y, p.z};
		}

		inline Vec2Expr mirrorX(const Vec2Expr& p)
		{
			Vec2Expr res(abs(p.x), p.y);
			if (p.frame)
			{
				auto f = PointFrame2D::identity();
				f->parent = p.frame;
				f->mirrorXBeforeParent = true;
				res.frame = std::move(f);
			}
			return res;
		}

		inline Vec2Expr repeat(const Vec2Expr& p, const Expr& spacing)
		{
			return {mod(p.x + spacing * 0.5f, spacing) - spacing * 0.5f,
					mod(p.y + spacing * 0.5f, spacing) - spacing * 0.5f};
		}

		inline Vec2Expr repeat(const Vec2Expr& p, const Expr& spacingX, const Expr& spacingY)
		{
			return {mod(p.x + spacingX * 0.5f, spacingX) - spacingX * 0.5f,
					mod(p.y + spacingY * 0.5f, spacingY) - spacingY * 0.5f};
		}

		// --- CSG & Domain Operations ---

		inline Expr sdfUnion(const Expr& a, const Expr& b)
		{
			Expr res = min(a, b);
			if (a.bounds && b.bounds)
			{
				Bounds2DExpr u = boundsUnion(*a.bounds, *b.bounds);
				if (!u.isUnbounded)
				{
					res.bounds = std::make_shared<Bounds2DExpr>(std::move(u));
				}
			}
			return res;
		}

		inline Expr sdfSubtract(const Expr& a, const Expr& b)
		{
			Expr res = max(a, -b);
			res.bounds = a.bounds;
			return res;
		}

		inline Expr sdfIntersect(const Expr& a, const Expr& b)
		{
			Expr res = max(a, b);
			if (a.bounds && b.bounds)
			{
				Bounds2DExpr inter = boundsIntersect(*a.bounds, *b.bounds);
				if (!inter.isUnbounded)
				{
					res.bounds = std::make_shared<Bounds2DExpr>(std::move(inter));
				}
			}
			else if (a.bounds)
			{
				res.bounds = a.bounds;
			}
			else if (b.bounds)
			{
				res.bounds = b.bounds;
			}
			return res;
		}

		inline Expr sdfOnion(const Expr& d, const Expr& thickness)
		{
			Expr res = abs(d) - thickness;
			if (d.bounds)
			{
				Bounds2DExpr expB = boundsExpand(*d.bounds, max(thickness, 0.0f) * d.bounds->metricScale);
				if (!expB.isUnbounded)
				{
					res.bounds = std::make_shared<Bounds2DExpr>(std::move(expB));
				}
			}
			return res;
		}

		// Hollow outline when thickness > 0; solid shape when thickness <= 0.
		inline Expr sdfOutline(const Expr& d, const Expr& thickness)
		{
			Expr res = ternary(step(1e-7f, thickness), abs(d) - thickness, d);
			if (d.bounds)
			{
				Bounds2DExpr expB = boundsExpand(*d.bounds, max(thickness, 0.0f) * d.bounds->metricScale);
				if (!expB.isUnbounded)
				{
					res.bounds = std::make_shared<Bounds2DExpr>(std::move(expB));
				}
			}
			return res;
		}

		inline Expr sdfRound(const Expr& d, const Expr& radius)
		{
			Expr res = d - radius;
			if (d.bounds)
			{
				Bounds2DExpr expB = boundsExpand(*d.bounds, max(radius, 0.0f) * d.bounds->metricScale);
				if (!expB.isUnbounded)
				{
					res.bounds = std::make_shared<Bounds2DExpr>(std::move(expB));
				}
			}
			return res;
		}

		inline Expr sdfErode(const Expr& d, const Expr& radius)
		{
			Expr res = d + radius;
			res.bounds = d.bounds;
			return res;
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
			Expr res(std::make_shared<SDFSmoothAddition>(a.node, b.node, radius.node));
			if (a.bounds && b.bounds)
			{
				Bounds2DExpr u = boundsUnion(*a.bounds, *b.bounds);
				Bounds2DExpr expU = boundsExpand(u, max(radius, 0.0f) * u.metricScale);
				if (!expU.isUnbounded)
				{
					res.bounds = std::make_shared<Bounds2DExpr>(std::move(expU));
				}
			}
			return res;
		}

		inline Expr sdfSmoothSubtract(const Expr& a, const Expr& b, const Expr& radius)
		{
			Expr res(std::make_shared<SDFSmoothSubtraction>(a.node, b.node, radius.node));
			res.bounds = a.bounds;
			return res;
		}

		// --- 2D Primitives ---

		inline Expr sdCircle(const Vec2Expr& p, const Expr& radius)
		{
			Expr res = length(p) - radius;
			res.bounds = bounds_detail::transformLocalBounds(p.frame, boundsCircle({0.0f, 0.0f}, radius));
			return res;
		}

		inline Expr sdBox(const Vec2Expr& p, const Vec2Expr& halfSize)
		{
			Expr dx = abs(p.x) - halfSize.x;
			Expr dy = abs(p.y) - halfSize.y;
			Expr res = length({max(dx, 0.0f), max(dy, 0.0f)}) + min(max(dx, dy), 0.0f);
			res.bounds = bounds_detail::transformLocalBounds(p.frame, boundsBox({0.0f, 0.0f}, halfSize));
			return res;
		}

		inline Expr sdRoundBox(const Vec2Expr& p, const Vec2Expr& halfSize, const Expr& radius)
		{
			Expr r = max(radius, 0.0f);
			Vec2Expr b = {max(halfSize.x - r, 0.0f), max(halfSize.y - r, 0.0f)};
			Expr res = sdBox(p, b) - r;
			Vec2Expr effHalf(max(abs(halfSize.x), r), max(abs(halfSize.y), r));
			res.bounds = bounds_detail::transformLocalBounds(p.frame, boundsBox({0.0f, 0.0f}, effHalf));
			return res;
		}

		inline Expr sdRoundedBox(const Vec2Expr& p, const Vec2Expr& halfSize, const Expr& radius)
		{
			return sdRoundBox(p, halfSize, radius);
		}

		inline Expr sdSegment(const Vec2Expr& p, const Vec2Expr& a, const Vec2Expr& b)
		{
			Vec2Expr pa = p - a;
			Vec2Expr ba = b - a;
			Expr h = clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
			Expr res = length(pa - ba * h);
			res.bounds = bounds_detail::transformLocalBounds(p.frame, boundsSegment(a, b, 0.0f));
			return res;
		}

		inline Expr sdLine(const Vec2Expr& p, const Vec2Expr& a, const Vec2Expr& b, const Expr& width)
		{
			Expr res = sdSegment(p, a, b) - width;
			res.bounds = bounds_detail::transformLocalBounds(p.frame, boundsSegment(a, b, width));
			return res;
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

			Expr res = -sqrt(dx) * sign(dy);
			Expr halfW = abs(w) * 0.5f;
			Expr yA = -h / 3.0f;
			Expr yB = (2.0f * h) / 3.0f;
			res.bounds =
				bounds_detail::transformLocalBounds(p.frame, boundsMinMax({-halfW, min(yA, yB)}, {halfW, max(yA, yB)}));
			return res;
		}

		inline Expr sdRoundTriangle(const Vec2Expr& p, const Expr& w, const Expr& h, const Expr& radius)
		{
			Expr aw = max(abs(w), 1e-4f);
			Expr ah = max(abs(h), 1e-4f);
			Expr sy = step(0.0f, h) * 2.0f - 1.0f;

			Expr halfW = aw * 0.5f;
			Expr leg = length({halfW, ah});
			Expr inradius = (halfW * ah) / (halfW + leg);

			Expr r = min(max(radius, 0.0f), inradius * 0.998f);
			Expr scale = 1.0f - r / inradius;
			Expr shiftY = (1.0f - scale) * (inradius - ah / 3.0f);

			Vec2Expr q = {p.x, p.y * sy - shiftY};
			Expr res = sdTriangle(q, aw * scale, ah * scale) - r;
			Expr yA = -h / 3.0f;
			Expr yB = (2.0f * h) / 3.0f;
			res.bounds =
				bounds_detail::transformLocalBounds(p.frame, boundsMinMax({-halfW, min(yA, yB)}, {halfW, max(yA, yB)}));
			return res;
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

			Expr res = sqrt(dx) * sign(-dy);
			res.bounds =
				bounds_detail::transformLocalBounds(pIn.frame, boundsBox({0.0f, 0.0f}, {abs(wi), abs(he) + abs(sk)}));
			return res;
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
			glm::vec2 minV = vertices[0];
			glm::vec2 maxV = vertices[0];

			for (size_t i = 0, j = N - 1; i < N; j = i, i++)
			{
				minV = glm::min(minV, vertices[i]);
				maxV = glm::max(maxV, vertices[i]);

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

			Expr res = s * sqrt(d);
			res.bounds = bounds_detail::transformLocalBounds(p.frame, boundsMinMax(minV, maxV));
			return res;
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
			Expr res = dist + displacement * sin(angle);
			res.bounds = bounds_detail::transformLocalBounds(
				p.frame, boundsCircle({0.0f, 0.0f}, max(radius, 0.0f) + abs(displacement)));
			return res;
		}

		inline Expr sdSineWave(const Vec2Expr& p, const Expr& amplitude, const Expr& frequency, const Expr& speed,
							   const Expr& offset = 0.0f)
		{
			Expr res = (p.y - offset) - amplitude * sin(frequency * p.x + speed * time());
			res.bounds = bounds_detail::transformLocalBounds(p.frame, boundsHalfPlaneBelow(offset + abs(amplitude)));
			return res;
		}

		// --- 3D Primitives ---

		inline Expr sdPlane(const Vec3Expr& p, const Vec3Expr& n, const Expr& distanceFromOrigin)
		{
			return dot(p, n) + distanceFromOrigin;
		}

		inline Expr sdPlane(const Vec3Expr& p, const Expr& height)
		{
			return sdPlane(p, Vec3Expr(0.0f, 1.0f, 0.0f), height);
		}

		inline Expr sdSphere(const Vec3Expr& p, const Expr& radius)
		{
			return length(p) - radius;
		}

		inline Expr sdBox(const Vec3Expr& p, const Vec3Expr& halfSize)
		{
			Vec3Expr d = abs(p) - halfSize;
			return length(max(d, 0.0f)) + min(vmax(d), 0.0f);
		}

		inline Expr sdCylinder(const Vec3Expr& p, const Expr& radius, const Expr& height)
		{
			Expr d = length(p.xz()) - radius;
			return max(d, abs(p.y) - height);
		}

		inline Expr sdTorus(const Vec3Expr& p, const Expr& majorRadius, const Expr& minorRadius)
		{
			Vec2Expr q = {length(p.xz()) - majorRadius, p.y};
			return length(q) - minorRadius;
		}

		inline Expr sdCapsule(const Vec3Expr& p, const Expr& radius, const Expr& height)
		{
			Vec3Expr q = {p.x, p.y - clamp(p.y, -height, height), p.z};
			return length(q) - radius;
		}

		inline Expr sdCapsule(const Vec3Expr& p, const Vec3Expr& a, const Vec3Expr& b, const Expr& radius)
		{
			Vec3Expr pa = p - a;
			Vec3Expr ba = b - a;
			Expr h = clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
			return length(pa - ba * h) - radius;
		}

	} // namespace SDF
} // namespace WeirdEngine
