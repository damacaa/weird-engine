#pragma once
#include <cstddef>
#include <cstring>
#include <ostream>
#include <string>
#include <string_view>

namespace WeirdEngine
{
	template <std::size_t N = 32> struct FixedString
	{
		char data[N]{};

		constexpr FixedString() noexcept = default;

		FixedString(const char* s) noexcept
		{
			assign(s);
		}

		FixedString(std::string_view s) noexcept
		{
			assign(s);
		}

		template <std::size_t M> FixedString(const FixedString<M>& other) noexcept
		{
			assign(other.data);
		}

		FixedString& operator=(const char* s) noexcept
		{
			assign(s);
			return *this;
		}

		FixedString& operator=(std::string_view s) noexcept
		{
			assign(s);
			return *this;
		}

		template <std::size_t M> FixedString& operator=(const FixedString<M>& other) noexcept
		{
			assign(other.data);
			return *this;
		}

		void assign(const char* s) noexcept
		{
			if (s)
			{
				std::size_t len = std::strlen(s);
				if (len >= N)
				{
					len = N - 1;
				}
				std::memcpy(data, s, len);
				data[len] = '\0';
			}
			else
			{
				data[0] = '\0';
			}
		}

		void assign(std::string_view s) noexcept
		{
			std::size_t len = s.size();
			if (len >= N)
			{
				len = N - 1;
			}
			std::memcpy(data, s.data(), len);
			data[len] = '\0';
		}

		constexpr std::size_t capacity() const noexcept
		{
			return N;
		}

		std::size_t length() const noexcept
		{
			return std::strlen(data);
		}

		std::size_t size() const noexcept
		{
			return length();
		}

		bool empty() const noexcept
		{
			return data[0] == '\0';
		}

		void clear() noexcept
		{
			data[0] = '\0';
		}

		const char* c_str() const noexcept
		{
			return data;
		}

		char* begin() noexcept
		{
			return data;
		}

		char* end() noexcept
		{
			return data + length();
		}

		const char* begin() const noexcept
		{
			return data;
		}

		const char* end() const noexcept
		{
			return data + length();
		}

		char& operator[](std::size_t idx) noexcept
		{
			return data[idx];
		}

		const char& operator[](std::size_t idx) const noexcept
		{
			return data[idx];
		}

		operator std::string_view() const noexcept
		{
			return std::string_view(data, length());
		}

		explicit operator std::string() const
		{
			return std::string(data, length());
		}

		template <std::size_t M> bool operator==(const FixedString<M>& other) const noexcept
		{
			return std::strcmp(data, other.data) == 0;
		}

		bool operator==(const char* other) const noexcept
		{
			return other && std::strcmp(data, other) == 0;
		}

		bool operator==(std::string_view other) const noexcept
		{
			return std::string_view(*this) == other;
		}

		template <std::size_t M> bool operator!=(const FixedString<M>& other) const noexcept
		{
			return !(*this == other);
		}

		bool operator!=(const char* other) const noexcept
		{
			return !(*this == other);
		}

		bool operator!=(std::string_view other) const noexcept
		{
			return !(*this == other);
		}

		friend std::ostream& operator<<(std::ostream& os, const FixedString& fs)
		{
			return os << fs.data;
		}
	};
} // namespace WeirdEngine
