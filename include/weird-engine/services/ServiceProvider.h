#pragma once

#include "weird-engine/services/AudioService.h"
#include "weird-engine/services/DebugService.h"
#include "weird-engine/services/InputService.h"
#include "weird-engine/services/Material2DService.h"
#include "weird-engine/services/Material3DService.h"
#include "weird-engine/services/PhysicsService.h"
#include "weird-engine/services/RandomService.h"
#include "weird-engine/services/RenderService.h"
#include "weird-engine/services/ResourceService.h"
#include "weird-engine/services/SceneControlService.h"
#include "weird-engine/services/SerializationService.h"
#include "weird-engine/services/ShapeService.h"
#include "weird-engine/services/TagService.h"
#include "weird-engine/services/TimeService.h"

namespace WeirdEngine
{
	class Scene;

	// Central access point for all non-ECS scene functionality. Systems take
	// a ServiceProvider& (plus the Registry&) and use it instead of reaching
	// into Scene internals. Owned by Scene, which binds it to its storage.
	// Each service is a self-contained class; include the specific service
	// header (e.g. services/PhysicsService.h) instead of this aggregate when
	// only one service is needed.
	class ServiceProvider
	{
	public:
		explicit ServiceProvider(Scene& scene);

		Registry& registry()
		{
			return m_registry;
		}

		TimeService& time()
		{
			return m_time;
		}

		PhysicsService& physics()
		{
			return m_physics;
		}

		ShapeService& shapes()
		{
			return m_shapes;
		}

		RenderService& render()
		{
			return m_render;
		}

		Material2DService& materials2D()
		{
			return m_materials2D;
		}

		Material3DService& materials3D()
		{
			return m_materials3D;
		}

		AudioService& audio()
		{
			return m_audio;
		}

		TagService& tags()
		{
			return m_tags;
		}

		SerializationService& serialization()
		{
			return m_serialization;
		}

		SceneControlService& sceneControl()
		{
			return m_sceneControl;
		}

		ResourceService& resources()
		{
			return m_resources;
		}

		DebugService& debug()
		{
			return m_debug;
		}

		InputService& input()
		{
			return m_input;
		}

		RandomService& random()
		{
			return m_random;
		}

	private:
		Registry& m_registry;
		TimeService m_time;
		PhysicsService m_physics;
		ShapeService m_shapes;
		RenderService m_render;
		Material2DService m_materials2D;
		Material3DService m_materials3D;
		AudioService m_audio;
		TagService m_tags;
		SerializationService m_serialization;
		SceneControlService m_sceneControl;
		ResourceService m_resources;
		DebugService m_debug;
		InputService m_input;
		RandomService m_random;
	};
} // namespace WeirdEngine
