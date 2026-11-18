#include <weird-engine.h>

#include "shape-editor/ShapeEditorScene.h"

#ifndef WEIRD_TOOLS_SHAPE_EDITOR_ONLY
#include "cluster-editor/ClusterEditor.h"
#include "MainMenuScene.h"
#include "scene-editor/SceneEditor.h"
#include "ToolsSession.h"
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
	using namespace WeirdEngine;

#ifdef WEIRD_TOOLS_SHAPE_EDITOR_ONLY
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = (argv[i] != nullptr) ? argv[i] : "";
		if ((arg == "--scene" || arg == "-s") && i + 1 < argc)
		{
			++i;
			continue;
		}
		if (!arg.empty() && arg[0] != '-')
		{
			Tools::g_initialShapePath = arg;
			break;
		}
	}

	DisplaySettings displaySettings;
	displaySettings.width = 1280;
	displaySettings.height = 800;
	displaySettings.worldSmoothFactor = 4.0f;
	displaySettings.windowTitle = "Weird Engine - Shape Editor";

	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<Tools::ShapeEditorScene>("shape-editor");

	start(sceneManager, displaySettings, {}, {}, argc, argv);
	return 0;
#else
	auto toLower = [](std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	};

	auto& session = Tools::ToolsSession::getInstance();
	session.loadConfig();

	bool hasSceneFlag = false;
	std::string positionalArg;
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = (argv[i] != nullptr) ? argv[i] : "";
		if (arg == "--scene" || arg == "-s")
		{
			hasSceneFlag = true;
			if (i + 1 < argc)
			{
				++i;
			}
			continue;
		}
		if (positionalArg.empty() && !arg.empty() && arg[0] != '-')
		{
			positionalArg = arg;
		}
	}

	std::string autoTargetScene;
	if (!positionalArg.empty())
	{
		std::filesystem::path p(positionalArg);
		std::string ext = toLower(p.extension().string());
		std::error_code ec;

		if (ext == ".weirdscene")
		{
			session.setSceneLaunchRequest({Tools::ToolLaunchRequest::Mode::OpenFile, positionalArg});
			autoTargetScene = Tools::SCENE_EDITOR;
		}
		else if (ext == ".weirdcluster")
		{
			session.setClusterLaunchRequest({Tools::ToolLaunchRequest::Mode::OpenFile, positionalArg});
			autoTargetScene = Tools::SCENE_CLUSTER_EDITOR;
		}
		else if (ext == ".weirdshape" || ext == ".weirdnode" || ext == ".json")
		{
			session.setShapeLaunchRequest({Tools::ToolLaunchRequest::Mode::OpenFile, positionalArg});
			autoTargetScene = Tools::SCENE_SHAPE_EDITOR;
		}
		else if (std::filesystem::is_directory(p, ec))
		{
			session.setSceneLaunchRequest({Tools::ToolLaunchRequest::Mode::OpenProject, positionalArg});
			autoTargetScene = Tools::SCENE_EDITOR;
		}
	}

	std::vector<char*> effectiveArgv;
	effectiveArgv.reserve(static_cast<size_t>(argc) + 2);
	for (int i = 0; i < argc; ++i)
	{
		effectiveArgv.push_back(argv[i]);
	}

	std::string sceneFlag = "--scene";
	if (!hasSceneFlag && !autoTargetScene.empty())
	{
		effectiveArgv.push_back(sceneFlag.data());
		effectiveArgv.push_back(autoTargetScene.data());
	}

	DisplaySettings displaySettings;
	displaySettings.width = 1280;
	displaySettings.height = 800;
	displaySettings.worldSmoothFactor = 0.5f;
	displaySettings.windowTitle = "Weird Editor";

	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<Tools::MainMenuScene>(Tools::SCENE_MAIN_MENU);
	sceneManager.registerScene<SceneEditor>(Tools::SCENE_EDITOR);
	sceneManager.registerScene<Tools::ClusterEditor>(Tools::SCENE_CLUSTER_EDITOR);
	sceneManager.registerScene<Tools::ShapeEditorScene>(Tools::SCENE_SHAPE_EDITOR);

	int effectiveArgc = static_cast<int>(effectiveArgv.size());
	start(sceneManager, displaySettings, {}, {}, effectiveArgc, effectiveArgv.data());
	return 0;
#endif
}
