#pragma once

#include "weird-engine/services/ServiceProvider.h"
#include "weird-engine/vec.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <json/json.h>
#include <optional>
#include <SDL3/SDL_dialog.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace WeirdEngine::Tools
{
	inline constexpr const char* SCENE_MAIN_MENU = "main-menu";
	inline constexpr const char* SCENE_EDITOR = "scene-editor";
	inline constexpr const char* SCENE_CLUSTER_EDITOR = "cluster-editor";
	inline constexpr const char* SCENE_SHAPE_EDITOR = "shape-editor";

	enum class RecentItemType
	{
		Project,
		Scene,
		Cluster,
		Shape
	};

	struct RecentItem
	{
		RecentItemType type = RecentItemType::Scene;
		std::string path;
	};

	struct ToolLaunchRequest
	{
		enum class Mode
		{
			ResumeOrDefault,
			NewEmpty,
			OpenFile,
			OpenProject
		};

		Mode mode = Mode::ResumeOrDefault;
		std::string path;
		bool addToRecent = true;
	};

	struct SavedEditorGroup
	{
		uint32_t id = 0;
		std::string name;
		std::vector<uint32_t> entityIds;
		std::vector<uint32_t> clusterIds;
	};

	struct SceneEditorSession
	{
		bool hasState = false;
		std::string projectRoot;
		std::string currentScenePath;
		bool isSceneDirty = false;
		vec3 cameraPosition{0.0f, 0.0f, 35.0f};
		bool gridEnabled = true;
		bool gridSnap = false;
		float gridSize = 1.0f;
		float playGravity = -9.8f;
		float playDamping = 0.001f;
		bool selectIndividualClusterEntities = false;
		std::vector<SavedEditorGroup> savedGroups;
		std::unordered_map<std::string, std::filesystem::file_time_type> clusterAssetWriteTimes;
		std::string tempSessionFilePath;
	};

	struct ClusterEditorSession
	{
		bool hasState = false;
		bool hasEntities = false;
		std::string currentFilePath;
		std::string fileNameBuf = "cluster";
		vec3 cameraPosition{0.0f, 0.0f, 35.0f};
		int toolMode = 0;
		int gridMode = 0;
		int selectedMaterial = 1;
		bool gravityEnabled = true;
		float playGravity = -10.0f;
		float playDamping = 0.01f;
		bool suppressRecentForCurrentFile = false;
		bool isDirty = false;
		std::string tempSessionFilePath;
	};

	struct ShapeEditorSession
	{
		bool hasState = false;
		nlohmann::json graphJson;
		std::string currentFilePath;
		bool suppressRecentForCurrentFile = false;
		bool isDirty = false;
		int previewMode = 0;
		float cameraZoom = 200.0f;
		bool audioPlaying = true;
		float audioVolume = 0.75f;
	};

	struct SceneClipboardState
	{
		bool hasData = false;
		bool fromCut = false;
		int pasteCount = 0;
		vec2 anchorWorld{0.0f, 0.0f};
		nlohmann::json data;
	};

	enum class PendingSceneExit
	{
		None,
		MainMenu,
		Quit
	};

	class ToolsSession
	{
	public:
		static ToolsSession& getInstance();

		ToolsSession(const ToolsSession&) = delete;
		ToolsSession& operator=(const ToolsSession&) = delete;

		~ToolsSession();

		static std::string normalizePath(const std::string& path);
		static std::string getSceneDisplayName(const std::string& sceneName);
		static std::string detectProjectRootFromAssetPath(const std::string& assetPath);
		static std::string resolveProjectFolder(const std::string& folderPath, bool* outHasAssetsDir = nullptr);
		static std::string getDefaultProjectBrowseDirectory(const std::string& currentRoot = {});
		static void showOpenFolderDialog(SDL_DialogFileCallback callback, void* userdata,
										 const std::string& defaultLocation = {},
										 const std::string& title = "Select Folder");
		static void requestApplicationQuit();

		// Config & Recent Items
		void loadConfig();
		void saveConfig();

		void addRecentItem(RecentItemType type, const std::string& path);
		void addRecentProject(const std::string& path);
		void addRecentScene(const std::string& path);
		void addRecentCluster(const std::string& path);
		void addRecentShape(const std::string& path);
		void removeRecentItem(RecentItemType type, const std::string& path);
		void clearRecent(std::optional<RecentItemType> filter = std::nullopt);

		const std::vector<RecentItem>& getRecentItems() const;
		std::vector<std::string> getRecentPathsByType(RecentItemType type) const;

		// Active project tracking
		void setActiveProjectRoot(const std::string& path);
		bool selectActiveProject(const std::string& folderPath, bool createAssetsIfMissing = false);
		const std::string& getActiveProjectRoot() const;

		// Navigation
		void navigateTo(ServiceProvider& services, const std::string& fromScene, const std::string& targetScene);
		void navigateToMainMenu(ServiceProvider& services);
		void navigateBack(ServiceProvider& services, const std::string& currentScene);

		void openSceneInEditor(ServiceProvider& services, const std::string& fromScene, const std::string& scenePath);
		void openProjectInSceneEditor(ServiceProvider& services, const std::string& fromScene,
									  const std::string& projectPath);
		void openNewSceneInEditor(ServiceProvider& services, const std::string& fromScene);

		void openClusterInEditor(ServiceProvider& services, const std::string& fromScene,
								 const std::string& clusterPath, bool addToRecent = true);
		void openNewClusterInEditor(ServiceProvider& services, const std::string& fromScene);

		void openShapeInEditor(ServiceProvider& services, const std::string& fromScene, const std::string& shapePath,
							   bool addToRecent = true);
		void openNewShapeInEditor(ServiceProvider& services, const std::string& fromScene);

		const std::string& getReturnScene() const;
		ToolLaunchRequest consumeSceneLaunchRequest();
		ToolLaunchRequest consumeClusterLaunchRequest();
		ToolLaunchRequest consumeShapeLaunchRequest();

		void setSceneLaunchRequest(ToolLaunchRequest request);
		void setClusterLaunchRequest(ToolLaunchRequest request);
		void setShapeLaunchRequest(ToolLaunchRequest request);

		void clearSceneState();
		void clearClusterState();
		void clearShapeState();
		void clearEditorSessions();
		bool shouldPreserveSceneSessionOnExit() const;

		void setPendingReturnExit(PendingSceneExit exit);
		PendingSceneExit consumePendingReturnExit();

		// Shared session state per tool
		SceneEditorSession& sceneState()
		{
			return m_sceneState;
		}
		const SceneEditorSession& sceneState() const
		{
			return m_sceneState;
		}

		ClusterEditorSession& clusterState()
		{
			return m_clusterState;
		}
		const ClusterEditorSession& clusterState() const
		{
			return m_clusterState;
		}

		ShapeEditorSession& shapeState()
		{
			return m_shapeState;
		}
		const ShapeEditorSession& shapeState() const
		{
			return m_shapeState;
		}

		SceneClipboardState& sceneClipboard()
		{
			return m_sceneClipboard;
		}
		const SceneClipboardState& sceneClipboard() const
		{
			return m_sceneClipboard;
		}

		static std::string getSceneTempSessionPath();
		static std::string getClusterTempSessionPath();

		/// Receives `proceed()`: may execute immediately if clean, or defer behind a Save/Discard/Cancel prompt.
		using ConfirmLeaveFn = std::function<void(std::function<void()> proceed)>;

		// Shared ImGui menu helpers for switching between tools
		static void renderFileNavigationMenuItems(ServiceProvider& services, const char* currentScene,
												  const ConfirmLeaveFn& confirmLeave = nullptr);
		static void renderToolsMenuItems(ServiceProvider& services, const char* currentScene,
										 const ConfirmLeaveFn& confirmLeave = nullptr);
		static void renderQuickBackMenuButton(ServiceProvider& services, const char* currentScene,
											  const ConfirmLeaveFn& confirmLeave = nullptr);

	private:
		ToolsSession();

		static constexpr size_t MAX_RECENT_PER_TYPE = 10;

		void importLegacyConfigs();
		void pruneRecentItems(RecentItemType type);

		bool m_configLoaded = false;
		std::string m_activeProjectRoot;
		std::vector<RecentItem> m_recentItems;
		std::string m_returnScene;
		std::string m_pendingTargetScene;
		PendingSceneExit m_pendingReturnExit = PendingSceneExit::None;

		ToolLaunchRequest m_sceneLaunch;
		ToolLaunchRequest m_clusterLaunch;
		ToolLaunchRequest m_shapeLaunch;

		SceneEditorSession m_sceneState;
		ClusterEditorSession m_clusterState;
		ShapeEditorSession m_shapeState;
		SceneClipboardState m_sceneClipboard;
	};
} // namespace WeirdEngine::Tools
