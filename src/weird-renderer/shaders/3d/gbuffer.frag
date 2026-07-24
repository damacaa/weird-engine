#version 300 es
precision highp float;
precision highp int;

// GBuffer outputs
layout(location = 0) out vec4 out_albedo;	// RGB = diffuse color, A = specular intensity
layout(location = 1) out vec4 out_worldPos; // RGB = world position, A = 1.0 (marks valid pixel)
layout(location = 2) out vec4 out_normal;	// RGB = world normal (signed, not remapped), A = unused
layout(location = 3) out int out_material;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform sampler2D u_diffuse0;
uniform sampler2D u_specular0;

uniform int u_materialIndex;
uniform int u_hasDiffuse;

void main()
{
	vec3 albedo = vec3(1.0);
	float specVal = 0.0;

	if (u_hasDiffuse == 1)
	{
		vec4 texColor = texture(u_diffuse0, v_texCoord);
		specVal = texture(u_specular0, v_texCoord).r;

		// Only use texture if it has meaningful content
		float texLum = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
		if (texLum > 0.01)
		{
			albedo = texColor.rgb;
		}
		else
		{
			albedo = vec3(0.83); // fallback if texture is blackish
		}
	}

	albedo = vec3(1.0); // Override albedo to white for testing purposes

	out_albedo = vec4(albedo, specVal);
	out_worldPos = vec4(v_worldPos, 1.0);
	out_normal = vec4(normalize(v_normal), 0.0);
	out_material = u_materialIndex;
}
