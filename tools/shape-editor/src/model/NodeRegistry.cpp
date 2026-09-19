#include "model/NodeRegistry.h"

namespace WeirdEngine::Editor
{
	NodeRegistry& NodeRegistry::get()
	{
		static NodeRegistry instance;
		return instance;
	}

	NodeRegistry::NodeRegistry()
	{
		registerAllNodes();
	}

	void NodeRegistry::registerNode(NodeDef def)
	{
		m_defs[def.typeId] = std::move(def);
	}

	const NodeDef* NodeRegistry::findDef(const std::string& typeId) const
	{
		// Map legacy point node types to canonical "point"
		if (typeId == "song_point" || typeId == "world_point" || typeId == "ui_point" || typeId == "local_point")
		{
			auto it = m_defs.find("point");
			if (it != m_defs.end())
				return &it->second;
		}
		if (typeId == "modulo")
		{
			auto it = m_defs.find("mod");
			if (it != m_defs.end())
				return &it->second;
		}
		auto it = m_defs.find(typeId);
		if (it != m_defs.end())
			return &it->second;
		return nullptr;
	}

	const std::map<std::string, NodeDef>& NodeRegistry::getAllDefs() const
	{
		return m_defs;
	}

	std::vector<const NodeDef*> NodeRegistry::getDefsByCategory(NodeCategory cat) const
	{
		std::vector<const NodeDef*> list;
		for (const auto& [_, def] : m_defs)
		{
			if (def.category == cat)
				list.push_back(&def);
		}
		return list;
	}
} // namespace WeirdEngine::Editor
