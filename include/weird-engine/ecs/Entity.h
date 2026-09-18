#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace WeirdEngine
{
	struct SimulationID;

	// Strong-type Entity struct
	struct Entity
	{
		std::uint32_t id{0};

		constexpr Entity() noexcept = default;
		constexpr Entity(std::uint32_t val) noexcept
			: id(val)
		{
		}
		constexpr Entity(int val) noexcept
			: id(static_cast<std::uint32_t>(val))
		{
		}
		constexpr Entity(std::size_t val) noexcept
			: id(static_cast<std::uint32_t>(val))
		{
		}

		// Prevent conversion or assignment from SimulationID
		template <typename T>
			requires std::same_as<std::decay_t<T>, SimulationID>
		Entity(T&&) = delete;

		template <typename T>
			requires std::same_as<std::decay_t<T>, SimulationID>
		Entity& operator=(T&&) = delete;

		constexpr operator std::size_t() const noexcept
		{
			return id;
		}

		constexpr bool operator==(const Entity&) const noexcept = default;
		constexpr auto operator<=>(const Entity&) const = default;

		constexpr bool operator==(std::integral auto other) const noexcept
		{
			return id == static_cast<std::uint32_t>(other);
		}
		constexpr auto operator<=>(std::integral auto other) const noexcept
		{
			return id <=> static_cast<std::uint32_t>(other);
		}

		constexpr Entity& operator++() noexcept
		{
			++id;
			return *this;
		}
		constexpr Entity operator++(int) noexcept
		{
			Entity tmp = *this;
			++id;
			return tmp;
		}
		constexpr Entity& operator--() noexcept
		{
			--id;
			return *this;
		}
		constexpr Entity operator--(int) noexcept
		{
			Entity tmp = *this;
			--id;
			return tmp;
		}

		constexpr Entity operator+(std::integral auto val) const noexcept
		{
			return Entity{id + static_cast<std::uint32_t>(val)};
		}
		constexpr Entity operator-(std::integral auto val) const noexcept
		{
			return Entity{id - static_cast<std::uint32_t>(val)};
		}

		template <typename BasicJsonType> friend void to_json(BasicJsonType& j, const Entity& e)
		{
			j = e.id;
		}

		template <typename BasicJsonType> friend void from_json(const BasicJsonType& j, Entity& e)
		{
			e.id = j.template get<std::uint32_t>();
		}
	};

	constexpr Entity MAX_ENTITIES{10000};
	constexpr Entity INVALID_ENTITY{MAX_ENTITIES};
} // namespace WeirdEngine

template <> struct std::hash<WeirdEngine::Entity>
{
	std::size_t operator()(const WeirdEngine::Entity& e) const noexcept
	{
		return std::hash<std::uint32_t>{}(e.id);
	}
};
