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
	FragColor = vec4(vec3(1.75), 1.0);
}