#include <scene.h>
#include <stdlib.h>

typedef struct PrototypeScene_
{
    int placeholder;
} PrototypeScene;

SCENE_INIT_FN_SIG(gs_prototype_init)
{
    PrototypeScene* s = (PrototypeScene*)malloc(sizeof(*s));
    return s;
}

SCENE_CLEANUP_FN_SIG(gs_prototype_cleanup)
{
    PrototypeScene* s = (PrototypeScene*)udata;
    free(s);
}

SCENE_UPDATE_FN_SIG(gs_prototype_update)
{
}
