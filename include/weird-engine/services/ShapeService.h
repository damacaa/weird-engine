#pragma once

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/Logger.h"
#include "weird-engine/math/SDF.h"
#include "weird-engine/services/ShapeTypes.h"
#include "weird-engine/shapes/NodeGraph.h"
#include "weird-engine/shapes/NodeGraphCompiler.h"
#include "weird-engine/shapes/NodeGraphSerializer.h"
#include "weird-physics/Simulation2D.h"
#include "weird-renderer/components/Shape.h"

namespace WeirdEngine
{
	class ShapeService
	{
	public:
		ShapeService(Registry& registry, Simulation2D& simulation, std::vector<std::shared_ptr<IMathExpression>>& sdfs)
			: m_registry(registry)
			, m_simulation(simulation)
			, m_sdfs(sdfs)
		{
		}

		static ShapeId registerDefaultSDF(const Expr& sdf);
		static ShapeId registerDefaultSDF(std::shared_ptr<IMathExpression> sdf);

		ShapeId registerSDF(const Expr& expr)
		{
			return registerSDF(expr.node);
		}

		ShapeId registerSDF(std::shared_ptr<IMathExpression> sdf)
		{
			m_sdfs.push_back(std::move(sdf));
			m_simulation.setSDFs(m_sdfs);

			return static_cast<ShapeId>(m_sdfs.size() - 1);
		}

		ShapeId loadCustomShape(const std::string& filePath)
		{
			auto it = m_customShapeCache.find(filePath);
			if (it != m_customShapeCache.end())
			{
				return it->second;
			}

			std::ifstream file(filePath);
			if (!file.is_open())
			{
				Logger::error("[ShapeService] Failed to open custom shape file: " + filePath);
				return 0;
			}

			json j;
			try
			{
				file >> j;
			}
			catch (const std::exception& e)
			{
				Logger::error(std::string("[ShapeService] Failed to parse custom shape JSON: ") + e.what());
				return 0;
			}

			NodeGraph graph;
			if (!NodeGraphSerializer::deserialize(graph, j))
			{
				Logger::error("[ShapeService] Failed to deserialize shape graph: " + filePath);
				return 0;
			}

			Expr expr = NodeGraphCompiler::evaluate(graph);
			ShapeId id = registerSDF(expr);
			m_customShapeCache[filePath] = id;
			Logger::log("[ShapeService] Loaded custom shape from " + filePath + " as shape ID " + std::to_string(id));
			return id;
		}

		const std::vector<std::shared_ptr<IMathExpression>>& getSDFs() const
		{
			return m_sdfs;
		}

		Entity addShape(const ShapeConfig& config)
		{
			Entity entity = m_registry.createEntity();
			if (entity == INVALID_ENTITY)
			{
				return INVALID_ENTITY;
			}
			Shape& shape = m_registry.addComponent<Shape>(entity);
			shape.distanceFieldId = config.shapeId;
			shape.combination = config.combination;
			shape.hasCollisions = config.hasCollision;
			shape.groupIdx = config.group;
			shape.material = config.material.id;

			std::copy_n(config.variables.data, 8, shape.parameters);

			return entity;
		}

		Entity addUIShape(const UIShapeConfig& config)
		{
			Entity entity = m_registry.createEntity();
			if (entity == INVALID_ENTITY)
			{
				return INVALID_ENTITY;
			}
			UIShape& shape = m_registry.addComponent<UIShape>(entity);
			shape.distanceFieldId = config.shapeId;
			shape.combination = config.combination;
			shape.groupIdx = config.group;
			shape.material = config.material.id;

			std::copy_n(config.variables.data, 8, shape.parameters);

			return entity;
		}

	private:
		Registry& m_registry;
		Simulation2D& m_simulation;
		std::vector<std::shared_ptr<IMathExpression>>& m_sdfs;
		std::unordered_map<std::string, ShapeId> m_customShapeCache;
	};
} // namespace WeirdEngine
