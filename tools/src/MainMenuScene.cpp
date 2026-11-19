#include "MainMenuScene.h"

#include "ToolsSession.h"
#include "weird-renderer/core/Display.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <string_view>
#include <unordered_set>

namespace WeirdEngine::Tools
{
	namespace
	{
		std::mutex& liveMainMenusMutex()
		{
			static std::mutex m;
			return m;
		}

		std::unordered_set<MainMenuScene*>& liveMainMenus()
		{
			static std::unordered_set<MainMenuScene*> s;
			return s;
		}

		std::string toLowerCopy(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		std::string trimCopy(std::string_view s)
		{
			size_t start = 0;
			while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
			{
				++start;
			}
			size_t end = s.size();
			while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
			{
				--end;
			}
			return std::string(s.substr(start, end - start));
		}

		void replaceAll(std::string& str, std::string_view from, std::string_view to)
		{
			if (from.empty())
			{
				return;
			}
			size_t pos = 0;
			while ((pos = str.find(from, pos)) != std::string::npos)
			{
				str.replace(pos, from.size(), to);
				pos += to.size();
			}
		}

		std::string sanitizeFolderName(const std::string& raw)
		{
			std::string result;
			result.reserve(raw.size());
			for (unsigned char c : trimCopy(raw))
			{
				if (c < 32 || c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' ||
					c == '?' || c == '*')
				{
					result.push_back('_');
				}
				else
				{
					result.push_back(static_cast<char>(c));
				}
			}
			while (!result.empty() && (result.back() == ' ' || result.back() == '.'))
			{
				result.pop_back();
			}
			return result;
		}

		std::string makeCmakeProjectName(const std::string& raw)
		{
			std::string trimmed = trimCopy(raw);
			std::string out;
			out.reserve(trimmed.size());
			bool lastUnderscore = false;
			for (unsigned char c : trimmed)
			{
				if (std::isalnum(c) || c == '-' || c == '_')
				{
					out.push_back(static_cast<char>(c));
					lastUnderscore = (c == '_');
				}
				else if (!lastUnderscore && !out.empty())
				{
					out.push_back('_');
					lastUnderscore = true;
				}
			}
			while (!out.empty() && (out.back() == '_' || out.back() == '-'))
			{
				out.pop_back();
			}
			if (out.empty())
			{
				return "WeirdGame";
			}
			if (!std::isalpha(static_cast<unsigned char>(out.front())) && out.front() != '_')
			{
				out = "Game_" + out;
			}
			return out;
		}

		std::string makeCmakeVarPrefix(const std::string& cmakeName)
		{
			std::string out;
			out.reserve(cmakeName.size());
			for (unsigned char c : cmakeName)
			{
				if (std::isalnum(c))
				{
					out.push_back(static_cast<char>(std::toupper(c)));
				}
				else if (out.empty() || out.back() != '_')
				{
					out.push_back('_');
				}
			}
			while (!out.empty() && out.back() == '_')
			{
				out.pop_back();
			}
			if (out.empty() || std::isdigit(static_cast<unsigned char>(out.front())))
			{
				out = "GAME_" + out;
			}
			return out;
		}

		std::string escapeCmakeString(const std::string& raw)
		{
			std::string out;
			out.reserve(raw.size());
			for (char c : raw)
			{
				if (c == '\\' || c == '"')
				{
					out.push_back('\\');
				}
				out.push_back(c);
			}
			return out;
		}

		std::filesystem::path findEmptyProjectTemplateDir()
		{
			std::vector<std::filesystem::path> candidates;
			std::error_code ec;
			std::filesystem::path cwd = std::filesystem::current_path(ec);
			if (!ec && !cwd.empty())
			{
				candidates.push_back(cwd / "examples" / "empty-project");
				candidates.push_back(cwd.parent_path() / "examples" / "empty-project");
				candidates.push_back(cwd.parent_path().parent_path() / "examples" / "empty-project");
				candidates.push_back(cwd.parent_path().parent_path().parent_path() / "examples" / "empty-project");
			}
#ifdef WEIRD_ENGINE_SOURCE_DIR
			candidates.push_back(std::filesystem::path(WEIRD_ENGINE_SOURCE_DIR) / "examples" / "empty-project");
#endif
			for (const auto& candidate : candidates)
			{
				if (std::filesystem::exists(candidate / "CMakeLists.txt", ec) &&
					std::filesystem::is_regular_file(candidate / "CMakeLists.txt", ec))
				{
					return std::filesystem::weakly_canonical(candidate, ec);
				}
			}
			return {};
		}

		bool isPathSameOrInside(const std::filesystem::path& candidate, const std::filesystem::path& base)
		{
			if (candidate.empty() || base.empty())
			{
				return false;
			}
			std::error_code ec;
			std::string candStr = toLowerCopy(std::filesystem::weakly_canonical(candidate, ec).generic_string());
			std::string baseStr = toLowerCopy(std::filesystem::weakly_canonical(base, ec).generic_string());
			while (candStr.size() > 1 && candStr.back() == '/')
			{
				candStr.pop_back();
			}
			while (baseStr.size() > 1 && baseStr.back() == '/')
			{
				baseStr.pop_back();
			}
			if (candStr == baseStr)
			{
				return true;
			}
			if (candStr.size() > baseStr.size() && candStr.compare(0, baseStr.size(), baseStr) == 0 &&
				candStr[baseStr.size()] == '/')
			{
				return true;
			}
			return false;
		}

		std::string getDefaultNewProjectParentDir(const std::filesystem::path& templateDir)
		{
			std::error_code ec;
			const std::filesystem::path engineRoot =
				templateDir.empty() ? std::filesystem::path() : templateDir.parent_path().parent_path();

			const std::string& activeRoot = ToolsSession::getInstance().getActiveProjectRoot();
			if (!activeRoot.empty())
			{
				std::filesystem::path activePath(activeRoot);
				if (!engineRoot.empty() && !isPathSameOrInside(activePath, engineRoot) &&
					activePath.has_parent_path() && std::filesystem::exists(activePath.parent_path(), ec))
				{
					return ToolsSession::normalizePath(activePath.parent_path().string());
				}
			}

			if (!engineRoot.empty() && engineRoot.has_parent_path() &&
				std::filesystem::exists(engineRoot.parent_path(), ec))
			{
				return ToolsSession::normalizePath(engineRoot.parent_path().string());
			}

			return ToolsSession::getDefaultProjectBrowseDirectory();
		}

		std::string computeEngineLocalPathForCMake(const std::filesystem::path& engineRoot,
												   const std::filesystem::path& destDir)
		{
			std::error_code ec;
			std::filesystem::path absEngine = std::filesystem::weakly_canonical(engineRoot, ec);
			std::filesystem::path absDest = std::filesystem::weakly_canonical(destDir, ec);
			if (absEngine.empty())
			{
				absEngine = engineRoot;
			}
			if (absDest.empty())
			{
				absDest = destDir;
			}

			std::filesystem::path rel = std::filesystem::relative(absEngine, absDest, ec);
			std::string result = (!ec && !rel.empty()) ? rel.generic_string() : absEngine.generic_string();
			if (!result.empty() && result.back() != '/')
			{
				result.push_back('/');
			}
			return result;
		}

		std::string transformTemplateCMakeLists(std::string content, const std::filesystem::path& engineRoot,
												const std::filesystem::path& destDir, const std::string& displayName)
		{
			const std::string cmakeName = makeCmakeProjectName(displayName);
			const std::string varPrefix = makeCmakeVarPrefix(cmakeName);
			const std::string engineLocalPath = computeEngineLocalPathForCMake(engineRoot, destDir);

			replaceAll(content, "project(WeirdGame)", "project(" + cmakeName + ")");
			replaceAll(content, "set(WEIRD_ENGINE_LOCAL_PATH \"../../\")",
					   "set(WEIRD_ENGINE_LOCAL_PATH \"" + escapeCmakeString(engineLocalPath) + "\")");
			replaceAll(content, "WEIRDGAME_", varPrefix + "_");
			replaceAll(content, "set(WEIRD_WEB_TITLE \"WeirdGame\")",
					   "set(WEIRD_WEB_TITLE \"" + escapeCmakeString(displayName) + "\")");
			return content;
		}

		bool createProjectFromTemplate(const std::filesystem::path& templateDir, const std::filesystem::path& destDir,
									   const std::string& displayName, std::string& outError)
		{
			outError.clear();
			std::error_code ec;
			if (templateDir.empty() || !std::filesystem::exists(templateDir / "CMakeLists.txt", ec))
			{
				outError = "Could not locate template at examples/empty-project.";
				return false;
			}
			if (isPathSameOrInside(destDir, templateDir))
			{
				outError = "Destination cannot be inside examples/empty-project.";
				return false;
			}
			if (std::filesystem::exists(destDir, ec))
			{
				outError = "Destination folder already exists.";
				return false;
			}

			if (!std::filesystem::create_directories(destDir, ec) || ec)
			{
				outError = "Failed to create folder: " + ec.message();
				return false;
			}

			auto rollback = [&]()
			{
				std::error_code rmEc;
				std::filesystem::remove_all(destDir, rmEc);
			};

			const std::filesystem::path engineRoot = templateDir.parent_path().parent_path();

			for (auto it = std::filesystem::recursive_directory_iterator(templateDir, ec);
				 it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
			{
				if (ec)
				{
					rollback();
					outError = "Failed to scan empty-project template: " + ec.message();
					return false;
				}

				const std::filesystem::path& srcPath = it->path();
				const std::string nameLower = toLowerCopy(srcPath.filename().string());
				if (it->is_directory(ec) &&
					(nameLower == ".git" || nameLower == "build" || nameLower.rfind("build-", 0) == 0))
				{
					it.disable_recursion_pending();
					continue;
				}

				std::filesystem::path relPath = std::filesystem::relative(srcPath, templateDir, ec);
				if (ec || relPath.empty())
				{
					continue;
				}
				const std::filesystem::path targetPath = destDir / relPath;

				if (it->is_directory(ec))
				{
					std::filesystem::create_directories(targetPath, ec);
					if (ec)
					{
						rollback();
						outError = "Failed to create directory " + targetPath.string() + ": " + ec.message();
						return false;
					}
					continue;
				}

				if (targetPath.has_parent_path())
				{
					std::filesystem::create_directories(targetPath.parent_path(), ec);
				}

				if (relPath.generic_string() == "CMakeLists.txt")
				{
					std::ifstream in(srcPath, std::ios::binary);
					if (!in.is_open())
					{
						rollback();
						outError = "Failed to read template CMakeLists.txt.";
						return false;
					}
					std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
					text = transformTemplateCMakeLists(std::move(text), engineRoot, destDir, displayName);

					std::ofstream out(targetPath, std::ios::binary | std::ios::trunc);
					if (!out.is_open())
					{
						rollback();
						outError = "Failed to write destination CMakeLists.txt.";
						return false;
					}
					out.write(text.data(), static_cast<std::streamsize>(text.size()));
					if (!out.good())
					{
						rollback();
						outError = "Failed to flush destination CMakeLists.txt.";
						return false;
					}
				}
				else if (it->is_regular_file(ec))
				{
					std::filesystem::copy_file(srcPath, targetPath, std::filesystem::copy_options::overwrite_existing,
											   ec);
					if (ec)
					{
						rollback();
						outError = "Failed to copy " + relPath.generic_string() + ": " + ec.message();
						return false;
					}
				}
			}

			// Guarantee expected starter folders exist even if omitted by directory iteration order
			for (const char* sub : {"assets", "include", "src"})
			{
				std::filesystem::create_directories(destDir / sub, ec);
			}

			return true;
		}

		const char* getRecentBadgeText(RecentItemType type)
		{
			switch (type)
			{
				case RecentItemType::Project:
					return "PROJECT";
				case RecentItemType::Scene:
					return "SCENE";
				case RecentItemType::Cluster:
					return "CLUSTER";
				case RecentItemType::Shape:
				default:
					return "SHAPE";
			}
		}

		ImVec4 getRecentBadgeColor(RecentItemType type)
		{
			switch (type)
			{
				case RecentItemType::Project:
					return ImVec4(0.55f, 0.45f, 0.85f, 1.0f);
				case RecentItemType::Scene:
					return ImVec4(0.24f, 0.56f, 0.90f, 1.0f);
				case RecentItemType::Cluster:
					return ImVec4(0.18f, 0.68f, 0.65f, 1.0f);
				case RecentItemType::Shape:
				default:
					return ImVec4(0.90f, 0.55f, 0.18f, 1.0f);
			}
		}
	} // namespace

	MainMenuScene::~MainMenuScene()
	{
		std::lock_guard<std::mutex> lock(liveMainMenusMutex());
		liveMainMenus().erase(this);
	}

	void MainMenuScene::onStart(Registry& /*registry*/, ServiceProvider& services)
	{
		{
			std::lock_guard<std::mutex> lock(liveMainMenusMutex());
			liveMainMenus().insert(this);
		}

		ToolsSession& session = ToolsSession::getInstance();
		session.loadConfig();
		session.clearEditorSessions();

		services.render().resetWorldSmoothFactor();
		services.debug().setDebugInput(false);
		services.debug().setDebugFly(false);

		auto& bg = services.render().getBackground();
		bg.type = BackgroundType::Solid;
		bg.primaryColor = vec4(0.07f, 0.07f, 0.08f, 1.0f);
	}

	void MainMenuScene::onDestroy(Registry& /*registry*/, ServiceProvider& /*services*/)
	{
		std::lock_guard<std::mutex> lock(liveMainMenusMutex());
		liveMainMenus().erase(this);
	}

	void MainMenuScene::onUpdate(Registry& /*registry*/, ServiceProvider& services)
	{
		pollPendingDialogs(services);
	}

	void SDLCALL MainMenuScene::onDialogCallback(void* userdata, const char* const* filelist, int /*filter*/)
	{
		std::unique_ptr<DialogRequestData> req(static_cast<DialogRequestData*>(userdata));
		if (!req || !req->scene || !filelist || !*filelist)
		{
			return;
		}

		std::lock_guard<std::mutex> liveLock(liveMainMenusMutex());
		if (liveMainMenus().count(req->scene) == 0)
		{
			return;
		}

		std::lock_guard<std::mutex> dialogLock(req->scene->m_dialogMutex);
		req->scene->m_pendingKind = req->kind;
		req->scene->m_pendingPath = *filelist;
	}

	void MainMenuScene::openProjectDialog()
	{
		std::string defaultDir = ToolsSession::getDefaultProjectBrowseDirectory();
		auto* req = new DialogRequestData{this, PendingDialogKind::ProjectFolder};
		ToolsSession::showOpenFolderDialog(onDialogCallback, req, defaultDir, "Select Weird Engine Project Folder");
	}

	void MainMenuScene::browseNewProjectParentDialog()
	{
		std::string defaultDir = m_newProjectParentBuf[0] != '\0' ? std::string(m_newProjectParentBuf)
																  : ToolsSession::getDefaultProjectBrowseDirectory();
		auto* req = new DialogRequestData{this, PendingDialogKind::NewProjectParentFolder};
		ToolsSession::showOpenFolderDialog(onDialogCallback, req, defaultDir, "Select Parent Folder for New Project");
	}

	void MainMenuScene::openClusterFileDialog()
	{
		static SDL_DialogFileFilter filters[1] = {{"Weird Cluster (*.weirdcluster)", "weirdcluster"}};
		const std::string& activeProject = ToolsSession::getInstance().getActiveProjectRoot();
		std::string defaultDir;
		if (!activeProject.empty() && std::filesystem::exists(std::filesystem::path(activeProject) / "assets"))
		{
			defaultDir = (std::filesystem::path(activeProject) / "assets").string();
		}
		auto* req = new DialogRequestData{this, PendingDialogKind::ClusterFile};
		SDL_ShowOpenFileDialog(onDialogCallback, req, nullptr, filters, 1,
							   defaultDir.empty() ? nullptr : defaultDir.c_str(), false);
	}

	void MainMenuScene::openShapeFileDialog()
	{
		static SDL_DialogFileFilter filters[1] = {{"Weird Shape (*.weirdshape)", "weirdshape"}};
		const std::string& activeProject = ToolsSession::getInstance().getActiveProjectRoot();
		std::string defaultDir;
		if (!activeProject.empty() && std::filesystem::exists(std::filesystem::path(activeProject) / "assets"))
		{
			defaultDir = (std::filesystem::path(activeProject) / "assets").string();
		}
		auto* req = new DialogRequestData{this, PendingDialogKind::ShapeFile};
		SDL_ShowOpenFileDialog(onDialogCallback, req, nullptr, filters, 1,
							   defaultDir.empty() ? nullptr : defaultDir.c_str(), false);
	}

	void MainMenuScene::pollPendingDialogs(ServiceProvider& services)
	{
		PendingDialogKind kind = PendingDialogKind::None;
		std::string path;
		{
			std::lock_guard<std::mutex> lock(m_dialogMutex);
			if (m_pendingKind != PendingDialogKind::None && !m_pendingPath.empty())
			{
				kind = m_pendingKind;
				path = std::move(m_pendingPath);
				m_pendingKind = PendingDialogKind::None;
				m_pendingPath.clear();
			}
		}

		if (kind == PendingDialogKind::None || path.empty())
		{
			return;
		}

		auto& session = ToolsSession::getInstance();
		switch (kind)
		{
			case PendingDialogKind::ProjectFolder:
				tryOpenProjectFolder(services, path);
				break;
			case PendingDialogKind::NewProjectParentFolder:
			{
				std::string normalized = ToolsSession::normalizePath(path);
				std::snprintf(m_newProjectParentBuf, sizeof(m_newProjectParentBuf), "%s", normalized.c_str());
				m_newProjectError.clear();
				break;
			}
			case PendingDialogKind::ClusterFile:
				session.openClusterInEditor(services, SCENE_MAIN_MENU, path, true);
				break;
			case PendingDialogKind::ShapeFile:
				session.openShapeInEditor(services, SCENE_MAIN_MENU, path, true);
				break;
			case PendingDialogKind::None:
			default:
				break;
		}
	}

	void MainMenuScene::onCustomUI(Registry& /*registry*/, ServiceProvider& services)
	{
		const int winW = WeirdRenderer::Display::width > 0 ? WeirdRenderer::Display::width : 1280;
		const int winH = WeirdRenderer::Display::height > 0 ? WeirdRenderer::Display::height : 800;

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(winW), static_cast<float>(winH)), ImGuiCond_Always);

		const ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
										   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
										   ImGuiWindowFlags_NoSavedSettings;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 22.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.085f, 0.088f, 0.095f, 1.0f));

		if (ImGui::Begin("##WeirdEditorMainMenu", nullptr, rootFlags))
		{
			// Header row
			ImGui::SetWindowFontScale(2.0f);
			ImGui::TextColored(ImVec4(0.95f, 0.96f, 0.98f, 1.0f), "Weird Editor");
			ImGui::SetWindowFontScale(1.0f);

			renderNewProjectModal(services);
			renderInitProjectModal(services);

			ImGui::Spacing();

			const float availW = ImGui::GetContentRegionAvail().x;
			const float colGap = 20.0f;
			const float leftW = std::floor((availW - colGap) * 0.46f);
			const float rightW = availW - leftW - colGap;

			renderToolsColumn(services, leftW);
			ImGui::SameLine(0.0f, colGap);
			renderRecentColumn(services, rightW);
		}
		ImGui::End();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
	}

	void MainMenuScene::tryOpenProjectFolder(ServiceProvider& services, const std::string& path)
	{
		bool hasAssets = false;
		std::string resolved = ToolsSession::resolveProjectFolder(path, &hasAssets);
		if (resolved.empty())
		{
			return;
		}
		if (hasAssets)
		{
			ToolsSession::getInstance().openProjectInSceneEditor(services, SCENE_MAIN_MENU, resolved);
		}
		else
		{
			m_pendingNewProjectFolder = resolved;
			m_openInitProjectModal = true;
		}
	}

	void MainMenuScene::openNewProjectModal()
	{
		const std::filesystem::path templateDir = findEmptyProjectTemplateDir();
		const std::string defaultParent = getDefaultNewProjectParentDir(templateDir);
		std::snprintf(m_newProjectParentBuf, sizeof(m_newProjectParentBuf), "%s", defaultParent.c_str());

		std::string candidateName = "NewProject";
		std::error_code ec;
		if (!defaultParent.empty())
		{
			std::filesystem::path parentPath(defaultParent);
			if (std::filesystem::exists(parentPath / candidateName, ec))
			{
				for (int suffix = 2; suffix < 100; ++suffix)
				{
					std::string numbered = "NewProject" + std::to_string(suffix);
					if (!std::filesystem::exists(parentPath / numbered, ec))
					{
						candidateName = numbered;
						break;
					}
				}
			}
		}
		std::snprintf(m_newProjectNameBuf, sizeof(m_newProjectNameBuf), "%s", candidateName.c_str());

		m_newProjectError.clear();
		m_openNewProjectModal = true;
	}

	void MainMenuScene::renderNewProjectModal(ServiceProvider& services)
	{
		if (m_openNewProjectModal)
		{
			ImGui::OpenPopup("New Project");
			m_openNewProjectModal = false;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("New Project", nullptr,
								   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			const std::filesystem::path templateDir = findEmptyProjectTemplateDir();
			constexpr float modalContentWidth = 500.0f;

			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + modalContentWidth);
			ImGui::TextWrapped("Create a new Weird Engine project based on examples/empty-project.");
			ImGui::Spacing();

			bool submitRequested = false;
			ImGui::Text("Project Name:");
			if (ImGui::IsWindowAppearing())
			{
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(modalContentWidth);
			if (ImGui::InputTextWithHint("##NewProjName", "Project Name (e.g. my-game)", m_newProjectNameBuf,
										 sizeof(m_newProjectNameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				submitRequested = true;
			}

			ImGui::Spacing();
			ImGui::Text("Parent Folder:");
			const float browseW = 92.0f;
			ImGui::SetNextItemWidth(modalContentWidth - browseW - ImGui::GetStyle().ItemSpacing.x);
			if (ImGui::InputText("##NewProjParent", m_newProjectParentBuf, sizeof(m_newProjectParentBuf),
								 ImGuiInputTextFlags_EnterReturnsTrue))
			{
				submitRequested = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Browse...", ImVec2(browseW, 0.0f)))
			{
				browseNewProjectParentDialog();
			}

			const std::string displayName = trimCopy(m_newProjectNameBuf);
			const std::string folderName = sanitizeFolderName(displayName);
			const std::string cmakeName = makeCmakeProjectName(displayName);
			const std::string parentTrimmed = trimCopy(m_newProjectParentBuf);

			std::error_code ec;
			const std::filesystem::path parentPath(parentTrimmed);
			const std::filesystem::path destDir =
				(!parentTrimmed.empty() && !folderName.empty()) ? (parentPath / folderName) : std::filesystem::path();

			std::string validationError;
			if (templateDir.empty())
			{
				validationError = "Template examples/empty-project could not be located.";
			}
			else if (folderName.empty())
			{
				validationError = "Enter a valid project name.";
			}
			else if (parentTrimmed.empty() || !std::filesystem::exists(parentPath, ec) ||
					 !std::filesystem::is_directory(parentPath, ec))
			{
				validationError = "Select an existing parent folder.";
			}
			else if (isPathSameOrInside(destDir, templateDir))
			{
				validationError = "Destination cannot be inside examples/empty-project.";
			}
			else if (std::filesystem::exists(destDir, ec))
			{
				validationError = "Destination folder already exists.";
			}

			ImGui::Spacing();
			if (!destDir.empty())
			{
				ImGui::TextDisabled("Folder: %s", destDir.generic_string().c_str());
			}
			if (!folderName.empty())
			{
				ImGui::TextDisabled("CMake project: %s", cmakeName.c_str());
			}

			if (!validationError.empty())
			{
				ImGui::Spacing();
				ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.40f, 1.0f), "%s", validationError.c_str());
			}
			else if (!m_newProjectError.empty())
			{
				ImGui::Spacing();
				ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.40f, 1.0f), "%s", m_newProjectError.c_str());
			}

			ImGui::PopTextWrapPos();
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			const bool canCreate = validationError.empty();
			if (!canCreate)
			{
				ImGui::BeginDisabled();
			}
			if ((ImGui::Button("Create & Open", ImVec2(140.0f, 0.0f)) || (canCreate && submitRequested)))
			{
				std::string err;
				if (createProjectFromTemplate(templateDir, destDir, displayName, err))
				{
					std::string createdPath = ToolsSession::normalizePath(destDir.string());
					m_newProjectError.clear();
					ImGui::CloseCurrentPopup();
					ToolsSession::getInstance().openProjectInSceneEditor(services, SCENE_MAIN_MENU, createdPath);
				}
				else
				{
					m_newProjectError = std::move(err);
				}
			}
			if (!canCreate)
			{
				ImGui::EndDisabled();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)) ||
				(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Escape)))
			{
				m_newProjectError.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void MainMenuScene::renderInitProjectModal(ServiceProvider& services)
	{
		if (m_openInitProjectModal)
		{
			ImGui::OpenPopup("Initialize Project Folder?");
			m_openInitProjectModal = false;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal("Initialize Project Folder?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextWrapped("The selected folder does not contain an 'assets/' directory:");
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.45f, 0.78f, 1.0f, 1.0f), "%s", m_pendingNewProjectFolder.c_str());
			ImGui::Spacing();
			ImGui::TextWrapped("Create an 'assets/' subfolder there and open the project in Scene Editor?");
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Create & Open", ImVec2(140.0f, 0.0f)))
			{
				std::string target = std::move(m_pendingNewProjectFolder);
				m_pendingNewProjectFolder.clear();
				ImGui::CloseCurrentPopup();
				if (ToolsSession::getInstance().selectActiveProject(target, true))
				{
					ToolsSession::getInstance().openProjectInSceneEditor(services, SCENE_MAIN_MENU, target);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
			{
				m_pendingNewProjectFolder.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void MainMenuScene::renderToolsColumn(ServiceProvider& services, float colWidth)
	{
		auto& session = ToolsSession::getInstance();

		ImGui::BeginChild("##MainMenuToolsColumn", ImVec2(colWidth, 0.0f), false);
		ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.88f, 1.0f), "START");
		ImGui::Spacing();

		auto renderToolCard = [&](const char* childId, const char* title, const char* description, ImVec4 accentColor,
								  const char* primaryLabel, auto&& onPrimary, const char* secondaryLabel = nullptr,
								  auto&& onSecondary = nullptr)
		{
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.125f, 0.130f, 0.140f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.24f, 0.25f, 0.28f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

			if (ImGui::BeginChild(childId, ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY))
			{
				ImGui::TextColored(accentColor, "%s", title);

				ImGui::Spacing();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.77f, 0.80f, 1.0f));
				ImGui::TextWrapped("%s", description);
				ImGui::PopStyleColor();
				ImGui::Spacing();

				ImGui::PushStyleColor(
					ImGuiCol_Button, ImVec4(accentColor.x * 0.45f, accentColor.y * 0.45f, accentColor.z * 0.45f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accentColor.x * 0.65f, accentColor.y * 0.65f,
																	 accentColor.z * 0.65f, 1.0f));
				if (ImGui::Button(primaryLabel, ImVec2(0.0f, 28.0f)))
				{
					onPrimary();
				}
				ImGui::PopStyleColor(2);

				if (secondaryLabel != nullptr)
				{
					ImGui::SameLine();
					if (ImGui::Button(secondaryLabel, ImVec2(0.0f, 28.0f)))
					{
						onSecondary();
					}
				}
			}
			ImGui::EndChild();
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor(2);
			ImGui::Spacing();
		};

		// 1. Project (Scene Editor) Card
		renderToolCard(
			"##CardProject", "Project (Scene Editor)",
			"Create a new project from empty-project or open an existing project folder in the Scene Editor.",
			ImVec4(0.32f, 0.66f, 0.98f, 1.0f), "New Project...", [&]() { openNewProjectModal(); }, "Open Project...",
			[&]() { openProjectDialog(); });

		// 2. Cluster Editor Card
		renderToolCard(
			"##CardClusterEditor", "Cluster Editor",
			"Create or tweak individual rigid-body particle clusters (.weirdcluster) with springs and constraints.",
			ImVec4(0.24f, 0.80f, 0.74f, 1.0f), "New Cluster",
			[&]() { session.openNewClusterInEditor(services, SCENE_MAIN_MENU); }, "Open Cluster...",
			[&]() { openClusterFileDialog(); });

		// 3. Shape Editor Card
		renderToolCard(
			"##CardShapeEditor", "Shape Editor",
			"Create or tweak individual procedural SDF shapes (.weirdshape) with live preview and audio synthesis.",
			ImVec4(0.95f, 0.62f, 0.24f, 1.0f), "New Shape",
			[&]() { session.openNewShapeInEditor(services, SCENE_MAIN_MENU); }, "Open Shape...",
			[&]() { openShapeFileDialog(); });

		ImGui::EndChild();
	}

	void MainMenuScene::renderRecentColumn(ServiceProvider& services, float colWidth)
	{
		auto& session = ToolsSession::getInstance();

		ImGui::BeginChild("##MainMenuRecentColumn", ImVec2(colWidth, 0.0f), false);
		ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.88f, 1.0f), "RECENT");
		ImGui::Spacing();

		// Filter buttons + Clear
		const char* filterNames[] = {"All", "Projects", "Clusters", "Shapes"};
		for (int i = 0; i < 4; ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine();
			}
			const bool active = (m_recentFilter == i);
			if (active)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.50f, 0.80f, 1.0f));
			}
			if (ImGui::Button(filterNames[i]))
			{
				m_recentFilter = i;
			}
			if (active)
			{
				ImGui::PopStyleColor();
			}
		}

		ImGui::SameLine((std::max)(0.0f, ImGui::GetWindowWidth() - 62.0f));
		if (ImGui::Button("Clear", ImVec2(62.0f, 0.0f)))
		{
			if (m_recentFilter == 0)
			{
				session.clearRecent(RecentItemType::Project);
				session.clearRecent(RecentItemType::Cluster);
				session.clearRecent(RecentItemType::Shape);
			}
			else if (m_recentFilter == 1)
				session.clearRecent(RecentItemType::Project);
			else if (m_recentFilter == 2)
				session.clearRecent(RecentItemType::Cluster);
			else if (m_recentFilter == 3)
				session.clearRecent(RecentItemType::Shape);
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##RecentSearch", "Filter recent projects, clusters, and shapes...", m_recentSearchBuf,
								 sizeof(m_recentSearchBuf));
		ImGui::Spacing();

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.115f, 0.120f, 0.130f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.23f, 0.26f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

		if (ImGui::BeginChild("##RecentItemsList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
		{
			const std::string searchLower = toLowerCopy(m_recentSearchBuf);
			const auto recentCopy = session.getRecentItems();

			int visibleCount = 0;
			std::optional<RecentItem> itemToOpen;
			std::optional<RecentItem> itemToRemove;

			for (size_t i = 0; i < recentCopy.size(); ++i)
			{
				const auto& item = recentCopy[i];

				if (item.type == RecentItemType::Scene)
					continue;
				if (m_recentFilter == 1 && item.type != RecentItemType::Project)
					continue;
				if (m_recentFilter == 2 && item.type != RecentItemType::Cluster)
					continue;
				if (m_recentFilter == 3 && item.type != RecentItemType::Shape)
					continue;

				std::filesystem::path p(item.path);
				std::string displayName = p.filename().string();
				if (displayName.empty())
				{
					displayName = item.path;
				}

				if (!searchLower.empty())
				{
					if (toLowerCopy(displayName).find(searchLower) == std::string::npos &&
						toLowerCopy(item.path).find(searchLower) == std::string::npos)
					{
						continue;
					}
				}

				++visibleCount;
				std::error_code ec;
				const bool existsOnDisk = std::filesystem::exists(p, ec);

				ImGui::PushID(static_cast<int>(i));

				const float rowHeight = 42.0f;
				const float removeBtnW = 26.0f;
				const float rowW = std::max(60.0f, ImGui::GetContentRegionAvail().x - removeBtnW - 8.0f);
				const ImVec2 rowStart = ImGui::GetCursorPos();

				if (!existsOnDisk)
				{
					ImGui::BeginDisabled();
				}

				if (ImGui::Selectable("##recent_row", false, ImGuiSelectableFlags_AllowOverlap,
									  ImVec2(rowW, rowHeight)))
				{
					itemToOpen = item;
				}

				if (!existsOnDisk)
				{
					ImGui::EndDisabled();
				}

				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("%s%s", item.path.c_str(), existsOnDisk ? "" : " (Missing on disk)");
				}

				// Render row contents directly in the Selectable rect so long paths clip cleanly
				const ImVec2 rowMin = ImGui::GetItemRectMin();
				const ImVec2 rowMax = ImGui::GetItemRectMax();
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				drawList->PushClipRect(rowMin, rowMax, true);

				char badgeBuf[24];
				std::snprintf(badgeBuf, sizeof(badgeBuf), "[%s]", getRecentBadgeText(item.type));
				const ImVec4 badgeCol = getRecentBadgeColor(item.type);
				drawList->AddText(ImVec2(rowMin.x + 8.0f, rowMin.y + 4.0f), ImGui::ColorConvertFloat4ToU32(badgeCol),
								  badgeBuf);

				const float badgeW = ImGui::CalcTextSize(badgeBuf).x;
				const bool isOpenProject = (item.type == RecentItemType::Project && session.sceneState().hasState &&
											item.path == session.sceneState().projectRoot);
				std::string titleText = displayName;
				if (!existsOnDisk)
				{
					titleText += " (missing)";
				}
				else if (isOpenProject)
				{
					titleText += " (open)";
				}
				const ImU32 titleCol =
					existsOnDisk ? (isOpenProject ? IM_COL32(125, 205, 255, 255) : ImGui::GetColorU32(ImGuiCol_Text))
								 : ImGui::GetColorU32(ImGuiCol_TextDisabled);
				drawList->AddText(ImVec2(rowMin.x + 14.0f + badgeW, rowMin.y + 4.0f), titleCol, titleText.c_str());

				drawList->AddText(ImVec2(rowMin.x + 8.0f, rowMin.y + 22.0f), IM_COL32(140, 145, 158, 255),
								  item.path.c_str());
				drawList->PopClipRect();

				ImGui::SameLine(0.0f, 6.0f);
				ImGui::SetCursorPosY(rowStart.y + 9.0f);
				if (ImGui::Button("x", ImVec2(removeBtnW, 24.0f)))
				{
					itemToRemove = item;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Remove from recent");
				}

				ImGui::SetCursorPos(ImVec2(rowStart.x, rowStart.y + rowHeight));
				ImGui::Dummy(ImVec2(0.0f, 4.0f));
				ImGui::PopID();
			}

			if (visibleCount == 0)
			{
				ImGui::Spacing();
				ImGui::TextDisabled("  No recent items matching filter.");
				ImGui::TextDisabled("  Open a project, cluster, or shape to see it here.");
			}

			if (itemToRemove.has_value())
			{
				session.removeRecentItem(itemToRemove->type, itemToRemove->path);
			}
			else if (itemToOpen.has_value())
			{
				switch (itemToOpen->type)
				{
					case RecentItemType::Project:
						tryOpenProjectFolder(services, itemToOpen->path);
						break;
					case RecentItemType::Cluster:
						session.openClusterInEditor(services, SCENE_MAIN_MENU, itemToOpen->path, true);
						break;
					case RecentItemType::Shape:
						session.openShapeInEditor(services, SCENE_MAIN_MENU, itemToOpen->path, true);
						break;
					case RecentItemType::Scene:
					default:
						break;
				}
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);

		ImGui::EndChild();
	}
} // namespace WeirdEngine::Tools
