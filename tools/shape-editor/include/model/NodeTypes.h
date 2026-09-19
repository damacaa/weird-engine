#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "weird-engine/math/MathExpressions.h"
#include "weird-engine/math/SDF.h"
#include <glm/glm.hpp>

namespace WeirdEngine::Editor
{
	enum class PinType
	{
		Float,
		Vec2
	};

	enum class NodeCategory
	{
		Input,
		Vector,
		Transforms,
		Primitives2D,
		Primitives3D,
		CSG,
		MathUnary,
		MathBinary,
		MathTernary,
		Output
	};

	inline const char* getCategoryName(NodeCategory cat)
	{
		switch (cat)
		{
			case NodeCategory::Input:
				return "Inputs & Coordinates";
			case NodeCategory::Vector:
				return "Vector Math";
			case NodeCategory::Transforms:
				return "Domain Transforms";
			case NodeCategory::Primitives2D:
				return "2D Primitives";
			case NodeCategory::Primitives3D:
				return "3D Primitives";
			case NodeCategory::CSG:
				return "CSG & Modifiers";
			case NodeCategory::MathUnary:
				return "Math (Unary)";
			case NodeCategory::MathBinary:
				return "Math (Binary)";
			case NodeCategory::MathTernary:
				return "Math (Ternary)";
			case NodeCategory::Output:
				return "Output";
			default:
				return "Other";
		}
	}

	using NodeValue = std::variant<Expr, Vec2Expr>;

	inline Expr asFloat(const NodeValue& val)
	{
		if (std::holds_alternative<Expr>(val))
			return std::get<Expr>(val);
		return Expr(0.0f);
	}

	inline Vec2Expr asVec2(const NodeValue& val)
	{
		if (std::holds_alternative<Vec2Expr>(val))
			return std::get<Vec2Expr>(val);
		return Vec2Expr(0.0f, 0.0f);
	}

	struct PinDef
	{
		std::string name;
		PinType type = PinType::Float;
		float defaultFloat = 0.0f;
		glm::vec2 defaultVec2 = {0.0f, 0.0f};
		float minFloat = -1000.0f;
		float maxFloat = 1000.0f;
		float speed = 0.1f;
	};

	struct NodeInstance;

	struct NodeDef
	{
		std::string typeId;
		std::string displayName;
		NodeCategory category;
		std::vector<PinDef> inputs;
		std::vector<PinDef> outputs;
		std::function<NodeValue(const std::vector<NodeValue>& in, const NodeInstance& node)> evaluate;
		std::function<std::string(const std::vector<std::string>& in, const NodeInstance& node)> toCppCode;
	};

	// Custom data for nodes that need editable instance properties (e.g. constant value, var index)
	struct NodeInstanceData
	{
		float customFloat = 1.0f;
		glm::vec2 customVec2 = {0.0f, 0.0f};
		int customInt = 0;
	};

	struct NodeInstance
	{
		int id = 0;
		std::string typeId;
		std::string customLabel;
		glm::vec2 position = {100.0f, 100.0f};
		NodeInstanceData data;

		// Local values for unconnected input pins
		std::vector<float> inputFloats;
		std::vector<glm::vec2> inputVec2s;
	};

	struct LinkInstance
	{
		int id = 0;
		int startPinId = 0; // Output pin
		int endPinId = 0;	// Input pin
	};

	inline int makePinId(int nodeId, bool isInput, int pinIndex)
	{
		return (nodeId * 100) + (isInput ? 0 : 50) + pinIndex;
	}

	inline void decomposePinId(int pinId, int& outNodeId, bool& outIsInput, int& outPinIndex)
	{
		outNodeId = pinId / 100;
		int remainder = pinId % 100;
		outIsInput = (remainder < 50);
		outPinIndex = outIsInput ? remainder : (remainder - 50);
	}
} // namespace WeirdEngine::Editor
