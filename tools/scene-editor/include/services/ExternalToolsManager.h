#pragma once

#include "core/EditorContext.h"
#include <filesystem>
#include <string>

namespace WeirdEngine::Editor
{
	class SceneOperations;

	class ExternalToolsManager
	{
	public:
		ExternalToolsManager(EditorContext& context, SceneOperations& sceneOps)
			: m_ctx(context)
			, m_sceneOps(sceneOps)
		{
		}

		std::filesystem::path getExecutableDirectory() const;
		std::filesystem::path findToolExecutable(const std::string& toolBaseName) const;

		void launchTool(const std::string& toolBaseName, const std::string& filePath = "") const;
		void launchClusterEditor(const std::string& filePath = "") const;
		void launchShapeEditor(const std::string& filePath = "") const;

		std::filesystem::path createNewClusterAsset(std::string name);
		std::filesystem::path createNewShapeAsset(std::string name);

		void pollAssetModifications();

	private:
		EditorContext& m_ctx;
		SceneOperations& m_sceneOps;
	};
} // namespace WeirdEngine::Editor
