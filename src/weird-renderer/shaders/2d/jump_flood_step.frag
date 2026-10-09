#version 300 es
precision highp float;
precision highp int;

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Inputs from vertex shader
in vec2 v_texCoord;

// Uniforms
uniform sampler2D t_prevSeeds; // previous seed texture
uniform int u_jump;			   // jump distance in texels
uniform vec2 u_texelSize;	   // 1.0 / texture resolution (e.g., 1/width, 1/height)
uniform ivec2 u_resolution;	   // texture resolution in texels

float distanceSqrt(vec2 a, vec2 b)
{
	vec2 d = (a - b);

	// Normalize Y scale to match X scale to fix the aspect ratio
	d.y *= (u_texelSize.x / u_texelSize.y);

	return dot(d, d);
}

// 8 directions
const ivec2 OFFSETS[8] = ivec2[8](ivec2(-1, 0), ivec2(1, 0), ivec2(0, -1), ivec2(0, 1), ivec2(-1, -1), ivec2(-1, 1),
								  ivec2(1, -1), ivec2(1, 1));

void main()
{
	vec2 uv = v_texCoord;
	ivec2 coord = ivec2(gl_FragCoord.xy);

	vec3 data = texelFetch(t_prevSeeds, coord, 0).xyz;
	// Current best seed from previous pass
	vec2 bestSeed = data.xy;
	float bestDist = data.z;

	// Check neighbors at jump distance
	for (int i = 0; i < 8; i++)
	{
		ivec2 sampleCoord = coord + (OFFSETS[i] * u_jump);

		// Skip samples outside the texture
		if (sampleCoord.x < 0 || sampleCoord.y < 0 || sampleCoord.x >= u_resolution.x ||
			sampleCoord.y >= u_resolution.y)
			continue;

		vec2 nSeed = texelFetch(t_prevSeeds, sampleCoord, 0).xy;

		if (nSeed.x < 0.0) // invalid
			continue;

		float d = distanceSqrt(nSeed, uv);
		if (d < bestDist)
		{
			bestDist = d;
			bestSeed = nSeed;
		}
	}

	// Write best seed found
	FragColor = vec4(bestSeed, bestDist, 1.0);
}
