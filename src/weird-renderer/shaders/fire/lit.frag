#version 330 core

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform sampler2D u_diffuse0;
uniform sampler2D u_specular0;

uniform vec4  u_lightColor;
uniform vec3  u_lightPos;
uniform vec3  u_camPos;
uniform vec3  u_directionalLightDir;

float u_near = 0.1f;
float u_far  = 100.0f;

uniform float u_time;

vec4 pointLight()
{
	vec3 lightVec = u_lightPos - v_worldPos;
	float dist = length(lightVec);
	float a = 3.0;
	float b = 0.7;
	float intensity = (10.0f + (1.0f * sin(50.0f * u_time))) / (a * dist * dist + b * dist + 1.0f);

	vec3 normal = normalize(v_normal);
	vec3 lightDir = normalize(lightVec);
	vec3 viewDir = normalize(u_camPos - v_worldPos);
	vec3 reflectDir = reflect(-lightDir, normal);

	float ambient  = 0.01;
	float diffuse  = max(dot(normal, lightDir), 0.0);
	float specular = pow(max(dot(viewDir, reflectDir), 0.0), 16) * 0.5;

	vec4 texColor  = texture(u_diffuse0, v_texCoord);
	float specVal  = texture(u_specular0, v_texCoord).r;

	//return (texColor * (diffuse * intensity + ambient) + specVal * specular * intensity) * u_lightColor;
	return vec4(diffuse * intensity + ambient);
}

vec4 directionalLight()
{
	vec3 normal = normalize(v_normal);
	vec3 lightDir = normalize(u_directionalLightDir);
	vec3 viewDir = normalize(u_camPos - v_worldPos);
	vec3 reflectDir = reflect(-lightDir, normal);

	float ambient  = 0.0001;
	float diffuse  = max(dot(normal, lightDir), 0.0);
	float specular = pow(max(dot(viewDir, reflectDir), 0.0), 16) * 0.5;

	vec4 texColor = texture(u_diffuse0, v_texCoord);
	float specVal = texture(u_specular0, v_texCoord).r;

	return (texColor * (diffuse + ambient) + specVal * specular) * u_lightColor;
}

vec4 spotLight()
{
	float outerCone = 0.90;
	float innerCone = 0.95;

	vec3 normal = normalize(v_normal);
	vec3 lightDir = normalize(u_lightPos - v_worldPos);
	vec3 viewDir = normalize(u_camPos - v_worldPos);
	vec3 reflectDir = reflect(-lightDir, normal);

	float diffuse  = max(dot(normal, lightDir), 0.0);
	float specular = pow(max(dot(viewDir, reflectDir), 0.0), 16) * 0.5;
	float angle    = dot(vec3(0.0, -1.0, 0.0), -lightDir);
	float intensity = clamp((angle - outerCone) / (innerCone - outerCone), 0.0, 1.0);
	float ambient   = 0.2;

	vec4 texColor = texture(u_diffuse0, v_texCoord);
	float specVal = texture(u_specular0, v_texCoord).r;

	return (texColor * (diffuse * intensity + ambient) + specVal * specular * intensity) * u_lightColor;
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

	FragColor.a = 1.0;
}
