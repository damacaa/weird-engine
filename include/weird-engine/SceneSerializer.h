#pragma once

#include <string>

namespace WeirdEngine
{
	class Scene;

	class SceneSerializer
	{
	public:
		// Serialize the full scene state (entities, components, physics constraints,
		// and entity tags) to a .weird file (JSON internally).
		// On failure (e.g. file cannot be opened) an error is printed to stderr and
		// the call returns without throwing.
		static void save(Scene& scene, const std::string& filename);

		// Restore scene state from a previously saved .weird file.
		// Called automatically by Scene::start() when a file path has been set via
		// Scene::setSceneFilePath(). On failure an error is printed to stderr and
		// the call returns without throwing.
		// Tags are restored into the scene's internal tag maps (accessible via
		// Scene::getEntityTag / getEntityByTag) and reflected in the TagMap
		// returned by Scene::loadWeirdFile().
		static void load(Scene& scene, const std::string& path);
	};
} // namespace WeirdEngine
