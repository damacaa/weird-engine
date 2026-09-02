#pragma once

#include "SDF.h"
#include <cstdint>

namespace WeirdEngine::Primitives
{
	static constexpr uint8_t WORLD_X = SystemParams::WORLD_X;
	static constexpr uint8_t WORLD_Y = SystemParams::WORLD_Y;

	struct Circle
	{
		static constexpr uint8_t POS_X = 0;
		static constexpr uint8_t POS_Y = 1;
		static constexpr uint8_t RADIUS = 2;
		static constexpr uint8_t THICKNESS = 3;

		static constexpr uint8_t TIME = SystemParams::TIME;
		static constexpr uint8_t WORLD_X = SystemParams::WORLD_X;
		static constexpr uint8_t WORLD_Y = SystemParams::WORLD_Y;
	};

	struct Box
	{
		static constexpr uint8_t POS_X = 0;
		static constexpr uint8_t POS_Y = 1;
		static constexpr uint8_t SIZE_X = 2;
		static constexpr uint8_t SIZE_Y = 3;
		static constexpr uint8_t THICKNESS = 4;

		static constexpr uint8_t WORLD_X = SystemParams::WORLD_X;
		static constexpr uint8_t WORLD_Y = SystemParams::WORLD_Y;
	};

	struct SineWave
	{
		static constexpr uint8_t AMPLITUDE = 0;
		static constexpr uint8_t PERIOD = 1;
		static constexpr uint8_t SPEED = 2;
		static constexpr uint8_t OFFSET = 3;

		static constexpr uint8_t TIME = SystemParams::TIME;
		static constexpr uint8_t WORLD_X = SystemParams::WORLD_X;
		static constexpr uint8_t WORLD_Y = SystemParams::WORLD_Y;
	};

	struct Triangle
	{
		static constexpr uint8_t POS_X = 0;
		static constexpr uint8_t POS_Y = 1;
		static constexpr uint8_t SIZE_X = 2;
		static constexpr uint8_t SIZE_Y = 3;
		static constexpr uint8_t THICKNESS = 4;

		static constexpr uint8_t TIME = SystemParams::TIME;
		static constexpr uint8_t WORLD_X = SystemParams::WORLD_X;
		static constexpr uint8_t WORLD_Y = SystemParams::WORLD_Y;
	};

	struct Ramp
	{
		static constexpr uint8_t POS_X = 0;
		static constexpr uint8_t POS_Y = 1;
		static constexpr uint8_t SIZE_X = 2;
		static constexpr uint8_t SIZE_Y = 3;
		static constexpr uint8_t SKEW = 4;

		static constexpr uint8_t WORLD_X = SystemParams::WORLD_X;
		static constexpr uint8_t WORLD_Y = SystemParams::WORLD_Y;
	};

	struct BoxRotated
	{
		static constexpr uint8_t POS_X = 0;
		static constexpr uint8_t POS_Y = 1;
		static constexpr uint8_t SIZE_X = 2;
		static constexpr uint8_t SIZE_Y = 3;
		static constexpr uint8_t ANGLE = 4;
		static constexpr uint8_t THICKNESS = 5;
	};

	struct TriangleRotated
	{
		static constexpr uint8_t POS_X = 0;
		static constexpr uint8_t POS_Y = 1;
		static constexpr uint8_t SIZE_X = 2;
		static constexpr uint8_t SIZE_Y = 3;
		static constexpr uint8_t ANGLE = 4;
		static constexpr uint8_t THICKNESS = 5;
	};

	struct RampRotated
	{
		static constexpr uint8_t POS_X = 0;
		static constexpr uint8_t POS_Y = 1;
		static constexpr uint8_t WIDTH = 2;
		static constexpr uint8_t HEIGHT = 3;
		static constexpr uint8_t SKEW = 4;
		static constexpr uint8_t ANGLE = 5;
	};

	struct Star
	{
		static constexpr uint8_t POS_X = 0;
		static constexpr uint8_t POS_Y = 1;
		static constexpr uint8_t RADIUS = 2;
		static constexpr uint8_t DISPLACEMENT = 3;
		static constexpr uint8_t POINTS = 4;
		static constexpr uint8_t SPEED = 5;
	};

	struct Line
	{
		static constexpr uint8_t POS_A_X = 0;
		static constexpr uint8_t POS_A_Y = 1;
		static constexpr uint8_t POS_B_X = 2;
		static constexpr uint8_t POS_B_Y = 3;
		static constexpr uint8_t WIDTH = 4;

		static constexpr uint8_t WORLD_X = SystemParams::WORLD_X;
		static constexpr uint8_t WORLD_Y = SystemParams::WORLD_Y;
	};
} // namespace WeirdEngine::Primitives