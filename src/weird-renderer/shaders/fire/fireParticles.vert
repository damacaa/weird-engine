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

void main()
{
	float y = 3.0f * fract((10.0f * gl_InstanceID * 3.14f) + (1.1f * u_time));
	float distanceToCenterXZ = 0.3f + (0.4f * smoothstep(0, 1, sqrt(1.5f * y))); // TODO: find an alternative to sqrt

	vec3 offset = vec3(
	distanceToCenterXZ * sin(u_time + gl_InstanceID), 
	plateauFunction(y, 1.5f + (fract(gl_InstanceID * 123.45678f) - 0.5f), 0.8f), 
	distanceToCenterXZ * cos(u_time + gl_InstanceID)
	);

    v_worldPos = vec3(u_model * vec4(in_position, 1.0)) + offset;
    v_normal   = u_normalMatrix * in_normal;
    v_color    = in_color;
    v_texCoord = in_texCoord;

    gl_Position = u_camMatrix * vec4(v_worldPos, 1.0);
}