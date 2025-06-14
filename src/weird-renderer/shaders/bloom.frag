#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_sceneTexture;
uniform sampler2D t_blurTexture;

uniform vec2 u_pixelOffset;

void main()
{
	// Sample scene texture and bright texture after applying blur
	vec4 scene = texture(t_sceneTexture, v_texCoord.xy + u_pixelOffset);
	vec4 blur =  texture(t_blurTexture, v_texCoord.xy + u_pixelOffset);

	// Mask blur texture to only be applied to dark parts of the scene texture
	// Avoids making bright parts too bright
	float value = (scene.x + scene.y + scene.z) / 3.0;
	float blurMask = 1.0 - value;
	blurMask = clamp(blurMask, 0.0, 1.0);
	blurMask *= blurMask;

	// Combine textures
	vec4 color = (scene + (blur * blurMask));

	// Color correction
  	color = vec4(pow(color.xyz, vec3(0.9)), color.w);
	
	FragColor = color.xyzw;
	// FragColor = vec4(vec3(blurMask), 1.0);
}
