/**
 * @file mvec_impl.c
 * @brief Implementation of libmvec vector math functions for cross-compilation
 * 
 * Provides implementations for ARM NEON vector math functions declared in
 * bits/math-vector.h but not available in the cross-compilation toolchain.
 * 
 * These are scalar fallback implementations that work correctly but without
 * SIMD optimization. For production use, link with actual libmvec.
 */

#include <math.h>

#if defined(__aarch64__) || defined(__ARM_ARCH_ISA_A64)

/* GCC/glibc has no __f32x4_t/__f64x2_t (ARMCLANG MVE types); define
 * equivalents via vector extensions (DP-X5 fix). */
typedef float __f32x4_t __attribute__((vector_size(16)));
typedef double __f64x2_t __attribute__((vector_size(16)));

// Types and declarations are already in bits/math-vector.h (included via math.h)
// We only provide the implementation here

// Single precision (float) vector functions - 4 elements

__attribute__((__aarch64_vector_pcs__)) 
__f32x4_t _ZGVnN4v_erfcf(__f32x4_t x) {
    float *px = (float*)&x;
    __f32x4_t result;
    float *pr = (float*)&result;
    pr[0] = erfcf(px[0]);
    pr[1] = erfcf(px[1]);
    pr[2] = erfcf(px[2]);
    pr[3] = erfcf(px[3]);
    return result;
}

__attribute__((__aarch64_vector_pcs__)) 
__f32x4_t _ZGVnN4v_erff(__f32x4_t x) {
    float *px = (float*)&x;
    __f32x4_t result;
    float *pr = (float*)&result;
    pr[0] = erff(px[0]);
    pr[1] = erff(px[1]);
    pr[2] = erff(px[2]);
    pr[3] = erff(px[3]);
    return result;
}

__attribute__((__aarch64_vector_pcs__)) 
__f32x4_t _ZGVnN4v_coshf(__f32x4_t x) {
    float *px = (float*)&x;
    __f32x4_t result;
    float *pr = (float*)&result;
    pr[0] = coshf(px[0]);
    pr[1] = coshf(px[1]);
    pr[2] = coshf(px[2]);
    pr[3] = coshf(px[3]);
    return result;
}

__attribute__((__aarch64_vector_pcs__)) 
__f32x4_t _ZGVnN4v_atanhf(__f32x4_t x) {
    float *px = (float*)&x;
    __f32x4_t result;
    float *pr = (float*)&result;
    pr[0] = atanhf(px[0]);
    pr[1] = atanhf(px[1]);
    pr[2] = atanhf(px[2]);
    pr[3] = atanhf(px[3]);
    return result;
}

__attribute__((__aarch64_vector_pcs__)) 
__f32x4_t _ZGVnN4v_asinhf(__f32x4_t x) {
    float *px = (float*)&x;
    __f32x4_t result;
    float *pr = (float*)&result;
    pr[0] = asinhf(px[0]);
    pr[1] = asinhf(px[1]);
    pr[2] = asinhf(px[2]);
    pr[3] = asinhf(px[3]);
    return result;
}

__attribute__((__aarch64_vector_pcs__)) 
__f32x4_t _ZGVnN4v_sinhf(__f32x4_t x) {
    float *px = (float*)&x;
    __f32x4_t result;
    float *pr = (float*)&result;
    pr[0] = sinhf(px[0]);
    pr[1] = sinhf(px[1]);
    pr[2] = sinhf(px[2]);
    pr[3] = sinhf(px[3]);
    return result;
}

__attribute__((__aarch64_vector_pcs__)) 
__f32x4_t _ZGVnN4v_acoshf(__f32x4_t x) {
    float *px = (float*)&x;
    __f32x4_t result;
    float *pr = (float*)&result;
    pr[0] = acoshf(px[0]);
    pr[1] = acoshf(px[1]);
    pr[2] = acoshf(px[2]);
    pr[3] = acoshf(px[3]);
    return result;
}

__attribute__((__aarch64_vector_pcs__)) 
__f32x4_t _ZGVnN4vv_powf(__f32x4_t x, __f32x4_t y) {
    float *px = (float*)&x;
    float *py = (float*)&y;
    __f32x4_t result;
    float *pr = (float*)&result;
    pr[0] = powf(px[0], py[0]);
    pr[1] = powf(px[1], py[1]);
    pr[2] = powf(px[2], py[2]);
    pr[3] = powf(px[3], py[3]);
    return result;
}

// Double precision (double) vector functions - 2 elements

__attribute__((__aarch64_vector_pcs__)) 
__f64x2_t _ZGVnN2v_erf(__f64x2_t x) {
    double *px = (double*)&x;
    __f64x2_t result;
    double *pr = (double*)&result;
    pr[0] = erf(px[0]);
    pr[1] = erf(px[1]);
    return result;
}

__attribute__((__aarch64_vector_pcs__)) 
__f64x2_t _ZGVnN2vv_pow(__f64x2_t x, __f64x2_t y) {
    double *px = (double*)&x;
    double *py = (double*)&y;
    __f64x2_t result;
    double *pr = (double*)&result;
    pr[0] = pow(px[0], py[0]);
    pr[1] = pow(px[1], py[1]);
    return result;
}

#endif // __aarch64__ || __ARM_ARCH_ISA_A64
