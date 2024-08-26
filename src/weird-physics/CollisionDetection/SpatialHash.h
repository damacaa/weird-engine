#include <unordered_map>
#include <vector>
#include <cmath>
#include <iostream>
#include<glm/glm.hpp>


using vec3 = glm::vec3;

// Define a hash function for Vector3
struct Vector3Hash {
	std::size_t operator()(const vec3& v) const
	{
		std::size_t h1 = std::hash<int>()(static_cast<int>(v.x));
		std::size_t h2 = std::hash<int>()(static_cast<int>(v.y));
		std::size_t h3 = std::hash<int>()(static_cast<int>(v.z));
		return h1 ^ (h2 << 1) ^ (h3 << 2); // Combine hash values
	}
};


class SpatialHash {
public:
	SpatialHash(float cellSize) : cellSize(cellSize), invCellSize(1.0f / cellSize) {}

	void insert(const vec3& center, float radius, int objectId)
	{
		auto cells = getCells(center, radius);
		for (const auto& cell : cells)
		{
			m_grid[cell].push_back(objectId);
		}
	}

	std::vector<int> retrieve(const vec3& center, float radius) const
	{
		std::vector<int> possibleCollisions;
		auto cells = getCells(center, radius);
		for (const auto& cell : cells)
		{
			auto it = m_grid.find(cell);
			if (it != m_grid.end())
			{
				possibleCollisions.insert(possibleCollisions.end(), it->second.begin(), it->second.end());
			}
		}
		return possibleCollisions;
	}

	void clear()
	{
		m_grid.clear();
	}

private:

	float cellSize;
	float invCellSize;
	std::unordered_map<vec3, std::vector<int>, Vector3Hash> m_grid;

	std::vector<vec3> getCells(const vec3& center, float radius) const
	{
		std::vector<vec3> cells;

		int minX = static_cast<int>(std::floor((center.x - radius) * invCellSize));
		int minY = static_cast<int>(std::floor((center.y - radius) * invCellSize));
		int minZ = static_cast<int>(std::floor((center.z - radius) * invCellSize));
		int maxX = static_cast<int>(std::floor((center.x + radius) * invCellSize));
		int maxY = static_cast<int>(std::floor((center.y + radius) * invCellSize));
		int maxZ = static_cast<int>(std::floor((center.z + radius) * invCellSize));

		int reserveSize = (maxX - minX + 1) * (maxY - minY + 1) * (maxZ - minZ + 1);
		cells.reserve(reserveSize);

		for (int x = minX; x <= maxX; ++x)
		{
			for (int y = minY; y <= maxY; ++y)
			{
				for (int z = minZ; z <= maxZ; ++z)
				{
					cells.push_back(vec3{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) });
				}
			}
		}

		return cells;
	}
};


