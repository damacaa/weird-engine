#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace WeirdEngine
{
	// Base
	struct IMathExpression
	{
		virtual void propagateValues(float* values) = 0;
		[[nodiscard]]
		virtual float getValue() const = 0;
		[[nodiscard]]
		virtual std::string print() const = 0;
		virtual ~IMathExpression() = default;
	};

#pragma region Variables
	// Variables
	struct FloatVariable : IMathExpression
	{
	private:
		std::ptrdiff_t m_offset;
		float* m_value = nullptr;

	public:
		explicit FloatVariable(std::ptrdiff_t offset)
			: m_offset(offset)
		{
		}

		void propagateValues(float* values) override
		{
			m_value = values + m_offset;
		}

		[[nodiscard]]
		float getValue() const override
		{
			return *m_value;
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

		void propagateValues(float* values) override {}

		[[nodiscard]]
		float getValue() const override
		{
			return m_value;
		}

		[[nodiscard]]
		std::string print() const override
		{
			return std::to_string(m_value) + "f";
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

		void propagateValues(float* values) override
		{
			valueA->propagateValues(values);
		}

		[[nodiscard]]
		float getValue() const override = 0;

		[[nodiscard]]
		std::string print() const override = 0;
	};

	// Sine
	struct Sine : OneFloatOperation
	{
		using OneFloatOperation::OneFloatOperation;

		[[nodiscard]]
		float getValue() const override
		{
			return sinf(valueA->getValue());
		}

		[[nodiscard]]
		std::string print() const override
		{
			return "sin(" + valueA->print() + ")";
		}
	};

	// Abs
	struct Abs : OneFloatOperation
	{
		using OneFloatOperation::OneFloatOperation;

		[[nodiscard]]
		float getValue() const override
		{
			return std::abs(valueA->getValue());
		}

		[[nodiscard]]
		std::string print() const override
		{
			return "abs(" + valueA->print() + ")";
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

		void propagateValues(float* values) override
		{
			valueA->propagateValues(values);
			valueB->propagateValues(values);
		}

		void setValues(std::shared_ptr<IMathExpression> a, std::shared_ptr<IMathExpression> b)
		{
			valueA = (std::move(a));
			valueB = (std::move(b));
		}

		[[nodiscard]]
		virtual float getValue() const override = 0;

		[[nodiscard]]
		virtual std::string print() const = 0;
	};

	// Add
	struct Addition : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

		[[nodiscard]]
		float getValue() const override
		{
			return valueA->getValue() + valueB->getValue();
		}

		[[nodiscard]]
		std::string print() const override
		{
			return "(" + valueA->print() + " + " + valueB->print() + ")";
		}
	};

	// Substract
	struct Substraction : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

		[[nodiscard]]
		float getValue() const override
		{
			return valueA->getValue() - valueB->getValue();
		}

		[[nodiscard]]
		std::string print() const override
		{
			return "(" + valueA->print() + " - " + valueB->print() + ")";
		}
	};

	// Multiplication
	struct Multiplication : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

		[[nodiscard]]
		float getValue() const override
		{
			return valueA->getValue() * valueB->getValue();
		}

		[[nodiscard]]
		std::string print() const override
		{
			return "(" + valueA->print() + " * " + valueB->print() + ")";
		}
	};

	// Atan2
	struct Atan2 : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

		[[nodiscard]]
		float getValue() const override
		{
			return atan2f(valueA->getValue(), valueB->getValue());
		}

		[[nodiscard]]
		std::string print() const override
		{
			return "atan(" + valueA->print() + ", " + valueB->print() + ")";
		}
	};

	struct Length : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

		[[nodiscard]]
		float getValue() const override
		{
			float a = valueA->getValue();
			float b = valueB->getValue();

			// Note: ensure glm is included in the project configuration
			return length(glm::vec2(a, b));
		}

		[[nodiscard]]
		std::string print() const override
		{
			return "length(vec2(" + valueA->print() + ", " + valueB->print() + "))";
		}
	};

	struct Max : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

		[[nodiscard]]
		float getValue() const override
		{
			float a = valueA->getValue();
			float b = valueB->getValue();

			return std::max(a, b);
		}

		[[nodiscard]]
		std::string print() const override
		{
			return "max(" + valueA->print() + ", " + valueB->print() + ")";
		}
	};

	struct Min : TwoFloatOperation
	{
		using TwoFloatOperation::TwoFloatOperation;

		[[nodiscard]]
		float getValue() const override
		{
			float a = valueA->getValue();
			float b = valueB->getValue();

			return std::min(a, b);
		}

		[[nodiscard]]
		std::string print() const override
		{
			return "min(" + valueA->print() + ", " + valueB->print() + ")";
		}
	};

#pragma endregion TwoFloatOperations
} // namespace WeirdEngine