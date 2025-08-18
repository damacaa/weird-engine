#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_colorTexture;

uniform bool u_horizontal;
uniform float u_weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
uniform float u_time;



vec3 toLinear(vec3 srgb) {
    return pow(srgb, vec3(2.2));// or use inverse gamma
}
vec3 toSRGB(vec3 linear) {
    return pow(linear, vec3(1.0 / 2.2));
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
    vec2 tex_offset = 1.0 / textureSize(t_colorTexture, 0);
    vec2 uv = tex_offset + vec2(hash(gl_FragCoord.xy + u_time));

    vec4 data = texture(t_colorTexture, v_texCoord);
    float mask = data.w;

    vec3 originalColor = toLinear(data.rgb);
    vec3 result = originalColor * u_weight[0];// TODO: precompute toLinear before this shader

    for (int i = 1; i < 5; ++i)
    {
        vec2 offset = u_horizontal ?  vec2((tex_offset.x * i), 0.0) : vec2(0.0, (tex_offset.y * i)); // (tex_offset.x * i) + hash(gl_FragCoord.xy + u_time)

        vec4 colRight = texture(t_colorTexture, v_texCoord + offset);
        result += mix(toLinear(colRight.rgb), originalColor, 1.0 - colRight.a) * u_weight[i];

        vec4 colLeft = texture(t_colorTexture, v_texCoord - offset);
        result += mix(toLinear(colLeft.rgb), originalColor, 1.0 - colLeft.a) * u_weight[i];
    }

    result = toSRGB(result);
    FragColor = vec4(mix(data.xyz, result, mask), data.a);
    // FragColor = vec4(vec3(mask), data.w);
}
