#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_colorTexture;

uniform bool u_horizontal;
uniform float u_weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);



vec3 toLinear(vec3 srgb) {
    return pow(srgb, vec3(2.2));// or use inverse gamma
}
vec3 toSRGB(vec3 linear) {
    return pow(linear, vec3(1.0 / 2.2));
}

void main()
{
    vec2 tex_offset = 1.0 / textureSize(t_colorTexture, 0);
    vec4 data = texture(t_colorTexture, v_texCoord);
    float mask = data.w;

    vec3 result = toLinear(data.rgb) * u_weight[0];// TODO: precompute toLinear before this shader

    for (int i = 1; i < 5; ++i)
    {
        vec2 offset = u_horizontal ?  vec2(tex_offset.x * i, 0.0) : vec2(0.0, tex_offset.y * i);

        vec4 colRight = texture(t_colorTexture, v_texCoord + offset);
        result += toLinear(colRight.rgb) * u_weight[i];

        vec4 colLeft = texture(t_colorTexture, v_texCoord - offset);
        result += toLinear(colLeft.rgb) * u_weight[i];
    }

    result = toSRGB(result);
    FragColor = vec4(mix(data.xyz, result, mask), data.a);
    // FragColor = vec4(vec3(mask), data.w);
}
