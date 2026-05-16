#version 300 es
precision highp float;
precision highp int;

// GBuffer outputs
layout(location = 0) out vec4 out_albedo;    // RGB = diffuse color, A = specular intensity
layout(location = 1) out vec4 out_worldPos;  // RGB = world position, A = 1.0 (marks valid pixel)
layout(location = 2) out vec4 out_normal;    // RGB = world normal (signed, not remapped), A = unused

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform sampler2D u_diffuse0;
uniform sampler2D u_specular0;

void main()
{
	vec4 texColor = texture(u_diffuse0, v_texCoord);
	float specVal = texture(u_specular0, v_texCoord).r;

	// Use 0.83 grey default (matching existing forward shader behavior)
	vec3 albedo = vec3(0.83);

	// Only use texture if it has meaningful content
	float texLum = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
	if (texLum > 0.01)
	{
		albedo = texColor.rgb;
	}

	out_albedo   = vec4(albedo, specVal);
	out_worldPos = vec4(v_worldPos, 1.0);
	out_normal   = vec4(normalize(v_normal), 0.0);
}
