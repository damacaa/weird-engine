#pragma once
#include <cstddef>
#include <memory>

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

// Two float operation
struct TwoFloatOperation : IMathExpression
{
protected:
	std::shared_ptr<IMathExpression> valueA;
	std::shared_ptr<IMathExpression> valueB;

public:
	TwoFloatOperation(std::shared_ptr<IMathExpression> a, std::shared_ptr<IMathExpression> b)
		: valueA(std::move(a)), valueB(std::move(b)) {}

	TwoFloatOperation(std::ptrdiff_t i, std::shared_ptr<IMathExpression> b)
		: valueA(std::make_shared<FloatVariable>(i)), valueB(std::move(b)) {}

	TwoFloatOperation(std::shared_ptr<IMathExpression> a, std::ptrdiff_t i)
		: valueA(std::move(a)), valueB(std::make_shared<FloatVariable>(i)) {}

	TwoFloatOperation(std::ptrdiff_t i, std::ptrdiff_t j)
		: valueA(std::make_shared<FloatVariable>(i)), valueB(std::make_shared<FloatVariable>(j)) {}

	void propagateValues(float* values) override
	{
		valueA->propagateValues(values);
		valueB->propagateValues(values);
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


//// One float operation
//struct OneFloatOperation : IMathExpression
//{
//	FloatVariable value;
//
//	OneFloatOperation(FloatVariable v) : value(v) {}
//
//	void propagateValues(float* values)
//	{
//		value.propagateValues(values);
//	}
//
//	virtual float getValue() = 0;
//};

//// Two vector operation
//struct TwoVectorOperation : IMathExpression
//{
//	FloatVariable valueAx, valueAy, valueBx, valueBY;
//
//	TwoVectorOperation(FloatVariable aX, FloatVariable aY, FloatVariable bX, FloatVariable bY) :
//		valueAx(aX), valueAy(aY), valueBx(bX), valueBY(bY) {}
//
//	void propagateValues(float* values)
//	{
//		valueAx.propagateValues(values);
//		valueAy.propagateValues(values);
//		valueBx.propagateValues(values);
//		valueBY.propagateValues(values);
//	}
//
//	virtual float getValue() = 0;
//};



//// Subtract
//struct Substraction : TwoFloatOperation
//{
//	using TwoFloatOperation::TwoFloatOperation;
//
//	float getValue() override
//	{
//		return valueA.getValue() - valueB.getValue();
//	}
//};
//
//// Subtract
//struct Multiplication : TwoFloatOperation
//{
//	using TwoFloatOperation::TwoFloatOperation;
//
//	float getValue() override
//	{
//		return valueA.getValue() * valueB.getValue();
//	}
//};
//
//// Subtract
//struct Division : TwoFloatOperation
//{
//	using TwoFloatOperation::TwoFloatOperation;
//
//	float getValue() override
//	{
//		return valueA.getValue() / valueB.getValue();
//	}
//};





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