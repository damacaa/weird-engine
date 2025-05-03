#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

// Imports the current position from the Vertex Shader
in vec3 crntPos;
// Imports the normal from the Vertex Shader
in vec3 Normal;
// Imports the color from the Vertex Shader
in vec3 color;
// Imports the texture coordinates from the Vertex Shader
in vec2 texCoord;

uniform float u_time;

void main()
{
	float d = 2.0f * length(texCoord - vec2(0.5f));
	float alpha = 1.0f - d;
	alpha *= max(0.0f, (5.0f - crntPos.y));
	alpha = clamp(alpha, 0, 1);
	// alpha = alpha * alpha * alpha;
	// FragColor = vec4(vec3(1.0f), alpha);
	FragColor = vec4(vec3(0.12f), alpha * 0.25f);
}