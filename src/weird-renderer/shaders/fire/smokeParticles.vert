#version 330 core

// Positions/Coordinates
layout (location = 0) in vec3 aPos;
// Normals (not necessarily normalized)
layout (location = 1) in vec3 aNormal;
// Colors
layout (location = 2) in vec3 aColor;
// Texture Coordinates
layout (location = 3) in vec2 aTex;


// Outputs the current position for the Fragment Shader
out vec3 crntPos;
// Outputs the normal for the Fragment Shader
out vec3 Normal;
// Outputs the color for the Fragment Shader
out vec3 color;
// Outputs the texture coordinates to the Fragment Shader
out vec2 texCoord;



// Imports the camera matrix
uniform mat4 camMatrix;
// Imports the transformation matrices
uniform mat4 model;

uniform float u_time;

// ChatGPT: I want a function that, for a given x, increases but then plateaus
float plateauFunction(float x, float maxValue, float growthRate) {
    return maxValue * (1.0 - exp(-growthRate * x));
}

void main()
{
	float y = 5.0f * fract((gl_InstanceID * 123.45678f) + (0.1f * u_time));

	vec3 offset = vec3(
		y * 0.3f * sin(gl_InstanceID * 987.654f), 
		y, 
		-0.01f * y
	);

	// calculates current position
	crntPos = vec3(model  * vec4(aPos, 1.0f)) + offset;
	// Assigns the normal from the Vertex Data to "Normal"
	Normal = aNormal;
	// Assigns the colors from the Vertex Data to "color"
	color = aColor;
	// Assigns the texture coordinates from the Vertex Data to "texCoord"
	texCoord = aTex;
	
	// Outputs the positions/coordinates of all vertices
	gl_Position = camMatrix * vec4(crntPos, 1.0);
}