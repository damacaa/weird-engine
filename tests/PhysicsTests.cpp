#include "TestHarness.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-engine/services/ShapeService.h"
#include "weird-engine/systems/PhysicsSystem2D.h"
#include "weird-physics/components/RigidBodyManager.h"
#include "weird-renderer/core/Display.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace WeirdEngine;
using WeirdTests::FloorPlaneSdf;
using WeirdTests::InspectableScene;
using WeirdTests::require;

namespace
{
	void testSimulation2DPendingAndCollisionFlags(bool threaded)
	{
		PhysicsSettings settings{};
		settings.runSimulationInThread = threaded;

		Simulation2D sim(64, settings);
		Registry reg;
		reg.registerComponent<RigidBody2D>(std::make_shared<RigidBodyManager>(sim));
		ECS::PhysicsSystem2D::init(reg, sim);

		std::vector<std::shared_ptr<IMathExpression>> sdfs{std::make_shared<FloorPlaneSdf>()};
		sim.setSDFs(sdfs);

		const Entity floorEntity = reg.createEntity();
		Shape floorShape{};
		floorShape.distanceFieldId = 0;
		floorShape.hasCollisions = true;
		sim.updateShape(floorEntity, floorShape);

		struct CallbackState
		{
			std::atomic<size_t> kinematicShapeHits{0};
			SimulationID kinematicId = INVALID_SIMULATION_ID;
		} cbState;

		const Entity shipProbe = reg.createEntity();
		auto& probeT = reg.addComponent<Transform>(shipProbe);
		probeT.position = vec3(0.0f, 0.25f, 0.0f);
		reg.setEntityDirty<Transform>(shipProbe, true);
		auto& probeRb = reg.addComponent<RigidBody2D>(shipProbe);
		probeRb.type = BodyType::Kinematic;
		probeRb.mass = 1.0f;
		probeRb.enableCollision = false;
		probeRb.enableShapeCollision = true;
		reg.setEntityDirty<RigidBody2D>(shipProbe, true);
		cbState.kinematicId = probeRb.simulationId;

		sim.setShapeCollisionCallback(
			[](PhysicsShapeCollisionEvent& ev, void* userData)
			{
				auto* st = static_cast<CallbackState*>(userData);
				if (ev.body == st->kinematicId)
				{
					st->kinematicShapeHits.fetch_add(1, std::memory_order_relaxed);
				}
			},
			&cbState);

		if (threaded)
		{
			sim.startSimulationThread();
		}

		std::vector<Entity> spawnedDots;
		for (int frame = 0; frame < 30; ++frame)
		{
			ECS::PhysicsSystem2D::update(reg, sim);

			for (Entity d : spawnedDots)
			{
				const vec2 pos = vec2(reg.getComponent<Transform>(d).position);
				require(pos.x > 10.0f && glm::length(pos) > 4.0f,
						"Newly spawned rigidbodies must never reset to (0, 0) across physics buffer swaps.");
			}

			sim.update(1.0 / 60.0);
			if (threaded)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(4));
			}

			if (frame < 25)
			{
				const Entity dot = reg.createEntity();
				auto& t = reg.addComponent<Transform>(dot);
				t.position = vec3(12.0f, 10.0f, 0.0f);
				reg.setEntityDirty<Transform>(dot, true);
				auto& rb = reg.addComponent<RigidBody2D>(dot);
				rb.type = BodyType::Dynamic;
				rb.mass = 0.05f;
				rb.velocity = vec2(3.0f, -24.0f);
				rb.enableCollision = false;
				rb.enableShapeCollision = true;
				reg.setEntityDirty<RigidBody2D>(dot, true);
				spawnedDots.push_back(dot);
			}
		}

		if (threaded)
		{
			sim.stopSimulationThread();
		}

		require(cbState.kinematicShapeHits.load(std::memory_order_relaxed) > 0,
				"Bodies with enableCollision=false and enableShapeCollision=true must emit shape collisions.");
	}

	void testShapeLayerOrderPhysicsAndSerialization(bool threaded)
	{
		auto& manager = SceneManager::getInstance();
		PhysicsSettings managerSettings;
		managerSettings.runSimulationInThread = threaded;
		manager.setPhysicsSettings(managerSettings);

		const std::string namePrefix = std::string("test-layer-order-") + (threaded ? "threaded" : "single");

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<Scene>>(namePrefix + "-src");
		manager.registerScene<InspectableScene<Scene>>(namePrefix + "-dst");
		manager.loadScene(namePrefix + "-src");

		nlohmann::json savedJson;
		{
			auto& scene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
			auto& registry = *scene.registry;
			auto& services = *scene.services;

			services.physics().setGravity(0.0f);
			services.physics().setDamping(0.95f);

			// 1. Verify per-layer automatic order assignment
			const Entity eAdd = services.shapes().addShape({.shapeId = DefaultShapes::Circle,
															.variables = {{DefaultShapes::Circle::PosX, 0.0f},
																		  {DefaultShapes::Circle::PosY, 0.0f},
																		  {DefaultShapes::Circle::Radius, 4.0f}},
															.material = 1,
															.combination = CombinationType::Addition,
															.hasCollision = true,
															.layer = 0});
			const Entity eSub = services.shapes().addShape({.shapeId = DefaultShapes::Circle,
															.variables = {{DefaultShapes::Circle::PosX, 0.0f},
																		  {DefaultShapes::Circle::PosY, 0.0f},
																		  {DefaultShapes::Circle::Radius, 4.0f}},
															.material = 2,
															.combination = CombinationType::Subtraction,
															.hasCollision = true,
															.layer = 0});
			const Entity eOtherLayer = services.shapes().addShape({.shapeId = DefaultShapes::Circle,
																   .variables = {{DefaultShapes::Circle::PosX, 50.0f},
																				 {DefaultShapes::Circle::PosY, 50.0f},
																				 {DefaultShapes::Circle::Radius, 1.0f}},
																   .material = 3,
																   .combination = CombinationType::Addition,
																   .hasCollision = true,
																   .layer = 2});

			require(registry.getComponent<Shape>(eAdd).order == 0 && registry.getComponent<Shape>(eSub).order == 1 &&
						registry.getComponent<Shape>(eOtherLayer).order == 0,
					"ShapeService::addShape must assign sequential per-layer orders starting at 0.");
			require(ShapeService::computeNextOrder<Shape>(registry, 0) == 2 &&
						ShapeService::computeNextOrder<Shape>(registry, 2) == 1 &&
						ShapeService::computeNextOrder<Shape>(registry, 5) == 0,
					"ShapeService::computeNextOrder must return per-layer max + 1 (or 0 for empty layer).");

			// 2. Verify Simulation2D respects intra-layer order (both single-threaded and threaded)
			{
				PhysicsSettings settings;
				settings.runSimulationInThread = threaded;
				settings.gravity = 0.0f;
				settings.damping = 0.95f;
				Simulation2D sim(16, settings);
				if (threaded)
				{
					sim.startSimulationThread();
				}

				Registry simReg;
				simReg.registerComponent<Transform>();
				simReg.registerComponent<Shape>();
				simReg.registerComponent<RigidBody2D>(std::make_shared<RigidBodyManager>(sim));
				ECS::PhysicsSystem2D::init(simReg, sim);

				const Entity simAdd = simReg.createEntity();
				{
					auto& s = simReg.addComponent<Shape>(simAdd);
					s.distanceFieldId = DefaultShapes::Circle;
					s.parameters[DefaultShapes::Circle::PosX] = 0.0f;
					s.parameters[DefaultShapes::Circle::PosY] = 0.0f;
					s.parameters[DefaultShapes::Circle::Radius] = 4.0f;
					s.combination = CombinationType::Addition;
					s.hasCollisions = true;
					s.layer = 0;
					s.order = 0;
					simReg.setComponentDirty(s);
				}
				const Entity simSub = simReg.createEntity();
				{
					auto& s = simReg.addComponent<Shape>(simSub);
					s.distanceFieldId = DefaultShapes::Circle;
					s.parameters[DefaultShapes::Circle::PosX] = 0.0f;
					s.parameters[DefaultShapes::Circle::PosY] = 0.0f;
					s.parameters[DefaultShapes::Circle::Radius] = 4.0f;
					s.combination = CombinationType::Subtraction;
					s.hasCollisions = true;
					s.layer = 0;
					s.order = 1;
					simReg.setComponentDirty(s);
				}

				const Entity bodyEnt = simReg.createEntity();
				auto& bodyTransform = simReg.addComponent<Transform>(bodyEnt);
				bodyTransform.position = vec3(1.5f, 0.0f, 0.0f);
				auto& bodyRb = simReg.addComponent<RigidBody2D>(bodyEnt);
				bodyRb.mass = 1.0f;
				bodyRb.type = BodyType::Dynamic;
				bodyRb.enableShapeCollision = true;
				simReg.setComponentDirty(bodyTransform);
				simReg.setComponentDirty(bodyRb);

				auto stepSim = [&](int count)
				{
					for (int step = 0; step < count; ++step)
					{
						ECS::PhysicsSystem2D::update(simReg, sim);
						sim.update(1.0 / 60.0);
						if (threaded)
						{
							std::this_thread::sleep_for(std::chrono::milliseconds(4));
						}
					}
					ECS::PhysicsSystem2D::update(simReg, sim);
				};

				// Initial order: Addition (0) then Subtraction (1) -> interior is carved out
				stepSim(10);
				const vec2 posCarved(simReg.getComponent<Transform>(bodyEnt).position);
				require(glm::length(posCarved - vec2(1.5f, 0.0f)) < 0.25f,
						"Simulation2D must not push body out when Subtraction has higher order than Addition.");

				// Swap order: Subtraction (0) then Addition (1) -> interior is solid
				{
					auto& sSub = simReg.getComponent<Shape>(simSub);
					auto& sAdd = simReg.getComponent<Shape>(simAdd);
					sSub.order = 0;
					sAdd.order = 1;
					simReg.setComponentDirty(sSub);
					simReg.setComponentDirty(sAdd);
				}

				stepSim(30);
				const vec2 posSolid(simReg.getComponent<Transform>(bodyEnt).position);
				require(glm::length(posSolid) > 3.5f,
						"Simulation2D must re-sort shapes on order change and push body out of solid Addition.");

				if (threaded)
				{
					sim.stopSimulationThread();
				}
			}

			// 3. Verify PhysicsService::sampleAt respects intra-layer order and GLOBAL_LAYER subtraction
			const RaymarchResult carvedSample = services.physics().sampleAt(vec2(1.5f, 0.0f), false);
			require(
				carvedSample.distance > 0.0f,
				"PhysicsService::sampleAt must evaluate Subtraction after Addition when Subtraction has higher order.");

			{
				auto& sSub = registry.getComponent<Shape>(eSub);
				auto& sAdd = registry.getComponent<Shape>(eAdd);
				sSub.order = 0;
				sAdd.order = 1;
				registry.setComponentDirty(sSub);
				registry.setComponentDirty(sAdd);
			}
			const RaymarchResult solidSample = services.physics().sampleAt(vec2(1.5f, 0.0f), false);
			require(
				solidSample.distance < -2.0f && solidSample.entity == eAdd,
				"PhysicsService::sampleAt must report solid Addition when Addition has higher order than Subtraction.");

			const Entity eGlobalSub = services.shapes().addShape({.shapeId = DefaultShapes::Circle,
																  .variables = {{DefaultShapes::Circle::PosX, 0.0f},
																				{DefaultShapes::Circle::PosY, 0.0f},
																				{DefaultShapes::Circle::Radius, 5.0f}},
																  .material = 0,
																  .combination = CombinationType::Subtraction,
																  .hasCollision = true,
																  .layer = Shape::GLOBAL_LAYER});
			const RaymarchResult globalCutSample = services.physics().sampleAt(vec2(1.5f, 0.0f), false);
			require(globalCutSample.distance > 0.0f,
					"PhysicsService::sampleAt must apply GLOBAL_LAYER subtraction after regular layers.");

			// 4. Verify SceneSerializer round-trips non-default layer & order for Shape and UIShape
			auto& sAdd = registry.getComponent<Shape>(eAdd);
			sAdd.layer = 3;
			sAdd.order = 7;
			auto& sSub = registry.getComponent<Shape>(eSub);
			sSub.layer = 3;
			sSub.order = 2;
			auto& sGlobal = registry.getComponent<Shape>(eGlobalSub);
			sGlobal.order = 4;

			const Entity eUi = services.shapes().addUIShape({.shapeId = DefaultShapes::Box,
															 .variables = {10.0f, 20.0f, 30.0f, 40.0f},
															 .material = 1,
															 .combination = CombinationType::Addition,
															 .layer = 5});
			registry.getComponent<UIShape>(eUi).order = 9;

			services.names().setName(eAdd, "OrderAdd");
			services.names().setName(eSub, "OrderSub");
			services.names().setName(eGlobalSub, "OrderGlobal");
			services.names().setName(eUi, "OrderUI");

			savedJson = SceneSerializer::serializeToJson(scene);
			savedJson.erase("camera");
		}

		manager.loadScene(namePrefix + "-dst");
		{
			auto& dstScene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
			auto& registry = *dstScene.registry;
			auto& services = *dstScene.services;

			SceneSerializer::loadFromJson(dstScene, savedJson, "", nullptr, false);

			const Entity rAdd = services.names().getEntityByName("OrderAdd");
			const Entity rSub = services.names().getEntityByName("OrderSub");
			const Entity rGlobal = services.names().getEntityByName("OrderGlobal");
			const Entity rUi = services.names().getEntityByName("OrderUI");

			require(rAdd != INVALID_ENTITY && rSub != INVALID_ENTITY && rGlobal != INVALID_ENTITY &&
						rUi != INVALID_ENTITY,
					"SceneSerializer must restore all named shapes for layer/order round-trip.");

			const auto& rsAdd = registry.getComponent<Shape>(rAdd);
			const auto& rsSub = registry.getComponent<Shape>(rSub);
			const auto& rsGlobal = registry.getComponent<Shape>(rGlobal);
			const auto& rsUi = registry.getComponent<UIShape>(rUi);

			require(rsAdd.layer == 3 && rsAdd.order == 7, "Shape layer and order must round-trip through JSON.");
			require(rsSub.layer == 3 && rsSub.order == 2, "Second shape layer and order must round-trip through JSON.");
			require(rsGlobal.layer == Shape::GLOBAL_LAYER && rsGlobal.order == 4,
					"GLOBAL_LAYER shape layer and order must round-trip through JSON.");
			require(rsUi.layer == 5 && rsUi.order == 9, "UIShape layer and order must round-trip through JSON.");
		}
	}
} // namespace

int main(int argc, char** argv)
{
	return WeirdTests::runTests(
		argc, argv, "WeirdPhysicsTests",
		{
			{"Simulation2DPendingAndCollisionFlags-single", []() { testSimulation2DPendingAndCollisionFlags(false); }},
			{"Simulation2DPendingAndCollisionFlags-threaded", []() { testSimulation2DPendingAndCollisionFlags(true); }},
			{"ShapeLayerOrderPhysicsAndSerialization-single",
			 []() { testShapeLayerOrderPhysicsAndSerialization(false); }},
			{"ShapeLayerOrderPhysicsAndSerialization-threaded",
			 []() { testShapeLayerOrderPhysicsAndSerialization(true); }},
		});
}
