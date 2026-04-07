#pragma once

#include "MathExpressions.h"

namespace WeirdEngine
{
	inline std::shared_ptr<IMathExpression> getStarShape()
	{
		// Define variables
		auto offsetX = std::make_shared<FloatVariable>(0);
		auto offsetY = std::make_shared<FloatVariable>(1);
		auto radious = std::make_shared<FloatVariable>(2);
		auto displacementStrength = std::make_shared<FloatVariable>(3);
		auto starPoints = std::make_shared<FloatVariable>(4);
		auto speed = std::make_shared<FloatVariable>(5);

		auto time = std::make_shared<FloatVariable>(8);
		auto x = std::make_shared<FloatVariable>(9);
		auto y = std::make_shared<FloatVariable>(10);

		// Define function
		std::shared_ptr<IMathExpression> positionX = std::make_shared<Substraction>(x, offsetX);
		std::shared_ptr<IMathExpression> positionY = std::make_shared<Substraction>(y, offsetY);

		// Circle
		std::shared_ptr<IMathExpression> circleDistance =
			std::make_shared<Substraction>(std::make_shared<Length>(positionX, positionY), radious);

		std::shared_ptr<IMathExpression> angularDisplacement =
			std::make_shared<Multiplication>(starPoints, std::make_shared<Atan2>(positionY, positionX));
		std::shared_ptr<IMathExpression> animationTime = std::make_shared<Multiplication>(speed, time);

		std::shared_ptr<IMathExpression> sinContent =
			std::make_shared<Sine>(std::make_shared<Substraction>(angularDisplacement, animationTime));
		std::shared_ptr<IMathExpression> displacement =
			std::make_shared<Multiplication>(displacementStrength, sinContent);

		std::shared_ptr<IMathExpression> starDistance = std::make_shared<Addition>(circleDistance, displacement);

		return starDistance;
	}
} // namespace WeirdEngine