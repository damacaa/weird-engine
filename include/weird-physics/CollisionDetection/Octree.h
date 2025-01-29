#pragma once
#include <vector>
#include <iostream>
#include <algorithm>
#include<glm/glm.hpp>

using Vector3 = glm::vec3;

// Define a class for the Octree
class Octree {
public:
	Octree(const Vector3& center, float halfSize, int depth = 0, int maxDepth = 5)
		: m_center(m_center), m_halfSize(halfSize), m_depth(depth), m_maxDepth(maxDepth) {}

	void insert(const Vector3& center, float radius, int objectId) {
		if (m_depth == m_maxDepth || (m_halfSize / 2.0f < radius)) {
			m_objects.push_back({ center, radius, objectId });
		}
		else {
			if (!m_children[0]) {
				subdivide();
			}
			for (auto& child : m_children) {
				if (child->contains(center, radius)) {
					child->insert(center, radius, objectId);
					return;
				}
			}
			m_objects.push_back({ m_center, radius, objectId });
		}
	}

	std::vector<int> retrieve(Vector3 center, float radius) {
		std::vector<int> possibleCollisions;
		if (m_depth == m_maxDepth || (m_halfSize / 2 < radius)) {
			for (const auto& obj : m_objects) {
				possibleCollisions.push_back(obj.id);
			}
		}
		else {
			for (const auto& child : m_children) {
				if (child && child->intersects(center, radius)) {
					auto childCollisions = child->retrieve(center, radius);
					possibleCollisions.insert(possibleCollisions.end(), childCollisions.begin(), childCollisions.end());
				}
			}
		}
		for (const auto& obj : m_objects) {
			possibleCollisions.push_back(obj.id);
		}
		return possibleCollisions;
	}

private:
	struct Object {
		Vector3 m_center;
		float radius;
		int id;
	};

	Vector3 m_center;
	float m_halfSize;
	int m_depth;
	int m_maxDepth;
	std::vector<Object> m_objects;
	std::unique_ptr<Octree> m_children[8];

	bool contains(Vector3 center, float radius) {
		float range = m_halfSize + radius;
		return std::abs(center.x - m_center.x) <= range &&
			std::abs(center.y - m_center.y) <= range &&
			std::abs(center.z - m_center.z) <= range;
	}

	bool intersects(Vector3 center, float radius) {
		float range = m_halfSize + radius;
		return std::abs(center.x - m_center.x) <= range &&
			std::abs(center.y - m_center.y) <= range &&
			std::abs(center.z - m_center.z) <= range;
	}

	void subdivide() {
		float newHalfSize = m_halfSize / 2;
		m_children[0] = std::make_unique<Octree>(m_center + Vector3{ newHalfSize, newHalfSize, newHalfSize }, newHalfSize, m_depth + 1, m_maxDepth);
		m_children[1] = std::make_unique<Octree>(m_center + Vector3{ newHalfSize, newHalfSize, -newHalfSize }, newHalfSize, m_depth + 1, m_maxDepth);
		m_children[2] = std::make_unique<Octree>(m_center + Vector3{ newHalfSize, -newHalfSize, newHalfSize }, newHalfSize, m_depth + 1, m_maxDepth);
		m_children[3] = std::make_unique<Octree>(m_center + Vector3{ newHalfSize, -newHalfSize, -newHalfSize }, newHalfSize, m_depth + 1, m_maxDepth);
		m_children[4] = std::make_unique<Octree>(m_center + Vector3{ -newHalfSize, newHalfSize, newHalfSize }, newHalfSize, m_depth + 1, m_maxDepth);
		m_children[5] = std::make_unique<Octree>(m_center + Vector3{ -newHalfSize, newHalfSize, -newHalfSize }, newHalfSize, m_depth + 1, m_maxDepth);
		m_children[6] = std::make_unique<Octree>(m_center + Vector3{ -newHalfSize, -newHalfSize, newHalfSize }, newHalfSize, m_depth + 1, m_maxDepth);
		m_children[7] = std::make_unique<Octree>(m_center + Vector3{ -newHalfSize, -newHalfSize, -newHalfSize }, newHalfSize, m_depth + 1, m_maxDepth);
	}
};

