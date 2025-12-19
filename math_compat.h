#ifndef MATH_COMPAT_H
#define MATH_COMPAT_H

#ifndef __linux__

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Provide fmin and fmax if not available */
#ifndef fmin
static inline double fmin(double x, double y) {
    return (x < y) ? x : y;
}
#endif

#ifndef fmax
static inline double fmax(double x, double y) {
    return (x > y) ? x : y;
}
#endif

/* Single-precision versions */
#ifndef fminf
static inline float fminf(float x, float y) {
    return (x < y) ? x : y;
}
#endif

#ifndef fmaxf
static inline float fmaxf(float x, float y) {
    return (x > y) ? x : y;
}
#endif

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif

#endif /* MATH_COMPAT_H */
