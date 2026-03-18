#version 330 core

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform vec3  u_lightPos;
uniform vec4  u_lightColor;
uniform float  u_ambient;

uniform vec3  u_camPos;
uniform vec3  u_fogColor = vec3(0,0,0);

uniform float u_time;

// Custom
uniform float u_near = 0.1f;
uniform float u_far  = 100.0f;

uniform float u_shininess = 100.0f;


vec3 pointLight()
{
	vec3 lightVec = u_lightPos - v_worldPos;
	float dist = length(lightVec);
	float a = 3.0;
	float b = 0.7;
	float intensity = u_lightColor.a * 1.0f / (a * dist * dist + b * dist + 1.0f);

	vec3 normal = normalize(v_normal);
	vec3 lightDir = normalize(lightVec);
	vec3 viewDir = normalize(u_camPos - v_worldPos);
	vec3 reflectDir = reflect(-lightDir, normal);

	float diffuse  = max(dot(normal, lightDir), 0.0);
	float viewFactor = max(dot(viewDir, reflectDir), 0.0);
	float specular = pow(viewFactor, u_shininess);

	return (((diffuse * intensity + u_ambient) + specular * intensity) * u_lightColor).xyz;
}

float linearizeDepth(float depth)
{
    return (2.0 * u_near * u_far) / (u_far + u_near - (depth * 2.0 - 1.0) * (u_far - u_near));
}

float smoothFogFactor(float depth, float fogStart, float fogEnd)
{
    float z = linearizeDepth(depth);
    // Use smoothstep for a more controllable falloff
    return smoothstep(fogStart, fogEnd, z);
}

void main()
{
    // Set the start and end points for the fog effect
    float fogStart = u_far * 0.5;
    float fogEnd = u_far;        

    // Calculate the fog factor
    float fogFactor = smoothFogFactor(gl_FragCoord.z, fogStart, fogEnd);

	// Get color using basic point light ligthing
	vec3 color = pointLight();

	// Blend fog
	color = mix(color, u_fogColor, fogFactor);

	// Apply the fog effect
    FragColor = vec4(color, 1.0f);
}
