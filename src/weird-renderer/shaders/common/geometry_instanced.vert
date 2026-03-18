#version 330 core

// Vertex attributes
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_texCoord;

// Outputs to fragment shader
out vec3 v_worldPos;
out vec3 v_normal;
out vec3 v_color;
out vec2 v_texCoord;

// Uniforms
//uniform mat4 u_model;
uniform mat4 u_camMatrix;
//uniform mat3 u_normalMatrix;


uniform samplerBuffer t_modelMatrices;

void main()
{
	int index = gl_InstanceID;
	vec4 r0 = texelFetch(t_modelMatrices, index + 0);
	vec4 r1 = texelFetch(t_modelMatrices, index + 1);
	vec4 r2 = texelFetch(t_modelMatrices, index + 2);
	vec4 r3 = texelFetch(t_modelMatrices, index + 3);
	mat4 model = mat4(r0, r1, r2, r3);

	// calculates current position
	v_worldPos = vec3(model * vec4(in_position, 1.0f));
	v_normal = in_normal;
	v_color = in_color;
	v_texCoord = mat2(0.0, -1.0, 1.0, 0.0) * in_texCoord;
	
	// Outputs the positions/coordinates of all vertices
	gl_Position = u_camMatrix * vec4(v_worldPos, 1.0);
}