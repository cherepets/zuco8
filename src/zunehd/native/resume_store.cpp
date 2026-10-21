#include "resume_store.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>

static const wchar_t kStorageRoot[] = L"\\xnaa";
static const wchar_t kResumeDirectory[] = L"\\xnaa\\zuco8\\resume";
static const DWORD kStorageBudget = 1536 * 1024;
static const DWORD kClusterBytes = 4096;
static const DWORD kMaxResumeBytes = 8 * 1024 * 1024;

static uint8_t* s_data;
static size_t s_size;
static size_t s_capacity;
static uint32_t s_cart_crc;
static bool s_writing;
static bool s_write_ok;

static bool s_exists_known;
static uint32_t s_exists_crc;
static bool s_exists;

static bool EnsureResumeDirectory(void)
{
    if (!CreateDirectory(kStorageRoot, 0) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return false;
    }
    if (!CreateDirectory(L"\\xnaa\\zuco8", 0) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return false;
    }
    if (!CreateDirectory(kResumeDirectory, 0) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return false;
    }
    return true;
}

static void BuildPath(wchar_t* path, uint32_t cart_crc, const wchar_t* extension)
{
    static const wchar_t kHex[] = L"0123456789abcdef";
    size_t length = wcslen(kResumeDirectory);
    int i;

    wcscpy(path, kResumeDirectory);
    path[length++] = L'\\';
    for (i = 7; i >= 0; --i)
    {
        path[length++] = kHex[(cart_crc >> (i * 4)) & 0x0f];
    }
    wcscpy(path + length, extension);
}

static bool JoinPath(wchar_t* path, const wchar_t* directory, const wchar_t* name)
{
    size_t directory_length = wcslen(directory);
    size_t name_length = wcslen(name);

    if (directory_length + 1 + name_length + 1 > MAX_PATH)
    {
        return false;
    }
    wcscpy(path, directory);
    path[directory_length] = L'\\';
    wcscpy(path + directory_length + 1, name);
    return true;
}

static bool IsDotEntry(const wchar_t* name)
{
    return name[0] == L'.' && (name[1] == 0 || (name[1] == L'.' && name[2] == 0));
}

static bool ReadExact(HANDLE file, void* data, DWORD size)
{
    DWORD read = 0;

    return ReadFile(file, data, size, &read, 0) && read == size;
}

static bool WriteExact(HANDLE file, const void* data, DWORD size)
{
    DWORD written = 0;

    return WriteFile(file, data, size, &written, 0) && written == size;
}

static DWORD StorageUsage(const wchar_t* directory, int depth)
{
    wchar_t pattern[MAX_PATH];
    wchar_t child[MAX_PATH];
    WIN32_FIND_DATA found;
    HANDLE finder;
    DWORD usage = 0;

    if (depth > 8 || !JoinPath(pattern, directory, L"*"))
    {
        return 0;
    }
    finder = FindFirstFile(pattern, &found);
    if (finder == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    do
    {
        if (IsDotEntry(found.cFileName))
        {
            continue;
        }
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            usage += kClusterBytes;
            if (JoinPath(child, directory, found.cFileName))
            {
                usage += StorageUsage(child, depth + 1);
            }
        }
        else
        {
            usage += (found.nFileSizeLow + kClusterBytes - 1) / kClusterBytes * kClusterBytes;
        }
    } while (FindNextFile(finder, &found));
    FindClose(finder);
    return usage;
}

static bool DeleteOldestResume(void)
{
    wchar_t pattern[MAX_PATH];
    wchar_t oldest[MAX_PATH];
    WIN32_FIND_DATA found;
    FILETIME oldest_time;
    HANDLE finder;
    bool have_oldest = false;

    JoinPath(pattern, kResumeDirectory, L"*.qr");
    finder = FindFirstFile(pattern, &found);
    if (finder == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    do
    {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            (!have_oldest || CompareFileTime(&found.ftLastWriteTime, &oldest_time) < 0) &&
            JoinPath(oldest, kResumeDirectory, found.cFileName))
        {
            oldest_time = found.ftLastWriteTime;
            have_oldest = true;
        }
    } while (FindNextFile(finder, &found));
    FindClose(finder);

    s_exists_known = false;
    return have_oldest && DeleteFile(oldest);
}

static bool MakeRoom(DWORD file_size)
{
    const DWORD needed = (file_size + kClusterBytes - 1) / kClusterBytes * kClusterBytes;

    while (StorageUsage(kStorageRoot, 0) + needed > kStorageBudget)
    {
        if (!DeleteOldestResume())
        {
            return false;
        }
    }
    return true;
}

static bool WriteResumeFile(uint32_t cart_crc, const uint8_t* data, DWORD size)
{
    wchar_t temporary_path[MAX_PATH];
    wchar_t path[MAX_PATH];
    HANDLE file;
    bool ok;

    BuildPath(temporary_path, cart_crc, L".tmp");
    BuildPath(path, cart_crc, L".qr");
    file = CreateFile(temporary_path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    ok = WriteExact(file, data, size) && FlushFileBuffers(file);
    if (!CloseHandle(file))
    {
        ok = false;
    }
    if (ok && !MoveFile(temporary_path, path))
    {
        ok = DeleteFile(path) && MoveFile(temporary_path, path);
    }
    if (!ok)
    {
        DeleteFile(temporary_path);
    }
    return ok;
}

static uint8_t* ReadWholeFile(uint32_t cart_crc, size_t* size)
{
    wchar_t path[MAX_PATH];
    HANDLE file;
    DWORD file_size;
    uint8_t* data;

    BuildPath(path, cart_crc, L".qr");
    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    file_size = GetFileSize(file, 0);
    if (file_size == 0xFFFFFFFF || file_size == 0 || file_size > kMaxResumeBytes)
    {
        CloseHandle(file);
        return 0;
    }
    data = (uint8_t*)malloc(file_size);
    if (!data || !ReadExact(file, data, file_size))
    {
        free(data);
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    *size = file_size;
    return data;
}

static void ReleaseBuffer(void)
{
    free(s_data);
    s_data = 0;
    s_size = 0;
    s_capacity = 0;
    s_writing = false;
}

extern "C" bool resume_store_exists(uint32_t cart_crc)
{
    wchar_t path[MAX_PATH];
    DWORD attributes;

    if (!s_exists_known || s_exists_crc != cart_crc)
    {
        BuildPath(path, cart_crc, L".qr");
        attributes = GetFileAttributes(path);
        s_exists = attributes != 0xFFFFFFFF && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
        s_exists_crc = cart_crc;
        s_exists_known = true;
    }
    return s_exists;
}

extern "C" bool resume_store_begin(uint32_t cart_crc)
{
    resume_store_discard(cart_crc);
    s_cart_crc = cart_crc;
    s_writing = true;
    s_write_ok = true;
    return true;
}

extern "C" bool resume_store_write(void* userdata, const void* data, size_t size)
{
    (void)userdata;
    if (!s_writing || !s_write_ok)
    {
        return false;
    }
    if (size > kMaxResumeBytes || s_size + size > kMaxResumeBytes)
    {
        s_write_ok = false;
        return false;
    }
    if (s_size + size > s_capacity)
    {
        size_t capacity = s_capacity ? s_capacity : 64 * 1024;
        uint8_t* grown;

        while (capacity < s_size + size)
        {
            capacity *= 2;
        }
        grown = (uint8_t*)realloc(s_data, capacity);
        if (!grown)
        {
            s_write_ok = false;
            return false;
        }
        s_data = grown;
        s_capacity = capacity;
    }
    memcpy(s_data + s_size, data, size);
    s_size += size;
    return true;
}

extern "C" bool resume_store_commit(void)
{
    bool ok = false;

    if (s_writing && s_write_ok && s_size > 0)
    {
        if (EnsureResumeDirectory() && MakeRoom((DWORD)s_size))
        {
            ok = WriteResumeFile(s_cart_crc, s_data, (DWORD)s_size);
            while (!ok && DeleteOldestResume())
            {
                ok = WriteResumeFile(s_cart_crc, s_data, (DWORD)s_size);
            }
        }
    }
    ReleaseBuffer();
    s_exists_known = false;
    return ok;
}

extern "C" void resume_store_abort(void)
{
    ReleaseBuffer();
}

extern "C" uint8_t* resume_store_load(uint32_t cart_crc, size_t* size)
{
    if (!size)
    {
        return 0;
    }
    return ReadWholeFile(cart_crc, size);
}

extern "C" void resume_store_free(uint8_t* data)
{
    free(data);
}

extern "C" bool resume_store_evict_oldest(void)
{
    return DeleteOldestResume();
}

extern "C" void resume_store_discard(uint32_t cart_crc)
{
    wchar_t path[MAX_PATH];

    ReleaseBuffer();
    BuildPath(path, cart_crc, L".qr");
    DeleteFile(path);
    BuildPath(path, cart_crc, L".tmp");
    DeleteFile(path);
    s_exists_known = false;
}
