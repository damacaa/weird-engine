#include "services/EditorFileManager.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "model/NodeGraphSerializer.h"

namespace WeirdEngine::Editor
{
	EditorFileManager::EditorFileManager()
	{
		loadRecentFiles();
	}

	void EditorFileManager::openSaveFileDialog()
	{
		SDL_DialogFileFilter filters[1] = {{"SDF Graph JSON (*.json)", "json"}};
		SDL_ShowSaveFileDialog(onSaveFileCallback, this, nullptr, filters, 1, "custom_shape.json");
	}

	void EditorFileManager::openLoadFileDialog()
	{
		SDL_DialogFileFilter filters[1] = {{"SDF Graph JSON (*.json)", "json"}};
		SDL_ShowOpenFileDialog(onOpenFileCallback, this, nullptr, filters, 1, nullptr, false);
	}

	void SDLCALL EditorFileManager::onOpenFileCallback(void* userdata, const char* const* filelist, int filter)
	{
		auto* self = static_cast<EditorFileManager*>(userdata);
		if (self && filelist && *filelist)
		{
			std::lock_guard<std::mutex> lock(self->m_fileActionMutex);
			self->m_pendingLoadPath = *filelist;
		}
	}

	void SDLCALL EditorFileManager::onSaveFileCallback(void* userdata, const char* const* filelist, int filter)
	{
		auto* self = static_cast<EditorFileManager*>(userdata);
		if (self && filelist && *filelist)
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
			std::cerr << "Failed to open file for writing: " << finalPath << std::endl;
			return false;
		}
		f << NodeGraphSerializer::serialize(graph).dump(2);
		addRecentFile(finalPath);
		return true;
	}

	bool EditorFileManager::loadGraphFromFile(NodeGraph& graph, const std::string& path)
	{
		std::ifstream f(path);
		if (!f.is_open())
		{
			std::cerr << "Failed to open file for reading: " << path << std::endl;
			return false;
		}
		try
		{
			json j;
			f >> j;
			if (NodeGraphSerializer::deserialize(graph, j))
			{
				addRecentFile(path);
				return true;
			}
		}
		catch (const std::exception& e)
		{
			std::cerr << "Failed to parse JSON file " << path << ": " << e.what() << std::endl;
		}
		return false;
	}

	void EditorFileManager::pollFileActions(NodeGraph& graph, bool& outDirty, bool& outSyncPositions)
	{
		std::string loadPath;
		std::string savePath;
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
