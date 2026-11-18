#pragma once

#include <deque>
#include <mutex>
#include <string>

#include "shape-editor/model/NodeGraph.h"
#include <SDL3/SDL_dialog.h>

namespace WeirdEngine::Tools::ShapeEd
{
	class EditorFileManager
	{
	public:
		EditorFileManager();
		~EditorFileManager();

		void openSaveFileDialog();
		void openLoadFileDialog();

		bool saveGraph(const NodeGraph& graph);
		bool saveGraphToFile(const NodeGraph& graph, const std::string& path);
		bool loadGraphFromFile(NodeGraph& graph, const std::string& path, bool addToRecent = true);

		const std::string& getCurrentFilePath() const;
		void setCurrentFilePath(const std::string& path);
		bool hasCurrentFilePath() const;

		void setSuppressRecentForCurrentFile(bool suppress);
		bool shouldSuppressRecentForCurrentFile() const;

		void pollFileActions(NodeGraph& graph, bool& outDirty, bool& outSyncPositions, bool* outDidLoad = nullptr,
							 bool* outDidSave = nullptr, bool* outSaveCancelled = nullptr);

		const std::deque<std::string>& getRecentFiles() const;
		void addRecentFile(const std::string& path);
		void clearRecentFiles();
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
		bool m_pendingSaveCancelled = false;
		std::deque<std::string> m_recentFiles;
		std::string m_currentFilePath;
		bool m_suppressRecentForCurrentFile = false;
#ifdef __EMSCRIPTEN__
		bool m_webLoadRequested = false;
		bool m_webSaveRequested = false;
#endif
	};
} // namespace WeirdEngine::Tools::ShapeEd
