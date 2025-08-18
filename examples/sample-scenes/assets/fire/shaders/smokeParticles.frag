#version 330 core

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;
in float v_delta;

uniform float u_time;
uniform vec3 u_smokeColor = vec3(0.12);

void main()
{
	// Distance to center generates a circular gradient
	float d = 2.0 * length(v_texCoord - vec2(0.5));
	float alpha = max(0.0, 1.0 - d);

	// Animate alpha
	alpha *= pow(1.0 - v_delta, 2.0);

	FragColor = vec4(u_smokeColor * (1.0 - v_delta), 0.75 * alpha);
}