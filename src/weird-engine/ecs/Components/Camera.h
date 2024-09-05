#pragma once
#include "../Component.h"


namespace ECS
{
	struct Camera : public Component {

		WeirdRenderer::Camera camera;

		Camera() : camera(vec3(0.0f)) {

		}

		Camera(vec3 v) : camera(v) {

		}


	};
}
