#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_colorTexture;

uniform bool u_horizontal;
uniform float u_weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

vec3 rgb2hsl(vec3 c) {
    float r = c.r, g = c.g, b = c.b;

    float maxC = max(r, max(g, b));
    float minC = min(r, min(g, b));
    float delta = maxC - minC;

    float h = 0.0;
    float s = 0.0;
    float l = (maxC + minC) * 0.5;

    if (delta > 0.00001) {
        s = delta / (1.0 - abs(2.0 * l - 1.0));

        if (maxC == r) {
            h = mod((g - b) / delta, 6.0);
        } else if (maxC == g) {
            h = (b - r) / delta + 2.0;
        } else {
            h = (r - g) / delta + 4.0;
        }

        h /= 6.0;
        if (h < 0.0) h += 1.0;
    }

    return vec3(h, s, l); // H ∈ [0,1], S ∈ [0,1], L ∈ [0,1]
}

float hue2rgb(float p, float q, float t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

vec3 hsl2rgb(vec3 hsl) {
    float h = hsl.x;
    float s = hsl.y;
    float l = hsl.z;

    float r, g, b;

    if (s == 0.0) {
        r = g = b = l; // achromatic
    } else {
        float q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
        float p = 2.0 * l - q;
        r = hue2rgb(p, q, h + 1.0/3.0);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0/3.0);
    }

    return vec3(r, g, b);
}

vec3 toLinear(vec3 srgb) {
    return pow(srgb, vec3(2.2)); // or use inverse gamma
}
vec3 toSRGB(vec3 linear) {
    return pow(linear, vec3(1.0 / 2.2));
}


// Taken form learnopengl.com and optimized it to reduce branching
void main()
{
    vec2 tex_offset = 1.0 / textureSize(t_colorTexture, 0);
    vec4 data = texture(t_colorTexture, v_texCoord);
    float mask = data.w;

    vec3 result = toLinear(data.rgb) * u_weight[0]; // TODO: precompute toLinear before this shader

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
