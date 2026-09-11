#pragma once

#include <map>
#include <string>
#include <vector>

#include "model/NodeTypes.h"

namespace WeirdEngine::Editor
{
	class NodeRegistry
	{
	public:
		static NodeRegistry& get();

		void registerNode(NodeDef def);
		const NodeDef* findDef(const std::string& typeId) const;
		const std::map<std::string, NodeDef>& getAllDefs() const;
		std::vector<const NodeDef*> getDefsByCategory(NodeCategory cat) const;

	private:
		NodeRegistry();
		void registerAllNodes();

		std::map<std::string, NodeDef> m_defs;
	};
} // namespace WeirdEngine::Editor
