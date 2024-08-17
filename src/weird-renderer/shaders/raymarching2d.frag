#version 330 core

#define DITHERING 0



#if (DITHERING == 1)

uniform float _Spread = 0.15f;
uniform int _ColorCount = 5;

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

float GetBayer2(int x, int y) {
    return float(bayer2[(x % 2) + (y % 2) * 2]) * (1.0f / 4.0f) - 0.5f;
}

float GetBayer4(int x, int y) {
    return float(bayer4[(x % 4) + (y % 4) * 4]) * (1.0f / 16.0f) - 0.5f;
}

float GetBayer8(int x, int y) {
    return float(bayer8[(x % 8) + (y % 8) * 8]) * (1.0f / 64.0f) - 0.5f;
}

#endif



struct Shape
{
    vec3 position;
    float size;
    //int materialId;
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


const int MAX_STEPS = 256;
const float EPSILON = 0.01;
const float NEAR = 0.1f;
const float FAR = 100.0f;

const vec3 background = vec3(0.2);

uniform sampler2D u_colorTexture;
uniform sampler2D u_depthTexture;

uniform vec3 directionalLightDirection;

// Operations
float fOpUnionSoft(float a, float b, float r)
{
    float e = max(r - abs(a - b), 0);
    return min(a, b) - e * e * 0.25 / r;
}

float fOpUnionSoft(float a, float b, float r,  float invR)
{
    float e = max(r - abs(a - b), 0);
    return min(a, b) - e * e * 0.25 * invR;
}

float smin(float a, float b, float k) {
  float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
  return mix(b, a, h) - k * h * (1.0 - h);
}

vec2 squareFrame(vec2 screenSize, vec2 coord) {
  vec2 position = 2.0 * (coord.xy / screenSize.xy) - 1.0;
  position.x *= screenSize.x / screenSize.y;
  return position;
}

const float PI = 3.14159265359;

vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
  return a + b * cos(6.28318 * (c * t + d));
}

// r^2 = x^2 + y^2
// r = sqrt(x^2 + y^2)
// r = length([x y])
// 0 = length([x y]) - r
float shape_circle(vec2 p) {
  return length(p) - 0.5;
}

// y = sin(5x + t) / 5
// 0 = sin(5x + t) / 5 - y
float shape_sine(vec2 p) {
  return p.y - sin(p.x * 5.0 + u_time) * 0.2;
}

float shape_box2d(vec2 p, vec2 b) {
  vec2 d = abs(p) - b;
  return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float shape_line(vec2 p, vec2 a, vec2 b) {
  vec2 dir = b - a;
  return abs(dot(normalize(vec2(dir.y, -dir.x)), a - p));
}

float shape_segment(vec2 p, vec2 a, vec2 b) {
  float d = shape_line(p, a, b);
  float d0 = dot(p - b, b - a);
  float d1 = dot(p - a, b - a);
  return d1 < 0.0 ? length(a - p) : d0 > 0.0 ? length(b - p) : d;
}

float shape_circles_smin(vec2 p, float t) {
  return smin(shape_circle(p - vec2(cos(t))), shape_circle(p + vec2(sin(t), 0)), 0.8);
}

vec3 draw_line(float d, float thickness) {
  const float aa = 3.0;
  return vec3(smoothstep(0.0, aa / u_resolution.y, max(0.0, abs(d) - thickness)));
}

vec3 draw_line(float d) {
  return draw_line(d, 0.0025);
}






vec3 getMaterial(vec2 p, int id)
{
    vec3 colors[3];
    colors[0] = vec3(0.2 + 0.4 * mod(floor(p.x) + floor(p.y), 2.0));
    colors[1] = vec3(1.0,0.05,0.01);
    colors[2] = vec3(0.1, 0.05, 0.80);

    return colors[id];
}

uniform float k = 3.5;

vec4 getColor(vec2 p)
{
    float d = FAR;
    vec3 col = vec3(0.0);


    for (int i = 0; i < u_loadedObjects; i++)
    {
      int id = i % 2 == 0 ? 1 : 2;

      float objectDist = shape_circle(p - data[i].position.xy);
        
      d = fOpUnionSoft(objectDist, d, k);
      float delta = 1 - (max( k - abs(objectDist - d), 0.0 ) / k); // After new d is calculated
      col = mix(getMaterial(p, id), col, delta);   

      //d = min(d, objectDist);
      //col = d == objectDist ? vec3(0.1 * data[i].position.z, 0.0, 0.0) : col;
    }

    return vec4(col, d);
}

float map(vec2 p)
{
    float d = FAR;


    for (int i = 0; i < u_loadedObjects; i++)
    {
        int id = i % 2 == 0 ? 1 : 2;

        float objectDist = shape_circle(p - data[i].position.xy);
        

        //d = min(d, objectDist);
        d = fOpUnionSoft(objectDist, d, k);
        float delta = 1 - (max( k - abs(objectDist - d), 0.0 ) / k); // After new d is calculated
    }

    return d;
}

float rayMarch(vec2 ro, vec2 rd)
{
    float d;

    float traveled = NEAR;

    for (int i = 0; i < MAX_STEPS; i++)
    {
        vec2 p = ro + (traveled * rd);

        d = map(p);


        if (abs(d) < EPSILON || traveled > FAR)
            break;

        traveled += d;
    }

    return traveled;
}

vec3 render(vec2 uv){
    /*float d = map(uv);
    d = uv.y < 0.0 ? -1.0 : d;
    vec3 col = vec3(d < 0.0 ? 1.0 : 0.0);*/

    vec4 col = getColor(uv);
    // Paint walls
    col = uv.y < 0.0 || uv.x < 0.0  || uv.x > 30.0 ? vec4(1.0, 1.0, 1.0, -1.0) : col;

    if(col.w > 0){

      float d = rayMarch(uv, directionalLightDirection.xy);
      return (d < FAR ? 0.3 : 1.0) * background;

      //float d = rayMarch(uv, normalize(vec2(15) - uv));
      //return (d < length(vec2(15) - uv) ? 0.3 : 1.0) * background;


      //return vec3(0.001 * d);
    }


    return col.xyz;
}

void main()
{

    float aspectRatio = u_resolution.x / u_resolution.y;
    vec2 pixelScale = vec2(aspectRatio * 0.2, 0.2);

    vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);

    vec2 uv = (2.0 * gl_FragCoord.xy - u_resolution.xy) / u_resolution.y;

    // Calculate true z value from the depth buffer: https://stackoverflow.com/questions/6652253/getting-the-true-z-value-from-the-depth-buffer

    
    vec3 col = render((-u_cameraMatrix[3].z * uv)-u_cameraMatrix[3].xy);

    col = pow(col, vec3(0.4545));

#if (DITHERING == 1)
    int x = int(gl_FragCoord.x);
    int y = int(gl_FragCoord.y);
    col  = col + _Spread * GetBayer4(x, y);

    col.r = floor((_ColorCount - 1.0f) * col.r + 0.5) / (_ColorCount - 1.0f);
    col.g = floor((_ColorCount - 1.0f) * col.g + 0.5) / (_ColorCount - 1.0f);
    col.b = floor((_ColorCount - 1.0f) * col.b + 0.5) / (_ColorCount - 1.0f);
#endif 

    //FragColor = vec4(vec3(uv, 0.0), 1.0);
    FragColor = vec4(col.xyz, 1.0);
}
