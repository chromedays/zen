#ifndef DEBUG_H
#define DEBUG_H

#ifdef ZEN_DEBUG

#include <assert.h>
#define ASSERT(exp) assert(exp)
#define PRINT(format, ...) d_print(format, __VA_ARGS__)
#define PRINTLN(format, ...) d_print(format "\n", __VA_ARGS__)

#else

#define ASSERT(exp) (exp)
#define PRINT(format, ...)
#define PRINTLN(format, ...)

#endif

#define PRINT_FN_SIG(name) void name(const char* str)
typedef PRINT_FN_SIG(PrintFn);

void d_print(const char* format, ...);
void d_set_print_callback(PrintFn* callback);

#endif // DEBUG_H