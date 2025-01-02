#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// Base
struct IMathExpression
{
	virtual void propagateValues(float* values) = 0;
	virtual float getValue() const = 0;
	virtual std::string print() = 0;
	virtual ~IMathExpression() = default;
};

// Variables
struct FloatVariable : IMathExpression
{
private:
	std::ptrdiff_t m_offset;
	float* m_value = nullptr;

public:
	explicit FloatVariable(std::ptrdiff_t offset) : m_offset(offset) {}

	void propagateValues(float* values) override
	{
		m_value = values + m_offset;
	}

	float getValue() const override
	{
		return *m_value;
	}

	std::string print()
	{
		return "var" + std::to_string(m_offset);
	}
};

struct StaticVariable : IMathExpression
{
private:
	float m_value;

public:
	explicit StaticVariable(float value) : m_value(value) {}

	void propagateValues(float* values) override
	{

	}

	float getValue() const override
	{
		return m_value;
	}

	std::string print()
	{
		return std::to_string(m_value) + "f";
	}
};

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
		: valueA(std::move(a)) {}

	OneFloatOperation(std::ptrdiff_t i)
		: valueA(std::make_shared<FloatVariable>(i)) {}

	OneFloatOperation(float constant)
		: valueA(std::make_shared<StaticVariable>(constant)) {}

	void setValue(std::shared_ptr<IMathExpression> a)
	{
		valueA = (std::move(a));
	}

	void propagateValues(float* values) override
	{
		valueA->propagateValues(values);
	}

	virtual float getValue() const override = 0;

	virtual std::string print() = 0;
};

// Sine
struct Sine : OneFloatOperation
{
	using OneFloatOperation::OneFloatOperation;

	float getValue() const override
	{
		return sinf(valueA->getValue());
	}

	std::string print()
	{
		return "sin(" + valueA->print() + ")";
	}
};

// Abs
struct Abs : OneFloatOperation
{
	using OneFloatOperation::OneFloatOperation;

	float getValue() const override
	{
		return abs(valueA->getValue());
	}

	std::string print()
	{
		return "abs(" + valueA->print() + ")";
	}
};


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
		: valueA(std::move(a)), valueB(std::move(b)) {}

	TwoFloatOperation(float constant, std::shared_ptr<IMathExpression> b)
		: valueA(std::make_shared<StaticVariable>(constant)), valueB(std::move(b)) {}

	TwoFloatOperation(std::shared_ptr<IMathExpression> a, float constant)
		: valueA(std::move(a)), valueB(std::make_shared<StaticVariable>(constant)) {}

	//TwoFloatOperation(std::ptrdiff_t i, std::ptrdiff_t j)
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

	virtual float getValue() const override = 0;

	virtual std::string print() = 0;
};

// Add
struct Addition : TwoFloatOperation
{
	using TwoFloatOperation::TwoFloatOperation;

	float getValue() const override
	{
		return valueA->getValue() + valueB->getValue();
	}

	std::string print()
	{
		return "(" + valueA->print() + " + " + valueB->print() + ")";
	}
};

// Substract
struct Substraction : TwoFloatOperation
{
	using TwoFloatOperation::TwoFloatOperation;

	float getValue() const override
	{
		return valueA->getValue() - valueB->getValue();
	}

	std::string print()
	{
		return "(" + valueA->print() + " - " + valueB->print() + ")";
	}
};

// Multiplication
struct Multiplication : TwoFloatOperation
{
	using TwoFloatOperation::TwoFloatOperation;

	float getValue() const override
	{
		return valueA->getValue() * valueB->getValue();
	}

	std::string print()
	{
		return "(" + valueA->print() + " * " + valueB->print() + ")";
	}
};

// Atan2
struct Atan2 : TwoFloatOperation
{
	using TwoFloatOperation::TwoFloatOperation;

	float getValue() const override
	{
		return atan2f(valueA->getValue(), valueB->getValue());
	}

	std::string print()
	{
		return "atan(" + valueA->print() + ", " + valueB->print() + ")";
	}
};


struct Length : TwoFloatOperation
{
	using TwoFloatOperation::TwoFloatOperation;

	float getValue() const override
	{
		float a = valueA->getValue();
		float b = valueB->getValue();

		return length(glm::vec2(a, b));
	}

	std::string print()
	{
		return "length(vec2(" + valueA->print() + ", " + valueB->print() + "))";
	}
};

struct Max : TwoFloatOperation
{
	using TwoFloatOperation::TwoFloatOperation;

	float getValue() const override
	{
		float a = valueA->getValue();
		float b = valueB->getValue();

		return std::max(a, b);
	}

	std::string print()
	{
		return "max(" + valueA->print() + ", " + valueB->print() + ")";
	}
};

struct Min : TwoFloatOperation
{
	using TwoFloatOperation::TwoFloatOperation;

	float getValue() const override
	{
		float a = valueA->getValue();
		float b = valueB->getValue();

		return std::min(a, b);
	}

	std::string print()
	{
		return "min(" + valueA->print() + ", " + valueB->print() + ")";
	}
};





//void testMath()
//{
//	{
//		auto var0 = std::make_shared<FloatVariable>(1);
//		auto addition1 = std::make_shared<Addition>(std::make_shared<FloatVariable>(0), var0);
//		auto addition2 = std::make_shared<Addition>(addition1, std::make_shared<Addition>(std::make_shared<FloatVariable>(1), std::make_shared<FloatVariable>(1)));
//		auto addition3 = std::make_shared<Addition>(addition2, 0);
//
//		IMathExpression* y = addition3.get();
//
//		float* variables = new float[2] {1, 2};
//		float* variables2 = new float[2] {5, 6};
//
//		y->propagateValues(variables);
//		float v = y->getValue();
//		y->propagateValues(variables2);
//		float v2 = y->getValue();
//
//		delete[] variables;
//		delete[] variables2;
//	}
//
//	{
//		// Circle sdf
//		float* variables = new float[5] {0, 0, 0.5f, 1.0f, 0.0f};
//
//		auto xExpression = std::make_shared<Substraction>(0, 3);
//		auto yExpression = std::make_shared<Substraction>(1, 4);
//		auto lengthExpression = std::make_shared<Substraction>(std::make_shared<Length>(xExpression, yExpression), 2);
//
//		IMathExpression* lengthFormula = lengthExpression.get();
//		lengthFormula->propagateValues(variables);
//		float result = lengthFormula->getValue();
//
//		auto x = xExpression.get()->getValue();
//
//	}
//
//
//
//
//
//
//	/*vec2* vectors = new vec2[2]{ vec2(0,5),vec2(4,3) };
//	float f = ((float*)vectors)[1];
//	delete[] vectors;*/
//}