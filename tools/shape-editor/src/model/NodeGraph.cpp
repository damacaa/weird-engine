#include "model/NodeGraph.h"

#include <algorithm>
#include <map>
#include <unordered_set>

namespace WeirdEngine::Editor
{
	NodeGraph::NodeGraph()
	{
		m_nextNodeId = 1;
		m_nextLinkId = 1;
	}

	void NodeGraph::clear()
	{
		m_nodes.clear();
		m_links.clear();
		m_outputNodeId = -1;
		m_nextNodeId = 1;
		m_nextLinkId = 1;
		m_parameters.fill(0.0f);
		m_parameterNames.fill("");
	}

	const std::array<float, 8>& NodeGraph::getParameters() const
	{
		return m_parameters;
	}

	std::array<float, 8>& NodeGraph::getParameters()
	{
		return m_parameters;
	}

	void NodeGraph::setParameter(size_t i, float val)
	{
		if (i < 8)
			m_parameters[i] = val;
	}

	void NodeGraph::setParameters(const std::array<float, 8>& p)
	{
		m_parameters = p;
	}

	const std::string& NodeGraph::getParameterName(size_t i) const
	{
		static const std::string empty;
		if (i < 8)
			return m_parameterNames[i];
		return empty;
	}

	void NodeGraph::setParameterName(size_t i, const std::string& name)
	{
		if (i < 8)
			m_parameterNames[i] = name;
	}

	const std::array<std::string, 8>& NodeGraph::getParameterNames() const
	{
		return m_parameterNames;
	}

	void NodeGraph::setParameterNames(const std::array<std::string, 8>& names)
	{
		m_parameterNames = names;
	}

	int NodeGraph::addNode(const std::string& typeId, glm::vec2 pos)
	{
		const NodeDef* def = NodeRegistry::get().findDef(typeId);
		if (!def)
			return -1;

		int id = m_nextNodeId++;
		NodeInstance node;
		node.id = id;
		node.typeId = typeId;
		node.position = pos;

		// Initialize default values for input pins
		node.inputFloats.resize(def->inputs.size(), 0.0f);
		node.inputVec2s.resize(def->inputs.size(), {0.0f, 0.0f});
		for (size_t i = 0; i < def->inputs.size(); ++i)
		{
			node.inputFloats[i] = def->inputs[i].defaultFloat;
			node.inputVec2s[i] = def->inputs[i].defaultVec2;
		}

		if (def->category == NodeCategory::Output)
		{
			m_outputNodeId = id;
		}

		m_nodes[id] = std::move(node);
		return id;
	}

	void NodeGraph::removeNode(int nodeId)
	{
		// Remove any links connected to this node
		auto it = m_links.begin();
		while (it != m_links.end())
		{
			int startNode, endNode, pinIdx;
			bool isInput;
			decomposePinId(it->startPinId, startNode, isInput, pinIdx);
			decomposePinId(it->endPinId, endNode, isInput, pinIdx);
			if (startNode == nodeId || endNode == nodeId)
			{
				it = m_links.erase(it);
			}
			else
			{
				++it;
			}
		}

		if (m_outputNodeId == nodeId)
		{
			m_outputNodeId = -1;
		}

		m_nodes.erase(nodeId);
	}

	bool NodeGraph::addLink(int startPinId, int endPinId)
	{
		int startNodeId, endNodeId, startPinIdx, endPinIdx;
		bool startIsInput, endIsInput;
		decomposePinId(startPinId, startNodeId, startIsInput, startPinIdx);
		decomposePinId(endPinId, endNodeId, endIsInput, endPinIdx);

		// Start pin must be an output, and end pin must be an input
		if (startIsInput || !endIsInput)
		{
			// Swap if user dragged from input to output
			std::swap(startPinId, endPinId);
			std::swap(startNodeId, endNodeId);
			std::swap(startIsInput, endIsInput);
			std::swap(startPinIdx, endPinIdx);
		}

		if (startIsInput || !endIsInput)
			return false;
		if (startNodeId == endNodeId)
			return false; // No self-loops

		const NodeInstance* startNode = getNode(startNodeId);
		const NodeInstance* endNode = getNode(endNodeId);
		if (!startNode || !endNode)
			return false;

		const NodeDef* startDef = NodeRegistry::get().findDef(startNode->typeId);
		const NodeDef* endDef = NodeRegistry::get().findDef(endNode->typeId);
		if (!startDef || !endDef)
			return false;

		if (static_cast<size_t>(startPinIdx) >= startDef->outputs.size() ||
			static_cast<size_t>(endPinIdx) >= endDef->inputs.size())
			return false;

		// Check type compatibility
		PinType outType = startDef->outputs[startPinIdx].type;
		PinType inType = endDef->inputs[endPinIdx].type;
		if (outType != inType)
			return false;

		// An input pin can only have ONE incoming connection. Remove existing link to endPinId.
		for (auto it = m_links.begin(); it != m_links.end(); ++it)
		{
			if (it->endPinId == endPinId)
			{
				m_links.erase(it);
				break;
			}
		}

		LinkInstance link;
		link.id = m_nextLinkId++;
		link.startPinId = startPinId;
		link.endPinId = endPinId;
		m_links.push_back(link);
		return true;
	}

	void NodeGraph::removeLink(int linkId)
	{
		for (auto it = m_links.begin(); it != m_links.end(); ++it)
		{
			if (it->id == linkId)
			{
				m_links.erase(it);
				break;
			}
		}
	}

	NodeInstance* NodeGraph::getNode(int id)
	{
		auto it = m_nodes.find(id);
		return (it != m_nodes.end()) ? &it->second : nullptr;
	}

	const NodeInstance* NodeGraph::getNode(int id) const
	{
		auto it = m_nodes.find(id);
		return (it != m_nodes.end()) ? &it->second : nullptr;
	}

	std::unordered_map<int, NodeInstance>& NodeGraph::getNodes()
	{
		return m_nodes;
	}

	const std::unordered_map<int, NodeInstance>& NodeGraph::getNodes() const
	{
		return m_nodes;
	}

	std::vector<LinkInstance>& NodeGraph::getLinks()
	{
		return m_links;
	}

	const std::vector<LinkInstance>& NodeGraph::getLinks() const
	{
		return m_links;
	}

	int NodeGraph::getOutputNodeId() const
	{
		return m_outputNodeId;
	}

	void NodeGraph::setOutputNodeId(int id)
	{
		m_outputNodeId = id;
	}

	int NodeGraph::getNextNodeId() const
	{
		return m_nextNodeId;
	}

	void NodeGraph::setNextNodeId(int id)
	{
		m_nextNodeId = id;
	}

	int NodeGraph::getNextLinkId() const
	{
		return m_nextLinkId;
	}

	void NodeGraph::setNextLinkId(int id)
	{
		m_nextLinkId = id;
	}

	void NodeGraph::autoLayout(float startX, float startY, float colSpacing, float rowSpacing)
	{
		if (m_nodes.empty())
			return;

		// 1. Build link connectivity graph
		std::unordered_map<int, int> inDegree;
		for (const auto& [id, node] : m_nodes)
		{
			inDegree[id] = 0;
		}

		for (const auto& link : m_links)
		{
			int upNodeId, upPinIdx;
			bool upIsInput;
			decomposePinId(link.startPinId, upNodeId, upIsInput, upPinIdx);

			int downNodeId, downPinIdx;
			bool downIsInput;
			decomposePinId(link.endPinId, downNodeId, downIsInput, downPinIdx);

			if (m_nodes.count(upNodeId) && m_nodes.count(downNodeId))
			{
				inDegree[downNodeId]++;
			}
		}

		// 2. Layer calculation using longest-path relaxation
		std::unordered_map<int, int> layerMap;
		for (const auto& [id, node] : m_nodes)
		{
			layerMap[id] = 0;
		}

		for (size_t iter = 0; iter < m_nodes.size(); ++iter)
		{
			bool changed = false;
			for (const auto& link : m_links)
			{
				int upId, upPin;
				bool upIn;
				decomposePinId(link.startPinId, upId, upIn, upPin);

				int downId, downPin;
				bool downIn;
				decomposePinId(link.endPinId, downId, downIn, downPin);

				if (m_nodes.count(upId) && m_nodes.count(downId))
				{
					if (layerMap[downId] <= layerMap[upId])
					{
						layerMap[downId] = layerMap[upId] + 1;
						changed = true;
					}
				}
			}
			if (!changed)
				break;
		}

		// Output terminal always belongs in the far right column
		int maxLayer = 0;
		for (const auto& [id, l] : layerMap)
		{
			if (id != m_outputNodeId && l > maxLayer)
				maxLayer = l;
		}
		if (m_outputNodeId > 0 && m_nodes.count(m_outputNodeId))
		{
			layerMap[m_outputNodeId] = maxLayer + 1;
		}

		// 3. Group nodes by layer column
		std::map<int, std::vector<int>> layerNodes;
		for (const auto& [id, l] : layerMap)
		{
			layerNodes[l].push_back(id);
		}

		// 4. Calculate layout coordinates with balanced vertical centering
		float maxColH = 0.0f;
		for (const auto& [layer, nodesInCol] : layerNodes)
		{
			float colH = static_cast<float>(nodesInCol.size()) * rowSpacing;
			if (colH > maxColH)
				maxColH = colH;
		}

		for (const auto& [layer, nodesInCol] : layerNodes)
		{
			float colX = startX + static_cast<float>(layer) * colSpacing;
			float colH = static_cast<float>(nodesInCol.size()) * rowSpacing;
			float colStartY = startY + (maxColH - colH) * 0.5f;

			for (size_t row = 0; row < nodesInCol.size(); ++row)
			{
				int id = nodesInCol[row];
				float y = colStartY + static_cast<float>(row) * rowSpacing;
				m_nodes[id].position = {colX, y};
			}
		}
	}
} // namespace WeirdEngine::Editor
