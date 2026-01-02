#pragma once

#include <cstdarg>
#include <cstdio>

/**
 * __FILE__ is the full path to the file - we really only care about the file's name,
 * and this macro does that at compile time.
 */
#ifdef __WINDOWS__
#define __BASE_FILENAME__ \
    (std::strrchr(__FILE__, '\\') ? std::strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __BASE_FILENAME__ \
    (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

void log(const char *format, ...);

#ifdef NDEBUG
#define LOG_DEBUG(msg, ...) \
    do                      \
    {                       \
    } while (0)
#else
#define LOG_DEBUG(msg, ...) log("<debug> %s:%d: " msg, __BASE_FILENAME__, __LINE__, ##__VA_ARGS__)
#endif

#define LOG(msg, ...) log("<info> %s:%d: " msg, __BASE_FILENAME__, __LINE__, ##__VA_ARGS__)

#define LOG_ERROR(msg, ...) log("<error> %s:%d: " msg, __BASE_FILENAME__, __LINE__, ##__VA_ARGS__)

#define LOG_WARN(msg, ...) log("<warn> %s:%d: " msg, __BASE_FILENAME__, __LINE__, ##__VA_ARGS__)
