#ifndef HISTR_H
#define HISTR_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

typedef char* histr_String;
typedef const char* const_histr_String;

histr_String histr_make();
histr_String histr_makestr(const char* s);
void histr_destroy(histr_String str);
int histr_len(const_histr_String str);
bool histr_cmp(const_histr_String str0, const_histr_String str1);
void histr_clear(histr_String str);
#define histr_append(str, s) str = histr_append_impl(str, s)
histr_String histr_append_impl(histr_String str, const char* s);
#define histr_subappend(str, s, n) str = histr_subappend_impl(str, s, n)
histr_String histr_subappend_impl(histr_String str, const char* s, int n);

#ifdef HISTR_IMPL

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define HISTR_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define HISTR_MIN(a, b) (((a) < (b)) ? (a) : (b))

#define HISTR_MALLOC(sz) malloc(sz)
#define HISTR_FREE(p) free(p)

#define HISTR_MOVE_PTR_BACKWARD(p, type) ((uint8_t*)(p) - sizeof(type))
#define HISTR_MOVE_PTR_FORWARD(p, type) ((uint8_t*)(p) + sizeof(type))

#define HISTR_DEFAULT_CAP 1280

typedef struct _histr_Header
{
    int cap; // capacity in bytes
    int len;
} histr_Header;

static histr_Header* histr_get_header(const_histr_String str)
{
    histr_Header* result =
        (histr_Header*)(HISTR_MOVE_PTR_BACKWARD(str, histr_Header));
    return result;
}

histr_String histr_make()
{
    int size = sizeof(histr_Header) + sizeof(char) * HISTR_DEFAULT_CAP;
    histr_Header* header = (histr_Header*)HISTR_MALLOC(size);
    header->cap = HISTR_DEFAULT_CAP;
    header->len = 0;

    histr_String result =
        (histr_String)HISTR_MOVE_PTR_FORWARD(header, histr_Header);

    histr_clear(result);

    return result;
}

histr_String histr_makestr(const char* s)
{
    histr_String str = histr_make();
    histr_append(str, s);
    return str;
}

void histr_destroy(histr_String str)
{
    if (str == NULL)
        return;

    histr_Header* header = histr_get_header(str);
    HISTR_FREE(header);
}

int histr_len(const_histr_String str)
{
    histr_Header* header = histr_get_header(str);
    return header->len;
}

bool histr_cmp(const_histr_String str0, const_histr_String str1)
{
    if (histr_get_header(str0)->len != histr_get_header(str1)->len)
        return false;
    bool result =
        (memcmp(str0, str1, histr_get_header(str0)->len * sizeof(char)) == 0);
    return result;
}

void histr_clear(histr_String str)
{
    str[0] = '\0';
    histr_get_header(str)->len = 0;
}

static int histr_strlen(const char* s)
{
    const char* c = s;
    int len = 0;
    while (*c++ != '\0')
        ++len;

    return len;
}

#define histr_try_expand(str, n) ((str) = histr_try_expand_impl((str), (n)))
static histr_String histr_try_expand_impl(histr_String str, int n)
{
    histr_Header* header = histr_get_header(str);

    if (n > header->cap)
    {
        int new_cap = header->cap;
        do
        {
            new_cap *= 2;
        } while (new_cap < n);

        histr_Header* new_header = (histr_Header*)HISTR_MALLOC(
            sizeof(histr_Header) + sizeof(char) * new_cap);
        new_header->cap = new_cap;
        histr_String new_str =
            (histr_String)HISTR_MOVE_PTR_FORWARD(new_header, histr_Header);
        memcpy(new_str, str, header->cap);
        histr_destroy(str);

        str = new_str;
    }

    return str;
}

histr_String histr_append_impl(histr_String str, const char* s)
{
    histr_String result = NULL;
    int cat_offset = histr_get_header(str)->len;
    int s_len = histr_strlen(s);
    int new_len = histr_get_header(str)->len + s_len;

    histr_try_expand(str, new_len + 1);

    memcpy(str + cat_offset, s, s_len * sizeof(char));
    histr_get_header(str)->len = new_len;
    str[histr_get_header(str)->len] = '\0';

    return str;
}

histr_String histr_subappend_impl(histr_String str, const char* s, int n)
{
    int s_len = histr_strlen(s);
    int copyn = HISTR_MIN(s_len, n);
    int cat_offset = histr_get_header(str)->len;
    int new_len = histr_get_header(str)->len + copyn;

    histr_try_expand_impl(str, new_len + 1);

    memcpy(str + cat_offset, s, copyn * sizeof(char));
    histr_get_header(str)->len = new_len;
    str[histr_get_header(str)->len] = '\0';

    return str;
}

#endif // HISTR_IMPL

#endif // HISTR_H
