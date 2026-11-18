#include "scene-editor/services/ExternalToolsManager.h"
#include "scene-editor/services/SceneOperations.h"

#include "ToolsSession.h"
#include "weird-engine.h"
#include "weird-engine/shapes/NodeGraph.h"
#include "weird-engine/shapes/NodeGraphCompiler.h"
#include "weird-engine/shapes/NodeGraphPresets.h"
#include "weird-engine/shapes/NodeGraphSerializer.h"

#include <fstream>
#include <json/json.h>

namespace WeirdEngine::Tools::SceneEd
{
	void ExternalToolsManager::launchClusterEditor(const std::string& filePath) const
	{
		if (!m_ctx.services)
		{
			return;
		}
		if (filePath.empty())
		{
			ToolsSession::getInstance().navigateTo(*m_ctx.services, SCENE_EDITOR, SCENE_CLUSTER_EDITOR);
			return;
		}

		std::filesystem::path resolved = m_sceneOps.resolveClusterPath(filePath);
		std::string targetPath = std::filesystem::exists(resolved) ? resolved.string() : filePath;
		ToolsSession::getInstance().openClusterInEditor(*m_ctx.services, SCENE_EDITOR, targetPath, false);
	}

	void ExternalToolsManager::launchShapeEditor(const std::string& filePath) const
	{
		if (!m_ctx.services)
		{
			return;
		}
		if (filePath.empty())
		{
			ToolsSession::getInstance().navigateTo(*m_ctx.services, SCENE_EDITOR, SCENE_SHAPE_EDITOR);
			return;
		}

		std::string resolved = m_ctx.services->resources().resolveAssetPath(filePath);
		if (!std::filesystem::exists(resolved) && m_ctx.project.isOpen())
		{
			std::filesystem::path candidate = m_ctx.project.getAssetsPath() / filePath;
			if (std::filesystem::exists(candidate))
			{
				resolved = candidate.string();
			}
		}
		ToolsSession::getInstance().openShapeInEditor(*m_ctx.services, SCENE_EDITOR,
													  std::filesystem::exists(resolved) ? resolved : filePath, false);
	}

	std::filesystem::path ExternalToolsManager::createNewClusterAsset(std::string name)
	{
		if (name.empty())
		{
			name = "NewCluster";
		}

		if (name.ends_with(".weirdcluster"))
		{
			name = name.substr(0, name.size() - 13);
		}

		std::filesystem::path targetDir = m_ctx.project.isOpen()
											  ? m_ctx.project.getAssetsPath()
											  : std::filesystem::path("examples/sample-scenes/assets");

		std::error_code ec;
		std::filesystem::create_directories(targetDir, ec);

		std::string finalName = name + ".weirdcluster";
		std::filesystem::path finalPath = targetDir / finalName;
		int counter = 1;
		while (std::filesystem::exists(finalPath))
		{
			finalName = name + "_" + std::to_string(counter++) + ".weirdcluster";
			finalPath = targetDir / finalName;
		}

		nlohmann::json j;
		j["version"] = 1;
		j["camera"] = {
			{"position", {0.0f, 0.0f, 25.0f}}, {"rotation", {0.0f, 0.0f, -1.0f}}, {"scale", {1.0f, 1.0f, 1.0f}}};
		j["entities"] = nlohmann::json::array();
		j["physics"] = {{"distanceConstraints", nlohmann::json::array()}, {"springs", nlohmann::json::array()}};

		std::ofstream outFile(finalPath);
		if (outFile.is_open())
		{
			outFile << j.dump(2);
			outFile.close();
			Logger::log("[SceneEditor] Created new cluster asset: " + finalPath.string());
			m_ctx.project.scanAssets();
			launchClusterEditor(finalPath.string());
			return finalPath;
		}
		else
		{
			Logger::error("[SceneEditor] Failed to create cluster asset at: " + finalPath.string());
			return "";
		}
	}

	std::filesystem::path ExternalToolsManager::createNewShapeAsset(std::string name)
	{
		if (name.empty())
		{
			name = "NewShape";
		}

		if (name.ends_with(".weirdshape"))
		{
			name = name.substr(0, name.size() - 11);
		}

		std::filesystem::path targetDir = m_ctx.project.isOpen()
											  ? m_ctx.project.getAssetsPath()
											  : std::filesystem::path("examples/sample-scenes/assets");

		std::error_code ec;
		std::filesystem::create_directories(targetDir, ec);

		std::string finalName = name + ".weirdshape";
		std::filesystem::path finalPath = targetDir / finalName;
		int counter = 1;
		while (std::filesystem::exists(finalPath))
		{
			finalName = name + "_" + std::to_string(counter++) + ".weirdshape";
			finalPath = targetDir / finalName;
		}

		NodeGraph graph;
		NodeGraphPresets::loadCircle(graph);
		nlohmann::json j = NodeGraphSerializer::serialize(graph);

		std::ofstream outFile(finalPath);
		if (outFile.is_open())
		{
			outFile << j.dump(2);
			outFile.close();
			Logger::log("[SceneEditor] Created new shape asset: " + finalPath.string());
			m_ctx.project.scanAssets();
			launchShapeEditor(finalPath.string());
			return finalPath;
		}
		else
		{
			Logger::error("[SceneEditor] Failed to create shape asset at: " + finalPath.string());
			return "";
		}
	}

	void ExternalToolsManager::pollAssetModifications()
	{
		if (!m_ctx.services)
		{
			return;
		}

		// 1. Poll custom shape modifications (.weirdshape)
		std::vector<ShapeId> reloadedIds;
		if (m_ctx.services->shapes().pollCustomShapesChanged(&reloadedIds))
		{
			for (ShapeId id : reloadedIds)
			{
				m_sceneOps.notifyShapeReloaded(id);
			}
		}

		// 2. Poll cluster modifications (.weirdcluster)
		std::unordered_set<std::string> modifiedClusterAssets;
		for (const auto& c : m_ctx.services->clusters().getAll())
		{
			if (c.assetPath.empty())
			{
				continue;
			}
			std::filesystem::path resolved = m_sceneOps.resolveClusterPath(c.assetPath);
			std::error_code ec;
			if (!std::filesystem::exists(resolved, ec) || ec)
			{
				continue;
			}
			auto writeTime = std::filesystem::last_write_time(resolved, ec);
			if (ec)
			{
				continue;
			}
			std::string normPath = ShapeService::normalizePath(resolved.string());
			auto it = m_ctx.clusterAssetWriteTimes.find(normPath);
			if (it == m_ctx.clusterAssetWriteTimes.end())
			{
				m_ctx.clusterAssetWriteTimes[normPath] = writeTime;
			}
			else if (writeTime > it->second)
			{
				it->second = writeTime;
				modifiedClusterAssets.insert(resolved.string());
			}
		}

		for (const auto& assetPath : modifiedClusterAssets)
		{
			m_sceneOps.reloadClustersFromAsset(assetPath);
		}
	}
} // namespace WeirdEngine::Tools::SceneEd
