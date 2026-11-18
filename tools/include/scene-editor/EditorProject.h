#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "ToolsSession.h"
#include "weird-engine/Logger.h"
#include <json/json.h>
#include <SDL3/SDL_dialog.h>
#include <unordered_set>

namespace WeirdEngine::Tools::SceneEd
{
	using json = nlohmann::json;

	struct AssetInfo
	{
		enum class Type
		{
			Scene,
			Cluster,
			CustomShape,
			Texture,
			Audio,
			Other
		};

		std::filesystem::path fullPath;
		std::string relativePath;
		std::string fileName;
		std::string stem;
		Type type = Type::Other;
		uintmax_t fileSize = 0;
	};

	class EditorProject
	{
	public:
		EditorProject()
		{
			std::lock_guard<std::mutex> lock(liveProjectsMutex());
			liveProjects().insert(this);
		}

		~EditorProject()
		{
			std::lock_guard<std::mutex> lock(liveProjectsMutex());
			liveProjects().erase(this);
		}

		bool isOpen() const
		{
			return !m_projectRoot.empty() && std::filesystem::exists(m_assetsPath);
		}

		const std::filesystem::path& getRoot() const
		{
			return m_projectRoot;
		}

		const std::filesystem::path& getAssetsPath() const
		{
			return m_assetsPath;
		}

		bool open(const std::filesystem::path& projectDir)
		{
			std::error_code ec;
			if (!std::filesystem::exists(projectDir, ec) || !std::filesystem::is_directory(projectDir, ec))
			{
				Logger::error("[EditorProject] Directory does not exist: " + projectDir.string());
				return false;
			}

			bool hasAssets = false;
			std::string resolvedRoot = ToolsSession::resolveProjectFolder(projectDir.string(), &hasAssets);
			std::filesystem::path rootPath = resolvedRoot.empty() ? projectDir : std::filesystem::path(resolvedRoot);
			std::filesystem::path assets = rootPath / "assets";
			if (!hasAssets)
			{
				std::filesystem::create_directories(assets, ec);
			}

			std::string canon = ToolsSession::normalizePath(rootPath.string());
			m_projectRoot = canon.empty() ? rootPath : std::filesystem::path(canon);
			m_assetsPath = m_projectRoot / "assets";
			Logger::log("[EditorProject] Opened project at " + m_projectRoot.string());

			ToolsSession::getInstance().setActiveProjectRoot(m_projectRoot.string());
			ToolsSession::getInstance().addRecentProject(m_projectRoot.string());
			scanAssets();
			return true;
		}

		void close()
		{
			m_projectRoot.clear();
			m_assetsPath.clear();
			m_scenes.clear();
			m_clusters.clear();
			m_shapes.clear();
			m_allAssets.clear();
		}

		void scanAssets()
		{
			m_scenes.clear();
			m_clusters.clear();
			m_shapes.clear();
			m_allAssets.clear();

			if (!isOpen())
			{
				return;
			}

			std::error_code ec;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(m_assetsPath, ec))
			{
				if (!entry.is_regular_file(ec))
				{
					continue;
				}

				AssetInfo info;
				info.fullPath = entry.path();
				info.relativePath = std::filesystem::relative(entry.path(), m_assetsPath, ec).generic_string();
				info.fileName = entry.path().filename().string();
				info.stem = entry.path().stem().string();
				info.fileSize = entry.file_size(ec);

				std::string ext = entry.path().extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

				if (ext == ".weirdscene")
				{
					info.type = AssetInfo::Type::Scene;
					m_scenes.push_back(info);
				}
				else if (ext == ".weirdcluster")
				{
					info.type = AssetInfo::Type::Cluster;
					m_clusters.push_back(info);
				}
				else if (ext == ".weirdshape")
				{
					info.type = AssetInfo::Type::CustomShape;
					m_shapes.push_back(info);
				}
				else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
				{
					info.type = AssetInfo::Type::Texture;
				}
				else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
				{
					info.type = AssetInfo::Type::Audio;
				}

				m_allAssets.push_back(info);
			}

			Logger::log("[EditorProject] Scanned: " + std::to_string(m_scenes.size()) + " scenes, " +
						std::to_string(m_clusters.size()) + " clusters, " + std::to_string(m_shapes.size()) +
						" shapes.");
		}

		const std::vector<AssetInfo>& getScenes() const
		{
			return m_scenes;
		}

		const std::vector<AssetInfo>& getClusters() const
		{
			return m_clusters;
		}

		const std::vector<AssetInfo>& getCustomShapes() const
		{
			return m_shapes;
		}

		const std::vector<AssetInfo>& getAllAssets() const
		{
			return m_allAssets;
		}

		std::vector<std::string> getRecentProjects() const
		{
			return ToolsSession::getInstance().getRecentPathsByType(RecentItemType::Project);
		}

		const std::string& getLastProject() const
		{
			return ToolsSession::getInstance().getActiveProjectRoot();
		}

		std::string getRelativeAssetPath(const std::string& fullOrRelPath) const
		{
			std::error_code ec;
			std::filesystem::path p(fullOrRelPath);
			if (p.is_absolute())
			{
				return std::filesystem::relative(p, m_assetsPath, ec).generic_string();
			}
			return fullOrRelPath;
		}

		std::string getFullAssetPath(const std::string& relativePath) const
		{
			std::filesystem::path p(relativePath);
			if (p.is_absolute())
			{
				return relativePath;
			}
			return (m_assetsPath / p).generic_string();
		}

		void openFolderDialog()
		{
			std::string defaultLoc =
				ToolsSession::getDefaultProjectBrowseDirectory(isOpen() ? m_projectRoot.string() : std::string());
			ToolsSession::showOpenFolderDialog(onFolderDialogCallback, this, defaultLoc,
											   "Select Weird Engine Project Folder");
		}

		void pollDialogs()
		{
			std::string folder;
			{
				std::lock_guard<std::mutex> lock(m_dialogMutex);
				folder = std::move(m_pendingFolder);
				m_pendingFolder.clear();
			}
			if (!folder.empty())
			{
				open(folder);
			}
		}

		void loadConfig()
		{
			ToolsSession::getInstance().loadConfig();
		}

		void saveConfig()
		{
			ToolsSession::getInstance().saveConfig();
		}

	private:
		static void SDLCALL onFolderDialogCallback(void* userdata, const char* const* filelist, int /*filter*/)
		{
			auto* self = static_cast<EditorProject*>(userdata);
			if (!self || !filelist || !*filelist)
			{
				return;
			}
			std::lock_guard<std::mutex> liveLock(liveProjectsMutex());
			if (liveProjects().count(self) > 0)
			{
				std::lock_guard<std::mutex> lock(self->m_dialogMutex);
				self->m_pendingFolder = *filelist;
			}
		}

		static std::mutex& liveProjectsMutex()
		{
			static std::mutex m;
			return m;
		}

		static std::unordered_set<EditorProject*>& liveProjects()
		{
			static std::unordered_set<EditorProject*> s;
			return s;
		}

		std::filesystem::path m_projectRoot;
		std::filesystem::path m_assetsPath;
		std::vector<AssetInfo> m_scenes;
		std::vector<AssetInfo> m_clusters;
		std::vector<AssetInfo> m_shapes;
		std::vector<AssetInfo> m_allAssets;

		std::mutex m_dialogMutex;
		std::string m_pendingFolder;
	};
} // namespace WeirdEngine::Tools::SceneEd
