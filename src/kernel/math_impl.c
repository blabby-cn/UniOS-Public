#include <stdint.h>
#include "math_impl.h"

float sqrtf(float x) { return __builtin_sqrtf(x); }
double sqrt(double x) { return __builtin_sqrt(x); }
float fabsf(float x) { return __builtin_fabsf(x); }
double fabs(double x) { return __builtin_fabs(x); }
float sinf(float x) { return __builtin_sinf(x); }
float cosf(float x) { return __builtin_cosf(x); }
float ceilf(float x) { return __builtin_ceilf(x); }
float floorf(float x) { return __builtin_floorf(x); }
float roundf(float x) { return __builtin_roundf(x); }
float fmodf(float x, float y) { return __builtin_fmodf(x, y); }
double pow(double x, double y) { return __builtin_pow(x, y); }

float tanf(float x) { return sinf(x) / cosf(x); }

float atan2f(float y, float x)
{
    if (x == 0.0f && y == 0.0f) return 0.0f;
    float abs_x = fabsf(x);
    float abs_y = fabsf(y);
    float t = (abs_y < abs_x) ? (abs_y / abs_x) : (abs_x / abs_y);
    float t3 = t * t * t;
    float t5 = t3 * t * t;
    float t7 = t5 * t * t;
    float a = t - t3/3.0f + t5/5.0f - t7/7.0f;
    if (abs_x < abs_y) a = 1.57079632679f - a;
    if (x < 0.0f) a = 3.1415926535f - a;
    if (y < 0.0f) a = -a;
    return a;
}

float acosf(float x)
{
    return 1.57079632679f - atan2f(x, sqrtf(1.0f - x*x));
}

int isnan(float x)
{
    union { float f; uint32_t u; } u;
    u.f = x;
    return (u.u & 0x7FFFFFFFU) > 0x7F800000U ? 1 : 0;
}
