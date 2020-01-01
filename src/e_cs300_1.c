#include "example.h"
#include <histr.h>
#include <stdlib.h>

#define MAX_MODEL_FILES_COUNT 100

typedef struct CS300_1_
{
    histr_String model_filenames[MAX_MODEL_FILES_COUNT];
    int model_filenames_count;
} CS300_1;

EXAMPLE_INIT_FN_SIG(cs300_1)
{
    Example* e = e_example_make("cs300_1", sizeof(CS300_1));
    CS300_1* s = (CS300_1*)e->scene;

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(cs300_1)
{
    Example* e = (Example*)udata;
    CS300_1* s = (CS300_1*)e->scene;

    free(e);
}

EXAMPLE_UPDATE_FN_SIG(cs300_1)
{
    Example* e = (Example*)udata;
    CS300_1* s = (CS300_1*)e->scene;
}
