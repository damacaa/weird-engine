#ifndef WEIRDSAMPLES_DEFAULT2DSDFS_H
#define WEIRDSAMPLES_DEFAULT2DSDFS_H

#include <vector>

#include "MathExpressions.h"

namespace WeirdEngine
{
	inline std::vector<std::shared_ptr<IMathExpression>> getSDFS()
	{
		std::vector<std::shared_ptr<IMathExpression>> m_sdfs;

		// Floor
		{
			// p.y - a * sinf(0.5f * p.x + u_time);

			// Define variables
			auto amplitude = std::make_shared<FloatVariable>(0);
			auto period = std::make_shared<FloatVariable>(1);
			auto speed = std::make_shared<FloatVariable>(2);
			auto offset = std::make_shared<FloatVariable>(3);

			auto time = std::make_shared<FloatVariable>(8);
			auto x = std::make_shared<FloatVariable>(9);
			auto y = std::make_shared<FloatVariable>(10);

			// Define function
			auto sineContent = std::make_shared<Addition>(std::make_shared<Multiplication>(period, x), std::make_shared<Multiplication>(speed, time));
			std::shared_ptr<IMathExpression> waveFormula = std::make_shared<Substraction>(std::make_shared<Substraction>(y, offset), std::make_shared<Multiplication>(amplitude, std::make_shared<Sine>(sineContent)));

			// Store function
			m_sdfs.push_back(waveFormula);
		}

		// Star
		{

			// vec2 starPosition = p - vec2(25.0f, 30.0f);
			// float infiniteShereDist = length(starPosition) - 5.0f;
			// float displacement = 5.0 * sin(5.0f * atan2f(starPosition.y, starPosition.x) - 5.0f * u_time);

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
			std::shared_ptr<IMathExpression> circleDistance = std::make_shared<Substraction>(std::make_shared<Length>(positionX, positionY), radious);

			std::shared_ptr<IMathExpression> angularDisplacement = std::make_shared<Multiplication>(starPoints, std::make_shared<Atan2>(positionY, positionX));
			std::shared_ptr<IMathExpression> animationTime = std::make_shared<Multiplication>(speed, time);

			std::shared_ptr<IMathExpression> sinContent = std::make_shared<Sine>(std::make_shared<Substraction>(angularDisplacement, animationTime));
			std::shared_ptr<IMathExpression> displacement = std::make_shared<Multiplication>(displacementStrength, sinContent);

			std::shared_ptr<IMathExpression> starDistance = std::make_shared<Addition>(circleDistance, displacement);

			// Store function
			m_sdfs.push_back(starDistance);
		}

		// Circle
		{

			// vec2 starPosition = p - vec2(25.0f, 30.0f);
			// float infiniteShereDist = length(starPosition) - 5.0f;
			// float displacement = 5.0 * sin(5.0f * atan2f(starPosition.y, starPosition.x) - 5.0f * u_time);

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
			std::shared_ptr<IMathExpression> circleDistance = std::make_shared<Substraction>(std::make_shared<Length>(positionX, positionY), radious);

			// Store function
			m_sdfs.push_back(circleDistance);
		}

		// Box
		{
			// Define variables
			auto offsetX = std::make_shared<FloatVariable>(0);
			auto offsetY = std::make_shared<FloatVariable>(1);
			auto bX = std::make_shared<FloatVariable>(2);
			auto bY = std::make_shared<FloatVariable>(3);
			auto r = std::make_shared<FloatVariable>(4);

			auto time = std::make_shared<FloatVariable>(8);
			auto x = std::make_shared<FloatVariable>(9);
			auto y = std::make_shared<FloatVariable>(10);

			// Define function
			std::shared_ptr<IMathExpression> positionX = std::make_shared<Substraction>(x, offsetX);
			std::shared_ptr<IMathExpression> positionY = std::make_shared<Substraction>(y, offsetY);

			std::shared_ptr<IMathExpression> dX = std::make_shared<Substraction>(std::make_shared<Abs>(positionX), bX);
			std::shared_ptr<IMathExpression> dY = std::make_shared<Substraction>(std::make_shared<Abs>(positionY), bY);

			std::shared_ptr<IMathExpression> aX = std::make_shared<Max>(0.0f, dX);
			std::shared_ptr<IMathExpression> aY = std::make_shared<Max>(0.0f, dY);

			std::shared_ptr<IMathExpression> length = std::make_shared<Length>(aX, aY);

			std::shared_ptr<IMathExpression> minMax = std::make_shared<Min>(0.0f, std::make_shared<Max>(dX, dY));

			std::shared_ptr<IMathExpression> boxDistance = std::make_shared<Addition>(length, minMax);

			// Store function
			m_sdfs.push_back(boxDistance);
		}

		return m_sdfs;
	}
}

#endif //WEIRDSAMPLES_DEFAULT2DSDFS_H