#pragma once

#include <memory>
#include <utility>

namespace WeirdEngine
{
	// Base for type-erased scene state (see Registry's scene state API).
	class ISceneState
	{
	public:
		virtual ~ISceneState() = default;
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

		T value;
	};
} // namespace WeirdEngine
