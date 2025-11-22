#pragma once

#include "log.h"

#include <Windows.h>

void log(const char* format, ...)
{
    SYSTEMTIME  system_time;
    GetSystemTime(&system_time);
    printf("[%04d-%02d-%02d %02d:%02d:%02d.%03lu] ", system_time.wYear, system_time.wMonth, system_time.wDay, system_time.wHour, system_time.wMinute, system_time.wSecond, system_time.wMilliseconds);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}