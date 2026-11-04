#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <type_traits>

namespace WeirdEngine
{
	template <typename T, std::size_t Capacity> class FixedVector
	{
	public:
		using value_type = T;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using reference = T&;
		using const_reference = const T&;
		using pointer = T*;
		using const_pointer = const T*;
		using iterator = T*;
		using const_iterator = const T*;

		T m_data[Capacity]{};
		size_type m_size{0};

		constexpr FixedVector() noexcept = default;

		constexpr FixedVector(std::initializer_list<T> init)
		{
			for (const auto& item : init)
			{
				push_back(item);
			}
		}

		template <typename InputIt> FixedVector(InputIt first, InputIt last)
		{
			assign(first, last);
		}

		template <typename InputIt> void assign(InputIt first, InputIt last)
		{
			m_size = 0;
			for (auto it = first; it != last && m_size < Capacity; ++it)
			{
				m_data[m_size++] = *it;
			}
		}

		constexpr size_type size() const noexcept
		{
			return m_size;
		}
		constexpr size_type capacity() const noexcept
		{
			return Capacity;
		}
		constexpr size_type max_size() const noexcept
		{
			return Capacity;
		}
		constexpr bool empty() const noexcept
		{
			return m_size == 0;
		}

		void reserve(size_type) noexcept {}

		void clear() noexcept
		{
			m_size = 0;
		}

		void push_back(const T& value)
		{
			if (m_size < Capacity)
			{
				m_data[m_size++] = value;
			}
		}

		void push_back(T&& value)
		{
			if (m_size < Capacity)
			{
				m_data[m_size++] = std::move(value);
			}
		}

		void pop_back() noexcept
		{
			if (m_size > 0)
			{
				--m_size;
			}
		}

		reference front() noexcept
		{
			return m_data[0];
		}
		const_reference front() const noexcept
		{
			return m_data[0];
		}

		reference back() noexcept
		{
			return m_data[m_size > 0 ? m_size - 1 : 0];
		}
		const_reference back() const noexcept
		{
			return m_data[m_size > 0 ? m_size - 1 : 0];
		}

		reference operator[](size_type index) noexcept
		{
			return m_data[index];
		}
		const_reference operator[](size_type index) const noexcept
		{
			return m_data[index];
		}

		pointer data() noexcept
		{
			return m_data;
		}
		const_pointer data() const noexcept
		{
			return m_data;
		}

		iterator begin() noexcept
		{
			return m_data;
		}
		const_iterator begin() const noexcept
		{
			return m_data;
		}
		const_iterator cbegin() const noexcept
		{
			return m_data;
		}

		iterator end() noexcept
		{
			return m_data + m_size;
		}
		const_iterator end() const noexcept
		{
			return m_data + m_size;
		}
		const_iterator cend() const noexcept
		{
			return m_data + m_size;
		}

		template <typename InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last)
		{
			iterator p = const_cast<iterator>(pos);
			if (p < begin() || p > end())
			{
				return end();
			}
			size_type index = static_cast<size_type>(p - begin());
			for (auto it = first; it != last && m_size < Capacity; ++it)
			{
				for (size_type j = m_size; j > index; --j)
				{
					m_data[j] = std::move(m_data[j - 1]);
				}
				m_data[index++] = *it;
				++m_size;
			}
			return begin() + index;
		}

		iterator erase(const_iterator pos)
		{
			iterator p = const_cast<iterator>(pos);
			if (p >= begin() && p < end())
			{
				for (iterator it = p; it + 1 < end(); ++it)
				{
					*it = std::move(*(it + 1));
				}
				--m_size;
				return p;
			}
			return end();
		}

		iterator erase(const_iterator first, const_iterator last)
		{
			iterator f = const_cast<iterator>(first);
			iterator l = const_cast<iterator>(last);
			if (f >= begin() && l <= end() && f <= l)
			{
				size_type numToMove = end() - l;
				for (size_type i = 0; i < numToMove; ++i)
				{
					*(f + i) = std::move(*(l + i));
				}
				m_size -= (l - f);
				return f;
			}
			return end();
		}

		void resize(size_type newSize, const T& value = T{})
		{
			if (newSize > Capacity)
			{
				newSize = Capacity;
			}
			while (m_size < newSize)
			{
				m_data[m_size++] = value;
			}
			if (m_size > newSize)
			{
				m_size = newSize;
			}
		}
	};
} // namespace WeirdEngine

namespace std
{
	template <typename T, std::size_t Capacity, typename U>
	constexpr typename WeirdEngine::FixedVector<T, Capacity>::size_type erase(WeirdEngine::FixedVector<T, Capacity>& c,
																			  const U& value)
	{
		auto it = std::remove(c.begin(), c.end(), value);
		auto r = static_cast<typename WeirdEngine::FixedVector<T, Capacity>::size_type>(c.end() - it);
		c.erase(it, c.end());
		return r;
	}

	template <typename T, std::size_t Capacity, typename Pred>
	constexpr typename WeirdEngine::FixedVector<T, Capacity>::size_type erase_if(
		WeirdEngine::FixedVector<T, Capacity>& c, Pred pred)
	{
		auto it = std::remove_if(c.begin(), c.end(), pred);
		auto r = static_cast<typename WeirdEngine::FixedVector<T, Capacity>::size_type>(c.end() - it);
		c.erase(it, c.end());
		return r;
	}
} // namespace std
