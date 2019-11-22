#ifndef DEBUG_H
#define DEBUG_H

#ifdef ZEN_DEBUG

#include <assert.h>
#define ASSERT(exp) assert(exp)
#define PRINT(msg)

#else

#define ASSERT(exp) (exp)
#define PRINT(msg)

#endif

#endif // DEBUG_H