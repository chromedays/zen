#ifndef UTIL_H
#define UTIL_H

// TODO: Cast to int
#define ARRAY_LENGTH(arr) ((int)(sizeof(arr) / sizeof(*(arr))))

#if defined(_MSC_VER)
#define ALIGN_AS(bytes) __declspec(align(bytes))
#elif defined(__GNUC__) || defined(__clang__)
#define ALIGN_AS(bytes) __attribute__((aligned(bytes)))
#else
#error "This compiler is not supported!"
#endif

#endif // UTIL_H
