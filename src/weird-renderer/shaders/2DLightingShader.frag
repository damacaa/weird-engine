#version 330 core

#define SHADOWS_ENABLED
// #define SOFT_SHADOWS
#define DITHERING

// #define DEBUG_SHOW_DISTANCE
// #define DEBUG_SHOW_COLORS

// Constants
const int MAX_STEPS = 1000;
const float EPSILON = 0.00001;
const float NEAR = 0.1f;
const float FAR = 1.2f;

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

uniform mat4 u_camMatrix;
uniform vec2 u_resolution;
uniform float u_time;

uniform sampler2D t_colorTexture;
uniform sampler2D t_distanceTexture;
uniform sampler2D t_backgroundTexture;

uniform vec2 u_directionalLightDirection = vec2(0.7071f, 0.7071f);

#ifdef DITHERING

uniform float u_spread = .05;
uniform int u_colorCount = 16;

// Dithering and posterizing
uniform int u_bayer2[2 * 2] = int[2 * 2](
0, 2,
3, 1);

uniform int u_bayer4[4 * 4] = int[4 * 4](
0, 8, 2, 10,
12, 4, 14, 6,
3, 11, 1, 9,
15, 7, 13, 5);

uniform int u_bayer8[8 * 8] = int[8 * 8](
0, 32, 8, 40, 2, 34, 10, 42,
48, 16, 56, 24, 50, 18, 58, 26,
12, 44, 4, 36, 14, 46, 6, 38,
60, 28, 52, 20, 62, 30, 54, 22,
3, 35, 11, 43, 1, 33, 9, 41,
51, 19, 59, 27, 49, 17, 57, 25,
15, 47, 7, 39, 13, 45, 5, 37,
63, 31, 55, 23, 61, 29, 53, 21);

float Getu_bayer2(int x, int y)
{
    return float(u_bayer2[(x % 2) + (y % 2) * 2]) * (1.0f / 4.0f) - 0.5f;
}

float Getu_bayer4(int x, int y)
{
    return float(u_bayer4[(x % 4) + (y % 4) * 4]) * (1.0f / 16.0f) - 0.5f;
}

float Getu_bayer8(int x, int y)
{
    return float(u_bayer8[(x % 8) + (y % 8) * 8]) * (1.0f / 64.0f) - 0.5f;
}

#endif

float map(vec2 p)
{
    return texture(t_distanceTexture, p).x;
}

float rayMarch(vec2 ro, vec2 rd, out float minDistance)
{
    float d;
    minDistance = 10000.0;

    float traveled = 0.0;

    for (int i = 0; i < MAX_STEPS; i++)
    {
        vec2 p = ro + (traveled * rd);

        d = map(p);

        minDistance = min(d, minDistance);

        if (d <= EPSILON)
        break;

        if (p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0)
        return FAR;

        // traveled += 0.01;
        traveled += d;
        // traveled += min(d, 0.01);

        if (traveled >= FAR)
        {
            return FAR;
        }
    }

    minDistance = 0.0;
    return traveled;
}

const float MAX_DIST_INSIDE = 0.1;

float rayMarchInside(vec2 ro, vec2 rd)
{
    float d;
    float traveled = 0.0;

    for (int i = 0; i < MAX_STEPS; i++)
    {
        vec2 p = ro + (traveled * rd);

        d = map(p);

        if (d >= -EPSILON)
        break;

        traveled += -d;

        if (traveled >= MAX_DIST_INSIDE)
        {
            return MAX_DIST_INSIDE;
        }
    }

    return traveled;
}

float render(vec2 uv)
{

    #ifdef SHADOWS_ENABLED

    // Point light
    vec2 rd = normalize(vec2(1.0) - uv);

    // Directional light
    // vec2 rd = u_directionalLightDirection.xy;

    float mapDistance = map(uv);
    float minD;
    vec2 offsetPosition = uv + (2.0 / u_resolution) * rd;// 2 pixels towards the light

    if (mapDistance <= 0.0)
    {
        float distanceInside = rayMarchInside(uv, rd);

        vec2 surfacePos = uv + (rd * (distanceInside * 1.5));
        float surfaceD = rayMarch(surfacePos, rd, minD);

        // distanceInside += surfaceD < FAR ? 0.05 : 0.0;
        distanceInside = surfaceD < FAR ? MAX_DIST_INSIDE : distanceInside; // what if MAX_DIST_INSIDE is too big?
        float factorInside = (1.0 - (distanceInside/ MAX_DIST_INSIDE));
        factorInside = 1.5 * pow(factorInside, 3);
        factorInside = max(1.0, factorInside);

        return factorInside;
    }

    float d = rayMarch(uv, rd, minD);

    const float NORMAL_EPSILON = 0.001;

    vec2 p = uv;
    float d1 = map(p + vec2(NORMAL_EPSILON, 0.0)) - map(p - vec2(NORMAL_EPSILON, 0.0));
    float d2 = map(p + vec2(0.0, NORMAL_EPSILON)) - map(p - vec2(0.0, NORMAL_EPSILON));

    vec2 normal = normalize(vec2(d1, d2));

    // If ray doesnt go to infinity, cast shadow
    // Original distance is substracted to fade  shadow when close to surfaces
    // return d < FAR ? min(0.5 + d, 0.85) : 1.0;

    float ddot = -(dot(-rd, normal));
    float ddotMask = max(1.0 - (100.0 * mapDistance), 0.0);
    float finalDdot = 5.0 * clamp(0.01 * (ddot * ddotMask), 0.0, 0.01);
    float extraShadow =  clamp(10.0 * (d + 0.09), 0.5, 1.0); // Harder shadows close to the object

    return d < FAR ? min(0.85 + finalDdot , 0.95) * extraShadow : 1.0; // + (0.05 * d)


    #ifdef SOFT_SHADOWS
    return mix(0.85, 1.0, d / FAR);
    #else
    return d < FAR ? 0.85 : 1.0;
    #endif

    #else// No shadows

    return 1.0;

    #endif
}

void main()
{
    vec2 screenUV = v_texCoord;
    vec4 colorSample = texture(t_colorTexture, screenUV);
    vec3 color = colorSample.rgb;// + (0.25 * floor(colorSample.a));
    float alpha = colorSample.a;// + (0.25 * floor(colorSample.a));
    vec4 data = texture(t_distanceTexture, screenUV);
    float distance = data.x;

    vec3 backgroundColor = texture(t_backgroundTexture, screenUV).rgb;// vec3(0.35);

    bool isShape = distance < 0.0;

    #ifdef DEBUG_SHOW_COLORS
    isShape = true;
    #endif

    color = mix(color, backgroundColor, isShape ? 1.0 - alpha : 1.0);

    float light = render(screenUV);
    vec3 lightColor = vec3(light);
    vec3 ambienLight = vec3(0.0, 0.0, 0.0);
    lightColor += ambienLight;
    vec3 col = lightColor * color.xyz;
    // col = vec3(light);


    #ifdef DITHERING

    int x = int(gl_FragCoord.x);
    int y = int(gl_FragCoord.y);
    col = col + u_spread * Getu_bayer4(x, y);

    col.r = floor((u_colorCount - 1.0f) * col.r + 0.5) / (u_colorCount - 1.0f);
    col.g = floor((u_colorCount - 1.0f) * col.g + 0.5) / (u_colorCount - 1.0f);
    col.b = floor((u_colorCount - 1.0f) * col.b + 0.5) / (u_colorCount - 1.0f);

    #endif


    FragColor = vec4(col.xyz, 1.0);


    // Distance debug
    #ifdef DEBUG_SHOW_DISTANCE

    float value = 0.5 * (cos(500.0 * distance) + 1.0);
    // value = value * value * value;
    vec3 debugColor = distance > 0 ? mix(vec3(1), vec3(0.2), value) :// outside
    (distance + 1.0) * mix(vec3(1.0, 0.2, 0.2), vec3(0.1), value);// inside

    FragColor = vec4(debugColor, 1.0);

    #endif
}
