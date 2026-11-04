#pragma once

#include <istream>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>

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
				os.write(reinterpret_cast<const char*>(&value), sizeof(T));
			}
			else
			{
				uint32_t elemSize = 0;
				os.write(reinterpret_cast<const char*>(&elemSize), sizeof(elemSize));
			}
		}

		bool deserializeBinary(std::istream& is, size_t fileElemSize) override
		{
			uint32_t elemSize = 0;
			is.read(reinterpret_cast<char*>(&elemSize), sizeof(elemSize));

			if constexpr (is_snapshot_serializable_v<T>)
			{
				if (elemSize != sizeof(T))
				{
					Logger::error("[Snapshot] SceneState size mismatch! Expected " + std::to_string(sizeof(T)) +
								  ", got " + std::to_string(elemSize) + ". Skipping.");
					if (elemSize > 0)
					{
						is.seekg(elemSize, std::ios::cur);
					}
					return false;
				}
				is.read(reinterpret_cast<char*>(&value), sizeof(T));
				return true;
			}
			else
			{
				if (elemSize > 0)
				{
					is.seekg(elemSize, std::ios::cur);
				}
				return false;
			}
		}

		T value;
	};
} // namespace WeirdEngine
