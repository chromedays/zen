#include "example.h"
#include "resource.h"
#include "util.h"

typedef struct HDR_
{
    Mesh fsq_mesh; // full screen quad
    VertexBuffer fsq_vb;
    GLuint fsq_shader;
} HDR;

EXAMPLE_INIT_FN_SIG(hdr)
{
    Example* e = (Example*)e_example_make("hdr", HDR);
    HDR* s = (HDR*)e->scene;

    {
        Vertex vertices[] = {
            {{-1, -1, 0}},
            {{1, -1, 0}},
            {{1, 1, 0}},
            {{-1, 1, 0}},
        };
        uint indices[] = {0, 1, 2, 2, 3, 0};
        s->fsq_mesh = rc_mesh_make_raw2(
            ARRAY_LENGTH(vertices), ARRAY_LENGTH(indices), vertices, indices);
    }
    r_vb_init(&s->fsq_vb, &s->fsq_mesh, GL_TRIANGLES);

    s->fsq_shader = e_shader_load(e, "fsq");

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(hdr)
{
    Example* e = (Example*)udata;
    HDR* s = (HDR*)e->scene;

    rc_mesh_cleanup(&s->fsq_mesh);
    r_vb_cleanup(&s->fsq_vb);
    glDeleteProgram(s->fsq_shader);

    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(hdr)
{
    Example* e = (Example*)udata;
    HDR* s = (HDR*)e->scene;

    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glUseProgram(s->fsq_shader);
    r_vb_draw(&s->fsq_vb);
}