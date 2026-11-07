#pragma once

#include <algorithm>
#include <array>
#include <filesystem>
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
	struct CustomShapeData
	{
		ShapeId id = 0;
		std::string filePath;
		std::filesystem::file_time_type lastWriteTime{};
		std::array<float, 8> defaultParameters = {0.0f};
		std::array<std::string, 8> parameterNames;
	};

	class ShapeService
	{
	public:
		ShapeService(Registry& registry, Simulation2D& simulation, std::vector<std::shared_ptr<IMathExpression>>& sdfs)
			: m_registry(registry)
			, m_simulation(simulation)
			, m_sdfs(sdfs)
		{
		}

		static std::string normalizePath(const std::string& path)
		{
			if (path.empty())
			{
				return {};
			}
			std::error_code ec;
			auto p = std::filesystem::weakly_canonical(std::filesystem::path(path), ec);
			if (ec)
			{
				p = std::filesystem::path(path).lexically_normal();
			}
			return p.generic_string();
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
			std::string normPath = normalizePath(filePath);
			auto it = m_customShapeCache.find(normPath);
			if (it == m_customShapeCache.end())
			{
				it = m_customShapeCache.find(filePath);
			}
			if (it != m_customShapeCache.end())
			{
				std::error_code ec;
				std::string checkPath = !it->second.filePath.empty() ? it->second.filePath : filePath;
				auto currentWriteTime = std::filesystem::last_write_time(checkPath, ec);
				if (!ec && currentWriteTime > it->second.lastWriteTime)
				{
					reloadCustomShape(normPath);
				}
				return it->second.id;
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

			CustomShapeData data;
			data.id = id;
			data.filePath = filePath;
			data.defaultParameters = graph.getParameters();
			data.parameterNames = graph.getParameterNames();

			std::error_code ec;
			auto writeTime = std::filesystem::last_write_time(filePath, ec);
			if (!ec)
			{
				data.lastWriteTime = writeTime;
			}

			m_customShapeCache[normPath] = data;
			m_shapeIdToCustomPath[id] = normPath;

			Logger::log("[ShapeService] Loaded custom shape from " + filePath + " as shape ID " + std::to_string(id));
			return id;
		}

		bool reloadCustomShape(const std::string& filePath)
		{
			std::string normPath = normalizePath(filePath);
			auto it = m_customShapeCache.find(normPath);
			if (it == m_customShapeCache.end())
			{
				it = m_customShapeCache.find(filePath);
			}
			if (it == m_customShapeCache.end())
			{
				return loadCustomShape(filePath) != 0;
			}

			std::string pathToOpen = it->second.filePath;
			std::ifstream file(pathToOpen);
			if (!file.is_open())
			{
				pathToOpen = filePath;
				file.open(pathToOpen);
				if (!file.is_open())
				{
					pathToOpen = normPath;
					file.open(pathToOpen);
					if (!file.is_open())
					{
						Logger::error("[ShapeService] Failed to open custom shape file for reload: " + filePath);
						return false;
					}
				}
			}

			json j;
			try
			{
				file >> j;
			}
			catch (const std::exception& e)
			{
				Logger::error(std::string("[ShapeService] Failed to parse custom shape JSON on reload: ") + e.what());
				return false;
			}

			NodeGraph graph;
			if (!NodeGraphSerializer::deserialize(graph, j))
			{
				Logger::error("[ShapeService] Failed to deserialize shape graph on reload: " + filePath);
				return false;
			}

			Expr expr = NodeGraphCompiler::evaluate(graph);
			ShapeId id = it->second.id;
			if (id < m_sdfs.size())
			{
				m_sdfs[id] = expr.node;
				m_simulation.setSDFs(m_sdfs);
			}
			else
			{
				Logger::error("[ShapeService] ShapeId " + std::to_string(id) + " out of bounds in m_sdfs!");
				return false;
			}

			it->second.defaultParameters = graph.getParameters();
			it->second.parameterNames = graph.getParameterNames();

			std::error_code ec;
			auto writeTime = std::filesystem::last_write_time(pathToOpen, ec);
			if (!ec)
			{
				it->second.lastWriteTime = writeTime;
			}

			Logger::log("[ShapeService] Reloaded custom shape: " + filePath + " (ID: " + std::to_string(id) + ")");
			return true;
		}

		bool reloadCustomShape(ShapeId id)
		{
			auto itPath = m_shapeIdToCustomPath.find(id);
			if (itPath == m_shapeIdToCustomPath.end())
			{
				return false;
			}
			return reloadCustomShape(itPath->second);
		}

		bool pollCustomShapesChanged(std::vector<ShapeId>* outReloadedIds = nullptr)
		{
			bool anyReloaded = false;
			for (auto& [key, data] : m_customShapeCache)
			{
				std::string checkPath = data.filePath;
				std::error_code ec;
				if (!std::filesystem::exists(checkPath, ec) || ec)
				{
					checkPath = key;
					if (!std::filesystem::exists(checkPath, ec) || ec)
					{
						continue;
					}
				}
				auto currentWriteTime = std::filesystem::last_write_time(checkPath, ec);
				if (ec)
				{
					continue;
				}
				if (currentWriteTime > data.lastWriteTime)
				{
					if (reloadCustomShape(checkPath))
					{
						anyReloaded = true;
						if (outReloadedIds)
						{
							outReloadedIds->push_back(data.id);
						}
					}
				}
			}
			return anyReloaded;
		}

		const CustomShapeData* getCustomShapeData(ShapeId id) const
		{
			auto itPath = m_shapeIdToCustomPath.find(id);
			if (itPath == m_shapeIdToCustomPath.end())
			{
				return nullptr;
			}
			auto itData = m_customShapeCache.find(itPath->second);
			if (itData == m_customShapeCache.end())
			{
				return nullptr;
			}
			return &itData->second;
		}

		const CustomShapeData* getCustomShapeData(const std::string& filePath) const
		{
			std::string normPath = normalizePath(filePath);
			auto it = m_customShapeCache.find(normPath);
			if (it != m_customShapeCache.end())
			{
				return &it->second;
			}
			it = m_customShapeCache.find(filePath);
			if (it != m_customShapeCache.end())
			{
				return &it->second;
			}
			return nullptr;
		}

		const std::unordered_map<std::string, CustomShapeData>& getCustomShapes() const
		{
			return m_customShapeCache;
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
		std::unordered_map<std::string, CustomShapeData> m_customShapeCache;
		std::unordered_map<ShapeId, std::string> m_shapeIdToCustomPath;
	};
} // namespace WeirdEngine
