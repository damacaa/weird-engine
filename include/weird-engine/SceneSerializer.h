#pragma once

#include <cstdint>
#include <string>

#include "weird-engine/ecs/Entity.h"
#include "weird-engine/services/NameService.h"

namespace WeirdEngine
{
	class Scene;

	class SceneSerializer
	{
	public:
		/// Name map type: entity name → entity id.
		using NameMap = ::WeirdEngine::NameMap;

		// Serialize the full scene state (entities, components, physics constraints,
		// and entity names) to a .weird file (JSON internally).
		// On failure (e.g. file cannot be opened) an error is printed to stderr and
		// the call returns without throwing.
		static void save(Scene& scene, const std::string& filename);

		// Restore scene state from a previously saved .weird file.
		// Called automatically by Scene::start() when a file path has been set via
		// Scene::setSceneFilePath(). On failure an error is printed to stderr and
		// the call returns without throwing.
		// If outNames is non-null, loaded names are stored there (using new entity
		// IDs) instead of being added to the scene's internal name maps.  When
		// outNames is nullptr the names are inserted into the scene as before.
		// If restoreCamera is true, camera state from the file is applied to the main camera.
		// If outEntities is non-null, every newly created Entity ID is appended to it.
		static void load(Scene& scene, const std::string& path, NameMap* outNames = nullptr, bool restoreCamera = true,
						 std::vector<Entity>* outEntities = nullptr);
	};
} // namespace WeirdEngine
