#version 330 core

#define CRT 1

// Outputs colors in RGBA
out vec3 FragColor;

uniform vec2 u_resolution;
uniform float u_renderScale;

uniform float u_time;

uniform sampler2D u_colorTexture;

float rand(vec2 co)
{
	return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

uniform float weights[9] = float[9](
	// 1,2,1,2,4,2,1,2,1
	// 0.5, 1, 0.5, 1, 10, 1, 0.5, 1, 0.5
	0, 0, 0, 4, 8, 4, 0, 0, 0);

void main()
{
	vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);

#if CRT

	vec3 col = vec3(0.0);
	float pixelSizedStep = 1.0 / (u_resolution.x);

	// for(int i = -1; i <= 1; i++)
	// {
	//     for(int j = -1; j <= 1; j++)
	//     {
	//         float weight = weights[((j+1)*3)+(i+1)];
	//         vec3 colorValue = texture(u_colorTexture, screenUV + (pixelSizedStep * vec2(i, j))).xyz;
	//         col += weight * colorValue;
	//     }
	// }

	// col = col / 16.0;

	float maxValue = 0.0;
	for (int i = -5; i <= 5; i++)
	{

		float weight = 2.5;
		vec3 colorValue = texture(u_colorTexture, screenUV + (0.0005 * vec2(i, 0))).xyz;
		col += weight * colorValue;

		float value = 0.3333 * (colorValue.x + colorValue.y + colorValue.z);
		maxValue = max(maxValue, value);
	}

	col = col / 30.0;

	col = mix(col, texture(u_colorTexture, screenUV).xyz, 1.0 - (0.5 * maxValue));

	// CRT Line effect
	float lineOffset = 0;
	float t = 2.0 * (gl_FragCoord.y - lineOffset);
	float sine = sin(3.14 * t * u_renderScale);
	float mask = 0.5 * (sine + 1.0);

	// mask = 1 - mask;
	// mask = mask * mask * mask;
	// mask = 1 - mask;

	// mask = mask > 0.25 ? 1.0 : .5;

	// mask += 0.2;
	// mask = min(1.5, 1.0 * mask);

	// mask = smoothstep(0.0, 1.0, mask * mask);
	// mask = max(mask, 0.85);

	mask = mix(1.0, 1.1, mask);

	col *= mask;

	// col = vec3(mask);

#else
	vec3 col = texture(u_colorTexture, screenUV).xyz;
#endif

	// Dot effect
	// vec2 TexCoords = fract(gl_FragCoord.xy * u_renderScale);
	// vec2 dist = TexCoords - vec2(0.5f, 0.5f);
	// float mask = ((1.0 -length(dist)) - 0.25) * 2;

	// FragColor = color.xyz * mask;
	// FragColor = color.wwww;

	FragColor = col;
}
