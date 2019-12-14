#include "scene.h"
#include "resource.h"
#include <himath.h>
#include <glad/glad.h>
#include <stdlib.h>

typedef struct HelloTriangle_
{
    GLuint vao;
    GLuint vbo;
    GLuint unlit_shader;
} HelloTriangle;

SCENE_INIT_FN_SIG(hello_triangle_init)
{
    HelloTriangle* scene = (HelloTriangle*)calloc(1, sizeof(*scene));

    FVec3 vertices[] = {
        {1, -0.5f, 0},
        {-1, -0.5f, 0},
        {0, 1, 0},
    };

    glGenVertexArrays(1, &scene->vao);
    glBindVertexArray(scene->vao);
    glGenBuffers(1, &scene->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, scene->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FVec3), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    scene->unlit_shader =
        rc_shader_load_from_files("hello_triangle/unlit.vert",
                                  "hello_triangle/unlit.frag", NULL, NULL, 0);

    return scene;
}

SCENE_CLEANUP_FN_SIG(hello_triangle_cleanup)
{
    HelloTriangle* scene = (HelloTriangle*)udata;
    glDeleteVertexArrays(1, &scene->vao);
    glDeleteBuffers(1, &scene->vbo);
    glDeleteProgram(scene->unlit_shader);
    free(scene);
}

SCENE_UPDATE_FN_SIG(hello_triangle_update)
{
    HelloTriangle* scene = (HelloTriangle*)udata;
    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(scene->unlit_shader);
    glBindVertexArray(scene->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
