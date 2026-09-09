#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <json/json.h>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "NodeRegistry.h"

namespace WeirdEngine::Editor
{
	using json = nlohmann::json;

	struct LinkInstance
	{
		int id = 0;
		int startPinId = 0; // Output pin
		int endPinId = 0;	// Input pin
	};

	inline int makePinId(int nodeId, bool isInput, int pinIndex)
	{
		return (nodeId * 100) + (isInput ? 0 : 50) + pinIndex;
	}

	inline void decomposePinId(int pinId, int& outNodeId, bool& outIsInput, int& outPinIndex)
	{
		outNodeId = pinId / 100;
		int remainder = pinId % 100;
		outIsInput = (remainder < 50);
		outPinIndex = outIsInput ? remainder : (remainder - 50);
	}

	class NodeGraph
	{
	public:
		NodeGraph()
		{
			m_nextNodeId = 1;
			m_nextLinkId = 1;
		}

		void clear()
		{
			m_nodes.clear();
			m_links.clear();
			m_outputNodeId = -1;
			m_nextNodeId = 1;
			m_nextLinkId = 1;
		}

		int addNode(const std::string& typeId, glm::vec2 pos = {200.0f, 200.0f})
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

		void removeNode(int nodeId)
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

		bool addLink(int startPinId, int endPinId)
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

		void removeLink(int linkId)
		{
			for (auto it = m_links.begin(); it != m_links.end(); ++it)
			{
				if (it->id == linkId)
				{
					m_links.erase(it);
					return;
				}
			}
		}

		NodeInstance* getNode(int id)
		{
			auto it = m_nodes.find(id);
			return it != m_nodes.end() ? &it->second : nullptr;
		}

		const NodeInstance* getNode(int id) const
		{
			auto it = m_nodes.find(id);
			return it != m_nodes.end() ? &it->second : nullptr;
		}

		std::unordered_map<int, NodeInstance>& getNodes()
		{
			return m_nodes;
		}
		const std::unordered_map<int, NodeInstance>& getNodes() const
		{
			return m_nodes;
		}

		std::vector<LinkInstance>& getLinks()
		{
			return m_links;
		}
		const std::vector<LinkInstance>& getLinks() const
		{
			return m_links;
		}

		int getOutputNodeId() const
		{
			return m_outputNodeId;
		}

		// ---------------------------------------------------------------------
		// AST Evaluation
		// ---------------------------------------------------------------------
		Expr evaluate()
		{
			if (m_outputNodeId <= 0)
			{
				// Find first output node if not recorded
				for (const auto& [id, node] : m_nodes)
				{
					if (node.typeId == "sdf_output")
					{
						m_outputNodeId = id;
						break;
					}
				}
			}

			if (m_outputNodeId <= 0)
			{
				return Expr(0.0f);
			}

			std::unordered_set<int> visited;
			NodeValue val = evaluateNode(m_outputNodeId, visited);
			return asFloat(val);
		}

		std::string generateCppCode() const
		{
			if (m_outputNodeId <= 0)
				return "// No SDF Output node in graph";

			std::unordered_set<int> visited;
			std::vector<std::string> lines;
			std::string exprCode = generateNodeCpp(m_outputNodeId, visited, lines);

			std::ostringstream ss;
			ss << "// Generated Weird Engine SDF Expression\n";
			ss << "using namespace WeirdEngine;\n";
			ss << "using namespace WeirdEngine::SDF;\n\n";
			for (const auto& line : lines)
			{
				ss << line << "\n";
			}
			ss << "Expr finalShape = " << exprCode << ";\n";
			return ss.str();
		}

		// ---------------------------------------------------------------------
		// JSON Serialization
		// ---------------------------------------------------------------------
		json serialize() const
		{
			json j;
			j["nodes"] = json::array();
			for (const auto& [id, node] : m_nodes)
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
			for (const auto& link : m_links)
			{
				json l;
				l["id"] = link.id;
				l["startPinId"] = link.startPinId;
				l["endPinId"] = link.endPinId;
				j["links"].push_back(l);
			}

			return j;
		}

		bool deserialize(const json& j)
		{
			clear();
			try
			{
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

						if (node.id >= m_nextNodeId)
							m_nextNodeId = node.id + 1;
						if (def->category == NodeCategory::Output)
							m_outputNodeId = node.id;

						m_nodes[node.id] = std::move(node);
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

						if (link.id >= m_nextLinkId)
							m_nextLinkId = link.id + 1;

						m_links.push_back(link);
					}
				}
				return true;
			}
			catch (const std::exception& e)
			{
				std::cerr << "Failed to deserialize NodeGraph: " << e.what() << std::endl;
				return false;
			}
		}

		// ---------------------------------------------------------------------
		// Automatic Layered DAG Graph Layout
		// ---------------------------------------------------------------------
		void autoLayout(float startX = 60.0f, float startY = 80.0f, float colSpacing = 250.0f,
						float rowSpacing = 120.0f)
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

		// ---------------------------------------------------------------------
		// Preset Templates
		// ---------------------------------------------------------------------
		void loadPresetCircle()
		{
			clear();
			int pId = addNode("point", {100.0f, 200.0f});
			int rId = addNode("const_float", {100.0f, 320.0f});
			if (auto r = getNode(rId))
				r->data.customFloat = 25.0f;

			int cId = addNode("circle", {340.0f, 220.0f});
			int outId = addNode("sdf_output", {580.0f, 220.0f});

			addLink(makePinId(pId, false, 0), makePinId(cId, true, 0));
			addLink(makePinId(rId, false, 0), makePinId(cId, true, 1));
			addLink(makePinId(cId, false, 0), makePinId(outId, true, 0));

			autoLayout();
		}

		void loadPresetMandalaStar()
		{
			clear();
			int pId = addNode("point", {80.0f, 150.0f});
			int radId = addNode("const_float", {80.0f, 260.0f});
			if (auto r = getNode(radId))
				r->data.customFloat = 30.0f;

			int dispId = addNode("const_float", {80.0f, 360.0f});
			if (auto d = getNode(dispId))
				d->data.customFloat = 6.0f;

			int ptsId = addNode("const_float", {80.0f, 460.0f});
			if (auto pts = getNode(ptsId))
				pts->data.customFloat = 6.0f;

			int spdId = addNode("const_float", {80.0f, 560.0f});
			if (auto spd = getNode(spdId))
				spd->data.customFloat = 0.5f;

			int starId = addNode("star", {320.0f, 200.0f});
			int outId = addNode("sdf_output", {580.0f, 200.0f});

			addLink(makePinId(pId, false, 0), makePinId(starId, true, 0));
			addLink(makePinId(radId, false, 0), makePinId(starId, true, 1));
			addLink(makePinId(dispId, false, 0), makePinId(starId, true, 2));
			addLink(makePinId(ptsId, false, 0), makePinId(starId, true, 3));
			addLink(makePinId(spdId, false, 0), makePinId(starId, true, 4));
			addLink(makePinId(starId, false, 0), makePinId(outId, true, 0));

			autoLayout();
		}

		void loadPresetAquaticWave()
		{
			clear();
			int pId = addNode("point", {80.0f, 180.0f});

			int waveId = addNode("sine_wave", {320.0f, 120.0f});
			if (auto w = getNode(waveId))
			{
				w->inputFloats[1] = 15.0f; // amplitude
				w->inputFloats[2] = 0.04f; // frequency
				w->inputFloats[3] = 0.6f;  // speed
				w->inputFloats[4] = 0.0f;  // offset
			}

			int bubbleId = addNode("circle", {320.0f, 320.0f});
			if (auto b = getNode(bubbleId))
			{
				b->inputFloats[1] = 18.0f; // radius
			}

			int blendId = addNode("smooth_union", {560.0f, 220.0f});
			if (auto bl = getNode(blendId))
			{
				bl->inputFloats[2] = 6.0f; // smooth radius
			}

			int outId = addNode("sdf_output", {780.0f, 220.0f});

			addLink(makePinId(pId, false, 0), makePinId(waveId, true, 0));
			addLink(makePinId(pId, false, 0), makePinId(bubbleId, true, 0));
			addLink(makePinId(waveId, false, 0), makePinId(blendId, true, 0));
			addLink(makePinId(bubbleId, false, 0), makePinId(blendId, true, 1));
			addLink(makePinId(blendId, false, 0), makePinId(outId, true, 0));

			autoLayout();
		}

		void loadPresetCsgRing()
		{
			clear();
			int pId = addNode("point", {80.0f, 200.0f});

			int outerId = addNode("circle", {320.0f, 120.0f});
			if (auto c = getNode(outerId))
				c->inputFloats[1] = 28.0f; // outer radius

			int innerId = addNode("circle", {320.0f, 280.0f});
			if (auto c = getNode(innerId))
				c->inputFloats[1] = 18.0f; // inner radius

			int subId = addNode("subtract", {550.0f, 200.0f});
			int outId = addNode("sdf_output", {760.0f, 200.0f});

			addLink(makePinId(pId, false, 0), makePinId(outerId, true, 0));
			addLink(makePinId(pId, false, 0), makePinId(innerId, true, 0));
			addLink(makePinId(outerId, false, 0), makePinId(subId, true, 0));
			addLink(makePinId(innerId, false, 0), makePinId(subId, true, 1));
			addLink(makePinId(subId, false, 0), makePinId(outId, true, 0));

			autoLayout();
		}

	private:
		NodeValue evaluateNode(int nodeId, std::unordered_set<int>& visited)
		{
			// Cycle detection
			if (visited.count(nodeId))
			{
				return NodeValue(Expr(0.0f));
			}
			visited.insert(nodeId);

			const NodeInstance* node = getNode(nodeId);
			if (!node)
				return NodeValue(Expr(0.0f));

			const NodeDef* def = NodeRegistry::get().findDef(node->typeId);
			if (!def)
				return NodeValue(Expr(0.0f));

			std::vector<NodeValue> evaluatedInputs;
			evaluatedInputs.reserve(def->inputs.size());

			for (size_t inIdx = 0; inIdx < def->inputs.size(); ++inIdx)
			{
				int pinId = makePinId(nodeId, true, static_cast<int>(inIdx));
				const LinkInstance* incomingLink = nullptr;
				for (const auto& link : m_links)
				{
					if (link.endPinId == pinId)
					{
						incomingLink = &link;
						break;
					}
				}

				if (incomingLink)
				{
					int upstreamNodeId, upstreamPinIdx;
					bool upstreamIsInput;
					decomposePinId(incomingLink->startPinId, upstreamNodeId, upstreamIsInput, upstreamPinIdx);

					evaluatedInputs.push_back(evaluateNode(upstreamNodeId, visited));
				}
				else
				{
					// Use local default or overridden input value
					if (def->inputs[inIdx].type == PinType::Float)
					{
						float v = (inIdx < node->inputFloats.size()) ? node->inputFloats[inIdx]
																	 : def->inputs[inIdx].defaultFloat;
						evaluatedInputs.push_back(NodeValue(Expr(v)));
					}
					else
					{
						glm::vec2 v = (inIdx < node->inputVec2s.size()) ? node->inputVec2s[inIdx]
																		: def->inputs[inIdx].defaultVec2;
						evaluatedInputs.push_back(NodeValue(Vec2Expr(v.x, v.y)));
					}
				}
			}

			visited.erase(nodeId);

			if (def->evaluate)
			{
				return def->evaluate(evaluatedInputs, *node);
			}

			return NodeValue(Expr(0.0f));
		}

		std::string generateNodeCpp(int nodeId, std::unordered_set<int>& visited, std::vector<std::string>& lines) const
		{
			if (visited.count(nodeId))
				return "/* cycle */";
			visited.insert(nodeId);

			const NodeInstance* node = getNode(nodeId);
			if (!node)
				return "0.0f";

			const NodeDef* def = NodeRegistry::get().findDef(node->typeId);
			if (!def)
				return "0.0f";

			std::vector<std::string> inStrs;
			for (size_t inIdx = 0; inIdx < def->inputs.size(); ++inIdx)
			{
				int pinId = makePinId(nodeId, true, static_cast<int>(inIdx));
				const LinkInstance* incomingLink = nullptr;
				for (const auto& link : m_links)
				{
					if (link.endPinId == pinId)
					{
						incomingLink = &link;
						break;
					}
				}

				if (incomingLink)
				{
					int upNodeId, upPinIdx;
					bool upIsInput;
					decomposePinId(incomingLink->startPinId, upNodeId, upIsInput, upPinIdx);
					inStrs.push_back(generateNodeCpp(upNodeId, visited, lines));
				}
				else
				{
					if (def->inputs[inIdx].type == PinType::Float)
					{
						float v = (inIdx < node->inputFloats.size()) ? node->inputFloats[inIdx]
																	 : def->inputs[inIdx].defaultFloat;
						inStrs.push_back(std::to_string(v) + "f");
					}
					else
					{
						glm::vec2 v = (inIdx < node->inputVec2s.size()) ? node->inputVec2s[inIdx]
																		: def->inputs[inIdx].defaultVec2;
						inStrs.push_back("Vec2Expr(" + std::to_string(v.x) + "f, " + std::to_string(v.y) + "f)");
					}
				}
			}

			visited.erase(nodeId);

			if (def->toCppCode)
			{
				return def->toCppCode(inStrs, *node);
			}
			return "0.0f";
		}

		std::unordered_map<int, NodeInstance> m_nodes;
		std::vector<LinkInstance> m_links;
		int m_outputNodeId = -1;
		int m_nextNodeId = 1;
		int m_nextLinkId = 1;
	};
} // namespace WeirdEngine::Editor
