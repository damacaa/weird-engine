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
//uniform mat4 u_camMatrix;
//uniform mat3 u_normalMatrix;


// Imports the camera matrix
uniform mat4 camMatrix;
// Imports the transformation matrices
uniform mat4 u_models[255];



void main()
{
	// calculates current position
	v_worldPos = vec3(u_models[gl_InstanceID] * vec4(in_position, 1.0f));
	v_normal = in_normal;
	v_color = in_color;
	v_texCoord = mat2(0.0, -1.0, 1.0, 0.0) * in_texCoord;
	
	// Outputs the positions/coordinates of all vertices
	gl_Position = camMatrix * vec4(v_worldPos, 1.0);
}