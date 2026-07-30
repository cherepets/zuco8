#pragma once

#ifndef BUFSIZ
#define BUFSIZ 512
#endif

typedef struct ZuneLocaleInfo
{
    char* decimal_point;
} ZuneLocaleInfo;

#ifdef __cplusplus
extern "C"
{
#endif

extern int zune_errno;
const char* zune_strerror(int error_number);
void* zune_freopen(const char* file_name, const char* mode, void* stream);
ZuneLocaleInfo* zune_localeconv(void);
long zune_time(long* value);
void zune_abort(void);
long long zune_llabs(long long value);

#ifdef __cplusplus
}
#endif

#define errno zune_errno
#define strerror zune_strerror
#define freopen zune_freopen
#define localeconv zune_localeconv
#define strcoll strcmp
#define time zune_time
#define abort zune_abort
#define llabs zune_llabs
