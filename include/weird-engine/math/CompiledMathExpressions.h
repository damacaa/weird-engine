#pragma once

#include "MathExpressions.h"

// Idea: compile shape into a  Bytecode VM
struct CompiledMathExpression
{
	virtual ~CompiledMathExpression() = default;

	enum class OpCode : uint8_t
	{
		PUSH_VAR,	// Push a variable from your float* array onto the stack
		PUSH_CONST, // Push a hardcoded number onto the stack
		SUBTRACT,	// Pop 2 values, subtract, push result
		LENGTH,		// Pop 2 values (x,y), calculate length, push result
		MAX			// Pop 2 values, push the maximum
	};

	struct Instruction
	{
		OpCode code;
		float operand; // Used for the variable index (like 9 for WORLD_X) or a constant
	};

	std::vector<Instruction> m_program;

	float getValue(const float* variables)
	{
		if (m_program.empty())
		{
			compile(m_program);
		}

		return evaluateVM(m_program, variables);
	}

	// Inside CircleNode:
	virtual void compile(std::vector<Instruction>& program) = 0;

	float evaluateVM(const std::vector<Instruction>& program, const float* variables)
	{
		float stack[32]; // A tiny, lightning-fast stack on the CPU
		int sp = 0;		 // Stack pointer

		for (const auto& inst : program)
		{
			switch (inst.code)
			{
				case OpCode::PUSH_VAR:
					stack[sp++] = variables[(int)inst.operand]; // Grab MouseX, Time, etc.
					break;
				case OpCode::PUSH_CONST:
					stack[sp++] = inst.operand;
					break;
				case OpCode::SUBTRACT:
					sp--;
					stack[sp - 1] = stack[sp - 1] - stack[sp];
					break;
				case OpCode::LENGTH:
					sp -= 2;
					stack[sp] = std::sqrt(stack[sp] * stack[sp] + stack[sp + 1] * stack[sp + 1]);
					sp++;
					break;
				case OpCode::MAX:
					sp--;
					stack[sp - 1] = std::max(stack[sp - 1], stack[sp]);
					break;
			}
		}
		return stack[0]; // The final SDF distance!
	}
};

struct CompiledCircle : CompiledMathExpression
{

	static constexpr uint8_t POS_X = 0;
	static constexpr uint8_t POS_Y = 1;
	static constexpr uint8_t RADIUS = 2;

	// Common
	static constexpr uint8_t TIME = 8;
	static constexpr uint8_t WORLD_X = 9;
	static constexpr uint8_t WORLD_Y = 10;

	// Inside CircleNode:
	void compile(std::vector<Instruction>& program) override
	{
		// Math: length(world - pos) - radius

		// 1. world.x - pos.x
		program.push_back({OpCode::PUSH_VAR, (float)WORLD_X});
		program.push_back({OpCode::PUSH_VAR, (float)POS_X});
		program.push_back({OpCode::SUBTRACT, 0.0f});

		// 2. world.y - pos.y
		program.push_back({OpCode::PUSH_VAR, (float)WORLD_Y});
		program.push_back({OpCode::PUSH_VAR, (float)POS_Y});
		program.push_back({OpCode::SUBTRACT, 0.0f});

		// 3. length(p)
		program.push_back({OpCode::LENGTH, 0.0f});

		// 4. subtract radius
		program.push_back({OpCode::PUSH_VAR, (float)RADIUS});
		program.push_back({OpCode::SUBTRACT, 0.0f});
	}
};
