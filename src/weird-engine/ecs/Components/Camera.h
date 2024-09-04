#pragma once
#include "../Component.h"


namespace ECS
{
	struct Camera : public Component {

		WeirdRenderer::Camera m_camera;

		Camera() : m_camera(vec3(0.0f)) {

		}

		Camera(vec3 v) : m_camera(v) {

		}


	};
}
