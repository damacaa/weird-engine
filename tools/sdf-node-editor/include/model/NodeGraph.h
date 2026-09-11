#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "model/NodeRegistry.h"
#include "model/NodeTypes.h"

namespace WeirdEngine::Editor
{
	class NodeGraph
	{
	public:
		NodeGraph();

		void clear();

		const std::array<float, 8>& getParameters() const;
		std::array<float, 8>& getParameters();
		void setParameter(size_t i, float val);
		void setParameters(const std::array<float, 8>& p);

		const std::string& getParameterName(size_t i) const;
		void setParameterName(size_t i, const std::string& name);
		const std::array<std::string, 8>& getParameterNames() const;
		void setParameterNames(const std::array<std::string, 8>& names);

		int addNode(const std::string& typeId, glm::vec2 pos = {200.0f, 200.0f});
		void removeNode(int nodeId);

		bool addLink(int startPinId, int endPinId);
		void removeLink(int linkId);

		NodeInstance* getNode(int id);
		const NodeInstance* getNode(int id) const;

		std::unordered_map<int, NodeInstance>& getNodes();
		const std::unordered_map<int, NodeInstance>& getNodes() const;

		std::vector<LinkInstance>& getLinks();
		const std::vector<LinkInstance>& getLinks() const;

		int getOutputNodeId() const;
		void setOutputNodeId(int id);

		int getNextNodeId() const;
		void setNextNodeId(int id);

		int getNextLinkId() const;
		void setNextLinkId(int id);

		void autoLayout(float startX = 60.0f, float startY = 80.0f, float colSpacing = 250.0f,
						float rowSpacing = 120.0f);

	private:
		std::unordered_map<int, NodeInstance> m_nodes;
		std::vector<LinkInstance> m_links;
		int m_outputNodeId = -1;
		int m_nextNodeId = 1;
		int m_nextLinkId = 1;
		std::array<float, 8> m_parameters = {0.0f};
		std::array<std::string, 8> m_parameterNames;
	};
} // namespace WeirdEngine::Editor
