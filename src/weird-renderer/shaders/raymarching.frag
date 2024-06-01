#version 330 core

// Operations
float fOpUnionSoft(float a, float b, float r)
{
    float e = max(r - abs(a - b), 0);
    return min(a, b) - e * e * 0.25 / r;
}

// Shapes
float fSphere(vec3 p, float r)
{
    return length(p) - r;
}

float fPlane(vec3 p, vec3 n, float distanceFromOrigin)
{
    return dot(p, n) + distanceFromOrigin;
}

float fCylinder(vec3 p, float r, float height)
{
    float d = length(p.xz) - r;
    d = max(d, abs(p.y) - height);
    return d;
}

struct Shape
{
    vec3 position;
    float size;
};

// Outputs colors in RGBA
layout(location = 0) out vec4 FragColor;

uniform int u_loadedObjects;
layout(std140) uniform u_shapes
{ // "preferably std430" ?
    Shape data[10];
};

uniform mat4 u_cameraMatrix;

uniform vec2 u_resolution;
uniform float u_time;

uniform float u_fov = 2.5;

const int MAX_STEPS = 256;
const float EPSILON = 0.01;
const float NEAR = 0.1f;
const float FAR = 100.0f;

const float OVERSHOOT = 1.0;

const vec3 background = vec3(0.0);

uniform sampler2D u_colorTexture;
uniform sampler2D u_depthTexture;

uniform vec3 directionalLightDirection;

vec2 fOpUnionID(vec2 res1, vec2 res2)
{
    return (res1.x < res2.x) ? res1 : res2;
}

vec2 fOpSubtractionID(vec2 res1, vec2 res2)
{
    return (-res1.x > res2.x) ? -res1 : res2;
}

vec2 fSmoothUnionID(vec2 res1, vec2 res2)
{
    return vec2(fOpUnionSoft(res1.x, res2.x, 0.5), res1.y);
}

float opSmoochUnion(float d1, float d2, float k)
{
    float h = max(k - abs(d1 - d2), 0.0);
    return min(d1, d2) + h * h * 0.35 / k;
}

vec2 map(vec3 p)
{

    vec2 res = vec2(1000000000.0, 0.0);

    for (int i = 0; i < u_loadedObjects; i++)
    {
        float sphereDist = fSphere(p - data[i].position, data[i].size);
        // float sphereDist =  fBox(p - data[i].position, data[i].size);
        float sphereID = 0.0;
        vec2 sphere = vec2(sphereDist, sphereID);

        res = fSmoothUnionID(sphere, res);
        // res = fOpUnionID(sphere, res);
    }

    float planeDist = fPlane(p, vec3(0, 1, 0), 0.0);
    float planeID = 1.0;
    vec2 plane = vec2(planeDist, planeID);

    // vec2 cylinder = vec2(fCylinder(p-vec3(0), 5, 100), 0.0);
    // plane = fOpSubtractionID(cylinder, plane);

    res = fOpUnionID(res, plane);

    return res;
}

vec2 rayMarch(vec3 ro, vec3 rd)
{

    vec2 hit;

    float traveled = NEAR;
    float id = 0;

    float overshoot = OVERSHOOT;
    float move = 0.0;

    for (int i = 0; i < MAX_STEPS; i++)
    {

        vec3 p = ro + (traveled * rd);

        hit = map(p);

        if (sign(hit.x) < 0.0)
        {

            float moveBack = move * overshoot;
            traveled -= moveBack;

            p = ro + (traveled * rd);

            hit = map(p);

            overshoot = 1.0; // * disable overshoot now
        }

        move = hit.x * overshoot;
        traveled += move;

        // eps *= EPS_MULTIPLIER;

        id = hit.y;

        if (abs(hit.x) < EPSILON || traveled > FAR)
            break;
    }

    return vec2(traveled, id);
}

vec3 getNormal(vec3 p)
{
    vec2 e = vec2(EPSILON, 0.0);
    vec3 n = vec3(map(p).x) - vec3(map(p - e.xyy).x, map(p - e.yxy).x, map(p - e.yyx).x);
    return normalize(n);
}

vec3 getLight(vec3 p, vec3 rd, vec3 color)
{

    vec3 lightPos = directionalLightDirection;

    vec3 L = normalize(lightPos - p);
    vec3 N = getNormal(p);
    vec3 V = -rd;
    vec3 R = reflect(-L, N);

    // color = vec3(0.0);

    vec3 specColor = vec3(0.5);
    vec3 specular = specColor * pow(clamp(dot(R, V), 0.0, 1.0), 100.0);

    vec3 diffuse = color * clamp(dot(L, N), 0.0, 1.0);

    vec3 ambient = color * 0.05;
    vec3 fresnel = 0.1 * color * pow(1.0 + dot(rd, N), 3.0);

    vec2 shadowObject = rayMarch(p - rd * 0.02, normalize(lightPos));
    float d = shadowObject.x;

    return (d > length(lightPos - p)) ? diffuse + ambient + specular + fresnel
                                      : ambient + fresnel;
}

vec3 getDirectionalLight(vec3 p, vec3 rd, vec3 color)
{
    vec3 L = directionalLightDirection;
    vec3 N = getNormal(p);
    vec3 V = -rd;
    vec3 R = reflect(-L, N);

    // color = vec3(0.0);

    vec3 specColor = vec3(0.5);
    vec3 specular = specColor * pow(clamp(dot(R, V), 0.0, 1.0), 100.0);

    vec3 diffuse = color * clamp(dot(L, N), 0.0, 1.0);

    vec3 ambient = color * 0.05;
    vec3 fresnel = 0.1 * color * pow(1.0 + dot(rd, N), 3.0);

    vec2 shadowObject = rayMarch(p - rd * 0.02, L);
    float d = shadowObject.x;

    return (d >= FAR * length(L)) ? diffuse + ambient + specular + fresnel
                                      : ambient + fresnel;
}

vec3 getMaterial(vec3 p, float id)
{
    vec3 m;
    vec3 colors[2];
    colors[0] = vec3(0.0, 1.0, 0.0);
    colors[1] = vec3(0.2 + 0.4 * mod(floor(p.x) + floor(p.z), 2.0));

    return colors[int(id)];
}

vec3 Render(in vec2 uv, in vec3 originalColor, in float depth)
{

    // Ray origin
    vec3 ro = -u_cameraMatrix[3].xyz;
    // Apply camera rotation
    ro = vec3(dot(u_cameraMatrix[0].xyz, ro), dot(u_cameraMatrix[1].xyz, ro), dot(u_cameraMatrix[2].xyz, ro));

    // Ray direction
    vec3 rd = (vec4(normalize(vec3(uv, -u_fov)), 0) * u_cameraMatrix).xyz;

    // Fish eye
    // float z = pow(1.0 - (uv.x * uv.x) - (uv.y * uv.y), 0.5);
    // vec3 rd = vec3(uv, -z);
    // rd = (vec4(rd, 0) * u_cameraMatrix).xyz;

    // Ray march to find closest SDF
    vec2 object = rayMarch(ro, rd);

    // If ray marched distance is bigger than depth from zbuffer, set alpha to 1
    float minDepth = min(object.x, depth);

    // Output color
    vec3 col;

    // Scene alpha over background
    float alpha = 1.0;
    if (minDepth < FAR)
    {

        if (object.x < depth)
        {
            // col += 3.0 / object.x;
            vec3 p = ro + object.x * rd;
            vec3 material = getMaterial(p, object.y);
            col += getDirectionalLight(p, rd, material);
        }
        else
        {
            col = originalColor;
        }

        // fog

        minDepth -= 0.85;
        alpha = 1.0 - exp(-0.001 * minDepth * minDepth);

        //float a = minDepth - (FAR - 980);
        //alpha = max(0.0, 0.001 * a * a *a );
    }

    col = mix(col, background, alpha);

    return col;
}

void main()
{

    float aspectRatio = u_resolution.x / u_resolution.y;
    vec2 pixelScale = vec2(aspectRatio * 0.2, 0.2);

    // vec2 fragCoord = floor(gl_FragCoord.xy*pixelScale) / pixelScale;
    vec2 fragCoord = gl_FragCoord.xy;

    vec2 screenUV = (fragCoord.xy / u_resolution.xy);

    vec2 uv = (2.0 * fragCoord.xy - u_resolution.xy) / u_resolution.y;

    // Calculate true z value from the depth buffer: https://stackoverflow.com/questions/6652253/getting-the-true-z-value-from-the-depth-buffer
    float depth = texture(u_depthTexture, screenUV).r;
    float z_n = 2.0 * depth - 1.0;
    float z_e = 2.0 * NEAR * FAR / (FAR + NEAR - z_n * (FAR - NEAR));

    vec3 originalColor = texture(u_colorTexture, screenUV).xyz;

    vec3 col = Render(uv, originalColor, z_e);
    col = pow(col, vec3(0.4545));

    FragColor = vec4(col, 1.0);
    // FragColor = vec4(screenUV, 0.0, 0.0);
}
