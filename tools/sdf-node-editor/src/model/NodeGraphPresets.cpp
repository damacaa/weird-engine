#include "model/NodeGraphPresets.h"

namespace WeirdEngine::Editor
{
	void NodeGraphPresets::loadCircle(NodeGraph& graph)
	{
		graph.clear();
		graph.setParameter(0, 25.0f); // var0: radius
		graph.setParameterName(0, "Radius");

		int pId = graph.addNode("point", {100.0f, 200.0f});
		int v0Id = graph.addNode("param_var", {100.0f, 320.0f});
		if (auto v0 = graph.getNode(v0Id))
			v0->data.customInt = 0;

		int cId = graph.addNode("circle", {340.0f, 220.0f});
		int outId = graph.addNode("sdf_output", {580.0f, 220.0f});

		graph.addLink(makePinId(pId, false, 0), makePinId(cId, true, 0));
		graph.addLink(makePinId(v0Id, false, 0), makePinId(cId, true, 1));
		graph.addLink(makePinId(cId, false, 0), makePinId(outId, true, 0));

		graph.autoLayout();
	}

	void NodeGraphPresets::loadStar(NodeGraph& graph)
	{
		graph.clear();
		graph.setParameter(0, 30.0f); // var0: radius
		graph.setParameter(1, 6.0f);  // var1: displacement
		graph.setParameter(2, 6.0f);  // var2: points
		graph.setParameter(3, 0.5f);  // var3: speed
		graph.setParameterName(0, "Radius");
		graph.setParameterName(1, "Displace");
		graph.setParameterName(2, "Points");
		graph.setParameterName(3, "Speed");

		int pId = graph.addNode("point", {80.0f, 150.0f});

		int v0Id = graph.addNode("param_var", {80.0f, 260.0f});
		if (auto v0 = graph.getNode(v0Id))
			v0->data.customInt = 0;

		int v1Id = graph.addNode("param_var", {80.0f, 360.0f});
		if (auto v1 = graph.getNode(v1Id))
			v1->data.customInt = 1;

		int v2Id = graph.addNode("param_var", {80.0f, 460.0f});
		if (auto v2 = graph.getNode(v2Id))
			v2->data.customInt = 2;

		int v3Id = graph.addNode("param_var", {80.0f, 560.0f});
		if (auto v3 = graph.getNode(v3Id))
			v3->data.customInt = 3;

		int starId = graph.addNode("star", {320.0f, 200.0f});
		int outId = graph.addNode("sdf_output", {580.0f, 200.0f});

		graph.addLink(makePinId(pId, false, 0), makePinId(starId, true, 0));
		graph.addLink(makePinId(v0Id, false, 0), makePinId(starId, true, 1));
		graph.addLink(makePinId(v1Id, false, 0), makePinId(starId, true, 2));
		graph.addLink(makePinId(v2Id, false, 0), makePinId(starId, true, 3));
		graph.addLink(makePinId(v3Id, false, 0), makePinId(starId, true, 4));
		graph.addLink(makePinId(starId, false, 0), makePinId(outId, true, 0));

		graph.autoLayout();
	}

	void NodeGraphPresets::loadAquaticWave(NodeGraph& graph)
	{
		graph.clear();
		graph.setParameter(0, 15.0f); // var0: wave amplitude
		graph.setParameter(1, 0.04f); // var1: wave frequency
		graph.setParameter(2, 0.6f);  // var2: wave speed
		graph.setParameter(3, 0.0f);  // var3: wave offset
		graph.setParameter(4, 18.0f); // var4: bubble radius
		graph.setParameter(5, 6.0f);  // var5: smooth union radius
		graph.setParameterName(0, "Amplitude");
		graph.setParameterName(1, "Frequency");
		graph.setParameterName(2, "Speed");
		graph.setParameterName(3, "Offset");
		graph.setParameterName(4, "BubbleRad");
		graph.setParameterName(5, "Smooth");

		int pId = graph.addNode("point", {80.0f, 180.0f});

		int v0Id = graph.addNode("param_var", {80.0f, 260.0f});
		if (auto v0 = graph.getNode(v0Id))
			v0->data.customInt = 0;

		int v1Id = graph.addNode("param_var", {80.0f, 340.0f});
		if (auto v1 = graph.getNode(v1Id))
			v1->data.customInt = 1;

		int v2Id = graph.addNode("param_var", {80.0f, 420.0f});
		if (auto v2 = graph.getNode(v2Id))
			v2->data.customInt = 2;

		int v3Id = graph.addNode("param_var", {80.0f, 500.0f});
		if (auto v3 = graph.getNode(v3Id))
			v3->data.customInt = 3;

		int v4Id = graph.addNode("param_var", {80.0f, 580.0f});
		if (auto v4 = graph.getNode(v4Id))
			v4->data.customInt = 4;

		int v5Id = graph.addNode("param_var", {80.0f, 660.0f});
		if (auto v5 = graph.getNode(v5Id))
			v5->data.customInt = 5;

		int waveId = graph.addNode("sine_wave", {320.0f, 120.0f});
		int bubbleId = graph.addNode("circle", {320.0f, 320.0f});
		int blendId = graph.addNode("smooth_union", {560.0f, 220.0f});
		int outId = graph.addNode("sdf_output", {780.0f, 220.0f});

		graph.addLink(makePinId(pId, false, 0), makePinId(waveId, true, 0));
		graph.addLink(makePinId(v0Id, false, 0), makePinId(waveId, true, 1));
		graph.addLink(makePinId(v1Id, false, 0), makePinId(waveId, true, 2));
		graph.addLink(makePinId(v2Id, false, 0), makePinId(waveId, true, 3));
		graph.addLink(makePinId(v3Id, false, 0), makePinId(waveId, true, 4));

		graph.addLink(makePinId(pId, false, 0), makePinId(bubbleId, true, 0));
		graph.addLink(makePinId(v4Id, false, 0), makePinId(bubbleId, true, 1));

		graph.addLink(makePinId(waveId, false, 0), makePinId(blendId, true, 0));
		graph.addLink(makePinId(bubbleId, false, 0), makePinId(blendId, true, 1));
		graph.addLink(makePinId(v5Id, false, 0), makePinId(blendId, true, 2));

		graph.addLink(makePinId(blendId, false, 0), makePinId(outId, true, 0));

		graph.autoLayout();
	}

	void NodeGraphPresets::loadCsgRing(NodeGraph& graph)
	{
		graph.clear();
		graph.setParameter(0, 28.0f); // var0: outer radius
		graph.setParameter(1, 18.0f); // var1: inner radius
		graph.setParameterName(0, "OuterRad");
		graph.setParameterName(1, "InnerRad");

		int pId = graph.addNode("point", {80.0f, 200.0f});

		int v0Id = graph.addNode("param_var", {80.0f, 280.0f});
		if (auto v0 = graph.getNode(v0Id))
			v0->data.customInt = 0;

		int v1Id = graph.addNode("param_var", {80.0f, 360.0f});
		if (auto v1 = graph.getNode(v1Id))
			v1->data.customInt = 1;

		int outerId = graph.addNode("circle", {320.0f, 120.0f});
		int innerId = graph.addNode("circle", {320.0f, 280.0f});
		int subId = graph.addNode("subtract", {550.0f, 200.0f});
		int outId = graph.addNode("sdf_output", {760.0f, 200.0f});

		graph.addLink(makePinId(pId, false, 0), makePinId(outerId, true, 0));
		graph.addLink(makePinId(v0Id, false, 0), makePinId(outerId, true, 1));

		graph.addLink(makePinId(pId, false, 0), makePinId(innerId, true, 0));
		graph.addLink(makePinId(v1Id, false, 0), makePinId(innerId, true, 1));

		graph.addLink(makePinId(outerId, false, 0), makePinId(subId, true, 0));
		graph.addLink(makePinId(innerId, false, 0), makePinId(subId, true, 1));
		graph.addLink(makePinId(subId, false, 0), makePinId(outId, true, 0));

		graph.autoLayout();
	}

	void NodeGraphPresets::loadInfiniteRepeat(NodeGraph& graph)
	{
		graph.clear();
		graph.setParameter(0, 50.0f); // var0: grid cell spacing (modulo period)
		graph.setParameter(1, 12.0f); // var1: circle radius
		graph.setParameterName(0, "Spacing");
		graph.setParameterName(1, "Radius");

		int pId = graph.addNode("point", {80.0f, 200.0f});

		int v0Id = graph.addNode("param_var", {80.0f, 320.0f});
		if (auto v0 = graph.getNode(v0Id))
			v0->data.customInt = 0;

		int v1Id = graph.addNode("param_var", {80.0f, 440.0f});
		if (auto v1 = graph.getNode(v1Id))
			v1->data.customInt = 1;

		int repId = graph.addNode("repeat", {320.0f, 200.0f});
		int circleId = graph.addNode("circle", {540.0f, 200.0f});
		int outId = graph.addNode("sdf_output", {760.0f, 200.0f});

		// p -> repeat.p
		graph.addLink(makePinId(pId, false, 0), makePinId(repId, true, 0));
		// var0 (spacing) -> repeat.spacing
		graph.addLink(makePinId(v0Id, false, 0), makePinId(repId, true, 1));

		// repeat.out -> circle.p
		graph.addLink(makePinId(repId, false, 0), makePinId(circleId, true, 0));
		// var1 (radius) -> circle.radius
		graph.addLink(makePinId(v1Id, false, 0), makePinId(circleId, true, 1));

		// circle.d -> sdf_output.d
		graph.addLink(makePinId(circleId, false, 0), makePinId(outId, true, 0));

		graph.autoLayout();
	}
} // namespace WeirdEngine::Editor
