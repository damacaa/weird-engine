#version 330 core

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform sampler2D u_diffuse;
uniform sampler2D u_specular;

uniform vec3  u_lightPos;
uniform vec4  u_lightColor;
uniform float  u_ambient;

uniform vec3  u_camPos;

uniform float u_time;

// Custom
uniform float u_near = 0.1f;
uniform float u_far  = 100.0f;


vec4 pointLight()
{
	vec3 lightVec = u_lightPos - v_worldPos;
	float dist = length(lightVec);
	float a = 3.0;
	float b = 0.7;
	float intensity = 1.0f/ (a * dist * dist + b * dist + 1.0f);

	vec3 normal = normalize(v_normal);
	vec3 lightDir = normalize(lightVec);
	vec3 viewDir = normalize(u_camPos - v_worldPos);
	vec3 reflectDir = reflect(-lightDir, normal);

	float diffuse  = max(dot(normal, lightDir), 0.0);
	float specular = pow(max(dot(viewDir, reflectDir), 0.0), 16) * 0.5;

	// vec4 texColor  = texture(u_diffuse, v_texCoord);
	// float specVal  = texture(u_specular, v_texCoord).r;

	//return (texColor * (diffuse * intensity + ambient) + specVal * specular * intensity) * u_lightColor;
	return (u_lightColor.a * diffuse * intensity + u_ambient) * u_lightColor;
}

float linearizeDepth(float depth)
{
	return (2.0 * u_near * u_far) / (u_far + u_near - (depth * 2.0 - 1.0) * (u_far - u_near));
}

float logisticDepth(float depth, float steepness, float offset)
{
	float z = linearizeDepth(depth);
	return 1.0 / (1.0 + exp(-steepness * (z - offset)));
}

void main()
{
	// Optional depth visualization
	float depth = logisticDepth(gl_FragCoord.z, 0.5, 5.0);
	// FragColor = directionalLight(); // <- switch here
	FragColor = pointLight();         // <- default
	// FragColor = vec4(vec3(depth * depth * depth), 1.0);

	//FragColor = vec4(v_normal.x, v_normal.y,  v_normal.z, 1);

	//FragColor.a = 1.0;
}
