#pragma once

#include "log.h"

#include <cstdio>
#include <cstring>

#ifdef NDEBUG
#define ASSERT_RET_IF_NOT(x, ret)                 \
    if (!(x))                                     \
    {                                             \
        LOG_ERROR("ASSERT_RET_IF_NOT(%s)\n", #x); \
        return ret;                               \
    }

#define ASSERT_RET_IF(x, ret)                 \
    if (!!(x))                                \
    {                                         \
        LOG_ERROR("ASSERT_RET_IF(%s)\n", #x); \
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
