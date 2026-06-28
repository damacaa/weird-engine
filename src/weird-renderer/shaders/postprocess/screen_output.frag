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

#ifdef SURFACE_BLUR

uniform float u_surfaceBlurRadius;
uniform float u_surfaceBlurSigmaColor;

// Bilateral filter: blurs flat regions while preserving edges.
// Spatial sigma is derived from the radius so the kernel falls off naturally.
vec3 surfaceBlur(sampler2D tex, vec2 uv, vec2 texelSize)
{
	vec3 center = texture(tex, uv).rgb;
	vec3 result = vec3(0.0);
	float totalWeight = 0.0;

	float sigmaSpace  = max(u_surfaceBlurRadius * 0.5, 0.5);
	float sigmaSpace2 = 2.0 * sigmaSpace * sigmaSpace;
	float sigmaColor2 = 2.0 * u_surfaceBlurSigmaColor * u_surfaceBlurSigmaColor;

	int r = int(u_surfaceBlurRadius);
	for (int y = -r; y <= r; y++)
	{
		for (int x = -r; x <= r; x++)
		{
			vec2 offset = vec2(float(x), float(y)) * texelSize;
			vec3 s = texture(tex, uv + offset).rgb;

			float spatialDist2 = float(x * x + y * y);
			vec3  diff         = s - center;
			float colorDist2   = dot(diff, diff);

			float w = exp(-spatialDist2 / sigmaSpace2 - colorDist2 / sigmaColor2);
			result      += s * w;
			totalWeight += w;
		}
	}
	return result / totalWeight;
}

#endif

void main()
{
	vec2 screenUV = gl_FragCoord.xy / u_resolution.xy;

#ifdef SURFACE_BLUR
	vec2 texelSize = 1.0 / u_renderResolution;
	vec3 col = surfaceBlur(t_colorTexture, screenUV, texelSize);
#else
	vec3 col = texture(t_colorTexture, screenUV).rgb;
#endif

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
