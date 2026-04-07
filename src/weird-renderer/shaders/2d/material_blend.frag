#version 330 core

#include "../common/utils.glsl"

// Outputs colors in RGBA
out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_colorTexture;

uniform bool u_horizontal;
uniform float u_weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
uniform float u_time;

void main()
{
	vec2 tex_offset = 1.0 / textureSize(t_colorTexture, 0);
	vec2 uv = tex_offset + vec2(hash(gl_FragCoord.xy + u_time)); // hash????

	vec4 data = texture(t_colorTexture, v_texCoord);
	float alpha = data.w;

	vec4 originalColor = vec4(data.rgb, alpha);
	vec4 result = originalColor * u_weight[0]; // TODO: precompute toLinear before this shader

	for (int i = 1; i < 5; ++i)
	{
		vec2 offset = u_horizontal
						  ? vec2((tex_offset.x * i), 0.0)
						  : vec2(0.0, (tex_offset.y * i)); // (tex_offset.x * i) + hash(gl_FragCoord.xy + u_time)

		vec4 colRight = texture(t_colorTexture, v_texCoord + offset);
		result += colRight * u_weight[i];

		vec4 colLeft = texture(t_colorTexture, v_texCoord - offset);
		result += colLeft * u_weight[i];
	}

	result = vec4(result.rgb, result.a);
	FragColor = result;

	// FragColor = vec4(vec3(mask), data.w);
}
