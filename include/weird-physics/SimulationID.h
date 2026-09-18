#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>

#include "weird-engine/ecs/Entity.h"

namespace WeirdEngine
{
	// Strong-type SimulationID struct
	struct SimulationID
	{
		std::uint32_t id{static_cast<std::uint32_t>(-1)};

		constexpr SimulationID() noexcept = default;
		constexpr SimulationID(std::uint32_t val) noexcept
			: id(val)
		{
		}
		constexpr SimulationID(int val) noexcept
			: id(static_cast<std::uint32_t>(val))
		{
		}
		constexpr SimulationID(std::size_t val) noexcept
			: id(static_cast<std::uint32_t>(val))
		{
		}

		// Prevent conversion or assignment from Entity
		template <typename T>
			requires std::same_as<std::decay_t<T>, Entity>
		SimulationID(T&&) = delete;

		template <typename T>
			requires std::same_as<std::decay_t<T>, Entity>
		SimulationID& operator=(T&&) = delete;

		constexpr operator std::size_t() const noexcept
		{
			return id;
		}

		constexpr bool operator==(const SimulationID&) const noexcept = default;
		constexpr auto operator<=>(const SimulationID&) const = default;

		constexpr bool operator==(std::integral auto other) const noexcept
		{
			return id == static_cast<std::uint32_t>(other);
		}
		constexpr auto operator<=>(std::integral auto other) const noexcept
		{
			return id <=> static_cast<std::uint32_t>(other);
		}

		constexpr SimulationID& operator++() noexcept
		{
			++id;
			return *this;
		}
		constexpr SimulationID operator++(int) noexcept
		{
			SimulationID tmp = *this;
			++id;
			return tmp;
		}
		constexpr SimulationID& operator--() noexcept
		{
			--id;
			return *this;
		}
		constexpr SimulationID operator--(int) noexcept
		{
			SimulationID tmp = *this;
			--id;
			return tmp;
		}

		constexpr SimulationID operator+(std::integral auto val) const noexcept
		{
			return SimulationID{id + static_cast<std::uint32_t>(val)};
		}
		constexpr SimulationID operator-(std::integral auto val) const noexcept
		{
			return SimulationID{id - static_cast<std::uint32_t>(val)};
		}

		template <typename BasicJsonType> friend void to_json(BasicJsonType& j, const SimulationID& s)
		{
			j = s.id;
		}

		template <typename BasicJsonType> friend void from_json(const BasicJsonType& j, SimulationID& s)
		{
			s.id = j.template get<std::uint32_t>();
		}
	};

	using SimulationId = SimulationID;
	constexpr SimulationID INVALID_SIMULATION_ID{static_cast<std::uint32_t>(-1)};

	// Prevent cross-comparison between Entity and SimulationID
	template <typename T, typename U>
		requires(std::same_as<std::decay_t<T>, Entity> && std::same_as<std::decay_t<U>, SimulationID>) ||
					(std::same_as<std::decay_t<T>, SimulationID> && std::same_as<std::decay_t<U>, Entity>)
	bool operator==(const T&, const U&) = delete;

	template <typename T, typename U>
		requires(std::same_as<std::decay_t<T>, Entity> && std::same_as<std::decay_t<U>, SimulationID>) ||
					(std::same_as<std::decay_t<T>, SimulationID> && std::same_as<std::decay_t<U>, Entity>)
	auto operator<=>(const T&, const U&) = delete;
} // namespace WeirdEngine

template <> struct std::hash<WeirdEngine::SimulationID>
{
	std::size_t operator()(const WeirdEngine::SimulationID& s) const noexcept
	{
		return std::hash<std::uint32_t>{}(s.id);
	}
};
