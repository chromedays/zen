#include <scene.h>
#include <renderer.h>
#include <resource.h>
#include <util.h>
#include <stdlib.h>
#include <math.h>

typedef struct PrototypeScene_
{
    int placeholder;
    Mesh circle_mesh;
    VertexBuffer circle_vb;
} PrototypeScene;

SCENE_INIT_FN_SIG(gs_prototype_init)
{
    PrototypeScene* s = (PrototypeScene*)malloc(sizeof(*s));

    Vertex vertices[128] = {0};
    float step = degtorad(360.f / (float)ARRAY_LENGTH(vertices));
    for (int i = 0; i < ARRAY_LENGTH(vertices); i++)
    {
        vertices[i].pos = (FVec3){
            cosf(step * (float)i),
            sinf(step * (float)i),
            0,
        };
    }
    s->circle_mesh =
        rc_mesh_make_raw2(ARRAY_LENGTH(vertices), 0, vertices, NULL);

    r_vb_init(&s->circle_vb, &s->circle_mesh, GL_TRIANGLE_FAN);

    return s;
}

SCENE_CLEANUP_FN_SIG(gs_prototype_cleanup)
{
    PrototypeScene* s = (PrototypeScene*)udata;
    r_vb_cleanup(&s->circle_vb);
    rc_mesh_cleanup(&s->circle_mesh);
    free(s);
}

SCENE_UPDATE_FN_SIG(gs_prototype_update)
{
    PrototypeScene* s = (PrototypeScene*)udata;

    glClearColor(0.5f, 0.5f, 0.5f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glDisable(GL_CULL_FACE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    r_vb_draw(&s->circle_vb);
}
