#pragma once
#include <vector>
#include <set>
template <typename T>
class UniformGrid2D
{
private:
	using vec = glm::vec2;
public:

	UniformGrid2D(float worldSize, float cellSize) :
		m_cellSize(cellSize),
		m_invCellSize(1.0f / m_cellSize),
		m_cellsPerSide(worldSize / cellSize),
		m_grid(m_cellsPerSide* m_cellsPerSide)
	{
		m_grid.resize(m_cellsPerSide * m_cellsPerSide);
		for (T i = 0; i < m_grid.size(); ++i)
		{
			m_grid[i] = std::vector<T>();
		}
	}

	void addElement(T id, vec pos)
	{
		int32_t x = pos.x / m_cellSize;
		int32_t y = pos.y / m_cellSize;

		if (y >= m_cellsPerSide || x >= m_cellsPerSide)
			return;

		m_grid[(y * m_cellsPerSide) + x].push_back(id);
	}

	std::vector<T>& getCell(T x, T y)
	{
		return m_grid[(y * m_cellsPerSide) + x];
	}

	int getCellCountPerSide()
	{
		return m_cellsPerSide;
	}

	void clear()
	{
		for (auto& v : m_grid)
		{
			v.clear();
		}
	}

	void getPossibleCollisions(std::vector<T>& vec, int32_t x, int32_t y)
	{
		for (int offsetX = std::max(0, x - 1); offsetX <= std::min(x + 1, (int32_t)m_cellsPerSide - 1); offsetX++)
		{
			for (int offsetY = std::max(0, y - 1); offsetY <= std::min(y + 1, (int32_t)m_cellsPerSide - 1); offsetY++)
			{
				auto& objectsInCell = getCell(offsetX, offsetY);
				if (objectsInCell.size() > 0)
					vec.insert(vec.end(), objectsInCell.begin(), objectsInCell.end());
			}
		}
	}

	void getPossibleCollisions(std::set<T>& vec, int32_t x, int32_t y)
	{
		for (int offsetX = std::max(0, x - 1); offsetX <= std::min(x + 1, (int32_t)m_cellsPerSide - 1); offsetX++)
		{
			for (int offsetY = std::max(0, y - 1); offsetY <= std::min(y + 1, (int32_t)m_cellsPerSide - 1); offsetY++)
			{
				auto& objectsInCell = getCell(offsetX, offsetY);
				/*	if (objectsInCell.size() > 0)
					vec.insert(vec.end(), objectsInCell.begin(), objectsInCell.end());*/

				for (const auto& e : objectsInCell)
				{
					vec.insert(e);
				}
			}
		}
	}

	void getPossibleCollisions(std::unordered_set<T>& vec, int32_t x, int32_t y)
	{
		for (int offsetX = std::max(0, x - 1); offsetX <= std::min(x + 1, (int32_t)m_cellsPerSide - 1); offsetX++)
		{
			for (int offsetY = std::max(0, y - 1); offsetY <= std::min(y + 1, (int32_t)m_cellsPerSide - 1); offsetY++)
			{
				auto& objectsInCell = getCell(offsetX, offsetY);
				/*	if (objectsInCell.size() > 0)
					vec.insert(vec.end(), objectsInCell.begin(), objectsInCell.end());*/

				for (const auto& e : objectsInCell)
				{
					vec.insert(e);
				}
			}
		}
	}

private:
	std::vector<std::vector<T>> m_grid;
	float m_cellSize;
	float m_invCellSize;
	size_t m_cellsPerSide;

};