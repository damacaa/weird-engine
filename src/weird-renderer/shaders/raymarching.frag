#version 330 core

// Dithering and posterizing
uniform int bayer2[2 * 2] = int[2 * 2](
    0, 2,
    3, 1
);

uniform int bayer4[4 * 4] = int[4 * 4](
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5
);

uniform int bayer8[8 * 8] = int[8 * 8](
    0, 32, 8, 40, 2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,  
    12, 44,  4, 36, 14, 46,  6, 38, 
    60, 28, 52, 20, 62, 30, 54, 22,  
    3, 35, 11, 43,  1, 33,  9, 41,  
    51, 19, 59, 27, 49, 17, 57, 25, 
    15, 47,  7, 39, 13, 45,  5, 37, 
    63, 31, 55, 23, 61, 29, 53, 21
);

uniform float _Spread = 0.15f;
uniform int _ColorCount = 5;


float GetBayer2(int x, int y) {
    return float(bayer2[(x % 2) + (y % 2) * 2]) * (1.0f / 4.0f) - 0.5f;
}

float GetBayer4(int x, int y) {
    return float(bayer4[(x % 4) + (y % 4) * 4]) * (1.0f / 16.0f) - 0.5f;
}

float GetBayer8(int x, int y) {
    return float(bayer8[(x % 8) + (y % 8) * 8]) * (1.0f / 64.0f) - 0.5f;
}

// Operations
float fOpUnionSoft(float a, float b, float r)
{
    float e = max(r - abs(a - b), 0);
    return min(a, b) - e * e * 0.25 / r;
}

vec2 fOpUnionSoft2( float a, float b, float k )
{
    float h = max( k - abs(a-b), 0.0 )/k;
    float m = h*h*h*0.5;
    float s = m*k*(1.0/3.0); 
    return (a < b) ? vec2(a-s,m) : vec2(b-s,1.0-m);
}

float fOpSmoochUnion(float d1, float d2, float k)
{
    float h = max(k - abs(d1 - d2), 0.0);
    return min(d1, d2) + h * h * 0.35 / k;
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







float map(vec3 p)
{
    float res = FAR;

    for (int i = 0; i < u_loadedObjects; i++)
    {
        float sphereDist = fSphere(p - data[i].position, data[i].size);

        res = fOpUnionSoft(sphereDist, res, 0.5);
    }

    float planeDist = fPlane(p, vec3(0, 1, 0), 0.0);

    res = min(res, planeDist);

    return res;
}


vec3 getMaterial(vec3 p, int id)
{
    vec3 m;
    vec3 colors[3];
    colors[0] = vec3(0.2 + 0.4 * mod(floor(p.x) + floor(p.z), 2.0));
    colors[1] = vec3(1.0,0.05,0.01);
    colors[2] = vec3(0.1, 0.05, 0.80);

    return colors[id];
}




vec3 getColor(vec3 p)
{

    float d = FAR;
    vec3 col = vec3(0.5);


    for (int i = 0; i < u_loadedObjects; i++)
    {
        float sphereDist = fSphere(p - data[i].position, data[i].size);
        
        vec2 result = fOpUnionSoft2(sphereDist, d, 0.5);
        

        int id = i % 2 == 0 ? 1 : 2;
        d = result.x;
        col = mix(getMaterial(p, id), col, result.y);
        
    }

    float planeDist = fPlane(p, vec3(0, 1, 0), 0.0);

    d = min(d, planeDist);
    col = mix(getMaterial(p, 0), col, d < planeDist ? 1.0 : 0.0);

    return col;
}


float rayMarch(vec3 ro, vec3 rd)
{
    float hit;

    float traveled = NEAR;

    for (int i = 0; i < MAX_STEPS; i++)
    {
        vec3 p = ro + (traveled * rd);

        hit = map(p);


        if (abs(hit) < EPSILON || traveled > FAR)
            break;

        traveled += hit;
    }

    return traveled;
}

vec3 getNormal(vec3 p)
{
    vec2 e = vec2(EPSILON, 0.0);
    vec3 n = vec3(map(p)) - vec3(map(p - e.xyy), map(p - e.yxy), map(p - e.yyx));
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

    float d = rayMarch(p - rd * 0.02, normalize(lightPos));

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

    vec3 specColor = vec3(1.0);
    vec3 specular = specColor * pow(clamp(dot(R, V), 0.0, 1.0), 100.0);

    vec3 diffuse = color * clamp(dot(L, N), 0.0, 1.0);

    vec3 ambient = color * 0.05;
    vec3 fresnel = 0.1 * color * pow(1.0 + dot(rd, N), 3.0);

    float d = rayMarch(p - rd * 0.02, L);

    return (d >= FAR * length(L)) ? diffuse + ambient + specular + fresnel
                                      : ambient + fresnel;
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
    float object = rayMarch(ro, rd);

    // If ray marched distance is bigger than depth from zbuffer, set alpha to 1
    float minDepth = min(object, depth);


    // Output color
    vec3 col;

    // Scene alpha over background
    float alpha = 1.0;
    if (minDepth < FAR)
    {

        if (object < depth)
        {
            vec3 p = ro + object * rd;
            vec3 material = getColor(p);
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

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{

    float aspectRatio = u_resolution.x / u_resolution.y;
    vec2 pixelScale = vec2(aspectRatio * 0.2, 0.2);

    vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);

    vec2 uv = (2.0 * gl_FragCoord.xy - u_resolution.xy) / u_resolution.y;

    // Calculate true z value from the depth buffer: https://stackoverflow.com/questions/6652253/getting-the-true-z-value-from-the-depth-buffer
    float depth = texture(u_depthTexture, screenUV).r;
    float z_n = 2.0 * depth - 1.0;
    float z_e = 2.0 * NEAR * FAR / (FAR + NEAR - z_n * (FAR - NEAR));

    vec3 originalColor = texture(u_colorTexture, screenUV).xyz;

    vec3 col = 1.0f*Render(uv, originalColor, z_e);

    col = pow(col, vec3(0.4545));

    int x = int(gl_FragCoord.x);
    int y = int(gl_FragCoord.y);
    col  = col + _Spread * GetBayer4(x, y);

    col.r = floor((_ColorCount - 1.0f) * col.r + 0.5) / (_ColorCount - 1.0f);
    col.g = floor((_ColorCount - 1.0f) * col.g + 0.5) / (_ColorCount - 1.0f);
    col.b = floor((_ColorCount - 1.0f) * col.b + 0.5) / (_ColorCount - 1.0f);

    FragColor = vec4(col.xyz, 1.0);
}
