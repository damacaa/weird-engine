#include "services/EditorFileManager.h"

#include <algorithm>
#include <fstream>

#include "model/NodeGraphSerializer.h"
#include <weird-engine/Logger.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace WeirdEngine::Editor
{
	namespace
	{
#ifdef __EMSCRIPTEN__
		constexpr const char* WEB_UPLOAD_PATH = "/sdf_editor_uploaded_graph.json";
		constexpr const char* WEB_DOWNLOAD_FILE = "custom_shape.json";
#endif
	} // namespace

	EditorFileManager::EditorFileManager()
	{
		loadRecentFiles();
	}

	void EditorFileManager::openSaveFileDialog()
	{
#ifdef __EMSCRIPTEN__
		m_webSaveRequested = true;
#else
		SDL_DialogFileFilter filters[1] = {{"SDF Graph JSON (*.json)", "json"}};
		SDL_ShowSaveFileDialog(onSaveFileCallback, this, nullptr, filters, 1, "custom_shape.json");
#endif
	}

	void EditorFileManager::openLoadFileDialog()
	{
#ifdef __EMSCRIPTEN__
		m_webLoadRequested = true;
#else
		SDL_DialogFileFilter filters[1] = {{"SDF Graph JSON (*.json)", "json"}};
		SDL_ShowOpenFileDialog(onOpenFileCallback, this, nullptr, filters, 1, nullptr, false);
#endif
	}

	void SDLCALL EditorFileManager::onOpenFileCallback(void* userdata, const char* const* filelist, int filter)
	{
		auto* self = static_cast<EditorFileManager*>(userdata);
		if (!filelist)
		{
			Logger::error(std::string("Open file dialog failed: ") + SDL_GetError());
			return;
		}
		if (self && *filelist)
		{
			std::lock_guard<std::mutex> lock(self->m_fileActionMutex);
			self->m_pendingLoadPath = *filelist;
		}
	}

	void SDLCALL EditorFileManager::onSaveFileCallback(void* userdata, const char* const* filelist, int filter)
	{
		auto* self = static_cast<EditorFileManager*>(userdata);
		if (!filelist)
		{
			Logger::error(std::string("Save file dialog failed: ") + SDL_GetError());
			return;
		}
		if (self && *filelist)
		{
			std::lock_guard<std::mutex> lock(self->m_fileActionMutex);
			self->m_pendingSavePath = *filelist;
		}
	}

	bool EditorFileManager::saveGraphToFile(const NodeGraph& graph, const std::string& path)
	{
		std::string finalPath = path;
		if (finalPath.find('.') == std::string::npos)
		{
			finalPath += ".json";
		}
		std::ofstream f(finalPath);
		if (!f.is_open())
		{
			Logger::error("Failed to open file for writing: " + finalPath);
			return false;
		}
		f << NodeGraphSerializer::serialize(graph).dump(2);
		addRecentFile(finalPath);
		Logger::log("Saved graph to " + finalPath);
		return true;
	}

	bool EditorFileManager::loadGraphFromFile(NodeGraph& graph, const std::string& path)
	{
		std::ifstream f(path);
		if (!f.is_open())
		{
			Logger::error("Failed to open file for reading: " + path);
			return false;
		}
		try
		{
			json j;
			f >> j;
			if (NodeGraphSerializer::deserialize(graph, j))
			{
				addRecentFile(path);
				Logger::log("Loaded graph from " + path);
				return true;
			}
			Logger::error("Failed to deserialize graph from " + path);
		}
		catch (const std::exception& e)
		{
			Logger::error("Failed to parse JSON file " + path + ": " + e.what());
		}
		return false;
	}

#ifdef __EMSCRIPTEN__
	void EditorFileManager::requestWebFileLoad()
	{
		EM_ASM(
			{
				var input = document.createElement('input');
				input.type = 'file';
				input.accept = '.json,application/json';
				input.style.display = 'none';
				document.body.appendChild(input);
				input.addEventListener(
					'change', function() {
						var file = input.files && input.files[0];
						if (!file)
						{
							document.body.removeChild(input);
							return;
						}
						var reader = new FileReader();
						reader.onload = function()
						{
							try
							{
								FS.writeFile(UTF8ToString($0), reader.result);
								Module.__sdfGraphPicked = true;
							}
							catch (error)
							{
								console.error('SDF editor: failed to stage picked graph: ' + error);
							}
							document.body.removeChild(input);
						};
						reader.onerror = function()
						{
							console.error('SDF editor: failed to read picked graph file');
							document.body.removeChild(input);
						};
						reader.readAsText(file);
					});
				input.click();
			},
			WEB_UPLOAD_PATH);
	}

	void EditorFileManager::saveGraphToWeb(const NodeGraph& graph)
	{
		std::string jsonText = NodeGraphSerializer::serialize(graph).dump(2);
		EM_ASM(
			{
				var blob = new Blob([UTF8ToString($0)],
									{
										type:
											'application/json'
									});
				var url = URL.createObjectURL(blob);
				var link = document.createElement('a');
				link.href = url;
				link.download = UTF8ToString($1);
				document.body.appendChild(link);
				link.click();
				document.body.removeChild(link);
				URL.revokeObjectURL(url);
			},
			jsonText.c_str(), WEB_DOWNLOAD_FILE);
		Logger::log(std::string("Graph downloaded as ") + WEB_DOWNLOAD_FILE);
	}
#endif

	void EditorFileManager::pollFileActions(NodeGraph& graph, bool& outDirty, bool& outSyncPositions)
	{
		std::string loadPath;
		std::string savePath;
#ifdef __EMSCRIPTEN__
		if (m_webSaveRequested)
		{
			m_webSaveRequested = false;
			saveGraphToWeb(graph);
		}
		if (m_webLoadRequested)
		{
			m_webLoadRequested = false;
			requestWebFileLoad();
		}
		int webFilePicked = EM_ASM_INT({
			if (Module.__sdfGraphPicked)
			{
				Module.__sdfGraphPicked = false;
				return 1;
			}
			return 0;
		});
		if (webFilePicked)
		{
			loadPath = WEB_UPLOAD_PATH;
		}
#endif
		{
			std::lock_guard<std::mutex> lock(m_fileActionMutex);
			if (!m_pendingLoadPath.empty())
			{
				loadPath = m_pendingLoadPath;
				m_pendingLoadPath.clear();
			}
			if (!m_pendingSavePath.empty())
			{
				savePath = m_pendingSavePath;
				m_pendingSavePath.clear();
			}
		}
		if (!loadPath.empty())
		{
			if (loadGraphFromFile(graph, loadPath))
			{
				outDirty = true;
				outSyncPositions = true;
			}
		}
		if (!savePath.empty())
		{
			saveGraphToFile(graph, savePath);
		}
	}

	const std::deque<std::string>& EditorFileManager::getRecentFiles() const
	{
		return m_recentFiles;
	}

	void EditorFileManager::addRecentFile(const std::string& path)
	{
		auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), path);
		if (it != m_recentFiles.end())
		{
			m_recentFiles.erase(it);
		}
		m_recentFiles.push_front(path);
		while (m_recentFiles.size() > 10)
		{
			m_recentFiles.pop_back();
		}
		saveRecentFiles();
	}

	void EditorFileManager::loadRecentFiles()
	{
		m_recentFiles.clear();
		std::ifstream f("sdf_editor_recent.json");
		if (f.is_open())
		{
			try
			{
				json j;
				f >> j;
				if (j.is_array())
				{
					for (const auto& item : j)
					{
						if (item.is_string())
						{
							m_recentFiles.push_back(item.get<std::string>());
						}
					}
				}
			}
			catch (...)
			{
			}
		}
	}

	void EditorFileManager::saveRecentFiles()
	{
		std::ofstream f("sdf_editor_recent.json");
		if (f.is_open())
		{
			json j = json::array();
			for (const auto& p : m_recentFiles)
			{
				j.push_back(p);
			}
			f << j.dump(2);
		}
	}
} // namespace WeirdEngine::Editor
