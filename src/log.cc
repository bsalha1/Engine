#include "log.h"

#ifdef __WINDOWS__
#include <Windows.h>
#else
#include <ctime>
#include <sys/time.h>
#endif

#ifdef __WINDOWS__

/**
 * Windows implementation of printing time prefix.
 */
void print_time_prefix()
{
    SYSTEMTIME system_time;
    GetSystemTime(&system_time);
    printf("[%04d-%02d-%02d %02d:%02d:%02d.%03lu] ",
           system_time.wYear,
           system_time.wMonth,
           system_time.wDay,
           system_time.wHour,
           system_time.wMinute,
           system_time.wSecond,
           system_time.wMilliseconds);
}

#else

/**
 * Linux implementation of printing time prefix.
 */
void print_time_prefix()
{
    struct timeval current_ts;
    if (gettimeofday(&current_ts, nullptr))
    {
        printf("[2000 Jan 01 00:00:00.000]");
        return;
    }

    struct tm *current_tm = localtime(&current_ts.tv_sec);
    if (current_tm == nullptr)
    {
        printf("[2000 Jan 01 00:00:00.000]");
        return;
    }
    else
    {
        char time_string[sizeof("2000 Jan 01 00:00:00")];
        strftime(time_string, sizeof(time_string), "%b %d %H:%M:%S", current_tm);
        printf("[%s.%03lu] ", time_string, current_ts.tv_usec / 1000);
    }
}

#endif

void log(const char *format, ...)
{
    print_time_prefix();

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}