#pragma once

#include "log.h"
#include "perf.h"

#include <cstdio>
#include <cstring>

#ifdef NDEBUG

__cold void assert_print(const char *msg, const char *file, const int line);

#define ASSERT_RET_IF_NOT(x, ret)             \
    if (unlikely(!(x)))                       \
    {                                         \
        assert_print(#x, __FILE__, __LINE__); \
        return ret;                           \
    }

#define ASSERT_RET_IF(x, ret)                 \
    if (unlikely(!!(x)))                      \
    {                                         \
        assert_print(#x, __FILE__, __LINE__); \
        return ret;                           \
    }

#else
#define ASSERT_RET_IF_NOT(x, ret) (x)
#define ASSERT_RET_IF(x, ret) (x)
#endif

#define ASSERT_RET_IF_GLEW_NOT_OK(x, ret)                                                    \
    {                                                                                        \
        const GLenum __err = (x);                                                            \
        if (__err != GLEW_OK)                                                                \
        {                                                                                    \
            LOG_ERROR("ASSERT_RET_IF_GLEW_NOT_OK(%s): %s\n", #x, glewGetErrorString(__err)); \
            return ret;                                                                      \
        }                                                                                    \
    }

#define ASSERT_PRINT(msg) LOG_ERROR("ASSERT_PRINT(%s)\n", msg)
