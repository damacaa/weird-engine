#version 430 core

layout (location = 0) in vec2 aPos;      // Vertex positions (e.g., for a quad)
layout (location = 1) in vec2 aTexCoords; // Texture coordinates

out vec2 TexCoords; // Pass texture coordinates to the fragment shader

void main()
{
    // Assuming aPos are normalized device coordinates for a screen-filling quad
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoords = aTexCoords; // Pass through texture coordinates
}
