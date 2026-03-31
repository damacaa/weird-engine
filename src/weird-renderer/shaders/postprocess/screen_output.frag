#version 330 core

out vec3 FragColor;

uniform vec2 u_resolution;
uniform sampler2D t_colorTexture;

#ifdef DITHERING

uniform float u_spread = 0.05;
uniform int u_colorCount = 16;

uniform int u_bayer4[4 * 4] = int[4 * 4](
0, 8, 2, 10,
12, 4, 14, 6,
3, 11, 1, 9,
15, 7, 13, 5);

float getBayer4(int x, int y)
{
	return float(u_bayer4[(x % 4) + (y % 4) * 4]) * (1.0 / 16.0) - 0.5;
}

#endif

void main()
{
	vec2 screenUV = gl_FragCoord.xy / u_resolution.xy;
	vec3 col = texture(t_colorTexture, screenUV).rgb;

	#ifdef DITHERING

	int x = int(gl_FragCoord.x);
	int y = int(gl_FragCoord.y);
	col += u_spread * getBayer4(x, y);

	col.r = floor((u_colorCount - 1.0) * col.r + 0.5) / (u_colorCount - 1.0);
	col.g = floor((u_colorCount - 1.0) * col.g + 0.5) / (u_colorCount - 1.0);
	col.b = floor((u_colorCount - 1.0) * col.b + 0.5) / (u_colorCount - 1.0);

	#endif

	FragColor = col;
}
