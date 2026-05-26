#ifndef VKERNEL_USERSPACE_FLOAT_H
#define VKERNEL_USERSPACE_FLOAT_H

#if defined(_MSC_VER)

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FLT_RADIX
#if defined(__FLT_RADIX__)
#define FLT_RADIX __FLT_RADIX__
#else
#define FLT_RADIX 2
#endif
#endif

#ifndef FLT_ROUNDS
#if defined(__FLT_ROUNDS__)
#define FLT_ROUNDS __FLT_ROUNDS__
#else
#define FLT_ROUNDS 1
#endif
#endif

#ifndef FLT_EVAL_METHOD
#if defined(__FLT_EVAL_METHOD__)
#define FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#else
#define FLT_EVAL_METHOD 0
#endif
#endif

#ifndef DECIMAL_DIG
#if defined(__DECIMAL_DIG__)
#define DECIMAL_DIG __DECIMAL_DIG__
#endif
#endif

#ifndef FLT_DECIMAL_DIG
#if defined(__FLT_DECIMAL_DIG__)
#define FLT_DECIMAL_DIG __FLT_DECIMAL_DIG__
#endif
#endif

#ifndef DBL_DECIMAL_DIG
#if defined(__DBL_DECIMAL_DIG__)
#define DBL_DECIMAL_DIG __DBL_DECIMAL_DIG__
#endif
#endif

#ifndef LDBL_DECIMAL_DIG
#if defined(__LDBL_DECIMAL_DIG__)
#define LDBL_DECIMAL_DIG __LDBL_DECIMAL_DIG__
#endif
#endif

#ifndef FLT_MANT_DIG
#define FLT_MANT_DIG __FLT_MANT_DIG__
#endif

#ifndef DBL_MANT_DIG
#define DBL_MANT_DIG __DBL_MANT_DIG__
#endif

#ifndef LDBL_MANT_DIG
#define LDBL_MANT_DIG __LDBL_MANT_DIG__
#endif

#ifndef FLT_DIG
#define FLT_DIG __FLT_DIG__
#endif

#ifndef DBL_DIG
#define DBL_DIG __DBL_DIG__
#endif

#ifndef LDBL_DIG
#define LDBL_DIG __LDBL_DIG__
#endif

#ifndef FLT_MIN_EXP
#define FLT_MIN_EXP __FLT_MIN_EXP__
#endif

#ifndef DBL_MIN_EXP
#define DBL_MIN_EXP __DBL_MIN_EXP__
#endif

#ifndef LDBL_MIN_EXP
#define LDBL_MIN_EXP __LDBL_MIN_EXP__
#endif

#ifndef FLT_MAX_EXP
#define FLT_MAX_EXP __FLT_MAX_EXP__
#endif

#ifndef DBL_MAX_EXP
#define DBL_MAX_EXP __DBL_MAX_EXP__
#endif

#ifndef LDBL_MAX_EXP
#define LDBL_MAX_EXP __LDBL_MAX_EXP__
#endif

#ifndef FLT_MIN_10_EXP
#define FLT_MIN_10_EXP __FLT_MIN_10_EXP__
#endif

#ifndef DBL_MIN_10_EXP
#define DBL_MIN_10_EXP __DBL_MIN_10_EXP__
#endif

#ifndef LDBL_MIN_10_EXP
#define LDBL_MIN_10_EXP __LDBL_MIN_10_EXP__
#endif

#ifndef FLT_MAX_10_EXP
#define FLT_MAX_10_EXP __FLT_MAX_10_EXP__
#endif

#ifndef DBL_MAX_10_EXP
#define DBL_MAX_10_EXP __DBL_MAX_10_EXP__
#endif

#ifndef LDBL_MAX_10_EXP
#define LDBL_MAX_10_EXP __LDBL_MAX_10_EXP__
#endif

#ifndef FLT_TRUE_MIN
#define FLT_TRUE_MIN __FLT_DENORM_MIN__
#endif

#ifndef DBL_TRUE_MIN
#define DBL_TRUE_MIN __DBL_DENORM_MIN__
#endif

#ifndef LDBL_TRUE_MIN
#define LDBL_TRUE_MIN __LDBL_DENORM_MIN__
#endif

#ifndef FLT_EPSILON
#define FLT_EPSILON __FLT_EPSILON__
#endif

#ifndef DBL_EPSILON
#define DBL_EPSILON __DBL_EPSILON__
#endif

#ifndef LDBL_EPSILON
#define LDBL_EPSILON __LDBL_EPSILON__
#endif

#ifndef FLT_MIN
#define FLT_MIN __FLT_MIN__
#endif

#ifndef DBL_MIN
#define DBL_MIN __DBL_MIN__
#endif

#ifndef LDBL_MIN
#define LDBL_MIN __LDBL_MIN__
#endif

#ifndef FLT_MAX
#define FLT_MAX __FLT_MAX__
#endif

#ifndef DBL_MAX
#define DBL_MAX __DBL_MAX__
#endif

#ifndef LDBL_MAX
#define LDBL_MAX __LDBL_MAX__
#endif

#ifndef HUGE_VALF
#define HUGE_VALF __builtin_huge_valf()
#endif

#ifndef HUGE_VAL
#define HUGE_VAL __builtin_huge_val()
#endif

#ifndef HUGE_VALL
#define HUGE_VALL __builtin_huge_vall()
#endif

#ifndef INFINITY
#define INFINITY __builtin_inff()
#endif

#ifndef NAN
#define NAN __builtin_nanf("")
#endif

#if !defined(_LDBL_EQ_DBL) && !defined(LDBL_EQ_DBL)
#if defined(__SIZEOF_LONG_DOUBLE__) && defined(__SIZEOF_DOUBLE__) && (__SIZEOF_LONG_DOUBLE__ == __SIZEOF_DOUBLE__)
#define _LDBL_EQ_DBL 1
#define LDBL_EQ_DBL 1
#elif defined(__LDBL_MANT_DIG__) && defined(__DBL_MANT_DIG__) && (__LDBL_MANT_DIG__ == __DBL_MANT_DIG__)
#define _LDBL_EQ_DBL 1
#define LDBL_EQ_DBL 1
#endif
#endif

#ifdef __cplusplus
}
#endif

#else
#include_next <float.h>
#endif

#endif /* VKERNEL_USERSPACE_FLOAT_H */
