#include "services/ExternalToolsManager.h"
#include "services/SceneOperations.h"

#include "weird-engine.h"
#include "weird-engine/shapes/NodeGraph.h"
#include "weird-engine/shapes/NodeGraphCompiler.h"
#include "weird-engine/shapes/NodeGraphPresets.h"
#include "weird-engine/shapes/NodeGraphSerializer.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#endif

#include <cstdlib>
#include <fstream>
#include <json/json.h>
#include <vector>

namespace WeirdEngine::Editor
{
	std::filesystem::path ExternalToolsManager::getExecutableDirectory() const
	{
#if defined(_WIN32)
		char buffer[MAX_PATH];
		DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
		if (length > 0)
		{
			return std::filesystem::path(buffer).parent_path();
		}
#endif
		return std::filesystem::current_path();
	}

	std::filesystem::path ExternalToolsManager::findToolExecutable(const std::string& toolBaseName) const
	{
		auto exeDir = getExecutableDirectory();
		std::string exeName = toolBaseName;
#if defined(_WIN32)
		if (!exeName.ends_with(".exe"))
		{
			exeName += ".exe";
		}
#endif

		// 1. Same directory as WeirdEditor (when CMAKE_RUNTIME_OUTPUT_DIRECTORY is used)
		std::filesystem::path candidate = exeDir / exeName;
		if (std::filesystem::exists(candidate))
		{
			return std::filesystem::canonical(candidate);
		}

		// 2. Relative paths across build directories
		std::vector<std::filesystem::path> searchPaths = {
			exeDir / ".." / exeName,
			exeDir / ".." / "tools" / "bin" / exeName,
			exeDir / ".." / ".." / "tools" / "bin" / exeName,
			exeDir / ".." / ".." / "tools" / toolBaseName / exeName,
			exeDir / ".." / ".." / "tools" / toolBaseName / "RelWithDebInfo" / exeName,
			exeDir / ".." / ".." / "tools" / toolBaseName / "Debug" / exeName,
			exeDir / ".." / ".." / "tools" / toolBaseName / "Release" / exeName,
			std::filesystem::current_path() / "build" / "tools" / "bin" / exeName,
			std::filesystem::current_path() / "build" / "tools" / "bin" / "RelWithDebInfo" / exeName,
			std::filesystem::current_path() / "build" / "tools" / "bin" / "Debug" / exeName,
		};

		for (const auto& p : searchPaths)
		{
			if (std::filesystem::exists(p))
			{
				return std::filesystem::canonical(p);
			}
		}

		return "";
	}

	void ExternalToolsManager::launchTool(const std::string& toolBaseName, const std::string& filePath) const
	{
		std::filesystem::path exePath = findToolExecutable(toolBaseName);
		if (exePath.empty())
		{
			Logger::error("[SceneEditor] Could not locate executable for: " + toolBaseName);
			return;
		}

		std::string cmd = "\"" + exePath.string() + "\"";
		if (!filePath.empty())
		{
			std::filesystem::path absPath = std::filesystem::absolute(filePath);
			cmd += " \"" + absPath.string() + "\"";
		}

		Logger::log("[SceneEditor] Launching tool: " + cmd);

#if defined(_WIN32)
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		std::vector<char> cmdVec(cmd.begin(), cmd.end());
		cmdVec.push_back('\0');

		std::string workDir = exePath.parent_path().string();

		if (CreateProcessA(NULL, cmdVec.data(), NULL, NULL, FALSE, 0, NULL, workDir.c_str(), &si, &pi))
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		else
		{
			Logger::error("[SceneEditor] Failed to launch process: " + std::to_string(GetLastError()));
		}
#else
		cmd += " &";
		std::system(cmd.c_str());
#endif
	}

	void ExternalToolsManager::launchClusterEditor(const std::string& filePath) const
	{
		launchTool("WeirdClusterEditor", filePath);
	}

	void ExternalToolsManager::launchShapeEditor(const std::string& filePath) const
	{
		launchTool("WeirdShapeEditor", filePath);
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
} // namespace WeirdEngine::Editor
