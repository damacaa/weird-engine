#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace WeirdEngine
{
	// Base
	struct IMathExpression
	{
		[[nodiscard]]
		virtual float getValue(const float* parameters) const = 0;
		[[nodiscard]]
		virtual std::string print() const = 0;
		[[nodiscard]]
		virtual bool isTrivial() const
		{
			return false;
		}
		virtual void getChildren(std::vector<std::shared_ptr<IMathExpression>>& out) const {}
		[[nodiscard]]
		virtual std::string printWithChildren(const std::vector<std::string>& childCode) const
		{
			return print();
		}
		[[nodiscard]]
		virtual std::shared_ptr<IMathExpression> clone(
			const std::vector<std::shared_ptr<IMathExpression>>& /*newChildren*/) const
		{
			return nullptr;
		}
		virtual ~IMathExpression() = default;
	};

	namespace detail
	{
#pragma region Variables
		// Variables
		struct FloatVariable : IMathExpression
		{
		private:
			std::ptrdiff_t m_offset;

		public:
			explicit FloatVariable(std::ptrdiff_t offset)
				: m_offset(offset)
			{
			}

			[[nodiscard]]
			std::ptrdiff_t getOffset() const
			{
				return m_offset;
			}

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return parameters[m_offset];
			}

			[[nodiscard]]
			bool isTrivial() const override
			{
				return true;
			}

			[[nodiscard]]
			std::string print() const override
			{
				return "var" + std::to_string(m_offset);
			}

			[[nodiscard]]
			std::shared_ptr<IMathExpression> clone(
				const std::vector<std::shared_ptr<IMathExpression>>& /*newChildren*/) const override
			{
				return std::make_shared<FloatVariable>(m_offset);
			}
		};

		struct FloatConstant : IMathExpression
		{
		private:
			float m_value;

		public:
			explicit FloatConstant(float value)
				: m_value(value)
			{
			}

			[[nodiscard]]
			float getConstantValue() const
			{
				return m_value;
			}

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return m_value;
			}

			[[nodiscard]]
			bool isTrivial() const override
			{
				return true;
			}

			[[nodiscard]]
			std::string print() const override
			{
				std::ostringstream ss;
				ss << std::fixed << std::setprecision(6) << m_value;
				return ss.str();
			}

			[[nodiscard]]
			std::shared_ptr<IMathExpression> clone(
				const std::vector<std::shared_ptr<IMathExpression>>& /*newChildren*/) const override
			{
				return std::make_shared<FloatConstant>(m_value);
			}
		};

#pragma endregion Variables

#pragma region OneFloatOperations
		// One float operation
		struct OneFloatOperation : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> valueA;

		public:
			OneFloatOperation()
				: valueA()
			{
			}

			OneFloatOperation(std::shared_ptr<IMathExpression> a)
				: valueA(std::move(a))
			{
			}

			OneFloatOperation(std::ptrdiff_t i)
				: valueA(std::make_shared<FloatVariable>(i))
			{
			}

			OneFloatOperation(float constant)
				: valueA(std::make_shared<FloatConstant>(constant))
			{
			}

			void setValue(std::shared_ptr<IMathExpression> a)
			{
				valueA = (std::move(a));
			}

			void getChildren(std::vector<std::shared_ptr<IMathExpression>>& out) const override
			{
				if (valueA)
					out.push_back(valueA);
			}

			[[nodiscard]]
			float getValue(const float* parameters) const override = 0;

			[[nodiscard]]
			std::string print() const override
			{
				return printWithChildren({valueA ? valueA->print() : ""});
			}
		};

#define WEIRD_MATH_CLONE_ONE(TypeName)                                                                                 \
	[[nodiscard]]                                                                                                      \
	std::shared_ptr<IMathExpression> clone(const std::vector<std::shared_ptr<IMathExpression>>& c) const override      \
	{                                                                                                                  \
		return std::make_shared<TypeName>(c.empty() ? nullptr : c[0]);                                                 \
	}

		// Sine
		struct Sine : OneFloatOperation
		{
			using OneFloatOperation::OneFloatOperation;

			WEIRD_MATH_CLONE_ONE(Sine)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return sinf(valueA->getValue(parameters));
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "sin(" + c[0] + ")";
			}
		};

		// Abs
		struct Abs : OneFloatOperation
		{
			using OneFloatOperation::OneFloatOperation;

			WEIRD_MATH_CLONE_ONE(Abs)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return std::abs(valueA->getValue(parameters));
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "abs(" + c[0] + ")";
			}
		};

		// Cosine
		struct Cosine : OneFloatOperation
		{
			using OneFloatOperation::OneFloatOperation;

			WEIRD_MATH_CLONE_ONE(Cosine)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return cosf(valueA->getValue(parameters));
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "cos(" + c[0] + ")";
			}
		};

		// Negation
		struct Negation : OneFloatOperation
		{
			using OneFloatOperation::OneFloatOperation;

			WEIRD_MATH_CLONE_ONE(Negation)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return -valueA->getValue(parameters);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "-(" + c[0] + ")";
			}
		};

		// Sqrt
		struct Sqrt : OneFloatOperation
		{
			using OneFloatOperation::OneFloatOperation;

			WEIRD_MATH_CLONE_ONE(Sqrt)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return std::sqrt(valueA->getValue(parameters));
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "sqrt(" + c[0] + ")";
			}
		};

		// Sign
		struct Sign : OneFloatOperation
		{
			using OneFloatOperation::OneFloatOperation;

			WEIRD_MATH_CLONE_ONE(Sign)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				float v = valueA->getValue(parameters);
				return (v > 0.0f) ? 1.0f : ((v < 0.0f) ? -1.0f : 0.0f);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "sign(" + c[0] + ")";
			}
		};

#pragma endregion OneFloatOperations

#pragma region TwoFloatOperations
		// Two float operation
		struct TwoFloatOperation : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> valueA;
			std::shared_ptr<IMathExpression> valueB;

		public:
			TwoFloatOperation()
				: valueA()
				, valueB()
			{
			}

			TwoFloatOperation(std::shared_ptr<IMathExpression> a, std::shared_ptr<IMathExpression> b)
				: valueA(std::move(a))
				, valueB(std::move(b))
			{
			}

			TwoFloatOperation(float constant, std::shared_ptr<IMathExpression> b)
				: valueA(std::make_shared<FloatConstant>(constant))
				, valueB(std::move(b))
			{
			}

			TwoFloatOperation(std::shared_ptr<IMathExpression> a, float constant)
				: valueA(std::move(a))
				, valueB(std::make_shared<FloatConstant>(constant))
			{
			}

			void setValues(std::shared_ptr<IMathExpression> a, std::shared_ptr<IMathExpression> b)
			{
				valueA = (std::move(a));
				valueB = (std::move(b));
			}

			void getChildren(std::vector<std::shared_ptr<IMathExpression>>& out) const override
			{
				if (valueA)
					out.push_back(valueA);
				if (valueB)
					out.push_back(valueB);
			}

			[[nodiscard]]
			virtual float getValue(const float* parameters) const override = 0;

			[[nodiscard]]
			std::string print() const override
			{
				return printWithChildren({valueA ? valueA->print() : "", valueB ? valueB->print() : ""});
			}
		};

#define WEIRD_MATH_CLONE_TWO(TypeName)                                                                                 \
	[[nodiscard]]                                                                                                      \
	std::shared_ptr<IMathExpression> clone(const std::vector<std::shared_ptr<IMathExpression>>& c) const override      \
	{                                                                                                                  \
		return std::make_shared<TypeName>(c.size() > 0 ? c[0] : nullptr, c.size() > 1 ? c[1] : nullptr);               \
	}

		// Add
		struct Addition : TwoFloatOperation
		{
			using TwoFloatOperation::TwoFloatOperation;

			WEIRD_MATH_CLONE_TWO(Addition)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return valueA->getValue(parameters) + valueB->getValue(parameters);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "(" + c[0] + " + " + c[1] + ")";
			}
		};

		// Subtraction
		struct Subtraction : TwoFloatOperation
		{
			using TwoFloatOperation::TwoFloatOperation;

			WEIRD_MATH_CLONE_TWO(Subtraction)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return valueA->getValue(parameters) - valueB->getValue(parameters);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "(" + c[0] + " - " + c[1] + ")";
			}
		};

		// Multiplication
		struct Multiplication : TwoFloatOperation
		{
			using TwoFloatOperation::TwoFloatOperation;

			WEIRD_MATH_CLONE_TWO(Multiplication)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return valueA->getValue(parameters) * valueB->getValue(parameters);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "(" + c[0] + " * " + c[1] + ")";
			}
		};

		struct Division : TwoFloatOperation
		{
			using TwoFloatOperation::TwoFloatOperation;

			WEIRD_MATH_CLONE_TWO(Division)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				float denominator = valueB->getValue(parameters);
				if (denominator == 0.0f)
					return 0.0f;
				return valueA->getValue(parameters) / denominator;
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "(" + c[0] + " / " + c[1] + ")";
			}
		};

		struct Mod : TwoFloatOperation
		{
			using TwoFloatOperation::TwoFloatOperation;

			WEIRD_MATH_CLONE_TWO(Mod)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				float b = valueB->getValue(parameters);
				if (b == 0.0f)
					return 0.0f;
				float a = valueA->getValue(parameters);
				return a - b * std::floor(a / b);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "mod(" + c[0] + ", " + c[1] + ")";
			}
		};

		// Atan2
		struct Atan2 : TwoFloatOperation
		{
			using TwoFloatOperation::TwoFloatOperation;

			WEIRD_MATH_CLONE_TWO(Atan2)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				return atan2f(valueA->getValue(parameters), valueB->getValue(parameters));
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "atan(" + c[0] + ", " + c[1] + ")";
			}
		};

		struct Length : TwoFloatOperation
		{
			using TwoFloatOperation::TwoFloatOperation;

			WEIRD_MATH_CLONE_TWO(Length)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				float a = valueA->getValue(parameters);
				float b = valueB->getValue(parameters);

				return std::hypot(a, b);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "length(vec2(" + c[0] + ", " + c[1] + "))";
			}
		};

		struct Max : TwoFloatOperation
		{
			using TwoFloatOperation::TwoFloatOperation;

			WEIRD_MATH_CLONE_TWO(Max)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				float a = valueA->getValue(parameters);
				float b = valueB->getValue(parameters);

				return std::max(a, b);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "max(" + c[0] + ", " + c[1] + ")";
			}
		};

		struct Min : TwoFloatOperation
		{
			using TwoFloatOperation::TwoFloatOperation;

			WEIRD_MATH_CLONE_TWO(Min)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				float a = valueA->getValue(parameters);
				float b = valueB->getValue(parameters);

				return std::min(a, b);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "min(" + c[0] + ", " + c[1] + ")";
			}
		};

		// Step: step(edge, x) -> x >= edge ? 1.0 : 0.0
		struct Step : TwoFloatOperation
		{
			using TwoFloatOperation::TwoFloatOperation;

			WEIRD_MATH_CLONE_TWO(Step)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				float edge = valueA->getValue(parameters);
				float x = valueB->getValue(parameters);
				return (x >= edge) ? 1.0f : 0.0f;
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "step(" + c[0] + ", " + c[1] + ")";
			}
		};

		// Three float operation
		struct ThreeFloatOperation : IMathExpression
		{
		protected:
			std::shared_ptr<IMathExpression> valueA;
			std::shared_ptr<IMathExpression> valueB;
			std::shared_ptr<IMathExpression> valueC;

		public:
			ThreeFloatOperation()
				: valueA()
				, valueB()
				, valueC()
			{
			}

			ThreeFloatOperation(std::shared_ptr<IMathExpression> a, std::shared_ptr<IMathExpression> b,
								std::shared_ptr<IMathExpression> c)
				: valueA(std::move(a))
				, valueB(std::move(b))
				, valueC(std::move(c))
			{
			}

			ThreeFloatOperation(std::shared_ptr<IMathExpression> a, std::shared_ptr<IMathExpression> b, float constant)
				: valueA(std::move(a))
				, valueB(std::move(b))
				, valueC(std::make_shared<FloatConstant>(constant))
			{
			}

			void setValues(std::shared_ptr<IMathExpression> a, std::shared_ptr<IMathExpression> b,
						   std::shared_ptr<IMathExpression> c)
			{
				valueA = (std::move(a));
				valueB = (std::move(b));
				valueC = (std::move(c));
			}

			void getChildren(std::vector<std::shared_ptr<IMathExpression>>& out) const override
			{
				if (valueA)
					out.push_back(valueA);
				if (valueB)
					out.push_back(valueB);
				if (valueC)
					out.push_back(valueC);
			}

			[[nodiscard]]
			virtual float getValue(const float* parameters) const override = 0;

			[[nodiscard]]
			std::string print() const override
			{
				return printWithChildren(
					{valueA ? valueA->print() : "", valueB ? valueB->print() : "", valueC ? valueC->print() : ""});
			}
		};

#define WEIRD_MATH_CLONE_THREE(TypeName)                                                                               \
	[[nodiscard]]                                                                                                      \
	std::shared_ptr<IMathExpression> clone(const std::vector<std::shared_ptr<IMathExpression>>& c) const override      \
	{                                                                                                                  \
		return std::make_shared<TypeName>(c.size() > 0 ? c[0] : nullptr, c.size() > 1 ? c[1] : nullptr,                \
										  c.size() > 2 ? c[2] : nullptr);                                              \
	}

		// Clamp
		struct Clamp : ThreeFloatOperation
		{
			using ThreeFloatOperation::ThreeFloatOperation;

			WEIRD_MATH_CLONE_THREE(Clamp)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				float a = valueA->getValue(parameters);
				float lo = valueB->getValue(parameters);
				float hi = valueC->getValue(parameters);

				return std::clamp(a, lo, hi);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "clamp(" + c[0] + ", " + c[1] + ", " + c[2] + ")";
			}
		};

		inline constexpr float fOpUnionSoft(float a, float b, float r)
		{
			float h = std::max(r - std::abs(a - b), 0.0f);
			return std::min(a, b) - h * h * 0.25f / r;
		}

		inline constexpr float fOpSubSoft(float a, float b, float r)
		{
			return -fOpUnionSoft(b, -a, r);
		}

		struct SDFSmoothAddition : ThreeFloatOperation
		{
			using ThreeFloatOperation::ThreeFloatOperation;

			WEIRD_MATH_CLONE_THREE(SDFSmoothAddition)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				float a = valueA->getValue(parameters);
				float b = valueB->getValue(parameters);
				float r = valueC->getValue(parameters);

				return fOpUnionSoft(a, b, r);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "fOpUnionSoft(" + c[0] + ", " + c[1] + ", " + c[2] + ")";
			}
		};

		struct SDFSmoothSubtraction : ThreeFloatOperation
		{
			using ThreeFloatOperation::ThreeFloatOperation;

			WEIRD_MATH_CLONE_THREE(SDFSmoothSubtraction)

			[[nodiscard]]
			float getValue(const float* parameters) const override
			{
				float a = valueA->getValue(parameters);
				float b = valueB->getValue(parameters);
				float r = valueC->getValue(parameters);

				return fOpSubSoft(a, b, r);
			}

			[[nodiscard]]
			std::string printWithChildren(const std::vector<std::string>& c) const override
			{
				return "fOpSubSoft(" + c[0] + ", " + c[1] + ", " + c[2] + ")";
			}
		};
	} // namespace detail

	using detail::FloatConstant;
	using detail::FloatVariable;
	using detail::fOpSubSoft;
	using detail::fOpUnionSoft;

	using Scale = detail::Multiplication;
	using SDFScale = Scale;

	// Compatibility aliases mapping legacy SDF operations to standard Min/Max
	using SDFAddition = detail::Min;
	using SDFIntersection = detail::Max;

	inline std::shared_ptr<IMathExpression> transformAST(
		const std::shared_ptr<IMathExpression>& node,
		const std::function<std::shared_ptr<IMathExpression>(const std::shared_ptr<IMathExpression>&)>& transformer)
	{
		if (!node)
			return nullptr;

		auto transformed = transformer(node);
		if (transformed != node)
			return transformed;

		std::vector<std::shared_ptr<IMathExpression>> children;
		node->getChildren(children);
		if (children.empty())
			return node;

		std::vector<std::shared_ptr<IMathExpression>> newChildren;
		newChildren.reserve(children.size());
		for (const auto& child : children)
		{
			newChildren.push_back(transformAST(child, transformer));
		}
		auto cloned = node->clone(newChildren);
		return cloned ? cloned : node;
	}
} // namespace WeirdEngine
