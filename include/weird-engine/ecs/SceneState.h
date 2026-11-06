#pragma once

#include <istream>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

#include "Entity.h"
#include "weird-engine/Logger.h"

namespace WeirdEngine
{
	// Base for type-erased scene state (see Registry's scene state API).
	class ISceneState
	{
	public:
		virtual ~ISceneState() = default;
		virtual void serializeBinary(std::ostream& os) const = 0;
		virtual bool deserializeBinary(std::istream& is, size_t fileElemSize) = 0;
		virtual size_t getStateSize() const = 0;
		virtual bool isSnapshotSerializable() const = 0;
		virtual bool deserializeToBuffer(std::istream& is, size_t fileElemSize, std::vector<uint8_t>& outBuffer) = 0;
		virtual void applyBuffer(const std::vector<uint8_t>& buffer) = 0;
	};

	// Type-erased owner of a single T. Scene state is stored once per type and
	// is never copied, so T can be non-copyable or expensive to move.
	template <typename T> class SceneStateHolder : public ISceneState
	{
	public:
		template <typename... Args>
		explicit SceneStateHolder(Args&&... args)
			: value(std::forward<Args>(args)...)
		{
		}

		size_t getStateSize() const override
		{
			return sizeof(T);
		}

		bool isSnapshotSerializable() const override
		{
			return is_snapshot_serializable_v<T>;
		}

		void serializeBinary(std::ostream& os) const override
		{
			if constexpr (is_snapshot_serializable_v<T>)
			{
				uint32_t elemSize = static_cast<uint32_t>(sizeof(T));
				os.write(reinterpret_cast<const char*>(&elemSize), sizeof(elemSize));

				alignas(T) char cleanBuf[sizeof(T)];
				std::memset(cleanBuf, 0, sizeof(T));
				if constexpr (std::is_copy_assignable_v<T>)
				{
					*reinterpret_cast<T*>(cleanBuf) = value;
				}
				else
				{
					std::memcpy(cleanBuf, &value, sizeof(T));
				}
				os.write(cleanBuf, sizeof(T));
			}
			else
			{
				uint32_t elemSize = 0;
				os.write(reinterpret_cast<const char*>(&elemSize), sizeof(elemSize));
			}
		}

		bool deserializeToBuffer(std::istream& is, size_t fileElemSize, std::vector<uint8_t>& outBuffer) override
		{
			uint32_t elemSize = 0;
			if (!readOrFail(is, &elemSize, sizeof(elemSize)))
				return false;

			if constexpr (is_snapshot_serializable_v<T>)
			{
				if (elemSize != sizeof(T))
				{
					Logger::error("[Snapshot] SceneState size mismatch! Expected " + std::to_string(sizeof(T)) +
								  ", got " + std::to_string(elemSize) + ".");
					return false;
				}
				outBuffer.resize(sizeof(T));
				if (!readOrFail(is, outBuffer.data(), sizeof(T)))
				{
					outBuffer.clear();
					return false;
				}
				return true;
			}
			else
			{
				if (elemSize > 0)
				{
					is.seekg(static_cast<std::streamoff>(elemSize), std::ios::cur);
					if (!is.good())
						return false;
				}
				return false;
			}
		}

		void applyBuffer(const std::vector<uint8_t>& buffer) override
		{
			if constexpr (is_snapshot_serializable_v<T>)
			{
				if (buffer.size() == sizeof(T))
				{
					if constexpr (std::is_copy_assignable_v<T>)
					{
						value = *reinterpret_cast<const T*>(buffer.data());
					}
					else
					{
						std::memcpy(&value, buffer.data(), sizeof(T));
					}
				}
			}
		}

		bool deserializeBinary(std::istream& is, size_t fileElemSize) override
		{
			std::vector<uint8_t> buf;
			if (!deserializeToBuffer(is, fileElemSize, buf))
				return false;
			applyBuffer(buf);
			return true;
		}

		T value;
	};
} // namespace WeirdEngine
