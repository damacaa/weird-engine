#version 330 core

// #define CORRECT_INSIDE
#define CORRECT_OUTSIDE

layout(location = 0) out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_distanceTexture;
uniform vec2 u_texelSize;

void main()
{
	vec2 uv = v_texCoord;
	float d = texture(t_distanceTexture, uv).x;

	bool condition = false;
#ifdef CORRECT_INSIDE
	condition = d > 0.0;
#endif

#ifdef CORRECT_OUTSIDE
	condition = d < 0.0;
#endif

	vec2 seed = vec2(-1.0);
	float initDistance = 1e9;

	if (condition)
	{
		// Default seed is the pixel center
		seed = uv;
		initDistance = 0.0;

		// --- SUB-PIXEL SEED CORRECTION ---
		// Sample neighbors to get the gradient of the downsampled SDF
		float dL = texture(t_distanceTexture, uv - vec2(u_texelSize.x, 0.0)).x;
		float dR = texture(t_distanceTexture, uv + vec2(u_texelSize.x, 0.0)).x;
		float dB = texture(t_distanceTexture, uv - vec2(0.0, u_texelSize.y)).x;
		float dT = texture(t_distanceTexture, uv + vec2(0.0, u_texelSize.y)).x;

		// Detect if this pixel is on the boundary
		bool isBoundary = false;
#ifdef CORRECT_OUTSIDE
		isBoundary = (dL > 0.0 || dR > 0.0 || dB > 0.0 || dT > 0.0);
#endif
#ifdef CORRECT_INSIDE
		isBoundary = (dL < 0.0 || dR < 0.0 || dB < 0.0 || dT < 0.0);
#endif

		if (isBoundary)
		{
			// Calculate gradient (change in distance per texel)
			vec2 grad = vec2(dR - dL, dT - dB) * 0.5;
			float gradSq = dot(grad, grad);

			if (gradSq > 1e-6)
			{
				// Newton-Raphson step: find exact location where distance == 0
				vec2 deltaTexels = -d * grad / gradSq;

				// Clamp to prevent erratic seeds if gradient is flat/bad
				deltaTexels = clamp(deltaTexels, vec2(-1.5), vec2(1.5));

				// Push the seed from the pixel center to the exact continuous surface!
				seed = uv + deltaTexels * u_texelSize;
			}
		}
	}

	FragColor = vec4(seed, initDistance, 0.0);
}