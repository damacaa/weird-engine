#version 300 es
precision highp float;
precision highp int;

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform sampler2D u_diffuse0;
uniform sampler2D u_specular0;

uniform vec3 u_camPos;
uniform vec3 u_lightPos;
uniform vec3 u_lightDirection;
uniform vec4 u_lightColor;

float u_near = 0.1;
float u_far = 100.0;

vec4 pointLight()
{
	vec3 lightVec = u_lightPos - v_worldPos;
	float dist = length(lightVec);
	float a = 3.0;
	float b = 0.7;
	float intensity = 10.0 / (a * dist * dist + b * dist + 1.0);

	vec3 normal = normalize(v_normal);
	vec3 lightDir = normalize(lightVec);
	vec3 viewDir = normalize(u_camPos - v_worldPos);
	vec3 reflectDir = reflect(-lightDir, normal);

	float ambient = 0.01;
	float diffuse = max(dot(normal, lightDir), 0.0);
	float specular = pow(max(dot(viewDir, reflectDir), 0.0), 16.0) * 0.5;

	vec4 texColor = texture(u_diffuse0, v_texCoord);
	float specVal = texture(u_specular0, v_texCoord).r;

	return (texColor * (diffuse * intensity + ambient) + specVal * specular * intensity) * u_lightColor;
}

vec4 directionalLight()
{
	vec3 normal = normalize(v_normal);
	vec3 lightDir = normalize(u_lightDirection);
	vec3 viewDir = normalize(u_camPos - v_worldPos);
	vec3 reflectDir = reflect(-lightDir, normal);

	float ambient = 0.1;
	float diffuse = max(dot(normal, lightDir), 0.0);
	float specular = pow(max(dot(viewDir, reflectDir), 0.0), 16.0) * 0.5;

	vec4 texColor = texture(u_diffuse0, v_texCoord);
	float specVal = texture(u_specular0, v_texCoord).r;

	texColor = vec4(0.83);

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

	float diffuse = max(dot(normal, lightDir), 0.0);
	float specular = pow(max(dot(viewDir, reflectDir), 0.0), 16.0) * 0.5;
	float angle = dot(vec3(0.0, -1.0, 0.0), -lightDir);
	float intensity = clamp((angle - outerCone) / (innerCone - outerCone), 0.0, 1.0);
	float ambient = 0.2;

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

const int bayer4x4[16] = int[16](
	0, 8, 2, 10,
	12, 4, 14, 6,
	3, 11, 1, 9,
	15, 7, 13, 5
);

float GetBayerThreshold(vec2 coord) 
{
	int x = int(coord.x) % 4;
	int y = int(coord.y) % 4;
	float bayerValue = float(bayer4x4[y * 4 + x]) + 0.5;
	return bayerValue / 16.0; 
}

// Generates a consistent 2D offset based on an object ID to prevent Dither Correlation
vec2 HashID(float id) 
{
	return vec2(
		fract(sin(id * 12.9898) * 43758.5453) * 100.0,
		fract(sin(id * 78.233) * 43758.5453) * 100.0
	);
}

void main()
{
	float alpha = 1.0;
	
	if (alpha < 1.0) 
	{
		vec2 offset = HashID(float(2));
		
		// Note: Change 1.0 to 2.0 or 4.0 here if you want chunkier retro dither pixels
		float bayer = GetBayerThreshold((gl_FragCoord.xy + offset) / 1.0); 
		
		if (alpha <= bayer) 
		{
			discard; // Skip surface by pushing distance past the FAR plane
		}
	}

	FragColor = directionalLight();
}
