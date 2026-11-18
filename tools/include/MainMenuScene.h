#pragma once

#include <mutex>
#include <SDL3/SDL_dialog.h>
#include <string>
#include <weird-engine.h>

namespace WeirdEngine::Tools
{
	class MainMenuScene : public Scene2D
	{
	public:
		MainMenuScene() = default;
		~MainMenuScene() override;

	protected:
		void onStart(Registry& registry, ServiceProvider& services) override;
		void onUpdate(Registry& registry, ServiceProvider& services) override;
		void onDestroy(Registry& registry, ServiceProvider& services) override;
		void onCustomUI(Registry& registry, ServiceProvider& services) override;

	private:
		enum class PendingDialogKind
		{
			None,
			ProjectFolder,
			ClusterFile,
			ShapeFile
		};

		struct DialogRequestData
		{
			MainMenuScene* scene = nullptr;
			PendingDialogKind kind = PendingDialogKind::None;
		};

		static void SDLCALL onDialogCallback(void* userdata, const char* const* filelist, int filter);

		void openProjectDialog();
		void openClusterFileDialog();
		void openShapeFileDialog();
		void pollPendingDialogs(ServiceProvider& services);

		void tryOpenProjectFolder(ServiceProvider& services, const std::string& path);
		void renderInitProjectModal(ServiceProvider& services);
		void renderToolsColumn(ServiceProvider& services, float colWidth);
		void renderRecentColumn(ServiceProvider& services, float colWidth);

		std::mutex m_dialogMutex;
		PendingDialogKind m_pendingKind = PendingDialogKind::None;
		std::string m_pendingPath;

		bool m_openInitProjectModal = false;
		std::string m_pendingNewProjectFolder;

		int m_recentFilter = 0; // 0 = All, 1 = Projects, 2 = Clusters, 3 = Shapes
		char m_recentSearchBuf[128] = "";
	};
} // namespace WeirdEngine::Tools
