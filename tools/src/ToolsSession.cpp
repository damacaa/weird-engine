// clang-format off
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#endif
// clang-format on

#include "ToolsSession.h"

#include "weird-engine/Logger.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <imgui.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <thread>

namespace WeirdEngine::Tools
{
	namespace
	{
		constexpr const char* TOOLS_CONFIG_FILE = "weird_editor_config.json";
		constexpr const char* LEGACY_SCENE_CONFIG_FILE = "scene_editor_config.json";
		constexpr const char* LEGACY_SHAPE_RECENT_FILE = "sdf_editor_recent.json";

		const char* recentTypeToString(RecentItemType type)
		{
			switch (type)
			{
				case RecentItemType::Project:
					return "project";
				case RecentItemType::Scene:
					return "scene";
				case RecentItemType::Cluster:
					return "cluster";
				case RecentItemType::Shape:
				default:
					return "shape";
			}
		}

		bool recentTypeFromString(const std::string& str, RecentItemType& outType)
		{
			if (str == "project")
			{
				outType = RecentItemType::Project;
				return true;
			}
			if (str == "scene")
			{
				outType = RecentItemType::Scene;
				return true;
			}
			if (str == "cluster")
			{
				outType = RecentItemType::Cluster;
				return true;
			}
			if (str == "shape")
			{
				outType = RecentItemType::Shape;
				return true;
			}
			return false;
		}
	} // namespace

	ToolsSession& ToolsSession::getInstance()
	{
		static ToolsSession instance;
		return instance;
	}

	ToolsSession::ToolsSession() = default;

	ToolsSession::~ToolsSession()
	{
		std::error_code ec;
		if (!m_sceneState.tempSessionFilePath.empty())
		{
			std::filesystem::remove(m_sceneState.tempSessionFilePath, ec);
		}
		if (!m_clusterState.tempSessionFilePath.empty())
		{
			std::filesystem::remove(m_clusterState.tempSessionFilePath, ec);
		}
	}

	std::string ToolsSession::normalizePath(const std::string& path)
	{
		if (path.empty())
		{
			return {};
		}

		std::error_code ec;
		std::filesystem::path p(path);
		if (std::filesystem::exists(p, ec))
		{
			std::filesystem::path canonical = std::filesystem::weakly_canonical(p, ec);
			if (!ec && !canonical.empty())
			{
				return canonical.generic_string();
			}
		}
		return p.lexically_normal().generic_string();
	}

	std::string ToolsSession::getSceneDisplayName(const std::string& sceneName)
	{
		if (sceneName == SCENE_MAIN_MENU)
			return "Main Menu";
		if (sceneName == SCENE_EDITOR)
			return "Scene Editor";
		if (sceneName == SCENE_CLUSTER_EDITOR)
			return "Cluster Editor";
		if (sceneName == SCENE_SHAPE_EDITOR)
			return "Shape Editor";
		return sceneName;
	}

	std::string ToolsSession::detectProjectRootFromAssetPath(const std::string& assetPath)
	{
		if (assetPath.empty())
		{
			return {};
		}

		std::error_code ec;
		std::filesystem::path p = std::filesystem::absolute(assetPath, ec);
		if (ec)
		{
			p = std::filesystem::path(assetPath);
		}

		for (std::filesystem::path cur = p.parent_path(); !cur.empty() && cur != cur.parent_path();
			 cur = cur.parent_path())
		{
			std::string folder = cur.filename().string();
			std::transform(folder.begin(), folder.end(), folder.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (folder == "assets")
			{
				std::filesystem::path projectRoot = cur.parent_path();
				if (!projectRoot.empty() && std::filesystem::exists(projectRoot, ec))
				{
					return normalizePath(projectRoot.string());
				}
			}
		}

		return {};
	}

	std::string ToolsSession::resolveProjectFolder(const std::string& folderPath, bool* outHasAssetsDir)
	{
		if (outHasAssetsDir != nullptr)
		{
			*outHasAssetsDir = false;
		}
		if (folderPath.empty())
		{
			return {};
		}

		std::error_code ec;
		std::filesystem::path dir(folderPath);
		if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
		{
			return {};
		}

		std::string norm = normalizePath(dir.string());
		if (norm.empty())
		{
			return {};
		}

		std::filesystem::path resolved(norm);
		std::string folderName = resolved.filename().string();
		std::transform(folderName.begin(), folderName.end(), folderName.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (folderName == "assets" && resolved.has_parent_path())
		{
			std::filesystem::path parent = resolved.parent_path();
			if (!parent.empty() && std::filesystem::exists(parent, ec) && std::filesystem::is_directory(parent, ec))
			{
				resolved = parent;
				norm = normalizePath(resolved.string());
			}
		}

		if (outHasAssetsDir != nullptr)
		{
			std::filesystem::path assetsDir = std::filesystem::path(norm) / "assets";
			*outHasAssetsDir = std::filesystem::exists(assetsDir, ec) && std::filesystem::is_directory(assetsDir, ec);
		}

		return norm;
	}

	namespace
	{
		SDL_Window* getPrimarySDLWindow()
		{
			if (SDL_Window* focused = SDL_GetKeyboardFocus())
			{
				return focused;
			}
			int count = 0;
			SDL_Window** windows = SDL_GetWindows(&count);
			SDL_Window* primary = (windows != nullptr && count > 0) ? windows[0] : nullptr;
			SDL_free(windows);
			return primary;
		}

#ifdef _WIN32
		std::wstring utf8ToWide(const std::string& utf8)
		{
			if (utf8.empty())
			{
				return {};
			}
			int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
			if (len <= 0)
			{
				return {};
			}
			std::wstring wide(static_cast<size_t>(len), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), len);
			return wide;
		}

		std::string wideToUtf8(const wchar_t* wide)
		{
			if (wide == nullptr || *wide == L'\0')
			{
				return {};
			}
			int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
			if (len <= 1)
			{
				return {};
			}
			std::string utf8(static_cast<size_t>(len - 1), '\0');
			WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), len, nullptr, nullptr);
			return utf8;
		}
#endif
	} // namespace

	std::string ToolsSession::getDefaultProjectBrowseDirectory(const std::string& currentRoot)
	{
		std::error_code ec;
		const std::string& root = !currentRoot.empty() ? currentRoot : getInstance().getActiveProjectRoot();
		if (!root.empty())
		{
			std::filesystem::path p(root);
			if (p.has_parent_path() && std::filesystem::exists(p.parent_path(), ec))
			{
				return p.parent_path().string();
			}
			if (std::filesystem::exists(p, ec))
			{
				return p.string();
			}
		}

		std::filesystem::path cwd = std::filesystem::current_path(ec);
		if (!ec && !cwd.empty())
		{
			return cwd.string();
		}
		return {};
	}

	void ToolsSession::showOpenFolderDialog(SDL_DialogFileCallback callback, void* userdata,
											const std::string& defaultLocation, const std::string& title)
	{
		if (callback == nullptr)
		{
			return;
		}

#ifdef _WIN32
		static std::atomic<bool> s_dialogActive{false};
		if (s_dialogActive.exchange(true))
		{
			const char* canceled[1] = {nullptr};
			callback(userdata, canceled, -1);
			return;
		}

		HWND ownerHwnd = nullptr;
		if (SDL_Window* window = getPrimarySDLWindow())
		{
			ownerHwnd = static_cast<HWND>(
				SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
		}

		std::string preferredDefault = defaultLocation;
		if (!preferredDefault.empty())
		{
			preferredDefault = std::filesystem::path(preferredDefault).make_preferred().string();
		}

		std::thread(
			[callback, userdata, ownerHwnd, preferredDefault, title]()
			{
				const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
				std::string chosenPath;

				if (SUCCEEDED(comHr) || comHr == RPC_E_CHANGED_MODE)
				{
					IFileOpenDialog* dialog = nullptr;
					const HRESULT createHr =
						CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
					if (SUCCEEDED(createHr) && dialog != nullptr)
					{
						DWORD options = 0;
						if (SUCCEEDED(dialog->GetOptions(&options)))
						{
							dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
						}

						if (!title.empty())
						{
							std::wstring wTitle = utf8ToWide(title);
							if (!wTitle.empty())
							{
								dialog->SetTitle(wTitle.c_str());
							}
						}

						if (!preferredDefault.empty())
						{
							std::wstring wDefault = utf8ToWide(preferredDefault);
							if (!wDefault.empty())
							{
								IShellItem* defaultItem = nullptr;
								if (SUCCEEDED(SHCreateItemFromParsingName(wDefault.c_str(), nullptr,
																		  IID_PPV_ARGS(&defaultItem))) &&
									defaultItem != nullptr)
								{
									dialog->SetFolder(defaultItem);
									defaultItem->Release();
								}
							}
						}

						if (SUCCEEDED(dialog->Show(ownerHwnd)))
						{
							IShellItem* resultItem = nullptr;
							if (SUCCEEDED(dialog->GetResult(&resultItem)) && resultItem != nullptr)
							{
								PWSTR filePath = nullptr;
								if (SUCCEEDED(resultItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath)) &&
									filePath != nullptr)
								{
									chosenPath = wideToUtf8(filePath);
									CoTaskMemFree(filePath);
								}
								resultItem->Release();
							}
						}

						dialog->Release();
					}

					if (SUCCEEDED(comHr))
					{
						CoUninitialize();
					}
				}

				s_dialogActive.store(false);

				if (!chosenPath.empty())
				{
					const char* files[2] = {chosenPath.c_str(), nullptr};
					callback(userdata, files, -1);
				}
				else
				{
					const char* canceled[1] = {nullptr};
					callback(userdata, canceled, -1);
				}
			})
			.detach();
#else
		(void)title;
		SDL_ShowOpenFolderDialog(callback, userdata, getPrimarySDLWindow(),
								 defaultLocation.empty() ? nullptr : defaultLocation.c_str(), false);
#endif
	}

	void ToolsSession::requestApplicationQuit()
	{
		SDL_Event quitEvent{};
		quitEvent.type = SDL_EVENT_QUIT;
		SDL_PushEvent(&quitEvent);
	}

	void ToolsSession::loadConfig()
	{
		if (m_configLoaded)
		{
			return;
		}
		m_configLoaded = true;

		std::ifstream f(TOOLS_CONFIG_FILE);
		if (f.is_open())
		{
			try
			{
				nlohmann::json j;
				f >> j;

				if (j.contains("activeProject") && j["activeProject"].is_string())
				{
					std::string loadedProject = resolveProjectFolder(j["activeProject"].get<std::string>());
					m_activeProjectRoot = loadedProject;
				}

				if (j.contains("recentItems") && j["recentItems"].is_array())
				{
					m_recentItems.clear();
					for (const auto& item : j["recentItems"])
					{
						if (!item.is_object() || !item.contains("type") || !item.contains("path"))
							continue;
						if (!item["type"].is_string() || !item["path"].is_string())
							continue;

						RecentItemType type{};
						if (!recentTypeFromString(item["type"].get<std::string>(), type))
							continue;

						std::string p = normalizePath(item["path"].get<std::string>());
						if (p.empty())
							continue;

						auto dup = std::find_if(m_recentItems.begin(), m_recentItems.end(),
												[&](const RecentItem& r) { return r.type == type && r.path == p; });
						if (dup == m_recentItems.end())
						{
							m_recentItems.push_back({type, std::move(p)});
						}
					}
				}
				return;
			}
			catch (const std::exception& e)
			{
				Logger::error(std::string("[ToolsSession] Failed to parse config: ") + e.what());
			}
		}

		importLegacyConfigs();
	}

	void ToolsSession::importLegacyConfigs()
	{
		bool importedAny = false;

		std::ifstream sceneCfg(LEGACY_SCENE_CONFIG_FILE);
		if (sceneCfg.is_open())
		{
			try
			{
				nlohmann::json j;
				sceneCfg >> j;
				if (j.contains("lastProject") && j["lastProject"].is_string())
				{
					m_activeProjectRoot = resolveProjectFolder(j["lastProject"].get<std::string>());
				}
				if (j.contains("recentProjects") && j["recentProjects"].is_array())
				{
					for (const auto& item : j["recentProjects"])
					{
						if (item.is_string())
						{
							std::string p = resolveProjectFolder(item.get<std::string>());
							if (!p.empty())
							{
								m_recentItems.push_back({RecentItemType::Project, p});
								importedAny = true;
							}
						}
					}
				}
			}
			catch (...)
			{
			}
		}

		std::ifstream shapeCfg(LEGACY_SHAPE_RECENT_FILE);
		if (shapeCfg.is_open())
		{
			try
			{
				nlohmann::json j;
				shapeCfg >> j;
				if (j.is_array())
				{
					for (const auto& item : j)
					{
						if (item.is_string())
						{
							std::string p = normalizePath(item.get<std::string>());
							if (!p.empty())
							{
								m_recentItems.push_back({RecentItemType::Shape, p});
								importedAny = true;
							}
						}
					}
				}
			}
			catch (...)
			{
			}
		}

		if (importedAny)
		{
			saveConfig();
		}
	}

	void ToolsSession::saveConfig()
	{
		nlohmann::json j;
		j["activeProject"] = m_activeProjectRoot;

		nlohmann::json recentArr = nlohmann::json::array();
		for (const auto& item : m_recentItems)
		{
			recentArr.push_back({{"type", recentTypeToString(item.type)}, {"path", item.path}});
		}
		j["recentItems"] = recentArr;

		std::ofstream f(TOOLS_CONFIG_FILE);
		if (f.is_open())
		{
			f << j.dump(2);
		}
	}

	void ToolsSession::pruneRecentItems(RecentItemType type)
	{
		size_t count = 0;
		m_recentItems.erase(std::remove_if(m_recentItems.begin(), m_recentItems.end(),
										   [&](const RecentItem& item)
										   {
											   if (item.type != type)
											   {
												   return false;
											   }
											   ++count;
											   return count > MAX_RECENT_PER_TYPE;
										   }),
							m_recentItems.end());
	}

	void ToolsSession::addRecentItem(RecentItemType type, const std::string& path)
	{
		loadConfig();
		std::string norm = normalizePath(path);
		if (norm.empty())
		{
			return;
		}

		m_recentItems.erase(std::remove_if(m_recentItems.begin(), m_recentItems.end(), [&](const RecentItem& item)
										   { return item.type == type && item.path == norm; }),
							m_recentItems.end());
		m_recentItems.insert(m_recentItems.begin(), {type, std::move(norm)});
		pruneRecentItems(type);
		saveConfig();
	}

	void ToolsSession::addRecentProject(const std::string& path)
	{
		addRecentItem(RecentItemType::Project, path);
	}

	void ToolsSession::addRecentScene(const std::string& path)
	{
		addRecentItem(RecentItemType::Scene, path);
	}

	void ToolsSession::addRecentCluster(const std::string& path)
	{
		addRecentItem(RecentItemType::Cluster, path);
	}

	void ToolsSession::addRecentShape(const std::string& path)
	{
		addRecentItem(RecentItemType::Shape, path);
	}

	void ToolsSession::removeRecentItem(RecentItemType type, const std::string& path)
	{
		loadConfig();
		std::string norm = normalizePath(path);
		m_recentItems.erase(std::remove_if(m_recentItems.begin(), m_recentItems.end(), [&](const RecentItem& item)
										   { return item.type == type && (item.path == path || item.path == norm); }),
							m_recentItems.end());
		saveConfig();
	}

	void ToolsSession::clearRecent(std::optional<RecentItemType> filter)
	{
		loadConfig();
		if (!filter.has_value())
		{
			m_recentItems.clear();
		}
		else
		{
			RecentItemType t = *filter;
			m_recentItems.erase(std::remove_if(m_recentItems.begin(), m_recentItems.end(),
											   [t](const RecentItem& item) { return item.type == t; }),
								m_recentItems.end());
		}
		saveConfig();
	}

	const std::vector<RecentItem>& ToolsSession::getRecentItems() const
	{
		return m_recentItems;
	}

	std::vector<std::string> ToolsSession::getRecentPathsByType(RecentItemType type) const
	{
		std::vector<std::string> result;
		for (const auto& item : m_recentItems)
		{
			if (item.type == type)
			{
				result.push_back(item.path);
			}
		}
		return result;
	}

	void ToolsSession::setActiveProjectRoot(const std::string& path)
	{
		loadConfig();
		std::string resolved = resolveProjectFolder(path);
		m_activeProjectRoot = resolved.empty() ? normalizePath(path) : resolved;
		saveConfig();
	}

	bool ToolsSession::selectActiveProject(const std::string& folderPath, bool createAssetsIfMissing)
	{
		loadConfig();
		bool hasAssets = false;
		std::string resolved = resolveProjectFolder(folderPath, &hasAssets);
		if (resolved.empty())
		{
			return false;
		}

		if (!hasAssets)
		{
			if (!createAssetsIfMissing)
			{
				return false;
			}
			std::error_code ec;
			std::filesystem::create_directories(std::filesystem::path(resolved) / "assets", ec);
			if (ec || !std::filesystem::exists(std::filesystem::path(resolved) / "assets", ec))
			{
				return false;
			}
		}

		if (m_sceneState.hasState && normalizePath(m_sceneState.projectRoot) != resolved)
		{
			std::error_code ec;
			if (!m_sceneState.tempSessionFilePath.empty())
			{
				std::filesystem::remove(m_sceneState.tempSessionFilePath, ec);
			}
			m_sceneState = SceneEditorSession{};
		}

		m_activeProjectRoot = resolved;
		addRecentProject(resolved);
		return true;
	}

	const std::string& ToolsSession::getActiveProjectRoot() const
	{
		return m_activeProjectRoot;
	}

	void ToolsSession::navigateTo(ServiceProvider& services, const std::string& fromScene,
								  const std::string& targetScene)
	{
		if (targetScene.empty() || fromScene == targetScene)
		{
			return;
		}
		m_pendingTargetScene = targetScene;
		if (targetScene == SCENE_MAIN_MENU)
		{
			m_returnScene.clear();
		}
		else
		{
			m_returnScene = fromScene;
		}
		services.sceneControl().goToNextScene(targetScene);
	}

	void ToolsSession::navigateToMainMenu(ServiceProvider& services)
	{
		m_pendingTargetScene = SCENE_MAIN_MENU;
		m_returnScene.clear();
		services.sceneControl().goToNextScene(SCENE_MAIN_MENU);
	}

	void ToolsSession::navigateBack(ServiceProvider& services, const std::string& currentScene)
	{
		if (!m_returnScene.empty() && m_returnScene != currentScene && m_returnScene != SCENE_MAIN_MENU)
		{
			std::string target = m_returnScene;
			m_pendingTargetScene = target;
			m_returnScene = SCENE_MAIN_MENU;
			services.sceneControl().goToNextScene(target);
			return;
		}
		navigateToMainMenu(services);
	}

	void ToolsSession::openSceneInEditor(ServiceProvider& services, const std::string& fromScene,
										 const std::string& scenePath)
	{
		m_sceneLaunch = {ToolLaunchRequest::Mode::OpenFile, scenePath};
		navigateTo(services, fromScene, SCENE_EDITOR);
	}

	void ToolsSession::openProjectInSceneEditor(ServiceProvider& services, const std::string& fromScene,
												const std::string& projectPath)
	{
		std::string resolved = resolveProjectFolder(projectPath);
		const std::string& target = resolved.empty() ? projectPath : resolved;
		if (m_sceneState.hasState && !target.empty() &&
			normalizePath(m_sceneState.projectRoot) == normalizePath(target))
		{
			m_sceneLaunch = ToolLaunchRequest{};
			navigateTo(services, fromScene, SCENE_EDITOR);
			return;
		}
		if (m_sceneState.hasState)
		{
			std::error_code ec;
			if (!m_sceneState.tempSessionFilePath.empty())
			{
				std::filesystem::remove(m_sceneState.tempSessionFilePath, ec);
			}
			m_sceneState = SceneEditorSession{};
		}
		m_sceneLaunch = {ToolLaunchRequest::Mode::OpenProject, target};
		navigateTo(services, fromScene, SCENE_EDITOR);
	}

	void ToolsSession::openNewSceneInEditor(ServiceProvider& services, const std::string& fromScene)
	{
		m_sceneLaunch = {ToolLaunchRequest::Mode::NewEmpty, ""};
		navigateTo(services, fromScene, SCENE_EDITOR);
	}

	void ToolsSession::openClusterInEditor(ServiceProvider& services, const std::string& fromScene,
										   const std::string& clusterPath, bool addToRecent)
	{
		m_clusterLaunch = {ToolLaunchRequest::Mode::OpenFile, clusterPath, addToRecent};
		navigateTo(services, fromScene, SCENE_CLUSTER_EDITOR);
	}

	void ToolsSession::openNewClusterInEditor(ServiceProvider& services, const std::string& fromScene)
	{
		m_clusterLaunch = {ToolLaunchRequest::Mode::NewEmpty, ""};
		navigateTo(services, fromScene, SCENE_CLUSTER_EDITOR);
	}

	void ToolsSession::openShapeInEditor(ServiceProvider& services, const std::string& fromScene,
										 const std::string& shapePath, bool addToRecent)
	{
		m_shapeLaunch = {ToolLaunchRequest::Mode::OpenFile, shapePath, addToRecent};
		navigateTo(services, fromScene, SCENE_SHAPE_EDITOR);
	}

	void ToolsSession::openNewShapeInEditor(ServiceProvider& services, const std::string& fromScene)
	{
		m_shapeLaunch = {ToolLaunchRequest::Mode::NewEmpty, ""};
		navigateTo(services, fromScene, SCENE_SHAPE_EDITOR);
	}

	const std::string& ToolsSession::getReturnScene() const
	{
		return m_returnScene;
	}

	ToolLaunchRequest ToolsSession::consumeSceneLaunchRequest()
	{
		ToolLaunchRequest req = std::move(m_sceneLaunch);
		m_sceneLaunch = ToolLaunchRequest{};
		return req;
	}

	ToolLaunchRequest ToolsSession::consumeClusterLaunchRequest()
	{
		ToolLaunchRequest req = std::move(m_clusterLaunch);
		m_clusterLaunch = ToolLaunchRequest{};
		return req;
	}

	ToolLaunchRequest ToolsSession::consumeShapeLaunchRequest()
	{
		ToolLaunchRequest req = std::move(m_shapeLaunch);
		m_shapeLaunch = ToolLaunchRequest{};
		return req;
	}

	void ToolsSession::setSceneLaunchRequest(ToolLaunchRequest request)
	{
		m_sceneLaunch = std::move(request);
	}

	void ToolsSession::setClusterLaunchRequest(ToolLaunchRequest request)
	{
		m_clusterLaunch = std::move(request);
	}

	void ToolsSession::setShapeLaunchRequest(ToolLaunchRequest request)
	{
		m_shapeLaunch = std::move(request);
	}

	void ToolsSession::clearSceneState()
	{
		std::error_code ec;
		if (!m_sceneState.tempSessionFilePath.empty())
		{
			std::filesystem::remove(m_sceneState.tempSessionFilePath, ec);
		}
		std::filesystem::remove(getSceneTempSessionPath(), ec);
		m_sceneState = SceneEditorSession{};
	}

	void ToolsSession::clearClusterState()
	{
		std::error_code ec;
		if (!m_clusterState.tempSessionFilePath.empty())
		{
			std::filesystem::remove(m_clusterState.tempSessionFilePath, ec);
		}
		std::filesystem::remove(getClusterTempSessionPath(), ec);
		m_clusterState = ClusterEditorSession{};
	}

	void ToolsSession::clearShapeState()
	{
		m_shapeState = ShapeEditorSession{};
	}

	void ToolsSession::clearEditorSessions()
	{
		clearSceneState();
		clearClusterState();
		clearShapeState();
		m_pendingTargetScene.clear();
		m_pendingReturnExit = PendingSceneExit::None;
		m_returnScene.clear();
	}

	bool ToolsSession::shouldPreserveSceneSessionOnExit() const
	{
		return m_pendingTargetScene == SCENE_CLUSTER_EDITOR || m_pendingTargetScene == SCENE_SHAPE_EDITOR;
	}

	void ToolsSession::setPendingReturnExit(PendingSceneExit exit)
	{
		m_pendingReturnExit = exit;
	}

	PendingSceneExit ToolsSession::consumePendingReturnExit()
	{
		PendingSceneExit e = m_pendingReturnExit;
		m_pendingReturnExit = PendingSceneExit::None;
		return e;
	}

	std::string ToolsSession::getSceneTempSessionPath()
	{
		std::error_code ec;
		std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
		if (ec || dir.empty())
		{
			return "weirdeditor_scene_session.weirdscene";
		}
		return (dir / "weirdeditor_scene_session.weirdscene").generic_string();
	}

	std::string ToolsSession::getClusterTempSessionPath()
	{
		std::error_code ec;
		std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
		if (ec || dir.empty())
		{
			return "weirdeditor_cluster_session.weirdcluster";
		}
		return (dir / "weirdeditor_cluster_session.weirdcluster").generic_string();
	}

	namespace
	{
		void runConfirmedLeave(const ToolsSession::ConfirmLeaveFn& confirmLeave, std::function<void()> proceed)
		{
			if (confirmLeave)
			{
				confirmLeave(std::move(proceed));
			}
			else if (proceed)
			{
				proceed();
			}
		}

		void leaveToMainMenuOrReturnDirtyParent(ToolsSession& session, ServiceProvider& services,
												const char* currentScene)
		{
			if (std::string(currentScene) != SCENE_EDITOR && session.getReturnScene() == SCENE_EDITOR &&
				session.sceneState().hasState && session.sceneState().isSceneDirty)
			{
				session.setPendingReturnExit(PendingSceneExit::MainMenu);
				session.navigateBack(services, currentScene);
				return;
			}
			session.navigateToMainMenu(services);
		}

		void quitOrReturnDirtyParent(ToolsSession& session, ServiceProvider& services, const char* currentScene)
		{
			if (std::string(currentScene) != SCENE_EDITOR && session.getReturnScene() == SCENE_EDITOR &&
				session.sceneState().hasState && session.sceneState().isSceneDirty)
			{
				session.setPendingReturnExit(PendingSceneExit::Quit);
				session.navigateBack(services, currentScene);
				return;
			}
			ToolsSession::requestApplicationQuit();
		}
	} // namespace

	void ToolsSession::renderFileNavigationMenuItems(ServiceProvider& services, const char* currentScene,
													 const ConfirmLeaveFn& confirmLeave)
	{
		auto& session = getInstance();
		const std::string& returnScene = session.getReturnScene();
		if (!returnScene.empty() && returnScene != currentScene && returnScene != SCENE_MAIN_MENU)
		{
			std::string backLabel = "Back to " + getSceneDisplayName(returnScene);
			if (ImGui::MenuItem(backLabel.c_str()))
			{
				std::string cur(currentScene);
				runConfirmedLeave(confirmLeave, [&services, cur]() { getInstance().navigateBack(services, cur); });
			}
		}

		if (ImGui::MenuItem("Main Menu"))
		{
			std::string cur(currentScene);
			runConfirmedLeave(confirmLeave, [&services, cur]()
							  { leaveToMainMenuOrReturnDirtyParent(getInstance(), services, cur.c_str()); });
		}
		if (ImGui::MenuItem("Quit Application"))
		{
			std::string cur(currentScene);
			runConfirmedLeave(confirmLeave,
							  [&services, cur]() { quitOrReturnDirtyParent(getInstance(), services, cur.c_str()); });
		}
	}

	void ToolsSession::renderToolsMenuItems(ServiceProvider& services, const char* currentScene,
											const ConfirmLeaveFn& confirmLeave)
	{
		auto& session = getInstance();
		const std::string cur(currentScene);
		if (ImGui::MenuItem("Main Menu"))
		{
			runConfirmedLeave(confirmLeave, [&services, cur]()
							  { leaveToMainMenuOrReturnDirtyParent(getInstance(), services, cur.c_str()); });
		}
		ImGui::Separator();

		const bool inScene = cur == SCENE_EDITOR;
		const bool inCluster = cur == SCENE_CLUSTER_EDITOR;
		const bool inShape = cur == SCENE_SHAPE_EDITOR;

		if (ImGui::MenuItem("Scene Editor", nullptr, inScene, !inScene))
		{
			auto proceed = [&services, cur]()
			{
				auto& s = getInstance();
				if (s.getReturnScene() == SCENE_EDITOR && s.sceneState().hasState)
				{
					s.navigateBack(services, cur);
				}
				else
				{
					s.navigateTo(services, cur, SCENE_EDITOR);
				}
			};
			runConfirmedLeave(confirmLeave, proceed);
		}
		if (ImGui::MenuItem("Cluster Editor", nullptr, inCluster, !inCluster))
		{
			auto proceed = [&services, cur]() { getInstance().navigateTo(services, cur, SCENE_CLUSTER_EDITOR); };
			if (inScene)
			{
				proceed();
			}
			else
			{
				runConfirmedLeave(confirmLeave, proceed);
			}
		}
		if (ImGui::MenuItem("Shape Editor", nullptr, inShape, !inShape))
		{
			auto proceed = [&services, cur]() { getInstance().navigateTo(services, cur, SCENE_SHAPE_EDITOR); };
			if (inScene)
			{
				proceed();
			}
			else
			{
				runConfirmedLeave(confirmLeave, proceed);
			}
		}
	}

	void ToolsSession::renderQuickBackMenuButton(ServiceProvider& services, const char* currentScene,
												 const ConfirmLeaveFn& confirmLeave)
	{
		auto& session = getInstance();
		const std::string& returnScene = session.getReturnScene();
		std::string label;
		const std::string cur(currentScene);
		bool returnToEditor = (!returnScene.empty() && returnScene != cur && returnScene != SCENE_MAIN_MENU);
		if (returnToEditor)
		{
			label = "< " + getSceneDisplayName(returnScene);
		}
		else
		{
			label = "< Main Menu";
		}

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.24f, 0.28f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.34f, 0.40f, 1.0f));
		if (ImGui::SmallButton(label.c_str()))
		{
			runConfirmedLeave(confirmLeave,
							  [&services, cur, returnToEditor]()
							  {
								  auto& s = getInstance();
								  if (returnToEditor)
									  s.navigateBack(services, cur);
								  else
									  leaveToMainMenuOrReturnDirtyParent(s, services, cur.c_str());
							  });
		}
		ImGui::PopStyleColor(2);
	}
} // namespace WeirdEngine::Tools
