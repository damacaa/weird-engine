#version 330 core
#extension GL_ARB_conservative_depth : enable
#extension GL_EXT_conservative_depth : enable

layout (depth_less) out float gl_FragDepth;


// #define DITHERING



#ifdef DITHERING

uniform float u_spread = 0.15f;
uniform int u_colorCount = 10;

// Dithering and posterizing
uniform int u_bayer2[2 * 2] = int[2 * 2](
    0, 2,
    3, 1
);

uniform int u_bayer4[4 * 4] = int[4 * 4](
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5
);

uniform int u_bayer8[8 * 8] = int[8 * 8](
    0, 32, 8, 40, 2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,  
    12, 44,  4, 36, 14, 46,  6, 38, 
    60, 28, 52, 20, 62, 30, 54, 22,  
    3, 35, 11, 43,  1, 33,  9, 41,  
    51, 19, 59, 27, 49, 17, 57, 25, 
    15, 47,  7, 39, 13, 45,  5, 37, 
    63, 31, 55, 23, 61, 29, 53, 21
);

float GetBayer2(int x, int y) {
    return float(u_bayer2[(x % 2) + (y % 2) * 2]) * (1.0f / 4.0f) - 0.5f;
}

float GetBayer4(int x, int y) {
    return float(u_bayer4[(x % 4) + (y % 4) * 4]) * (1.0f / 16.0f) - 0.5f;
}

float GetBayer8(int x, int y) {
    return float(u_bayer8[(x % 8) + (y % 8) * 8]) * (1.0f / 64.0f) - 0.5f;
}

#endif

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

float vmax(vec3 v) {
	return max(max(v.x, v.y), v.z);
}

float fBox(vec3 p, vec3 b) {
	vec3 d = abs(p) - b;
	return length(max(d, vec3(0))) + vmax(min(d, vec3(0)));
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

// Outputs colors in RGBA
layout(location = 0) out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_colorTexture;
uniform sampler2D t_depthTexture;
uniform samplerBuffer t_shapeBuffer;
uniform int u_loadedObjects;


uniform mat4 u_camMatrix;
uniform float u_fov = 2.5;
uniform vec2 u_resolution;

uniform vec3 u_lightPos;
uniform vec3 u_lightDirection;
uniform vec4 u_lightColor;

uniform float u_time;






const int MAX_STEPS = 256;
const float EPSILON = 0.01;
const float NEAR = 0.1f;
const float FAR = 100.0f;

const float OVERSHOOT = 1.0;

const vec3 background = vec3(0.0);





float map(vec3 p)
{
    float res = FAR;

    for (int i = 0; i < u_loadedObjects; i++)
    {
        vec4 positionSize = texelFetch(t_shapeBuffer, i);
        // float objectDist = fSphere(p - data[i].position, data[i].size);
        //float objectDist = fSphere(p - vec3(2.0f * sin(-u_time), 1.0f, 2.0f * cos(-u_time)), 0.5f);
        float objectDist = fSphere(p - positionSize.xyz, 0.5f + (sin(u_time) + 1)); // positionSize.w);
        

        res = fOpUnionSoft(objectDist, res, 0.5);
    }

    float planeDist = fPlane(p, vec3(0, 1, 0), 0.2 * sin(length(p) + u_time) + 0.5);
    // float planeDist = fPlane(p, vec3(0, 1, 0), 0.5 * ((sin(2 * p.x) + sin(2 * p.z)) * sin(u_time)) + 0.5);

    res = min(res, planeDist);

    return res;
}


vec3 getMaterial(vec3 p, int id)
{
    vec3 colors[3];
    colors[0] = vec3(0.2 + 0.4 * mod(floor(p.x) + floor(p.z), 2.0));
    colors[1] = vec3(1.0,0.05,0.01);
    colors[2] = vec3(0.1, 0.05, 0.80);

    return colors[id];
}

vec3 getColor(vec3 p)
{
    float d = FAR;
    vec3 col = vec3(0.0);

    float k = 0.5;

    for (int i = 0; i < u_loadedObjects; i++)
    {
        int id = i % 2 == 0 ? 1 : 2;

        vec4 positionSize = texelFetch(t_shapeBuffer, i);
        // float objectDist = fSphere(p - data[i].position, data[i].size);
        //float objectDist = fSphere(p - vec3(2.0f * sin(-u_time), 1.0f, 2.0f * cos(-u_time)), 0.5f);
        float objectDist = fSphere(p - positionSize.xyz, 0.5f);
        
        //float delta = objectDist / (objectDist + d); // Calculate using old d
        d = fOpUnionSoft(objectDist, d, k);
        float delta = 1 - (max( k - abs(objectDist - d), 0.0 ) / k); // After new d is calculated

        col = mix(getMaterial(p, id), col, delta);   
    }

    float planeDist = fPlane(p, vec3(0, 1, 0), 0.0);

    d = min(d, planeDist);
    col = d >= planeDist ? getMaterial(p, 0) : col;

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

    vec3 lightPos = u_lightPos;

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
    vec3 L = u_lightDirection;
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


vec4 render(in vec2 uv, in vec4 originalColor)
{

    // Ray origin
    vec3 ro = -u_camMatrix[3].xyz;
    // Apply camera rotation
    ro = vec3(dot(u_camMatrix[0].xyz, ro), dot(u_camMatrix[1].xyz, ro), dot(u_camMatrix[2].xyz, ro));

    // Ray direction
    vec3 rd = (vec4(normalize(vec3(uv, -u_fov)), 0) * u_camMatrix).xyz;

    // Fish eye
    // float z = pow(1.0 - (uv.x * uv.x) - (uv.y * uv.y), 0.5);
    // vec3 rd = vec3(uv, -z);
    // rd = (vec4(rd, 0) * u_camMatrix).xyz;

    // Ray march to find closest SDF
    float object = rayMarch(ro, rd);

    // If ray marched distance is bigger than depth from zbuffer, set alpha to 1
    float minDepth = object;

    float z_e = object;
    float z_n = (FAR + NEAR - (2.0 * NEAR * FAR) / z_e) / (FAR - NEAR);
    float depth = (z_n + 1.0) * 0.5;

    
    gl_FragDepth = depth;


//    if(handleTransparency)
//        return vec4(1,0,0,1);

    // Output color
    vec3 col;

    // Scene alpha over background
    float alpha = 0.0;
    if (minDepth < FAR)
    {

        vec3 p = ro + object * rd;
        vec3 material = getColor(p);
        col = getDirectionalLight(p, rd, material);



        // fog

        // minDepth -= 0.85;
        // alpha = exp(-0.0004 * minDepth * minDepth);

        //col = mix(col, background, alpha);

        //float a = minDepth - (FAR - 980);
        //alpha = max(0.0, 0.001 * a * a *a );

        // alpha = 1.0f;

        alpha = 1.0f - smoothstep(FAR * 0.5, FAR, minDepth);

    }

    return vec4(col, alpha);

    // col = mix(col, background, alpha);

    // return col;
}

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}


void main()
{

    float aspectRatio = u_resolution.x / u_resolution.y;
    vec2 pixelScale = vec2(aspectRatio * 0.2, 0.2);

    vec2 screenUV = v_texCoord;

    vec2 uv = (2.0f * v_texCoord) - 1.0f;

    // Calculate true z value from the depth buffer: https://stackoverflow.com/questions/6652253/getting-the-true-z-value-from-the-depth-buffer
    float depth = texture(t_depthTexture, screenUV).r;
    float z_n = 2.0 * depth - 1.0;
    float z_e = 2.0 * NEAR * FAR / (FAR + NEAR - z_n * (FAR - NEAR));

    vec4 originalColor = texture(t_colorTexture, screenUV);

    vec4 col = render(uv, originalColor);

    col = vec4(pow(col.xyz, vec3(0.4545)), col.w);

#ifdef DITHERING
    int x = int(gl_FragCoord.x);
    int y = int(gl_FragCoord.y);
    col  = col + u_spread * GetBayer4(x, y);

    col.r = floor((u_colorCount - 1.0f) * col.r + 0.5) / (u_colorCount - 1.0f);
    col.g = floor((u_colorCount - 1.0f) * col.g + 0.5) / (u_colorCount - 1.0f);
    col.b = floor((u_colorCount - 1.0f) * col.b + 0.5) / (u_colorCount - 1.0f);
#endif 



//    FragColor = vec4(0.5f * originalColor.xyz, 1.0);
//
//    FragColor = max( originalColor, col);
//    FragColor = vec4(vec3(1.0f * depth), 1.0);

    FragColor = vec4(col.xyz, col.w);
    //FragColor = vec4(originalColor.www, 1);
}
