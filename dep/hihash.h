//#ifndef HIHASH_H
//#define HIHASH_H

#ifndef __cplusplus
#include <stdbool.h>
#endif //__cplusplus

#ifndef HH_VALUE_TYPE
#error "MUST DEFINE HH_VALUE_TYPE"
#endif // HH_VALUE_TYPE

// MACRO MAGIC
//#define HH_XSTR(x) HH_STR(x)
//#define HH_STR(x) #x
//#define HH_VALUE_TYPE_STR HH_XSTR(HH_VALUE_TYPE)

// MORE MACRO MAGIC
#define HH_SYM__(name, type) name##_##type
#define HH_SYM_(name, type_macro) HH_SYM__(name, type_macro)
#define HH_SYM(name) HH_SYM_(name, HH_VALUE_TYPE)

typedef struct HH_SYM(_hh_Bucket)
{
    char* key;
    HH_VALUE_TYPE value;
    bool occupied;
} HH_SYM(hh_Bucket);

typedef struct HH_SYM(_hh_HashTable)
{
    HH_SYM(hh_Bucket) * buckets;
    int bucket_count;
    int count;
} HH_SYM(hh_HashTable);

HH_SYM(hh_HashTable) HH_SYM(hh_make)(int bucket_count);
void HH_SYM(hh_cleanup)(HH_SYM(hh_HashTable) * ht);
bool HH_SYM(hh_put_val)(HH_SYM(hh_HashTable) * ht,
                        const char* key,
                        HH_VALUE_TYPE value);
bool HH_SYM(hh_put_ref)(HH_SYM(hh_HashTable) * ht,
                        const char* key,
                        HH_VALUE_TYPE* value);
HH_VALUE_TYPE HH_SYM(hh_get_val)(HH_SYM(hh_HashTable) * ht, const char* key);
HH_VALUE_TYPE* HH_SYM(hh_get_ref)(HH_SYM(hh_HashTable) * ht, const char* key);

#ifdef HIHASH_IMPL
#include <stdlib.h>
#include <string.h>

#ifndef HH_MALLOC
#define HH_MALLOC(sz) malloc(sz)
#endif // HH_MALLOC

#ifndef HH_FREE
#define HH_FREE(p) free(p)
#endif // HH_FREE

#define HH_STEP 2
#define HH_HASH(key) HH_SYM(hh_djb2)(key)

HH_SYM(hh_HashTable) HH_SYM(hh_make)(int bucket_count)
{
    HH_SYM(hh_HashTable) ht = {0};
    size_t size = bucket_count * sizeof(HH_SYM(hh_Bucket));
    ht.buckets = (HH_SYM(hh_Bucket)*)HH_MALLOC(size);
    memset(ht.buckets, 0, size);
    ht.bucket_count = bucket_count;

    return ht;
}

void HH_SYM(hh_cleanup)(HH_SYM(hh_HashTable) * ht)
{
    if (ht->buckets)
    {
        for (int i = 0; i < ht->bucket_count; ++i)
        {
            HH_SYM(hh_Bucket)* bucket = ht->buckets + i;
            if (bucket->key)
                HH_FREE(bucket->key);
        }
        HH_FREE(ht->buckets);
        ht->buckets = NULL;
    }
    ht->bucket_count = 0;
    ht->count = 0;
}

static int HH_SYM(hh_djb2)(const char* str)
{
    int hash = 5381;
    int c;
    while (c = *str++)
        hash = ((hash << 5) + hash) + c;

    return hash;
}

bool HH_SYM(hh_put_val)(HH_SYM(hh_HashTable) * ht,
                        const char* key,
                        HH_VALUE_TYPE value)
{
    bool result = HH_SYM(hh_put_ref)(ht, key, &value);
    return result;
}

bool HH_SYM(hh_put_ref)(HH_SYM(hh_HashTable) * ht,
                        const char* key,
                        HH_VALUE_TYPE* value)
{
    if (HH_SYM(hh_get_ref)(ht, key))
        return false;

    int hash = HH_HASH(key);
    int index = abs(hash) % ht->bucket_count;

    // TODO: Fix possibility of inf loop
    while (ht->buckets[index].occupied)
        index = (index + HH_STEP) % ht->bucket_count;

    HH_SYM(hh_Bucket)* bucket = ht->buckets + index;
    int key_len = (int)strlen(key);
    bucket->key = (char*)HH_MALLOC(sizeof(char) * (key_len + 1));
    memcpy(bucket->key, key, sizeof(char) * (key_len + 1));
    bucket->value = *value;
    bucket->occupied = true;

    return true;
}

HH_VALUE_TYPE HH_SYM(hh_get_val)(HH_SYM(hh_HashTable) * ht, const char* key)
{
    HH_VALUE_TYPE* result = HH_SYM(hh_get_ref)(ht, key);
    return *result;
}

HH_VALUE_TYPE* HH_SYM(hh_get_ref)(HH_SYM(hh_HashTable) * ht, const char* key)
{
    int hash = HH_HASH(key);
    int index = abs(hash) % ht->bucket_count;

    while (ht->buckets[index].occupied)
    {
        if (strcmp(ht->buckets[index].key, key) == 0)
            return &ht->buckets[index].value;

        index = (index + HH_STEP) % ht->bucket_count;
    }

    return NULL;
}

#endif // HIHASH_IMPL

//#endif // HIHASH_H