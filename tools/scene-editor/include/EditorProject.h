#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "weird-engine/Logger.h"
#include <json/json.h>
#include <SDL3/SDL_dialog.h>

namespace WeirdEngine::Editor
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
		EditorProject() = default;

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

			std::filesystem::path assets = projectDir / "assets";
			if (!std::filesystem::exists(assets, ec))
			{
				if (projectDir.filename() == "assets")
				{
					assets = projectDir;
				}
				else
				{
					std::filesystem::create_directories(assets, ec);
				}
			}

			m_projectRoot = std::filesystem::canonical(projectDir, ec);
			if (m_projectRoot.empty())
			{
				m_projectRoot = projectDir;
			}

			m_assetsPath = assets;
			Logger::log("[EditorProject] Opened project at " + m_projectRoot.string());

			addRecentProject(m_projectRoot.string());
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

		const std::vector<std::string>& getRecentProjects() const
		{
			return m_recentProjects;
		}

		const std::string& getLastProject() const
		{
			return m_lastProject;
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
			SDL_ShowOpenFolderDialog(onFolderDialogCallback, this, nullptr, nullptr, false);
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
			std::ifstream f("scene_editor_config.json");
			if (f.is_open())
			{
				try
				{
					json j;
					f >> j;
					if (j.contains("recentProjects") && j["recentProjects"].is_array())
					{
						m_recentProjects.clear();
						for (const auto& item : j["recentProjects"])
						{
							m_recentProjects.push_back(item.get<std::string>());
						}
					}
					if (j.contains("lastProject") && j["lastProject"].is_string())
					{
						m_lastProject = j["lastProject"].get<std::string>();
					}
				}
				catch (...)
				{
				}
			}
		}

		void saveConfig()
		{
			json j;
			j["recentProjects"] = m_recentProjects;
			j["lastProject"] = isOpen() ? m_projectRoot.string() : m_lastProject;

			std::ofstream f("scene_editor_config.json");
			if (f.is_open())
			{
				f << j.dump(2);
			}
		}

	private:
		static void SDLCALL onFolderDialogCallback(void* userdata, const char* const* filelist, int /*filter*/)
		{
			auto* self = static_cast<EditorProject*>(userdata);
			if (self && filelist && *filelist)
			{
				std::lock_guard<std::mutex> lock(self->m_dialogMutex);
				self->m_pendingFolder = *filelist;
			}
		}

		void addRecentProject(const std::string& path)
		{
			m_recentProjects.erase(std::remove(m_recentProjects.begin(), m_recentProjects.end(), path),
								   m_recentProjects.end());
			m_recentProjects.insert(m_recentProjects.begin(), path);
			if (m_recentProjects.size() > 8)
			{
				m_recentProjects.resize(8);
			}
			m_lastProject = path;
			saveConfig();
		}

		std::filesystem::path m_projectRoot;
		std::filesystem::path m_assetsPath;
		std::string m_lastProject;
		std::vector<std::string> m_recentProjects;
		std::vector<AssetInfo> m_scenes;
		std::vector<AssetInfo> m_clusters;
		std::vector<AssetInfo> m_shapes;
		std::vector<AssetInfo> m_allAssets;

		std::mutex m_dialogMutex;
		std::string m_pendingFolder;
	};
} // namespace WeirdEngine::Editor
