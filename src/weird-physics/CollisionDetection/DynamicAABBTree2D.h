#pragma once
#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>

struct AABB {
	float minX, minY;
	float maxX, maxY;

	AABB()
		: minX(std::numeric_limits<float>::max()), minY(std::numeric_limits<float>::max()),
		maxX(std::numeric_limits<float>::lowest()), maxY(std::numeric_limits<float>::lowest()) {}

	AABB(float minX, float minY, float maxX, float maxY)
		: minX(minX), minY(minY), maxX(maxX), maxY(maxY) {}

	bool overlaps(const AABB& other) const {
		return (minX <= other.maxX && maxX >= other.minX) &&
			(minY <= other.maxY && maxY >= other.minY);
	}

	AABB combine(const AABB& other) const {
		return AABB(
			std::min(minX, other.minX), std::min(minY, other.minY),
			std::max(maxX, other.maxX), std::max(maxY, other.maxY)
		);
	}

	float perimeter() const {
		float width = maxX - minX;
		float height = maxY - minY;
		return 2.0f * (width + height);
	}
};

struct TreeNode {
	int parent = -1;
	int left = -1;
	int right = -1;
	int height = 0;
	AABB box;
	int objectIndex = -1; // For leaf nodes, stores the index of the object

	bool isLeaf() const {
		return left == -1 && right == -1;
	}
};

class DynamicAABBTree {
public:
	DynamicAABBTree() : root(-1) {}

	size_t count = 0;

	int insertObject(const AABB& box) {
		int newNode = allocateNode();
		nodes[newNode].box = box;
		nodes[newNode].objectIndex = newNode; // Storing its own index for now

		insertLeaf(newNode);

		count++;
		return newNode;
	}

	void removeObject(int nodeIndex) {
		removeLeaf(nodeIndex);
		freeNode(nodeIndex);
	}

	void updateObject(int nodeIndex, const AABB& newBox) {
		removeLeaf(nodeIndex);
		nodes[nodeIndex].box = newBox;
		insertLeaf(nodeIndex);
	}

	void query(const AABB& queryBox, std::vector<int>& results) const {
		if (root == -1) return;

		std::vector<int> stack;
		stack.push_back(root);

		while (!stack.empty()) {
			int nodeIndex = stack.back();
			stack.pop_back();

			const TreeNode& node = nodes[nodeIndex];
			if (node.box.overlaps(queryBox)) {
				if (node.isLeaf()) {
					results.push_back(node.objectIndex);
				}
				else {
					stack.push_back(node.left);
					stack.push_back(node.right);
				}
			}
		}
	}

	std::vector<TreeNode> nodes;
private:
	int root;

	int allocateNode() {
		nodes.push_back(TreeNode());
		return nodes.size() - 1;
	}

	void freeNode(int index) {
		// For simplicity, we do not actually free nodes in this example
		nodes[index] = TreeNode();
	}

	void insertLeaf(int leaf) {
		if (root == -1) {
			root = leaf;
			nodes[root].parent = -1;
			return;
		}

		// Find the best sibling for the new leaf
		int currentIndex = root;
		while (!nodes[currentIndex].isLeaf()) {
			const TreeNode& current = nodes[currentIndex];

			AABB combinedBox = current.box.combine(nodes[leaf].box);
			float combinedPerimeter = combinedBox.perimeter();
			float cost = 2.0f * combinedPerimeter;

			float inheritanceCost = 2.0f * (combinedPerimeter - current.box.perimeter());

			float costLeft;
			if (nodes[current.left].isLeaf()) {
				costLeft = nodes[current.left].box.combine(nodes[leaf].box).perimeter() + inheritanceCost;
			}
			else {
				float oldPerimeter = nodes[current.left].box.perimeter();
				float newPerimeter = nodes[current.left].box.combine(nodes[leaf].box).perimeter();
				costLeft = (newPerimeter - oldPerimeter) + inheritanceCost;
			}

			float costRight;
			if (nodes[current.right].isLeaf()) {
				costRight = nodes[current.right].box.combine(nodes[leaf].box).perimeter() + inheritanceCost;
			}
			else {
				float oldPerimeter = nodes[current.right].box.perimeter();
				float newPerimeter = nodes[current.right].box.combine(nodes[leaf].box).perimeter();
				costRight = (newPerimeter - oldPerimeter) + inheritanceCost;
			}

			// Determine which side to descend
			if (cost < costLeft && cost < costRight) break;

			currentIndex = (costLeft < costRight) ? current.left : current.right;
		}

		int sibling = currentIndex;
		int oldParent = nodes[sibling].parent;
		int newParent = allocateNode();

		nodes[newParent].parent = oldParent;
		nodes[newParent].box = nodes[leaf].box.combine(nodes[sibling].box);
		nodes[newParent].height = nodes[sibling].height + 1;

		if (oldParent != -1) {
			if (nodes[oldParent].left == sibling) {
				nodes[oldParent].left = newParent;
			}
			else {
				nodes[oldParent].right = newParent;
			}
		}
		else {
			root = newParent;
		}

		nodes[newParent].left = sibling;
		nodes[newParent].right = leaf;
		nodes[sibling].parent = newParent;
		nodes[leaf].parent = newParent;

		// Walk back up the tree, fixing heights and AABBs
		currentIndex = nodes[leaf].parent;
		while (currentIndex != -1) {
			currentIndex = balance(currentIndex);

			int left = nodes[currentIndex].left;
			int right = nodes[currentIndex].right;

			nodes[currentIndex].height = 1 + std::max(nodes[left].height, nodes[right].height);
			nodes[currentIndex].box = nodes[left].box.combine(nodes[right].box);

			currentIndex = nodes[currentIndex].parent;
		}
	}

	void removeLeaf(int leaf) {
		if (leaf == root) {
			root = -1;
			return;
		}

		int parent = nodes[leaf].parent;
		int grandParent = nodes[parent].parent;
		int sibling = (nodes[parent].left == leaf) ? nodes[parent].right : nodes[parent].left;

		if (grandParent != -1) {
			if (nodes[grandParent].left == parent) {
				nodes[grandParent].left = sibling;
			}
			else {
				nodes[grandParent].right = sibling;
			}
			nodes[sibling].parent = grandParent;
			freeNode(parent);

			// Adjust ancestor bounds
			int index = grandParent;
			while (index != -1) {
				index = balance(index);

				int left = nodes[index].left;
				int right = nodes[index].right;

				nodes[index].box = nodes[left].box.combine(nodes[right].box);
				nodes[index].height = 1 + std::max(nodes[left].height, nodes[right].height);

				index = nodes[index].parent;
			}
		}
		else {
			root = sibling;
			nodes[sibling].parent = -1;
			freeNode(parent);
		}
	}

	int balance(int iA) {
		TreeNode& A = nodes[iA];
		if (A.isLeaf() || A.height < 2) {
			return iA;
		}

		int iB = A.left;
		int iC = A.right;
		TreeNode& B = nodes[iB];
		TreeNode& C = nodes[iC];

		int balance = C.height - B.height;

		// Rotate C up
		if (balance > 1) {
			int iF = C.left;
			int iG = C.right;
			TreeNode& F = nodes[iF];
			TreeNode& G = nodes[iG];

			// Swap A and C
			C.left = iA;
			C.parent = A.parent;
			A.parent = iC;

			// Update parent
			if (C.parent != -1) {
				if (nodes[C.parent].left == iA) {
					nodes[C.parent].left = iC;
				}
				else {
					nodes[C.parent].right = iC;
				}
			}
			else {
				root = iC;
			}

			// Rotate
			if (F.height > G.height) {
				C.right = iF;
				A.right = iG;
				G.parent = iA;
				A.box = A.box.combine(G.box);
				A.height = 1 + std::max(B.height, G.height);
				C.box = A.box.combine(F.box);
				C.height = 1 + std::max(A.height, F.height);
			}
			else {
				C.right = iG;
				A.right = iF;
				F.parent = iA;
				A.box = A.box.combine(F.box);
				A.height = 1 + std::max(B.height, F.height);
				C.box = A.box.combine(G.box);
				C.height = 1 + std::max(A.height, G.height);
			}
			return iC;
		}

		// Rotate B up
		if (balance < -1) {
			int iD = B.left;
			int iE = B.right;
			TreeNode& D = nodes[iD];
			TreeNode& E = nodes[iE];

			// Swap A and B
			B.left = iA;
			B.parent = A.parent;
			A.parent = iB;

			// Update parent
			if (B.parent != -1) {
				if (nodes[B.parent].left == iA) {
					nodes[B.parent].left = iB;
				}
				else {
					nodes[B.parent].right = iB;
				}
			}
			else {
				root = iB;
			}

			// Rotate
			if (D.height > E.height) {
				B.right = iD;
				A.left = iE;
				E.parent = iA;
				A.box = A.box.combine(E.box);
				A.height = 1 + std::max(C.height, E.height);
				B.box = A.box.combine(D.box);
				B.height = 1 + std::max(A.height, D.height);
			}
			else {
				B.right = iE;
				A.left = iD;
				D.parent = iA;
				A.box = A.box.combine(D.box);
				A.height = 1 + std::max(C.height, D.height);
				B.box = A.box.combine(E.box);
				B.height = 1 + std::max(A.height, E.height);
			}
			return iB;
		}

		return iA;
	}
};

