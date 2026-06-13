/* math3d.h — minimal column-major mat4 / vec3 math for OpenGL */
#pragma once
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Column-major flat array: m[col*4 + row] */
typedef float Mat4[16];

typedef struct { float x, y, z; }    Vec3;

static inline void mat4_identity(Mat4 out)
{
    memset(out, 0, 16 * sizeof(float));
    out[0] = out[5] = out[10] = out[15] = 1.0f;
}

/* out = a * b  (safe for in-place: out may alias a or b) */
static inline void mat4_mul(Mat4 out, const Mat4 a, const Mat4 b)
{
    Mat4 tmp;
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) {
            tmp[c*4+r] = 0.0f;
            for (int k = 0; k < 4; k++)
                tmp[c*4+r] += a[k*4+r] * b[c*4+k];
        }
    memcpy(out, tmp, sizeof(Mat4));
}

static inline void mat4_perspective(Mat4 out,
    float fovy_deg, float aspect, float near_z, float far_z)
{
    memset(out, 0, sizeof(Mat4));
    float f = 1.0f / tanf(fovy_deg * (float)M_PI / 360.0f);
    out[0]  = f / aspect;
    out[5]  = f;
    out[10] = (far_z + near_z) / (near_z - far_z);
    out[11] = -1.0f;
    out[14] = (2.0f * far_z * near_z) / (near_z - far_z);
}

static inline void mat4_translate(Mat4 out, float x, float y, float z)
{
    mat4_identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

static inline void mat4_scale_uniform(Mat4 out, float s)
{
    mat4_identity(out);
    out[0] = out[5] = out[10] = s;
}

/* Rotation around X axis, angle in degrees */
static inline void mat4_rotate_x(Mat4 out, float deg)
{
    float a = deg * (float)M_PI / 180.0f;
    float c = cosf(a), s = sinf(a);
    mat4_identity(out);
    out[5]  =  c;  out[6]  =  s;
    out[9]  = -s;  out[10] =  c;
}

/* Rotation around Y axis, angle in degrees */
static inline void mat4_rotate_y(Mat4 out, float deg)
{
    float a = deg * (float)M_PI / 180.0f;
    float c = cosf(a), s = sinf(a);
    mat4_identity(out);
    out[0]  =  c;  out[2]  = -s;
    out[8]  =  s;  out[10] =  c;
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    return (Vec3){ a.x-b.x, a.y-b.y, a.z-b.z };
}

static inline Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    return (Vec3){
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}

static inline float vec3_len(Vec3 v)
{
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

static inline Vec3 vec3_normalize(Vec3 v)
{
    float l = vec3_len(v);
    if (l < 1e-9f) return (Vec3){ 0.0f, 0.0f, 1.0f };
    return (Vec3){ v.x/l, v.y/l, v.z/l };
}
