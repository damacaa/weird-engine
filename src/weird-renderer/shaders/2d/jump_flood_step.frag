#version 300 es
precision highp float;
precision highp int;

// Constants

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform sampler2D t_prevSeeds; // previous seed texture
uniform vec2 u_jumpSize;	   // in UV units (so jump in pixels / texture size)
uniform vec2 u_texelSize;	   // 1.0 / texture resolution (e.g., 1/width, 1/height)

float distanceSqrt(vec2 a, vec2 b)
{
	vec2 d = (a - b);

	// Normalize Y scale to match X scale to fix the aspect ratio
	d.y *= (u_texelSize.x / u_texelSize.y);

	return dot(d, d);
}

void main()
{
	vec2 uv = v_texCoord;

	vec3 data = texture(t_prevSeeds, uv).xyz;
	// Current best seed from previous pass
	vec2 bestSeed = data.xy;
	// TODO: include in output and read from texture instead
	// TODO: replace distance with distanceSqrt to avoid square roots, real distance will be calculated in a different
	// shader
	float bestDist = data.z;

	// 8 directions
	const vec2 OFFSETS[8] =
		vec2[8](vec2(-1.0, 0.0), vec2(1.0, 0.0), vec2(0.0, -1.0), vec2(0.0, 1.0), vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));

	// Check neighbors at jump distance
	for (int i = 0; i < 8; i++)
	{
		vec2 sampleUV = uv + (OFFSETS[i] * u_jumpSize);

		// Optional: clamp or skip if out of bounds
		if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
			continue;

		vec2 nSeed = texture(t_prevSeeds, sampleUV).xy;

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
