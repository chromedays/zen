#include "filesystem.h"
#include "debug.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

void fs_for_each_files_with_ext(Path p,
                                const char* ext,
                                FileForeachFn* for_each_fn,
                                void* udata)
{
    histr_String pattern = histr_makestr(p.abs_path_str);
    histr_append(pattern, FS_PATH_SEPARATOR);
    histr_append(pattern, "*.");
    histr_append(pattern, ext);

    WIN32_FIND_DATAA fd;
    HANDLE fh = FindFirstFileA(pattern, &fd);
    if (fh != INVALID_HANDLE_VALUE)
    {
        do
        {
            Path file_path = fs_path_copy(p);
            fs_path_append(&file_path, fd.cFileName);
            for_each_fn(&file_path, udata);
            fs_path_cleanup(&file_path);
        } while (FindNextFileA(fh, &fd));
        FindClose(fh);
    }
}

Path fs_path_make_working_dir()
{
    DWORD len = GetCurrentDirectoryA(0, NULL);
    ASSERT(len <= MAX_PATH);
    char buf[MAX_PATH + 1] = {0};
    GetCurrentDirectoryA(sizeof(buf), buf);

    Path result = fs_path_make(buf);
    return result;
}
