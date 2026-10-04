#version 300 es
precision highp float;

in vec2 v_texCoord;
out vec4 FragColor;

uniform sampler2D t_input;

#ifdef FXAA

uniform vec2 u_inverseResolution;

const float FXAA_EDGE_THRESHOLD = 0.125;
const float FXAA_EDGE_THRESHOLD_MIN = 0.05;
const float FXAA_SUBPIX_TRIM = 0.75;

vec3 sampleLinear(vec2 uv)
{
	return texture(t_input, uv).rgb;
}

vec3 linearToSRGB(vec3 linearCol)
{
	return pow(max(linearCol, vec3(0.0)), vec3(0.4545));
}

float rgbToLuma(vec3 srgb)
{
	return dot(srgb, vec3(0.299, 0.587, 0.114));
}

vec3 applyFXAA(vec2 uv)
{
	vec2 rcpFrame = u_inverseResolution;

	vec3 rgbM = sampleLinear(uv);
	float lumaM = rgbToLuma(linearToSRGB(rgbM));

	float lumaN = rgbToLuma(linearToSRGB(sampleLinear(uv + vec2(0.0, -rcpFrame.y))));
	float lumaS = rgbToLuma(linearToSRGB(sampleLinear(uv + vec2(0.0, rcpFrame.y))));
	float lumaW = rgbToLuma(linearToSRGB(sampleLinear(uv + vec2(-rcpFrame.x, 0.0))));
	float lumaE = rgbToLuma(linearToSRGB(sampleLinear(uv + vec2(rcpFrame.x, 0.0))));

	float rangeMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
	float rangeMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
	float range = rangeMax - rangeMin;

	// Early exit if contrast below threshold (not an edge)
	if (range < max(FXAA_EDGE_THRESHOLD_MIN, rangeMax * FXAA_EDGE_THRESHOLD))
	{
		return linearToSRGB(rgbM);
	}

	float lumaNW = rgbToLuma(linearToSRGB(sampleLinear(uv + vec2(-rcpFrame.x, -rcpFrame.y))));
	float lumaNE = rgbToLuma(linearToSRGB(sampleLinear(uv + vec2(rcpFrame.x, -rcpFrame.y))));
	float lumaSW = rgbToLuma(linearToSRGB(sampleLinear(uv + vec2(-rcpFrame.x, rcpFrame.y))));
	float lumaSE = rgbToLuma(linearToSRGB(sampleLinear(uv + vec2(rcpFrame.x, rcpFrame.y))));

	// Sub-pixel antialiasing filter
	float lumaL = lumaN + lumaS + lumaW + lumaE;
	float subpixA = 2.0 * lumaL + (lumaNW + lumaNE + lumaSW + lumaSE);
	float subpixB = subpixA * (1.0 / 12.0) - lumaM;
	float subpixC = clamp(abs(subpixB) / range, 0.0, 1.0);
	float subpixBlend = smoothstep(0.0, 1.0, subpixC);
	float subpixelOffset = subpixBlend * subpixBlend * FXAA_SUBPIX_TRIM;

	// Edge direction (horizontal vs vertical)
	float edgeHorz = abs(-2.0 * lumaM + lumaN + lumaS) * 2.0 + abs(-2.0 * lumaW + lumaNW + lumaSW) +
					 abs(-2.0 * lumaE + lumaNE + lumaSE);
	float edgeVert = abs(-2.0 * lumaM + lumaW + lumaE) * 2.0 + abs(-2.0 * lumaN + lumaNW + lumaNE) +
					 abs(-2.0 * lumaS + lumaSW + lumaSE);
	bool isHorizontal = (edgeHorz >= edgeVert);

	// Select edge orientation
	float luma1 = isHorizontal ? lumaN : lumaW;
	float luma2 = isHorizontal ? lumaS : lumaE;
	float grad1 = abs(luma1 - lumaM);
	float grad2 = abs(luma2 - lumaM);

	bool is1Steeper = grad1 >= grad2;
	float gradScaled = max(grad1, grad2) * 0.25;

	float stepLength = isHorizontal ? rcpFrame.y : rcpFrame.x;
	if (is1Steeper)
	{
		stepLength = -stepLength;
	}

	vec2 uvEdge = uv;
	if (isHorizontal)
	{
		uvEdge.y += stepLength * 0.5;
	}
	else
	{
		uvEdge.x += stepLength * 0.5;
	}

	vec2 stepDir = isHorizontal ? vec2(rcpFrame.x, 0.0) : vec2(0.0, rcpFrame.y);

	// Explore along edge tangent in negative and positive directions
	vec2 uvNeg = uvEdge - stepDir;
	vec2 uvPos = uvEdge + stepDir;

	float lumaNeg = rgbToLuma(linearToSRGB(sampleLinear(uvNeg)));
	float lumaPos = rgbToLuma(linearToSRGB(sampleLinear(uvPos)));

	float edgeLuma = is1Steeper ? (luma1 + lumaM) * 0.5 : (luma2 + lumaM) * 0.5;

	bool doneNeg = abs(lumaNeg - edgeLuma) >= gradScaled;
	bool donePos = abs(lumaPos - edgeLuma) >= gradScaled;

	const int MAX_STEPS = 8;
	for (int i = 1; i < MAX_STEPS; i++)
	{
		float stepScale = (i < 4) ? 1.0 : ((i < 6) ? 1.5 : 2.0);
		if (!doneNeg)
		{
			uvNeg -= stepDir * stepScale;
			lumaNeg = rgbToLuma(linearToSRGB(sampleLinear(uvNeg)));
			doneNeg = abs(lumaNeg - edgeLuma) >= gradScaled;
		}
		if (!donePos)
		{
			uvPos += stepDir * stepScale;
			lumaPos = rgbToLuma(linearToSRGB(sampleLinear(uvPos)));
			donePos = abs(lumaPos - edgeLuma) >= gradScaled;
		}
		if (doneNeg && donePos)
		{
			break;
		}
	}

	float dstNeg = isHorizontal ? (uv.x - uvNeg.x) : (uv.y - uvNeg.y);
	float dstPos = isHorizontal ? (uvPos.x - uv.x) : (uvPos.y - uv.y);

	bool isCloserNeg = dstNeg < dstPos;
	float dst = min(dstNeg, dstPos);
	float edgeSpan = dstNeg + dstPos;

	float edgeOffset = 0.5 - dst / edgeSpan;

	// Check if end of edge has same sign as center (if not, it's not a valid edge span)
	float lumaEnd = isCloserNeg ? lumaNeg : lumaPos;
	bool goodSpan = (lumaEnd - edgeLuma < 0.0) != (lumaM - edgeLuma < 0.0);
	if (!goodSpan)
	{
		edgeOffset = 0.0;
	}

	float finalOffset = max(subpixelOffset, edgeOffset);

	vec2 finalUV = uv;
	if (isHorizontal)
	{
		finalUV.y += stepLength * finalOffset;
	}
	else
	{
		finalUV.x += stepLength * finalOffset;
	}

	return linearToSRGB(sampleLinear(finalUV));
}

#endif // FXAA

void main()
{
#ifdef FXAA
	FragColor = vec4(applyFXAA(v_texCoord), texture(t_input, v_texCoord).a);
#else
	vec4 col = texture(t_input, v_texCoord);
	FragColor = vec4(pow(max(col.rgb, vec3(0.0)), vec3(0.4545)), col.a);
#endif
}