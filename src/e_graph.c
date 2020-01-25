#include "example.h"
#include "resource.h"
#include "app.h"
#include <himath.h>
#include <glad/glad.h>
#include <stdlib.h>

typedef struct Graph_
{
    Mesh point_mesh;
    VertexBuffer point_vb;
    GLuint unlit_shader;
} Graph;

EXAMPLE_INIT_FN_SIG(graph)
{
    Example* e = e_example_make("graph", sizeof(Graph));
    Graph* s = (Graph*)e->scene;

    // s->point_mesh = rc_mesh_make_sphere(0.1f, 32, 32);
    s->point_mesh = rc_mesh_make_cube();
    r_vb_init(&s->point_vb, &s->point_mesh, GL_TRIANGLES);

    s->unlit_shader = e_shader_load(e, "unlit");

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(graph)
{
    Example* e = (Example*)udata;
    Graph* s = (Graph*)e->scene;
    glDeleteProgram(s->unlit_shader);
    r_vb_cleanup(&s->point_vb);
    rc_mesh_cleanup(&s->point_mesh);
    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(graph)
{
    Example* e = (Example*)udata;
    Graph* s = (Graph*)e->scene;

    ExamplePerFrameUBO per_frame = {0};
    per_frame.view =
        mat4_lookat((FVec3){0, 0, 3}, (FVec3){0, 0, 0}, (FVec3){0, 1, 0});
#if 1
    per_frame.proj = mat4_ortho((float)input->window_size.x, 0,
                                (float)input->window_size.y, 0, 0.1f, 100);
#else
    per_frame.proj = mat4_ortho(-10, 10, 20, 0, 0.1f, 100);
#endif
    e_apply_per_frame_ubo(e, &per_frame);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(s->unlit_shader);
    ExamplePerObjectUBO per_object = {0};
    {
        Mat4 trans = mat4_translation((FVec3){11, 11, 0});
        Mat4 scale = mat4_scale(20);
        per_object.model = mat4_mul(&trans, &scale);
    }
    per_object.color = (FVec3){0, 1, 0};
    e_apply_per_object_ubo(e, &per_object);
    r_vb_draw(&s->point_vb);
}
