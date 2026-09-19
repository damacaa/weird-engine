#include "model/NodeGraphSerializer.h"

#include <iostream>

namespace WeirdEngine::Editor
{
	json NodeGraphSerializer::serialize(const NodeGraph& graph)
	{
		json j;
		j["nodes"] = json::array();
		for (const auto& [id, node] : graph.getNodes())
		{
			json n;
			n["id"] = node.id;
			n["typeId"] = node.typeId;
			n["posX"] = node.position.x;
			n["posY"] = node.position.y;
			n["customFloat"] = node.data.customFloat;
			n["customVec2X"] = node.data.customVec2.x;
			n["customVec2Y"] = node.data.customVec2.y;
			n["customInt"] = node.data.customInt;
			n["inputFloats"] = node.inputFloats;

			json vec2Arr = json::array();
			for (const auto& v : node.inputVec2s)
			{
				vec2Arr.push_back({v.x, v.y});
			}
			n["inputVec2s"] = vec2Arr;
			j["nodes"].push_back(n);
		}

		j["links"] = json::array();
		for (const auto& link : graph.getLinks())
		{
			json l;
			l["id"] = link.id;
			l["startPinId"] = link.startPinId;
			l["endPinId"] = link.endPinId;
			j["links"].push_back(l);
		}

		j["parameters"] = json::array();
		for (float p : graph.getParameters())
		{
			j["parameters"].push_back(p);
		}

		j["parameterNames"] = json::array();
		for (const auto& name : graph.getParameterNames())
		{
			j["parameterNames"].push_back(name);
		}

		return j;
	}

	bool NodeGraphSerializer::deserialize(NodeGraph& graph, const json& j)
	{
		graph.clear();
		try
		{
			if (j.contains("parameters") && j["parameters"].is_array())
			{
				for (size_t i = 0;
					 i < std::min(graph.getParameters().size(), static_cast<size_t>(j["parameters"].size())); ++i)
				{
					graph.setParameter(i, j["parameters"][i].get<float>());
				}
			}

			if (j.contains("parameterNames") && j["parameterNames"].is_array())
			{
				for (size_t i = 0;
					 i < std::min(graph.getParameterNames().size(), static_cast<size_t>(j["parameterNames"].size()));
					 ++i)
				{
					if (j["parameterNames"][i].is_string())
					{
						graph.setParameterName(i, j["parameterNames"][i].get<std::string>());
					}
				}
			}

			int nextNodeId = 1;
			int nextLinkId = 1;
			int outputNodeId = -1;

			if (j.contains("nodes") && j["nodes"].is_array())
			{
				for (const auto& n : j["nodes"])
				{
					std::string typeId = n["typeId"].get<std::string>();
					const NodeDef* def = NodeRegistry::get().findDef(typeId);
					if (!def)
						continue;

					NodeInstance node;
					node.id = n["id"].get<int>();
					node.typeId = typeId;
					node.position = {n["posX"].get<float>(), n["posY"].get<float>()};
					node.data.customFloat = n["customFloat"].get<float>();
					node.data.customVec2 = {n["customVec2X"].get<float>(), n["customVec2Y"].get<float>()};
					node.data.customInt = n["customInt"].get<int>();

					if (n.contains("inputFloats") && n["inputFloats"].is_array())
					{
						node.inputFloats = n["inputFloats"].get<std::vector<float>>();
					}
					else
					{
						node.inputFloats.resize(def->inputs.size(), 0.0f);
					}

					if (n.contains("inputVec2s") && n["inputVec2s"].is_array())
					{
						for (const auto& v : n["inputVec2s"])
						{
							if (v.is_array() && v.size() >= 2)
								node.inputVec2s.push_back({v[0].get<float>(), v[1].get<float>()});
						}
					}
					node.inputVec2s.resize(def->inputs.size(), {0.0f, 0.0f});

					if (node.id >= nextNodeId)
						nextNodeId = node.id + 1;
					if (def->category == NodeCategory::Output)
						outputNodeId = node.id;

					graph.getNodes()[node.id] = std::move(node);
				}
			}

			if (j.contains("links") && j["links"].is_array())
			{
				for (const auto& l : j["links"])
				{
					LinkInstance link;
					link.id = l["id"].get<int>();
					link.startPinId = l["startPinId"].get<int>();
					link.endPinId = l["endPinId"].get<int>();

					if (link.id >= nextLinkId)
						nextLinkId = link.id + 1;

					graph.getLinks().push_back(link);
				}
			}

			graph.setNextNodeId(nextNodeId);
			graph.setNextLinkId(nextLinkId);
			graph.setOutputNodeId(outputNodeId);
			return true;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Failed to deserialize NodeGraph: " << e.what() << std::endl;
			return false;
		}
	}
} // namespace WeirdEngine::Editor
