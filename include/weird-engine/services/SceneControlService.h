#pragma once

#include <string>
#include <utility>

namespace WeirdEngine
{
	class SceneControlService
	{
	public:
		SceneControlService(bool& isComplete, std::string& nextScene)
			: m_isComplete(isComplete)
			, m_nextScene(nextScene)
		{
		}

		void goToNextScene(std::string next = "")
		{
			m_isComplete = true;
			m_nextScene = std::move(next);
		}

	private:
		bool& m_isComplete;
		std::string& m_nextScene;
	};
} // namespace WeirdEngine
