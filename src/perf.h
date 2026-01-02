#pragma once

#ifdef NPERF

#define __cold

#define likely(x) (x)

#define unlikely(x) (x)

#else

/**
 * This tells the compiler to not inline the given function such that we do not hog up
 * the cache with its instructions.
 */
#define __cold __attribute__((cold))

/**
 * Give compiler a hint about whether or not the given branch is taken often or not.
 * @{
 */
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
/**
 * @}
 */

#endif