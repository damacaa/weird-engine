#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_colorTexture;
uniform float u_threshold = 1.0;

void main()
{
	vec4 color = texture(t_colorTexture, v_texCoord.xy);

	// If any channel is above the threshold, it is written to the bright texture
	float value = max(color.x, max(color.y, color.z));
	value = value >= u_threshold ? value: 0.0;

	vec4 bloomColor = value * color;
	bloomColor.a = color.a;

	FragColor = bloomColor;
}
