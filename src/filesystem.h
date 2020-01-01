#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <histr.h>

#ifdef _WIN32
#define FS_PATH_SEPARATOR "\\"
#else
#define FS_PATH_SEPARATOR "/"
#endif

typedef struct Path_
{
    histr_String abs_path_str;
} Path;

enum FileType
{
    FileType_Directory,
    FileType_File,
    FileType_Count
};

#define FILE_FOREACH_FN_DECL(name) void name(Path* file_path, void* udata)
typedef FILE_FOREACH_FN_DECL(FileForeachFn);

void fs_for_each_files_with_ext(Path* p,
                                const char* ext,
                                FileForeachFn* for_each_fn,
                                void* udata);

Path fs_path_make_working_dir();

Path fs_path_make(const char* abs_path_str);
Path fs_path_copy(Path p);
void fs_path_cleanup(Path* p);
void fs_path_append(Path* p, const char* subpath_str);
void fs_path_append2(Path* p,
                     const char* subpath_str1,
                     const char* subpath_str2);
void fs_path_append3(Path* p,
                     const char* subpath_str1,
                     const char* subpath_str2,
                     const char* subpath_str3);

#endif // FILESYSTEM_H