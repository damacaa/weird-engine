#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_sceneDepth;
uniform vec2 u_pixelSize;

float linearDepth(float z, float near, float far)
{
    return (2.0 * near) / (far + near - z * (far - near));
}

void main()
{
    vec2 uv = v_texCoord;
    float depth = linearDepth(texture(t_sceneDepth, uv).x, 0.1, 100.0);

    float maxDifference = 0.0;
    vec2 offsets[4] = vec2[](
        vec2(u_pixelSize.x, 0.0),
        vec2(-u_pixelSize.x, 0.0),
        vec2(0.0, u_pixelSize.y),
        vec2(0.0, -u_pixelSize.y));

    for (int i = 0; i < 4; i++)
    {
        float neighbor = texture(t_sceneDepth, uv + offsets[i]).x;
        neighbor = linearDepth(neighbor, 0.1, 100.0);
        float diff = neighbor - depth;
        maxDifference = max(maxDifference, diff);
    }

    float alpha = maxDifference > 0.025 ? 1.0 : 0.0;
    FragColor = vec4(vec3(1.0), alpha);
}
