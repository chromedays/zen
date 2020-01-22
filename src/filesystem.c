#include "filesystem.h"
#include <stddef.h>
#include <string.h>

static void fs_path_update_filename(Path* p)
{
    p->filename = p->abs_path_str;
    for (int i = histr_len(p->abs_path_str) - 1; i >= 0; i--)
    {
        if (strncmp(p->abs_path_str + i, FS_PATH_SEPARATOR,
                    strlen(FS_PATH_SEPARATOR)) == 0)
        {
            p->filename = p->abs_path_str + i + 1;
            break;
        }
    }
}

Path fs_path_make(const char* abs_path_str)
{
    // TODO: Verify if abs_path_str contains absolute path
    Path result = {0};
    result.abs_path_str = histr_makestr(abs_path_str);
    fs_path_update_filename(&result);

    return result;
}

Path fs_path_copy(Path p)
{
    Path result = fs_path_make(p.abs_path_str);
    return result;
}

void fs_path_cleanup(Path* p)
{
    histr_destroy(p->abs_path_str);
    p->abs_path_str = NULL;
}

void fs_path_append(Path* p, const char* subpath_str)
{
    histr_append(p->abs_path_str, FS_PATH_SEPARATOR);
    histr_append(p->abs_path_str, subpath_str);
    fs_path_update_filename(p);
}

void fs_path_append2(Path* p,
                     const char* subpath_str1,
                     const char* subpath_str2)
{
    histr_append(p->abs_path_str, FS_PATH_SEPARATOR);
    histr_append(p->abs_path_str, subpath_str1);
    histr_append(p->abs_path_str, FS_PATH_SEPARATOR);
    histr_append(p->abs_path_str, subpath_str2);
    fs_path_update_filename(p);
}
void fs_path_append3(Path* p,
                     const char* subpath_str1,
                     const char* subpath_str2,
                     const char* subpath_str3)
{
    histr_append(p->abs_path_str, FS_PATH_SEPARATOR);
    histr_append(p->abs_path_str, subpath_str1);
    histr_append(p->abs_path_str, FS_PATH_SEPARATOR);
    histr_append(p->abs_path_str, subpath_str2);
    histr_append(p->abs_path_str, FS_PATH_SEPARATOR);
    histr_append(p->abs_path_str, subpath_str3);
    fs_path_update_filename(p);
}
