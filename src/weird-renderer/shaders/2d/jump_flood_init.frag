#version 300 es
precision highp float;
precision highp int;

layout(location = 0) out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_distanceTexture;
uniform vec2 u_texelSize;
uniform float u_overscan;

void main()
{
	vec2 uv = v_texCoord;
	float d = texture(t_distanceTexture, uv).x;

	// Real signed squared distance in the flood metric. The distance texture stores screen-width units,
	// while the flood metric is normalized to the overscan texture width, hence the overscan scale factor
	float dFlood = d / (1.0 + u_overscan);
	float distSq = dFlood * dFlood;

	// Neighbor samples for boundary detection and the local gradient
	float dL = texture(t_distanceTexture, uv - vec2(u_texelSize.x, 0.0)).x;
	float dR = texture(t_distanceTexture, uv + vec2(u_texelSize.x, 0.0)).x;
	float dB = texture(t_distanceTexture, uv - vec2(0.0, u_texelSize.y)).x;
	float dT = texture(t_distanceTexture, uv + vec2(0.0, u_texelSize.y)).x;

	// Boundary texel: a 4-neighbor on the opposite side of the surface
	bool isBoundary = false;
	if (d < 0.0)
		isBoundary = (dL > 0.0 || dR > 0.0 || dB > 0.0 || dT > 0.0);
	else if (d > 0.0)
		isBoundary = (dL < 0.0 || dR < 0.0 || dB < 0.0 || dT < 0.0);

	vec2 seed = vec2(-1.0);
	float initDistance = 1e9;

	bool inside = d < 0.0;

	// Near-surface texels (within one texel of the surface, either side) store their real signed squared
	// distance: negative inside, positive outside. This makes the bilinearly interpolated field cross zero
	// at the true contour instead of snapping to the texel lattice, while the flood propagates exact
	// Euclidean distances to the sub-pixel seeds
	bool nearSurface = isBoundary && abs(dFlood) < u_texelSize.x;

	if (inside || nearSurface)
	{
		// Default seed is the pixel center, with the real signed distance to the surface
		seed = uv;
		initDistance = inside ? -distSq : distSq;

		if (isBoundary)
		{
			// --- SUB-PIXEL SEED CORRECTION ---
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
