#version 330 core

in vec2 TexCoord;  // Texture coordinates from the vertex shader

uniform sampler2D u_depthTexture;  // The depth texture from the FBO

out vec4 FragColor;  // The color output

void main() {
    // Read the depth value from the texture (normalized to [0, 1])
    float depth = texture(u_depthTexture, TexCoord).r;  // Depth is stored in the red channel

    // Convert depth to grayscale
    //FragColor = vec4(vec3(depth), 1.0);  // Set the color based on the depth
    //FragColor = vec4(vec3(TexCoord, 0.0), 1.0);  // Set the color based on the depth
    FragColor = vec4(vec3(1.0, 0.0, 0.0), 1.0);  // Set the color based on the depth
}