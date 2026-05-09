#version 300 es
precision highp float;
precision highp int;

out vec4 FragColor;

uniform vec2 u_resolution;
uniform vec2 u_renderResolution;
uniform sampler2D t_colorTexture;
uniform float u_ditheringSpread;
uniform int u_ditheringColorCount;

#ifdef DITHERING

const int u_bayer4[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);

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

	// Map fragment coordinates to render resolution space for consistent dithering
	vec2 renderCoord = gl_FragCoord.xy * (u_renderResolution / u_resolution);
	int x = int(renderCoord.x);
	int y = int(renderCoord.y);
	
	// Boost saturation slightly before dithering to avoid "muddy" colors
	float luminance = dot(col, vec3(0.299, 0.587, 0.114));
	col = mix(vec3(luminance), col, 1.5);
	col = clamp(col, 0.0, 1.0);

	col += u_ditheringSpread * getBayer4(x, y);

	vec3 levels = vec3(u_ditheringColorCount); 

	col = floor(col * levels + 0.5) / levels;

#endif

	FragColor = vec4(col, 1.0);
}
