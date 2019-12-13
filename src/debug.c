#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

static PRINT_FN_SIG(d_default_print_callback);

static PrintFn* g_log_fn = &d_default_print_callback;

void d_print(const char* format, ...)
{
    char buf[1000] = {0};
    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);
    g_log_fn(buf);
}

void d_set_print_callback(PrintFn* callback)
{
    g_log_fn = callback;
}

static PRINT_FN_SIG(d_default_print_callback)
{
    printf("%s", str);
}
