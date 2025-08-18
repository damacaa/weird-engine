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

// ChatGPT: I want a function that, for a given x, gros linearly but then plateaus
float plateauFunction(float x, float maxValue, float growthRate) 
{
    return maxValue * (1.0 - exp(-growthRate * x));
}

void main()
{
	// Animation delta
	float delta = fract((10.0 * gl_InstanceID * 3.14) + (1.1 * u_time));

	float maxHeight = 1.5 + (fract(gl_InstanceID * 123.45678) - 0.5); // Constant + random offset
	float y = plateauFunction(2.0 * delta, maxHeight, 0.8);
	
	float distanceToCenterXZ = 0.1 + (0.3 * smoothstep(0, 1, sqrt(1.5 * delta))); // TODO: find an alternative to sqrt

	vec3 offset = vec3(
		distanceToCenterXZ * sin(u_time + gl_InstanceID), 
		y, 
		distanceToCenterXZ * cos(u_time + gl_InstanceID)
	);

    v_worldPos = vec3(u_model * vec4(2.0 * (1.0 - delta * delta) * in_position, 1.0)) + offset;
    v_normal   = u_normalMatrix * in_normal;
    v_color    = in_color;
    v_texCoord = in_texCoord;

    gl_Position = u_camMatrix * vec4(v_worldPos, 1.0);
}