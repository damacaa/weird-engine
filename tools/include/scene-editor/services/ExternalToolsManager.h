#pragma once

#include "scene-editor/core/EditorContext.h"
#include <filesystem>
#include <string>

namespace WeirdEngine::Tools::SceneEd
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

		void launchClusterEditor(const std::string& filePath = "") const;
		void launchShapeEditor(const std::string& filePath = "") const;

		std::filesystem::path createNewClusterAsset(std::string name);
		std::filesystem::path createNewShapeAsset(std::string name);

		void pollAssetModifications();

	private:
		EditorContext& m_ctx;
		SceneOperations& m_sceneOps;
	};
} // namespace WeirdEngine::Tools::SceneEd
