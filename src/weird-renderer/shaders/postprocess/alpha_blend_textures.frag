#version 300 es
precision highp float;
precision highp int;

// Outputs colors in RGBA
out vec4 FragColor;

uniform sampler2D t_2DSceneTexture;
uniform sampler2D t_3DSceneTexture;

uniform vec2 u_resolution;

void main()
{
	vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);

	vec4 color2D = texture(t_2DSceneTexture, screenUV);
	vec4 color3D = texture(t_3DSceneTexture, screenUV);

	FragColor = vec4(mix(color2D, color3D, color3D.a).xyz, 1.0);
	// FragColor = mix(color2D, color3D, screenUV.x).xyz;

	// FragColor = max(color2D, color3D).xyz;
}
