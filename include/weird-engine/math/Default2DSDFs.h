#ifndef WEIRDSAMPLES_DEFAULT2DSDFS_H
#define WEIRDSAMPLES_DEFAULT2DSDFS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "CompiledMathExpressions.h"
#include "MathExpressions.h"
#include "Primitives.h"
#include "StarShape.h"

namespace WeirdEngine
{
	namespace DefaultShapes
	{
		enum Type
		{
			CIRCLE,
			BOX,
			BOX_LINE,
			LINE,
			RAMP,
			SINE,
			STAR,
			SIZE
		};

		inline std::vector<std::shared_ptr<IMathExpression>> getSDFS()
		{
			std::vector<std::shared_ptr<IMathExpression>> m_sdfs;
			m_sdfs.resize(DefaultShapes::SIZE);

			// Sine
			{
				auto amplitude = std::make_shared<FloatVariable>(Primitives::SineWave::AMPLITUDE);
				auto period = std::make_shared<FloatVariable>(Primitives::SineWave::PERIOD);
				auto speed = std::make_shared<FloatVariable>(Primitives::SineWave::SPEED);
				auto offset = std::make_shared<FloatVariable>(Primitives::SineWave::OFFSET);

				m_sdfs[SINE] = std::make_shared<Primitives::SineWave>(amplitude, period, speed, offset);
			}

			// Star
			m_sdfs[STAR] = getStarShape();

			// Circle
			{
				auto m_px = std::make_shared<FloatVariable>(Primitives::Circle::POS_X);
				auto m_py = std::make_shared<FloatVariable>(Primitives::Circle::POS_Y);
				auto m_r = std::make_shared<FloatVariable>(Primitives::Circle::RADIUS);

				m_sdfs[CIRCLE] = std::make_shared<Primitives::Circle>(m_px, m_py, m_r);
			}

			// Box
			{
				auto px = std::make_shared<FloatVariable>(Primitives::Box::POS_X);
				auto py = std::make_shared<FloatVariable>(Primitives::Box::POS_Y);
				auto sx = std::make_shared<FloatVariable>(Primitives::Box::SIZE_X);
				auto sy = std::make_shared<FloatVariable>(Primitives::Box::SIZE_Y);

				m_sdfs[BOX] = std::make_shared<Primitives::Box>(px, py, sx, sy);
			}

			// Box line
			{
				auto px = std::make_shared<FloatVariable>(Primitives::BoxLine::POS_X);
				auto py = std::make_shared<FloatVariable>(Primitives::BoxLine::POS_Y);
				auto sx = std::make_shared<FloatVariable>(Primitives::BoxLine::SIZE_X);
				auto sy = std::make_shared<FloatVariable>(Primitives::BoxLine::SIZE_Y);
				auto thickness = std::make_shared<FloatVariable>(Primitives::BoxLine::THICKNESS);

				m_sdfs[BOX_LINE] = std::make_shared<Primitives::BoxLine>(px, py, sx, sy, thickness);
			}

			// Line
			{
				auto ax = std::make_shared<FloatVariable>(Primitives::Line::POS_A_X);
				auto ay = std::make_shared<FloatVariable>(Primitives::Line::POS_A_Y);
				auto bx = std::make_shared<FloatVariable>(Primitives::Line::POS_B_X);
				auto by = std::make_shared<FloatVariable>(Primitives::Line::POS_B_Y);
				auto width = std::make_shared<FloatVariable>(Primitives::Line::WIDTH);

				m_sdfs[LINE] = std::make_shared<Primitives::Line>(ax, ay, bx, by, width);
			}

			// Ramp
			{
				auto px = std::make_shared<FloatVariable>(Primitives::Ramp::POS_X);
				auto py = std::make_shared<FloatVariable>(Primitives::Ramp::POS_Y);
				auto width = std::make_shared<FloatVariable>(Primitives::Ramp::WIDTH);
				auto height = std::make_shared<FloatVariable>(Primitives::Ramp::HEIGHT);
				auto skew = std::make_shared<FloatVariable>(Primitives::Ramp::SKEW);

				m_sdfs[RAMP] = std::make_shared<Primitives::Ramp>(px, py, width, height, skew);
			}

			return m_sdfs;
		}
	} // namespace DefaultShapes
} // namespace WeirdEngine
#endif // WEIRDSAMPLES_DEFAULT2DSDFS_H