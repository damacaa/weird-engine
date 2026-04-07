#pragma once
#include <cstdint>

namespace WeirdEngine
{
	// Entity type
	using Entity = std::uint32_t;
	constexpr Entity MAX_ENTITIES = 10000;
	constexpr Entity INVALID_ENTITY = MAX_ENTITIES;
} // namespace WeirdEngine
