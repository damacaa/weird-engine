#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/math/SDF.h"
#include "weird-engine/services/ShapeTypes.h"
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
	};
} // namespace WeirdEngine
