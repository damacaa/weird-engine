// This is a helper file
// Operations
float fOpUnionSoft(float a, float b, float r)
{
    r *= 1.0;
    float h = max(r - abs(a - b), 0.0f);
    return min(a, b) - h * h * 0.25 / r;
}

float fOpUnionSoft(float a, float b, float r, float invR)
{
    r *= 1.0;
    float h = max(r - abs(a - b), 0.0f);
    return min(a, b) - h * h * 0.25 * invR;
}

// Smooth subtraction: a - b
float fOpSubSoft(float a, float b, float r)
{
    return -fOpUnionSoft(b, -a, r);
}

float shape_circle(vec2 p, float r = 0.5)
{
    return length(p) - r;
}

float sdSegment( in vec2 p, in vec2 a, in vec2 b )
{
    vec2 pa = p-a, ba = b-a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    return length( pa - ba*h );
}

float sdBox( in vec2 p, in vec2 b )
{
    vec2 d = abs(p)-b;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

const int N = 4;
float sdPolygon( in vec2[N] v, in vec2 p )
{
    float d = dot(p-v[0],p-v[0]);
    float s = 1.0;
    for( int i=0, j=N-1; i<N; j=i, i++ )
    {
        vec2 e = v[j] - v[i];
        vec2 w =    p - v[i];
        vec2 b = w - e*clamp( dot(w,e)/dot(e,e), 0.0, 1.0 );
        d = min( d, dot(b,b) );
        bvec3 c = bvec3(p.y>=v[i].y,p.y<v[j].y,e.x*w.y>e.y*w.x);
        if( all(c) || all(not(c)) ) s*=-1.0;
    }
    return s*sqrt(d);
}

// y = sin(5x + t) / 5
// 0 = sin(5x + t) / 5 - y
float shape_sine(vec2 p, float time)
{
    return p.y - sin(p.x * 5.0 + time) * 0.2;
}

float shape_box2d(vec2 p, vec2 b)
{
    vec2 d = abs(p) - b;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float shape_line(vec2 p, vec2 a, vec2 b)
{
    vec2 dir = b - a;
    return abs(dot(normalize(vec2(dir.y, -dir.x)), a - p));
}

float shape_segment(vec2 p, vec2 a, vec2 b)
{
    float d = shape_line(p, a, b);
    float d0 = dot(p - b, b - a);
    float d1 = dot(p - a, b - a);
    return d1 < 0.0 ? length(a - p) : d0 > 0.0 ? length(b - p)
    : d;
}

vec3 draw_line(float d, float thicu_kness, vec2 resolution)
{
    const float aa = 3.0;
    return vec3(smoothstep(0.0, aa / resolution.y, max(0.0, abs(d) - thicu_kness)));
}

vec3 draw_line(float d, vec2 resolution)
{
    return draw_line(d, 0.0025, resolution);
}

float sdParallelogramHorizontal( in vec2 p, float wi, float he, float sk )
{
    vec2 e = vec2(sk,he);
    p = (p.y<0.0)?-p:p;
    vec2  w = p - e; w.x -= clamp(w.x,-wi,wi);
    vec2  d = vec2(dot(w,w), -w.y);
    float s = p.x*e.y - p.y*e.x;
    p = (s<0.0)?-p:p;
    vec2  v = p - vec2(wi,0); v -= e*clamp(dot(v,e)/dot(e,e),-1.0,1.0);
    d = min( d, vec2(dot(v,v), wi*he-abs(s)));
    return sqrt(d.x)*sign(-d.y);
}

float sdParallelogramVertical(in vec2 p, float wi, float he, float sk)
{
    vec2 e = vec2(wi, sk);
    p = (p.x < 0.0) ? -p : p;
    vec2 w = p - e; w.y -= clamp(w.y, -he, he);
    vec2 d = vec2(dot(w, w), -w.x);
    float s = p.y * e.x - p.x * e.y;
    p = (s < 0.0) ? -p : p;
    vec2 v = p - vec2(0, he);
    v -= e * clamp(dot(v, e) / dot(e, e), -1.0, 1.0);
    d = min(d, vec2(dot(v, v), wi * he - abs(s)));
    return sqrt(d.x) * sign(-d.y);
}