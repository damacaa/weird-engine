#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_colorTexture;

uniform bool u_horizontal;
uniform float u_weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

// Taken form learnopengl.com and optimized it to reduce branching
void main()
{             
    vec2 tex_offset = 1.0 / textureSize(t_colorTexture, 0); // gets size of single texel
    vec3 result = texture(t_colorTexture, v_texCoord).rgb * u_weight[0]; // current fragment's contribution

    for(int i = 1; i < 5; ++i)
    {
        vec2 offset = u_horizontal ?  vec2(tex_offset.x * i, 0.0) : vec2(0.0, tex_offset.y * i);
        result += texture(t_colorTexture, v_texCoord + offset).rgb * u_weight[i];
        result += texture(t_colorTexture, v_texCoord - offset).rgb * u_weight[i];
    }

    // Make sure there are no negative values
    FragColor = vec4(max(result, 0.0), 1.0);
}
