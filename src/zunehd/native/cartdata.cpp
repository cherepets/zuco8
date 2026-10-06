#include "cartdata.h"

#include <windows.h>
#include <string.h>

static const DWORD kCartDataBytes = 256;
static const size_t kCartDataIdMax = 64;

static const wchar_t kCartDataDirectory[] = L"\\xnaa\\zuco8\\cartdata";

static uint8_t s_loaded_snapshot[kCartDataBytes];
static wchar_t s_data_path[MAX_PATH];
static uint8_t* s_selected_ram;

static bool EnsureCartDataDirectory(void)
{
    if (!CreateDirectory(L"\\xnaa", 0) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return false;
    }
    if (!CreateDirectory(L"\\xnaa\\zuco8", 0) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return false;
    }
    if (!CreateDirectory(kCartDataDirectory, 0) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return false;
    }
    return true;
}

static bool IsValidId(const char* id)
{
    size_t length = 0;

    if (!id)
    {
        return false;
    }
    while (id[length])
    {
        const char c = id[length];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_'))
        {
            return false;
        }
        if (++length > kCartDataIdMax)
        {
            return false;
        }
    }
    return length != 0;
}

static bool BuildDataPath(const char* id)
{
    const size_t directory_length = wcslen(kCartDataDirectory);
    const size_t id_length = strlen(id);
    wchar_t* filename;
    size_t i;

    if (directory_length + 1 + id_length + 4 + 1 > MAX_PATH)
    {
        return false;
    }

    wcscpy(s_data_path, kCartDataDirectory);
    filename = s_data_path + directory_length;
    *filename++ = L'\\';
    for (i = 0; i < id_length; ++i)
    {
        char c = id[i];
        if (c >= 'A' && c <= 'Z')
        {
            c = (char)(c - 'A' + 'a');
        }
        *filename++ = (wchar_t)(unsigned char)c;
    }
    wcscpy(filename, L".dat");
    return true;
}

static bool WriteSnapshot(const uint8_t* bytes)
{
    wchar_t temporary_path[MAX_PATH];
    HANDLE file;
    DWORD written = 0;
    bool complete;

    if (wcslen(s_data_path) + 4 >= MAX_PATH)
    {
        return false;
    }
    wcscpy(temporary_path, s_data_path);
    wcscat(temporary_path, L".tmp");
    DeleteFile(temporary_path);

    file = CreateFile(temporary_path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    complete = WriteFile(file, bytes, kCartDataBytes, &written, 0) &&
        written == kCartDataBytes && FlushFileBuffers(file);
    if (!CloseHandle(file))
    {
        complete = false;
    }
    if (!complete)
    {
        DeleteFile(temporary_path);
        return false;
    }

    if (!MoveFile(temporary_path, s_data_path) &&
        (!DeleteFile(s_data_path) || !MoveFile(temporary_path, s_data_path)))
    {
        DeleteFile(temporary_path);
        return false;
    }
    return true;
}

static void SaveSelectedWindow(void)
{
    if (!s_selected_ram)
    {
        return;
    }
    if (memcmp(s_loaded_snapshot, s_selected_ram, kCartDataBytes) == 0)
    {
        return;
    }
    if (WriteSnapshot(s_selected_ram))
    {
        memcpy(s_loaded_snapshot, s_selected_ram, kCartDataBytes);
    }
}

extern "C" void cartdata_reset(void)
{
    s_selected_ram = 0;
    memset(s_loaded_snapshot, 0, kCartDataBytes);
}

extern "C" void cartdata_flush(void)
{
    SaveSelectedWindow();
}

extern "C" bool cartdata_select(const char* id, uint8_t* ram)
{
    HANDLE file;
    DWORD read = 0;
    DWORD size;

    if (!ram)
    {
        return false;
    }

    cartdata_flush();
    memset(ram, 0, kCartDataBytes);
    cartdata_reset();

    if (!IsValidId(id) || !EnsureCartDataDirectory() || !BuildDataPath(id))
    {
        return false;
    }

    s_selected_ram = ram;

    file = CreateFile(s_data_path, GENERIC_READ, FILE_SHARE_READ, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    size = GetFileSize(file, 0);
    if (size == kCartDataBytes && ReadFile(file, ram, kCartDataBytes, &read, 0) && read == kCartDataBytes)
    {
        memcpy(s_loaded_snapshot, ram, kCartDataBytes);
        CloseHandle(file);
        return true;
    }
    CloseHandle(file);
    memset(ram, 0, kCartDataBytes);
    return false;
}
