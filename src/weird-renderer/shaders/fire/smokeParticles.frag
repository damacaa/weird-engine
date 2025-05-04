#version 330 core

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

uniform float u_time;

void main()
{
	float d = 2.0f * length(v_texCoord - vec2(0.5f));
	float alpha = 1.0f - d;
	alpha *= max(0.0f, (5.0f - v_worldPos.y));
	alpha = clamp(alpha, 0, 1);
	// alpha = alpha * alpha * alpha;
	// FragColor = vec4(vec3(1.0f), alpha);
	FragColor = vec4(vec3(0.12f), alpha * 0.25f);
}