#include "assert_util.h"

#ifdef NDEBUG

__cold void assert_print(const char *msg, const char *file, const int line)
{
    LOG_ERROR("ASSERT(%s) %s:%d\n", msg, file, line);
}

#endif