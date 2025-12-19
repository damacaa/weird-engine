#version 330 core

// #define SHADOWS_ENABLED
// #define DITHERING
// #define ANTIALIASING

// #define DEBUG_SHOW_DISTANCE
// #define DEBUG_SHOW_COLORS

// Constants
const int MAX_STEPS = 128;
const float EPSILON = 0.05;
const float NEAR = 0.1f;
const float FAR = 1.4f;
const float NORMAL_EPSILON = 0.1;

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
uniform sampler2D t_shadowDistanceTexture;

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
    return texture(t_shadowDistanceTexture, p).x;
}

float mapInside(vec2 p)
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

float calculateLight(vec2 uv, vec2 rd, float shadows, float innerDistance)
{
    vec2 p = uv;
    float d1 = mapInside(p + vec2(NORMAL_EPSILON, 0.0)) - mapInside(p - vec2(NORMAL_EPSILON, 0.0));
    float d2 = mapInside(p + vec2(0.0, NORMAL_EPSILON)) - mapInside(p - vec2(0.0, NORMAL_EPSILON));
    vec2 normal = normalize(vec2(d1, d2));
    float lightNormalDot = -(dot(-rd, normal));

    float zoom = -u_camMatrix[3].z;
    float light = mix(1.0, max(1.0, lightNormalDot * 2.0 - (innerDistance * 10.0 * zoom)), shadows - 0.85);

    #ifndef SHADOWS_ENABLED
    light = 1.0;
    #endif

    return light * light;
}

vec2 softShadow(vec2 ro, vec2 rd, float minD, float far, float k, out int iter) {
    float res = 1.0;
    float t = minD;
    iter = 0;

    for(int i = 0; i < MAX_STEPS; i++)
    {
        vec2 p = ro + rd * t;

        // Center the coordinates at 0.5, take absolute value
        // If result > 0.5, it was outside [0, 1]
        vec2 dist = abs(p - 0.5);
        if (max(dist.x, dist.y) > 0.5) {
            break;
        }

        float h = map(ro + rd * t); // Your SDF function

        // Improve shadow quality by comparing distance to object (h) vs distance traveled (t)
        res = min(res, k * h / t);

        if(h <= 0.0 || res < EPSILON) return vec2(0.0); // Fully in shadow
        if(t > far) break;          // Missed everything

        t += h; // TODO: why do I need smaller steps to get rid of artifacts?!?!

        iter++;
    }

    // Clamp result to prevent weird artifacts
    return vec2(t, clamp(res, 0.0, 1.0));
}



float renderShadows(vec2 uv)
{
    #ifdef SHADOWS_ENABLED

    // Point light
    vec2 rd = normalize(vec2(1.0) - uv);

    // Directional light
    // vec2 rd = u_directionalLightDirection.xy;

    float mapDistance = map(uv);
    float minD = mapDistance;
    vec2 offsetPosition = uv + (2.0 / u_resolution) * rd;// 2 pixels towards the light

    int iter;
    vec2 raymarchInfo = softShadow(uv, rd, minD, FAR, 8.0, iter);
    float d = 1.0; // raymarchInfo.x;


    float shadowFactor = raymarchInfo.y;
    // mix(ShadowColor, LightColor, factor)
    float shadowValue = mix(0.85, 1.0, shadowFactor);

    //    const float NORMAL_EPSILON = 0.001;
    //    vec2 p = uv;
    //    float d1 = map(p + vec2(NORMAL_EPSILON, 0.0)) - map(p - vec2(NORMAL_EPSILON, 0.0));
    //    float d2 = map(p + vec2(0.0, NORMAL_EPSILON)) - map(p - vec2(0.0, NORMAL_EPSILON));
    //    vec2 normal = normalize(vec2(d1, d2));
    //     float ddot = -(dot(-rd, normal));
    //     float ddotMask = max(1.0 - (100.0 * mapDistance), 0.0);
    //     float finalDdot = 5.0 * clamp(0.01 * (ddot * ddotMask), 0.0, 0.01); // * d to be affected by distance. Issues with overlapping shadows
    //     shadowValue = min(shadowValue + finalDdot, 0.95);

    float extraShadow =  clamp(10.0 * (d + 0.09), 0.5, 1.0); // Harder shadows close to the object
    shadowValue *= extraShadow;

    // return iter / 100.0;
    return shadowValue;

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



    #ifdef ANTIALIASING

    // Distance change over one pixel
    float smoothing = 1.0 * fwidth(distance);

    // Convert the distance to a factor between 0.0 and 1.0
    float shapeFactor = 1.0 - smoothstep(-smoothing, smoothing, distance);

    #else

    float shapeFactor = distance <= 0.0 ? 1.0 : 0.0;

    #endif

    #ifdef DEBUG_SHOW_COLORS
    shapeFactor = 1.0;
    #endif

    float zoom = -u_camMatrix[3].z;

    vec2 rd = normalize(vec2(1.0) - screenUV);
    float shadows = renderShadows(screenUV + ((0.1 / zoom) * rd));
    float light = calculateLight(screenUV, rd, shadows, -distance); //distance <= 0.0? mix(1.2, 0.5, 1.0 - shadows) : 1.0; // render(screenUV);

    // Refraction
    #ifdef REFRACTION

    vec2 p = screenUV;
    float d1 = mapInside(p + vec2(NORMAL_EPSILON, 0.0)) - mapInside(p - vec2(NORMAL_EPSILON, 0.0));
    float d2 = mapInside(p + vec2(0.0, NORMAL_EPSILON)) - mapInside(p - vec2(0.0, NORMAL_EPSILON));
    vec2 g = vec2(d1, d2);
    float len2 = dot(g, g);
    // Prevent NaN vector if length == 0
    vec2 normal = (len2 > 1e-8) ? g * inversesqrt(len2) : vec2(0.0);

    // normal = vec2(1.0, 1.0);

    // Sample background with refraction
    float refractionDistance = -1.0 / (1.0 - clamp(((-distance * 100.0) + 1.0), 0.0, 10.0));
    refractionDistance = max(0.0, refractionDistance - 0.1);
    // refractionDistance = sqrt(refractionDistance);
    // refractionDistance = 1.0;
    vec2 backgroundOffset = 0.01 * shapeFactor * refractionDistance * normal;
    vec3 backgroundColor = texture(t_backgroundTexture, screenUV + backgroundOffset).rgb;

    #else

    vec3 backgroundColor = texture(t_backgroundTexture, screenUV).rgb;

    #endif

    // Combine the material's alpha with shape factor
    float finalAlpha = alpha * shapeFactor;
    // finalAlpha = 0.25;
    color = mix(color * light, backgroundColor * shadows, 1.0 - finalAlpha);
    vec3 col = color;

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

    #ifdef REFRACTION
        FragColor = vec4(vec3(length(refractionDistance * 0.1)), 1.0);
    // return;
    #endif


    float debugDistance = distance;
    // float debugDistance = texture(t_shadowDistanceTexture, screenUV).x;

    float value = 0.5 * (cos(500.0 * debugDistance) + 1.0);
    // value = debugDistance * 10.;
    // value = value * value * value;
    vec3 debugColor = debugDistance > 0.0 ? mix(vec3(1), vec3(0.2), value) :// outside
    (debugDistance + 1.0) * mix(vec3(1.0, 0.2, 0.2), vec3(0.1), value);// inside

    FragColor = vec4(debugColor, 1.0);

    #endif
}
