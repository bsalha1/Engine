#pragma once

#include <Windows.h>
#include <cstdio>
#include <cstdarg>

/**
 * __FILE__ is the full path to the file - we really only care about the file's name,
 * and this macro does that at compile time.
 */
#define __BASE_FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

void log(const char* format, ...);

#define LOG(msg, ...) \
    log("<info> %s: " msg, __BASE_FILENAME__, ##__VA_ARGS__)

#define LOG_ERROR(msg, ...) \
    log("<error> %s: " msg, __BASE_FILENAME__, ##__VA_ARGS__)
