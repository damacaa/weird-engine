#pragma once
class UniformGrid2D {

	using vec = glm::vec2;

public:

	UniformGrid2D(float worldSize, float cellSize) :
		m_cellSize(cellSize),
		m_invCellSize(1.0f/ m_cellSize),
		m_cellsPerSide(worldSize / cellSize),
		m_grid(m_cellsPerSide * m_cellsPerSide)
	{
		int a = 0;
		m_grid.resize(m_cellsPerSide * m_cellsPerSide);
		for (size_t i = 0; i < m_grid.size(); ++i) {
			m_grid[i] = std::vector<int>();
		}
	}

	void addElement(int id, vec pos) {
		int x = pos.x / m_cellSize;
		int y = pos.y / m_cellSize;

		if (y >= m_cellsPerSide || x >= m_cellsPerSide)
			return;

		m_grid[(y * m_cellsPerSide) + x].push_back(id);
	}

	std::vector<int>& getCell(int x, int y) {
		return m_grid[(y * m_cellsPerSide) + x];
	}

	int getCellCountPerSide() {
		return m_cellsPerSide;
	}

	void clear() {
		for (auto& v : m_grid) {
			v.clear();
		}
	}

private:
	std::vector<std::vector<int>> m_grid;
	float m_cellSize;
	float m_invCellSize;
	unsigned int m_cellsPerSide;

};