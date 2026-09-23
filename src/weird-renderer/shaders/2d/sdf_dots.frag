#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;

// SDF Operations
float fOpUnionSoft(float a, float b, float r, float invR)
{
	float h = max(r - abs(a - b), 0.0);
	return min(a, b) - h * h * 0.25 * invR;
}

float shape_circle(vec2 p, float r)
{
	return length(p) - r;
}

float shape_circle(vec2 p)
{
	return shape_circle(p, 0.5);
}

// #define BLEND_SHAPES
// #define MOTION_BLUR
// #define MOTION_BLUR_FILL_OVERRIDE
// #define DEBUG_SHOW_GRID

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform float u_time;
uniform float u_k;

uniform sampler2D t_shapeDistanceTexture; // Intermediate distance field from Pass 1 (shapes)
uniform sampler2D t_colorTexture;         // Previous frame distance texture (for motion blur)

uniform int u_loadedObjects;
uniform highp sampler2D t_shapeBuffer;
uniform highp sampler2D t_gridHeader;
uniform highp sampler2D t_gridIndices;

uniform vec2 u_gridBoundsMin;
uniform vec2 u_gridStep;
uniform int u_gridCols;
uniform int u_gridRows;

uniform vec2 u_resolution;
uniform float u_overscan;

uniform mat4 u_camMatrix;
uniform mat4 u_oldCamMatrix;
uniform vec3 u_camPositionChange;

uniform float u_deltaTime;
uniform float u_motionBlurBlendSpeed;

// Bilinear sampling for motion blur history
vec2 smoothSample(sampler2D tex, vec2 uv)
{
	vec2 texelSize = 1.0 / u_resolution;
	vec2 P = uv * u_resolution - 0.5;

	ivec2 i_j = ivec2(floor(P));
	vec2 f = fract(P);

	vec2 uv00 = (vec2(i_j) + 0.5) * texelSize;
	vec2 uv10 = (vec2(i_j) + vec2(1.5, 0.5)) * texelSize;
	vec2 uv01 = (vec2(i_j) + vec2(0.5, 1.5)) * texelSize;
	vec2 uv11 = (vec2(i_j) + vec2(1.5, 1.5)) * texelSize;

	vec2 d00 = texture(tex, uv00).xy;
	float d10 = texture(tex, uv10).x;
	float d01 = texture(tex, uv01).x;
	float d11 = texture(tex, uv11).x;

	float d_top = mix(d00.x, d10, f.x);
	float d_bottom = mix(d01, d11, f.x);
	float d = mix(d_top, d_bottom, f.y);

	return vec2(d, d00.y);
}

void main()
{
#ifdef UI_PIPELINE
	// UI: origin at bottom-left, UV stays in [0,1] to match screen-space layout
	vec2 uv = v_texCoord;
#else
	// World: remap UV to [-1,1] so the origin is centred on screen
	vec2 uv = (2.0 * v_texCoord) - 1.0;
	uv *= (1.0 + u_overscan);
#endif

	float aspectRatio = u_resolution.x / u_resolution.y;
	uv.x *= aspectRatio;

	float zoom = -u_camMatrix[3].z;
	vec2 pos = (zoom * uv) - u_camMatrix[3].xy;

	// Read shape distance and material from Pass 1
	vec4 shapeSample = texture(t_shapeDistanceTexture, v_texCoord);
	float minDist = shapeSample.x;
	float minColorDist = minDist;
	int finalMaterialId = int(shapeSample.y);
	float mask = 0.0;

	float inv_k = 1.0 / u_k;

	ivec2 cellCoord = ivec2((pos - u_gridBoundsMin) / u_gridStep);
	cellCoord = clamp(cellCoord, ivec2(0), ivec2(u_gridCols - 1, u_gridRows - 1));

	vec2 cellData = texelFetch(t_gridHeader, cellCoord, 0).xy;
	int startIndex = int(cellData.x);
	int count = int(cellData.y);

	for (int i = 0; i < count; i++)
	{
		int flatIndex = startIndex + i;
		ivec2 indexCoord = ivec2(flatIndex % 1024, flatIndex / 1024);
		int objectIndex = int(texelFetch(t_gridIndices, indexCoord, 0).x);

		// Shape buffer wraps to 2D at 16384 texels wide (matches DataBuffer::uploadRawData on ES drivers)
		vec4 positionSizeMaterial = texelFetch(t_shapeBuffer, ivec2(objectIndex % 16384, objectIndex / 16384), 0);
		int materialId = int(positionSizeMaterial.w);

#ifdef UI_PIPELINE
		float objectDist = shape_circle(pos - positionSizeMaterial.xy, 5.0);
#else
		float objectDist = shape_circle(pos - positionSizeMaterial.xy);
#endif

		mask = max(mask, -objectDist * 4.0);

#ifdef BLEND_SHAPES
		finalMaterialId = objectDist <= minColorDist ? materialId : finalMaterialId;
		minDist = fOpUnionSoft(objectDist, minDist, u_k, inv_k);
		minColorDist = min(minColorDist, objectDist);
#else
		finalMaterialId = objectDist <= minDist ? materialId : finalMaterialId;
		minDist = min(minDist, objectDist);
#endif
	}

#ifdef UI_PIPELINE
	minDist = min(minDist, 10.0); // Clamp max distance in UI mode
#endif

#ifdef DEBUG_SHOW_GRID
	finalMaterialId = count / 10; // Color cells based on how many objects they contain, for debugging purposes
#endif

	float d = minDist;

#ifndef UI_PIPELINE
	float distanceBonus = (0.00002 * zoom * zoom); // Compensate for precision issues when zoomed out far away
	distanceBonus = min(distanceBonus, 1.0 * zoom / u_resolution.y); // Cap distance bonus to prevent artifacts
	d -= distanceBonus;
#endif

	float finalDistance = d / zoom;
	finalDistance *= 0.5 / aspectRatio;

	float material = float(finalMaterialId);

#ifdef MOTION_BLUR
	// Compensating for both camera translation and zoom change
	float oldZoom = -u_oldCamMatrix[3].z;
	float zoomRatio = zoom / oldZoom;
	vec2 prevTexCoord;
	prevTexCoord.x = zoomRatio * (v_texCoord.x - 0.5) + 0.5 + 0.5 * u_camPositionChange.x / (oldZoom * aspectRatio);
	prevTexCoord.y = zoomRatio * (v_texCoord.y - 0.5) + 0.5 + 0.5 * u_camPositionChange.y / oldZoom;

	// Sample previous texture with bilinear filtering
	vec2 previousData = smoothSample(t_colorTexture, prevTexCoord);
	float previousDistance = previousData.x;

	vec4 previousDataExact = texture(t_colorTexture, prevTexCoord);
	float previousDistanceExact = previousDataExact.x;
	float previousMaterial = previousDataExact.y;

	material = finalDistance >= 0.0 && previousDistance < 0.0 ? previousMaterial : material;

	float distanceChange = finalDistance - previousDistanceExact;
	float blendDistance;

#ifdef MOTION_BLUR_FILL_OVERRIDE
	float fadeStep = u_deltaTime * u_motionBlurBlendSpeed * 0.1;
	blendDistance = min(previousDistance + fadeStep, finalDistance);
#else
	blendDistance = previousDistance + (distanceChange * min(1.0, u_deltaTime * u_motionBlurBlendSpeed) *
										(distanceChange < 0.0 ? 5.0 : 1.0));
#endif

	blendDistance = clamp(blendDistance, -0.1, 0.1);

#ifdef UI_PIPELINE
	finalDistance = blendDistance;
#else
	finalDistance = mix(blendDistance, finalDistance, mask);
#endif

#endif

#ifdef DEBUG_SHOW_GRID
	vec2 localP = pos - u_gridBoundsMin;
	bool inBounds = localP.x >= 0.0 && localP.y >= 0.0 && localP.x < float(u_gridCols) * u_gridStep.x &&
					localP.y < float(u_gridRows) * u_gridStep.y;
	if (inBounds)
	{
		vec2 fracLocal = mod(localP, u_gridStep);
		vec2 distToLine = min(fracLocal, u_gridStep - fracLocal);
		float gridDistWorld = min(distToLine.x, distToLine.y);
		float gridDistNorm = (gridDistWorld / zoom) * (0.5 / aspectRatio);
		float lineHalfWidth = 1.0 / min(u_resolution.x, u_resolution.y);
		if (gridDistNorm < lineHalfWidth)
		{
			finalDistance = gridDistNorm - lineHalfWidth;
		}
	}
#endif

	FragColor = vec4(finalDistance, material, mask, 0.0);
	FragColor.a += u_time * 0.001;
}
