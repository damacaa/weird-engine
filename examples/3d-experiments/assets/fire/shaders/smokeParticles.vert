#version 330 core

// Vertex attributes
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_texCoord;

// Outputs to fragment shader
out vec3 v_worldPos;
out vec3 v_normal;
out vec3 v_color;
out vec2 v_texCoord;

out float v_delta;

// Uniforms
uniform mat4 u_model;
uniform mat4 u_camMatrix;
uniform mat3 u_normalMatrix;

uniform float u_time;

// ChatGPT: I want a function that, for a given x, increases but then plateaus
float plateauFunction(float x, float maxValue, float growthRate) 
{
    return maxValue * (1.0 - exp(-growthRate * x));
}

float rand(float x)
{
    return fract(sin(x) * 43758.5453123);
}

void main()
{
	// Animation delta
	float delta = fract((gl_InstanceID * 123.45678f) + (0.1f * u_time));

	// Position
	vec3 offset = vec3(
		delta * 5.0f * (rand(gl_InstanceID * 1.2345) - 0.5f), 
		delta * (7.0 + (3.0 * rand(gl_InstanceID * 1.2345))), 
		delta * 5.0f * (rand(gl_InstanceID * 0.9876) - 0.5f)
	);

	// Scale
	float scale = 1.0 + (3.0 * delta);

    v_worldPos = vec3(u_model * vec4(scale * in_position, 1.0)) + offset;
    v_normal   = u_normalMatrix * in_normal;
    v_color    = in_color;
    v_texCoord = in_texCoord;

	// Pass delta to fragment shader
	v_delta = delta;

    gl_Position = u_camMatrix * vec4(v_worldPos, 1.0);
}

	