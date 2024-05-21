#version 330 core

layout (location = 0) in vec2 aPos;  // Vertex positions for the full-screen quad
layout (location = 1) in vec2 aTexCoord;  // Texture coordinates

out vec2 TexCoord;  // Pass the texture coordinates to the fragment shader

void main() {
    TexCoord = aTexCoord;  // Pass the texture coordinates to the fragment shader
    gl_Position = vec4(aPos, 0.0, 1.0);  // Map to NDC (Normalized Device Coordinates)
}