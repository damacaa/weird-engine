#pragma once

#include <deque>
#include <mutex>
#include <string>

#include "model/NodeGraph.h"
#include <SDL3/SDL_dialog.h>

namespace WeirdEngine::Editor
{
	class EditorFileManager
	{
	public:
		EditorFileManager();

		void openSaveFileDialog();
		void openLoadFileDialog();

		bool saveGraphToFile(const NodeGraph& graph, const std::string& path);
		bool loadGraphFromFile(NodeGraph& graph, const std::string& path);

		void pollFileActions(NodeGraph& graph, bool& outDirty, bool& outSyncPositions);

		const std::deque<std::string>& getRecentFiles() const;
		void addRecentFile(const std::string& path);
		void loadRecentFiles();
		void saveRecentFiles();

	private:
		static void SDLCALL onOpenFileCallback(void* userdata, const char* const* filelist, int filter);
		static void SDLCALL onSaveFileCallback(void* userdata, const char* const* filelist, int filter);

#ifdef __EMSCRIPTEN__
		void requestWebFileLoad();
		void saveGraphToWeb(const NodeGraph& graph);
#endif

		std::mutex m_fileActionMutex;
		std::string m_pendingLoadPath;
		std::string m_pendingSavePath;
		std::deque<std::string> m_recentFiles;
#ifdef __EMSCRIPTEN__
		bool m_webLoadRequested = false;
		bool m_webSaveRequested = false;
#endif
	};
} // namespace WeirdEngine::Editor
