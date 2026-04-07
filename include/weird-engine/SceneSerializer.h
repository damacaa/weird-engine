#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace WeirdEngine
{
	class Scene;

	class SceneSerializer
	{
	public:
		/// Tag map type: tag name → entity id.
		using TagMap = std::unordered_map<std::string, std::uint32_t>;

		// Serialize the full scene state (entities, components, physics constraints,
		// and entity tags) to a .weird file (JSON internally).
		// On failure (e.g. file cannot be opened) an error is printed to stderr and
		// the call returns without throwing.
		static void save(Scene& scene, const std::string& filename);

		// Restore scene state from a previously saved .weird file.
		// Called automatically by Scene::start() when a file path has been set via
		// Scene::setSceneFilePath(). On failure an error is printed to stderr and
		// the call returns without throwing.
		// If outTags is non-null, loaded tags are stored there (using new entity
		// IDs) instead of being added to the scene's internal tag maps.  When
		// outTags is nullptr the tags are inserted into the scene as before.
		static void load(Scene& scene, const std::string& path, TagMap* outTags = nullptr);
	};
} // namespace WeirdEngine
