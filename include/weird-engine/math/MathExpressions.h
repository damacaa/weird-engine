#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
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
		virtual void collectHelperFunctions(std::unordered_set<std::string>& helpers) const {}
		[[nodiscard]]
		virtual std::string getHelperFunctions() const
		{
			std::unordered_set<std::string> helpers;
			collectHelperFunctions(helpers);
			std::string result;
			for (const auto& h : helpers)
			{
				result += h + "\n";
			}
			return result;
		}
		virtual ~IMathExpression() = default;
	};

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

		void collectHelperFunctions(std::unordered_set<std::string>& helpers) const override
		{
			if (valueA)
				valueA->collectHelperFunctions(helpers);
		}
		[[nodiscard]]
		std::string print() const override
		{
			return printWithChildren({valueA ? valueA->print() : ""});
		}
	};

	// Sine
	struct Sine : OneFloatOperation
	{
		using OneFloatOperation::OneFloatOperation;

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

		// TwoFloatOperation(std::ptrdiff_t i, std::ptrdiff_t j)
		//	: valueA(std::make_shared<FloatVariable>(i)), valueB(std::make_shared<FloatVariable>(j)) {}

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

		void collectHelperFunctions(std::unordered_set<std::string>& helpers) const override
		{
			if (valueA)
				valueA->collectHelperFunctions(helpers);
			if (valueB)
				valueB->collectHelperFunctions(helpers);
		}

		[[nodiscard]]
		std::string print() const override
		{
			return printWithChildren({valueA ? valueA->print() : "", valueB ? valueB->print() : ""});
		}
	};

	// Add
	struct Addition : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

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

	// Deprecated alias of Subtraction
	using Substraction = Subtraction;

	// Multiplication
	struct Multiplication : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

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

		[[nodiscard]]
		float getValue(const float* parameters) const override
		{
			float a = valueA->getValue(parameters);
			float b = valueB->getValue(parameters);

			// Note: ensure glm is included in the project configuration
			return length(glm::vec2(a, b));
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

	struct SDFAddition : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

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

	struct SDFSubtraction : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

		[[nodiscard]]
		float getValue(const float* parameters) const override
		{
			float a = valueA->getValue(parameters);
			float b = valueB->getValue(parameters);

			return std::max(a, -b);
		}

		[[nodiscard]]
		std::string printWithChildren(const std::vector<std::string>& c) const override
		{
			return "max(" + c[0] + ", -(" + c[1] + "))";
		}
	};

	struct SDFIntersection : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

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

	struct SDFOnion : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

		[[nodiscard]]
		float getValue(const float* parameters) const override
		{
			float a = valueA->getValue(parameters);
			float b = valueB->getValue(parameters);

			return abs(a) - b;
		}

		[[nodiscard]]
		std::string printWithChildren(const std::vector<std::string>& c) const override
		{
			return "(abs(" + c[0] + ") - " + c[1] + ")";
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

		void collectHelperFunctions(std::unordered_set<std::string>& helpers) const override
		{
			if (valueA)
				valueA->collectHelperFunctions(helpers);
			if (valueB)
				valueB->collectHelperFunctions(helpers);
			if (valueC)
				valueC->collectHelperFunctions(helpers);
		}

		[[nodiscard]]
		std::string print() const override
		{
			return printWithChildren(
				{valueA ? valueA->print() : "", valueB ? valueB->print() : "", valueC ? valueC->print() : ""});
		}
	};

	// Clamp
	struct Clamp : ThreeFloatOperation
	{
		using ThreeFloatOperation::ThreeFloatOperation;

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

	// TODO: reuse the same ones from physics engine
	static float fOpUnionSoft(float a, float b, float r)
	{
		r *= 1.0f; // 4.0f orignal wtf
		float h = std::max(r - abs(a - b), 0.0f);
		return std::min(a, b) - h * h * 0.25f / r;
	}

	// Smooth subtraction (a - b), 2D SDF
	static float fOpSubSoft(float a, float b, float r)
	{
		return -fOpUnionSoft(b, -a, r);
	}

	struct SDFSmoothAddition : ThreeFloatOperation
	{
		using ThreeFloatOperation::ThreeFloatOperation;

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

#pragma endregion TwoFloatOperations
} // namespace WeirdEngine