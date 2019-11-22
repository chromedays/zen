#ifndef DEBUG_H
#define DEBUG_H

#ifdef ZEN_DEBUG
#include <assert.h>
#define ASSERT(exp) assert(exp)
#else
#define ASSERT(exp) (exp)
#endif

#endif // DEBUG_H