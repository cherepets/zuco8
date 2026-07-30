#include <stdio.h>
#include <windows.h>

#include "C99/z8lua_ce_compat.h"

static char s_decimal_point[] = ".";
static ZuneLocaleInfo s_locale = { s_decimal_point };

extern "C" int zune_errno = 0;

extern "C" const char* zune_strerror(int error_number)
{
    (void)error_number;
    return "FILE ERROR";
}

extern "C" void* zune_freopen(const char* file_name, const char* mode,
    void* stream)
{
    if (stream)
    {
        fclose((FILE*)stream);
    }
    return fopen(file_name, mode);
}

extern "C" ZuneLocaleInfo* zune_localeconv(void)
{
    return &s_locale;
}

extern "C" long zune_time(long* value)
{
    long seconds = (long)(GetTickCount() / 1000);

    if (value)
    {
        *value = seconds;
    }
    return seconds;
}

extern "C" void zune_abort(void)
{
    ExitProcess(3);
}

extern "C" long long zune_llabs(long long value)
{
    return value < 0 ? -value : value;
}
