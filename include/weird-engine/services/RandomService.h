#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "weird-engine/Assert.h"

namespace WeirdEngine
{
	class RandomService
	{
	public:
		using result_type = std::mt19937::result_type;

		RandomService()
			: m_generator(static_cast<result_type>(s_globalSeed))
		{
		}

		// ---- Global seed control ----
		static void setGlobalSeed(uint64_t seed)
		{
			s_globalSeed = seed;
		}

		static uint64_t getGlobalSeed()
		{
			return s_globalSeed;
		}

		// Reseed local generator directly
		void seed(uint64_t seedValue)
		{
			m_generator.seed(static_cast<result_type>(seedValue));
		}

		// Seed deterministically based on global seed + level + stream
		void seedLevel(int level, uint32_t stream = 0)
		{
			m_generator.seed(static_cast<result_type>(s_globalSeed + static_cast<uint64_t>(level - 1) + stream));
		}

		// ---- Uniform distributions ----
		template <std::integral T> T range(T minVal, T maxVal)
		{
			if (minVal > maxVal)
				std::swap(minVal, maxVal);
			if constexpr (sizeof(T) < sizeof(short))
			{
				std::uniform_int_distribution<int> dist(static_cast<int>(minVal), static_cast<int>(maxVal));
				return static_cast<T>(dist(m_generator));
			}
			else
			{
				std::uniform_int_distribution<T> dist(minVal, maxVal);
				return dist(m_generator);
			}
		}

		template <std::floating_point T> T range(T minVal, T maxVal)
		{
			if (minVal > maxVal)
				std::swap(minVal, maxVal);
			std::uniform_real_distribution<T> dist(minVal, maxVal);
			return dist(m_generator);
		}

		int getInt(int minVal, int maxVal)
		{
			return range(minVal, maxVal);
		}

		float getFloat(float minVal, float maxVal)
		{
			return range(minVal, maxVal);
		}

		float getFloat()
		{
			return range(0.0f, 1.0f);
		}

		bool getBool(double probability = 0.5)
		{
			if (probability <= 0.0)
				return false;
			if (probability >= 1.0)
				return true;
			std::bernoulli_distribution dist(probability);
			return dist(m_generator);
		}

		template <std::floating_point T = float> T normal(T mean, T stddev)
		{
			std::normal_distribution<T> dist(mean, stddev);
			return dist(m_generator);
		}

		float getNormal(float mean, float stddev)
		{
			return normal<float>(mean, stddev);
		}

		template <typename T, size_t N> const T& choice(const T (&arr)[N])
		{
			static_assert(N > 0, "Cannot choose from empty array");
			size_t idx = static_cast<size_t>(range<size_t>(0, N - 1));
			return arr[idx];
		}

		template <typename T> const T& choice(std::span<const T> items)
		{
			WEIRD_ASSERT(!items.empty(), "Cannot choose from empty span");
			size_t idx = static_cast<size_t>(range<size_t>(0, items.size() - 1));
			return items[idx];
		}

		template <typename T> const T& choice(const std::vector<T>& items)
		{
			return choice(std::span<const T>(items));
		}

		// UniformRandomBitGenerator concept support
		static constexpr result_type min()
		{
			return std::mt19937::min();
		}

		static constexpr result_type max()
		{
			return std::mt19937::max();
		}

		result_type operator()()
		{
			return m_generator();
		}

		std::mt19937& engine()
		{
			return m_generator;
		}

	private:
		std::mt19937 m_generator;
		static inline uint64_t s_globalSeed = 0;
	};
} // namespace WeirdEngine
