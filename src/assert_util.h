#pragma once

#include <cstdio>
#include <Windows.h>

#include "log.h"

/**
 * __FILE__ is the full path to the file - we really only care about the file's name.
 */
#define __BASE_FILENAME__ \
    (std::strrchr(__FILE__, '\\') ? std::strrchr(__FILE__, '\\') + 1 : __FILE__)

#define ASSERT_RET_IF_NOT_WINDOWS_API(x, ret)                                                                                       \
    if (!(x))                                                                                                                       \
    {                                                                                                                               \
        const DWORD last_error = GetLastError();                                                                                    \
        LPWSTR last_error_msg = nullptr;                                                                                            \
        FormatMessageW(                                                                                                             \
            FORMAT_MESSAGE_ALLOCATE_BUFFER |                                                                                        \
            FORMAT_MESSAGE_FROM_SYSTEM |                                                                                            \
            FORMAT_MESSAGE_IGNORE_INSERTS,                                                                                          \
            nullptr,                                                                                                                \
            last_error,                                                                                                             \
            0,                                                                                                                      \
            last_error_msg,                                                                                                         \
            0,                                                                                                                      \
            nullptr);                                                                                                               \
        LOG_ERROR("ASSERT_RET_IF_NOT_WINDOWS_API(%s) %s:%d, LastError: %lu - %ws\n", #x, __BASE_FILENAME__, __LINE__, last_error, last_error_msg); \
        LocalFree(last_error_msg);                                                                                                  \
        return ret;                                                                                                                 \
    }

#define ASSERT_RET_IF_NOT(x, ret)                                     \
    if (!(x))                                                         \
    {                                                                 \
        LOG_ERROR("ASSERT_RET_IF_NOT(%s) %s:%d\n", #x, __BASE_FILENAME__, __LINE__); \
        return ret;                                                   \
    }

#define ASSERT_RET_IF(x, ret)                                     \
    if (!!(x))                                                         \
    {                                                                 \
        LOG_ERROR("ASSERT_RET_IF(%s) %s:%d\n", #x, __BASE_FILENAME__, __LINE__); \
        return ret;                                                   \
    }

#define ASSERT_RET_IF_GLEW_NOT_OK(x, ret)                             \
    { \
        const GLenum __err = (x); \
        if (__err != GLEW_OK)                                                         \
        {                                                                 \
            LOG_ERROR("ASSERT_RET_IF_GLEW_NOT_OK(%s): %s:%d error: %s\n", #x, __BASE_FILENAME__, __LINE__, glewGetErrorString(__err)); \
            return ret;                                                   \
        } \
    }

#define ASSERT_PRINT(msg) LOG_ERROR("ASSERT_PRINT(%s) %s:%d\n", msg, __BASE_FILENAME__, __LINE__)
