#include "TestHarness.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-engine/services/ShapeService.h"
#include "weird-physics/components/Spring.h"
#include "weird-renderer/components/SDFRenderer.h"
#include "weird-renderer/core/Display.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace WeirdEngine;
using WeirdTests::FloorPlaneSdf;
using WeirdTests::InspectableScene;
using WeirdTests::require;
using WeirdTests::SnapshotFile;

namespace
{
	void testResourceAndShapeAssetPathResolution()
	{
		ResourceManager resourceMgr;
		ResourceService resources(resourceMgr, ASSETS_PATH);

		const std::string sceneCtx = std::string(ASSETS_PATH) + "aquarium.weirdscene";
		const std::string resolvedRel = resources.resolveAssetPath("clam.weirdshape");
		const std::string resolvedSibling = resources.resolveAssetPath("clam.weirdshape", sceneCtx);
		const std::string resolvedWinLegacy =
			resources.resolveAssetPath("D:\\Projects\\weird-engine\\examples\\sample-scenes\\assets\\Fan.weirdshape");

		require(std::filesystem::exists(resolvedRel) && resolvedRel == resolvedSibling,
				"Relative shape asset paths must resolve cleanly against asset root and scene directory.");
		require(std::filesystem::exists(resolvedWinLegacy) &&
					std::filesystem::path(resolvedWinLegacy).filename() == "Fan.weirdshape",
				"Legacy cross-machine asset paths must resolve via assets/ suffix fallback.");
		require(resources.makePortableAssetPath(resolvedRel, sceneCtx) == "clam.weirdshape",
				"Portable asset path helper must strip local asset base directory.");

		Registry reg;
		PhysicsSettings settings{};
		Simulation2D sim(64, settings);
		std::vector<std::shared_ptr<IMathExpression>> sdfs{std::make_shared<FloorPlaneSdf>()};
		ShapeService shapes(reg, sim, sdfs, &resources);
		ShapeId clamId = shapes.loadCustomShape("clam.weirdshape");
		require(clamId != 0 && shapes.reloadCustomShape("clam.weirdshape"),
				"ShapeService must load and reload custom shapes from relative asset paths.");
		require(shapes.getCustomShapeData("clam.weirdshape") != nullptr,
				"ShapeService cache lookup must match relative and resolved paths consistently.");
	}

	void testPlatformerWorldClusterFixedBody(bool threaded)
	{
		auto& manager = SceneManager::getInstance();
		PhysicsSettings settings;
		settings.runSimulationInThread = threaded;
		manager.setPhysicsSettings(settings);

		const std::string name = std::string("test-platformer-") + (threaded ? "threaded" : "single");

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<Scene>>(name, ASSETS_PATH "platformer_world.weirdscene");
		manager.loadScene(name);

		auto& scene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
		auto& registry = *scene.registry;
		auto& services = *scene.services;

		ClusterInstance* ameba = services.clusters().getByName("ameba_1");
		ClusterInstance* man = services.clusters().getByName("man_2");
		require(ameba != nullptr && man != nullptr,
				"platformer_world.weirdscene must load clusters ameba_1 and man_2.");

		const uint32_t amebaId = ameba->id;
		const uint32_t manId = man->id;

		auto countFixedInCluster = [&](const ClusterInstance& cluster, Entity& outSampleEntity, vec3& outSamplePos)
		{
			size_t count = 0;
			outSampleEntity = INVALID_ENTITY;
			for (Entity e : cluster.entities)
			{
				if (!registry.hasComponent<RigidBody2D>(e))
					continue;
				const auto& rb = registry.getComponent<RigidBody2D>(e);
				if (rb.type == BodyType::Fixed || rb.isFixed)
				{
					++count;
					outSampleEntity = e;
					outSamplePos = registry.getComponent<Transform>(e).position;
				}
			}
			return count;
		};

		Entity fixedAmebaEnt = INVALID_ENTITY;
		vec3 amebaFixedPosBefore(0.0f);
		require(countFixedInCluster(*ameba, fixedAmebaEnt, amebaFixedPosBefore) == 1,
				"ameba_1 in platformer_world.weirdscene must start with 1 Fixed anchor.");

		Entity fixedManSample = INVALID_ENTITY;
		vec3 manFixedSamplePos(0.0f);
		require(countFixedInCluster(*man, fixedManSample, manFixedSamplePos) == 6,
				"man_2 in platformer_world.weirdscene must start with 6 Fixed vertices.");

		// Reload both clusters directly from their .weirdcluster assets (whose bodies are all Dynamic):
		// scene overrides must survive cluster reloads even when template entity ordering differs.
		require(services.clusters().reloadInstance(amebaId, ASSETS_PATH "Organisms/ameba.weirdcluster"),
				"ameba_1 must reload from ameba.weirdcluster.");
		require(services.clusters().reloadInstance(manId, ASSETS_PATH "Organisms/man.weirdcluster"),
				"man_2 must reload from man.weirdcluster.");

		ameba = services.clusters().getById(amebaId);
		man = services.clusters().getById(manId);
		require(ameba != nullptr && man != nullptr, "Reloaded clusters must remain registered.");

		Entity fixedAmebaAfter = INVALID_ENTITY;
		vec3 amebaFixedPosAfter(0.0f);
		require(countFixedInCluster(*ameba, fixedAmebaAfter, amebaFixedPosAfter) == 1,
				"ameba_1 must keep its Fixed anchor after reloading from ameba.weirdcluster.");
		require(glm::distance(vec2(amebaFixedPosAfter), vec2(amebaFixedPosBefore)) < 0.1f,
				"ameba_1 Fixed anchor must map back to the same corner vertex after scrambled reload.");
		require(ameba->hasOverrideMetadata &&
					(ameba->getOverrides(fixedAmebaAfter) & ClusterOverrideFlag::BodyType) != 0,
				"ameba_1 reload must promote inferred Fixed override into explicit override metadata.");

		Entity fixedManAfter = INVALID_ENTITY;
		require(countFixedInCluster(*man, fixedManAfter, manFixedSamplePos) == 6,
				"man_2 must keep all 6 Fixed vertices after reloading from man.weirdcluster.");
		const Entity manHeadAfter = man->getEntity("head");
		require(manHeadAfter != INVALID_ENTITY && registry.hasComponent<RigidBody2D>(manHeadAfter) &&
					registry.getComponent<RigidBody2D>(manHeadAfter).type == BodyType::Fixed,
				"man_2 local-named 'head' must remain BodyType::Fixed after cluster reload.");

		// Second reload + snapshot roundtrip must preserve the Fixed anchor idempotently.
		require(services.clusters().reloadInstance(amebaId, ASSETS_PATH "Organisms/ameba.weirdcluster"),
				"ameba_1 second reload must succeed.");
		ameba = services.clusters().getById(amebaId);
		require(countFixedInCluster(*ameba, fixedAmebaAfter, amebaFixedPosAfter) == 1,
				"ameba_1 must retain Fixed anchor across repeated reloads.");

		SnapshotFile saved;
		services.serialization().saveSnapshot(saved.path.string());
		require(std::filesystem::exists(saved.path), "platformer_world snapshot must save.");

		auto& mutRb = registry.getComponent<RigidBody2D>(fixedAmebaAfter);
		mutRb.type = BodyType::Dynamic;
		mutRb.isFixed = false;
		require(services.serialization().loadSnapshot(saved.path.string()),
				"platformer_world snapshot must restore cleanly.");

		const auto& restoredRb = registry.getComponent<RigidBody2D>(fixedAmebaAfter);
		const auto& restoredPos = registry.getComponent<Transform>(fixedAmebaAfter).position;
		require(restoredRb.type == BodyType::Fixed && restoredRb.isFixed,
				"ameba_1 anchor rigidbody must remain BodyType::Fixed after snapshot restore.");
		require(glm::length(restoredPos - amebaFixedPosAfter) < 1e-4f,
				"ameba_1 Fixed anchor position must remain unchanged.");
	}

	void testClusterReloadPreservesSceneOverrides(bool threaded)
	{
		auto& manager = SceneManager::getInstance();
		PhysicsSettings settings;
		settings.runSimulationInThread = threaded;
		manager.setPhysicsSettings(settings);

		const std::string namePrefix = std::string("test-cluster-ov-") + (threaded ? "threaded" : "single");

		struct TempAssetFile
		{
			std::filesystem::path path;
			explicit TempAssetFile(const std::string& ext)
			{
				static size_t seq = 0;
				const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
				path = std::filesystem::temp_directory_path() /
					   ("weird-cluster-override-" + std::to_string(tick) + "-" + std::to_string(seq++) + ext);
			}
			~TempAssetFile()
			{
				std::error_code ec;
				std::filesystem::remove(path, ec);
			}
		};

		TempAssetFile clusterFile(".weirdcluster");
		TempAssetFile sceneFile(".weirdscene");

		// v1 cluster asset: two dots ("anchor" at (0,0) and free dot at (2,0)), all Dynamic, material 1, mass 1.0
		{
			nlohmann::json cj = {
				{"version", 1},
				{"clusters", nlohmann::json::array()},
				{"entities",
				 {{{"id", 10},
				   {"dot", {{"isStatic", false}, {"materialId", 1}}},
				   {"rigidBody2D",
					{{"bodyType", 0},
					 {"enableCollision", true},
					 {"enableShapeCollision", true},
					 {"mass", 1.0f},
					 {"physicsPosition", {0.0f, 0.0f}},
					 {"simulationId", 0},
					 {"velocity", {0.0f, 0.0f}}}},
				   {"transform",
					{{"position", {0.0f, 0.0f, 0.0f}},
					 {"rotation", {0.0f, 0.0f, 0.0f}},
					 {"scale", {1.0f, 1.0f, 1.0f}}}}},
				  {{"id", 20},
				   {"dot", {{"isStatic", false}, {"materialId", 1}}},
				   {"rigidBody2D",
					{{"bodyType", 0},
					 {"enableCollision", true},
					 {"enableShapeCollision", true},
					 {"mass", 1.0f},
					 {"physicsPosition", {2.0f, 0.0f}},
					 {"simulationId", 1},
					 {"velocity", {0.0f, 0.0f}}}},
				   {"transform",
					{{"position", {2.0f, 0.0f, 0.0f}},
					 {"rotation", {0.0f, 0.0f, 0.0f}},
					 {"scale", {1.0f, 1.0f, 1.0f}}}}}}},
				{"names", {{{"name", "anchor"}, {"entityId", 10}}}},
				{"entityTags", nlohmann::json::array()},
				{"physics",
				 {{"distanceConstraints", nlohmann::json::array()},
				  {"springs", {{{"entity", 30}, {"entityA", 10}, {"entityB", 20}, {"distance", 2.0f}, {"k", 0.5f}}}}}},
				{"materials2D", nlohmann::json::array()}};
			std::ofstream out(clusterFile.path);
			out << cj.dump(2);
		}

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<Scene>>(namePrefix + "-a");
		manager.loadScene(namePrefix + "-a");

		{
			auto& scene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
			auto& registry = *scene.registry;
			auto& services = *scene.services;

			ClusterInstance& cluster = services.clusters().instantiate(clusterFile.path.string(), vec2(5.0f, 1.0f));
			const uint32_t clusterId = cluster.id;
			const Entity anchor = cluster.getEntity("anchor");
			require(anchor != INVALID_ENTITY, "Cluster must resolve local name 'anchor'.");

			// Add an external scene entity attached to 'anchor' via a Spring
			const Entity extAnchor = registry.createEntity();
			registry.addComponent<Transform>(extAnchor).position = vec3(5.0f, 4.0f, 0.0f);
			registry.addComponent<RigidBody2D>(extAnchor).type = BodyType::Fixed;
			const Entity extSpringEnt = registry.createEntity();
			auto& extSpring = registry.addComponent<Spring>(extSpringEnt);
			extSpring.entityA = extAnchor;
			extSpring.entityB = anchor;
			extSpring.restDistance = 2.5f;
			extSpring.stiffness = 0.75f;

			// Override multiple properties on 'anchor' in the scene
			auto& rb = registry.getComponent<RigidBody2D>(anchor);
			rb.type = BodyType::Fixed;
			rb.isFixed = true;
			rb.mass = 4.5f;
			rb.enableCollision = false;
			registry.getComponent<Dot>(anchor).materialId = 9;
			registry.getComponent<Transform>(anchor).position = vec3(5.0f, 2.5f, 0.0f);
			services.names().setName(anchor, "PinnedBridgeAnchor");
			services.tags().addTag(anchor, "bridge_pin");
			services.clusters().markEntityOverride(
				anchor, ClusterOverrideFlag::BodyType | ClusterOverrideFlag::Mass |
							ClusterOverrideFlag::EnableCollision | ClusterOverrideFlag::DotMaterial |
							ClusterOverrideFlag::Position | ClusterOverrideFlag::GlobalName |
							ClusterOverrideFlag::CustomTags);

			SceneSerializer::save(scene, sceneFile.path.string());

			// Verify reload inside the current scene updates un-overridden fields and keeps all overrides
			// First write v2 cluster asset: reversed entity order, dot 20 material->7 & mass->3.0, plus new dot 25
			{
				nlohmann::json cj2 = {
					{"version", 1},
					{"clusters", nlohmann::json::array()},
					{"entities",
					 {{{"id", 20},
					   {"dot", {{"isStatic", false}, {"materialId", 7}}},
					   {"rigidBody2D",
						{{"bodyType", 0},
						 {"enableCollision", true},
						 {"enableShapeCollision", true},
						 {"mass", 3.0f},
						 {"physicsPosition", {2.0f, 0.0f}},
						 {"simulationId", 0},
						 {"velocity", {0.0f, 0.0f}}}},
					   {"transform",
						{{"position", {2.0f, 0.0f, 0.0f}},
						 {"rotation", {0.0f, 0.0f, 0.0f}},
						 {"scale", {1.0f, 1.0f, 1.0f}}}}},
					  {{"id", 25},
					   {"dot", {{"isStatic", false}, {"materialId", 6}}},
					   {"rigidBody2D",
						{{"bodyType", 0},
						 {"enableCollision", true},
						 {"enableShapeCollision", true},
						 {"mass", 1.0f},
						 {"physicsPosition", {4.0f, 0.0f}},
						 {"simulationId", 1},
						 {"velocity", {0.0f, 0.0f}}}},
					   {"transform",
						{{"position", {4.0f, 0.0f, 0.0f}},
						 {"rotation", {0.0f, 0.0f, 0.0f}},
						 {"scale", {1.0f, 1.0f, 1.0f}}}}},
					  {{"id", 10},
					   {"dot", {{"isStatic", false}, {"materialId", 1}}},
					   {"rigidBody2D",
						{{"bodyType", 0},
						 {"enableCollision", true},
						 {"enableShapeCollision", true},
						 {"mass", 1.0f},
						 {"physicsPosition", {0.0f, 0.0f}},
						 {"simulationId", 2},
						 {"velocity", {0.0f, 0.0f}}}},
					   {"transform",
						{{"position", {0.0f, 0.0f, 0.0f}},
						 {"rotation", {0.0f, 0.0f, 0.0f}},
						 {"scale", {1.0f, 1.0f, 1.0f}}}}}}},
					{"names", {{{"name", "anchor"}, {"entityId", 10}}}},
					{"entityTags", nlohmann::json::array()},
					{"physics",
					 {{"distanceConstraints", nlohmann::json::array()},
					  {"springs",
					   {{{"entity", 30}, {"entityA", 10}, {"entityB", 20}, {"distance", 2.0f}, {"k", 0.5f}}}}}},
					{"materials2D", nlohmann::json::array()}};
				std::ofstream out(clusterFile.path);
				out << cj2.dump(2);
			}

			require(services.clusters().reloadInstance(clusterId, clusterFile.path.string()),
					"Cluster reload from v2 asset must succeed.");
			require(registry.isEntityAlive(extSpringEnt) && registry.hasComponent<Spring>(extSpringEnt),
					"External spring attached to cluster member must survive cluster reload.");
			const Entity newAnchor = cluster.getEntity("anchor");
			require(registry.getComponent<Spring>(extSpringEnt).entityB == newAnchor,
					"Surviving external spring must rebind to the reloaded cluster member.");
		}

		// Now load the previously saved .weirdscene (with persisted overrides & sourceEntityIds) into a fresh Scene,
		// then reload its cluster against v2 asset and verify everything survives save/load + reload.
		manager.registerScene<InspectableScene<Scene>>(namePrefix + "-b", sceneFile.path.string());
		manager.loadScene(namePrefix + "-b");
		{
			auto& scene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
			auto& registry = *scene.registry;
			auto& services = *scene.services;

			require(services.clusters().getAll().size() == 1, "Saved scene must restore cluster instance.");
			const uint32_t clusterId = services.clusters().getAll().front().id;
			require(services.clusters().reloadInstance(clusterId, clusterFile.path.string()),
					"Cluster reload after scene save/load must succeed.");

			ClusterInstance* cluster = services.clusters().getById(clusterId);
			require(cluster != nullptr, "Reloaded cluster must exist.");
			const Entity anchor = cluster->getEntity("anchor");
			require(anchor != INVALID_ENTITY, "Reloaded cluster must retain 'anchor' localName.");

			const auto& rbAnchor = registry.getComponent<RigidBody2D>(anchor);
			require(rbAnchor.type == BodyType::Fixed && rbAnchor.isFixed,
					"BodyType override on cluster member must survive save/load + reload.");
			require(std::abs(rbAnchor.mass - 4.5f) < 1e-4f, "Mass override must survive reload.");
			require(!rbAnchor.enableCollision && rbAnchor.enableShapeCollision,
					"Collision flag override must survive reload.");
			require(registry.getComponent<Dot>(anchor).materialId == 9, "Dot material override must survive reload.");
			require(std::abs(registry.getComponent<Transform>(anchor).position.y - 2.5f) < 1e-3f,
					"Relative position override must survive reload.");
			require(services.names().getEntityByName("PinnedBridgeAnchor") == anchor,
					"Global entity name override must survive reload.");
			require(services.tags().hasTag(anchor, "bridge_pin"), "Custom entity tag override must survive reload.");

			int dotMat7Count = 0;
			int dotMat6Count = 0;
			bool updatedUnoverriddenMass = false;
			for (Entity e : cluster->entities)
			{
				if (!registry.hasComponent<Dot>(e) || e == anchor)
					continue;
				const auto& d = registry.getComponent<Dot>(e);
				if (d.materialId == 7)
				{
					++dotMat7Count;
					if (std::abs(registry.getComponent<RigidBody2D>(e).mass - 3.0f) < 1e-4f)
						updatedUnoverriddenMass = true;
				}
				else if (d.materialId == 6)
				{
					++dotMat6Count;
				}
			}
			require(dotMat7Count == 1 && updatedUnoverriddenMass,
					"Un-overridden cluster member must pull updated material & mass from reloaded asset.");
			require(dotMat6Count == 1, "Newly added entity in reloaded cluster asset must appear in scene.");
		}
	}

	void testSceneSerializerInMemoryRoundTrip(bool threaded)
	{
		auto& manager = SceneManager::getInstance();
		PhysicsSettings settings;
		settings.runSimulationInThread = threaded;
		manager.setPhysicsSettings(settings);

		const std::string namePrefix = std::string("test-serializer-json-") + (threaded ? "threaded" : "single");

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<Scene>>(namePrefix + "-src");
		manager.registerScene<InspectableScene<Scene>>(namePrefix + "-dst");
		manager.loadScene(namePrefix + "-src");

		nlohmann::json savedJson;
		Entity srcBox = INVALID_ENTITY;
		{
			auto& scene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
			auto& registry = *scene.registry;
			auto& services = *scene.services;

			srcBox = registry.createEntity();
			auto& shape = registry.addComponent<Shape>(srcBox);
			shape.distanceFieldId = static_cast<ShapeId>(DefaultShapes::Box);
			shape.parameters[0] = 3.5f;
			shape.parameters[1] = -2.0f;
			shape.parameters[2] = 1.25f;
			shape.parameters[3] = 0.75f;
			shape.parameters[DefaultShapes::Box::OutlineThickness] = 0.15f;
			shape.material = 4;
			services.names().setName(srcBox, "UndoBox");
			services.tags().addTag(srcBox, "editable");

			savedJson = SceneSerializer::serializeToJson(scene);
			require(savedJson["entities"].size() == 1 &&
						!savedJson["entities"][0]["shape"].contains("distanceFieldId") &&
						savedJson["entities"][0]["shape"].value("type", "") == "Box",
					"Serialized built-in Shape JSON must store string 'type' without numeric 'distanceFieldId'.");
			savedJson.erase("camera");
		}

		manager.loadScene(namePrefix + "-dst");
		{
			auto& dstScene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
			auto& registry = *dstScene.registry;
			auto& services = *dstScene.services;

			const Entity camEnt = services.render().getCameraEntity();
			registry.getComponent<Transform>(camEnt).position = vec3(42.0f, -17.0f, 20.0f);

			std::unordered_map<Entity, Entity> newToSaved;
			SceneSerializer::loadFromJson(dstScene, savedJson, "", nullptr, false, nullptr, &newToSaved);

			const vec3 camPosAfter = registry.getComponent<Transform>(camEnt).position;
			require(std::abs(camPosAfter.x - 42.0f) < 1e-4f && std::abs(camPosAfter.y + 17.0f) < 1e-4f,
					"loadFromJson(restoreCamera=false) must preserve the editor camera position.");

			const Entity restoredBox = services.names().getEntityByName("UndoBox");
			require(restoredBox != INVALID_ENTITY && registry.hasComponent<Shape>(restoredBox),
					"In-memory loadFromJson must restore entity and Shape component.");
			require(newToSaved.count(restoredBox) == 1 && newToSaved[restoredBox] == srcBox,
					"outSourceEntityIds must map restored entity back to serialized source id.");
			require(services.tags().hasTag(restoredBox, "editable"),
					"In-memory loadFromJson must restore entity tags.");
			const auto& restoredShape = registry.getComponent<Shape>(restoredBox);
			require(restoredShape.distanceFieldId == DefaultShapes::Box &&
						std::abs(restoredShape.parameters[0] - 3.5f) < 1e-4f &&
						std::abs(restoredShape.parameters[DefaultShapes::Box::OutlineThickness] - 0.15f) < 1e-4f &&
						restoredShape.material == 4,
					"In-memory loadFromJson must restore Shape type, parameters, OutlineThickness, and material.");
		}
	}
} // namespace

int main(int argc, char** argv)
{
	return WeirdTests::runTests(
		argc, argv, "WeirdSerializationTests",
		{
			{"ResourceAndShapeAssetPathResolution", testResourceAndShapeAssetPathResolution},
			{"SceneSerializerInMemoryRoundTrip-single", []() { testSceneSerializerInMemoryRoundTrip(false); }},
			{"SceneSerializerInMemoryRoundTrip-threaded", []() { testSceneSerializerInMemoryRoundTrip(true); }},
			{"PlatformerWorldClusterFixedBody-single", []() { testPlatformerWorldClusterFixedBody(false); }},
			{"PlatformerWorldClusterFixedBody-threaded", []() { testPlatformerWorldClusterFixedBody(true); }},
			{"ClusterReloadPreservesSceneOverrides-single", []() { testClusterReloadPreservesSceneOverrides(false); }},
			{"ClusterReloadPreservesSceneOverrides-threaded", []() { testClusterReloadPreservesSceneOverrides(true); }},
		});
}
