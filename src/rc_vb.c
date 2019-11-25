#include "resource.h"
#include "debug.h"
#include <stddef.h>

void vb_init(VertexBuffer* vb, const Mesh* mesh, GLenum mode)
{
    ASSERT(mesh->vertices);
    glGenVertexArrays(1, &vb->vao);
    glBindVertexArray(vb->vao);

    glGenBuffers(1, &vb->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vb->vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh->vertices_count * sizeof(Vertex),
                 mesh->vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (GLvoid*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (GLvoid*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (GLvoid*)offsetof(Vertex, normal));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (mesh->indices)
    {
        glGenBuffers(1, &vb->ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vb->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     mesh->indices_count * sizeof(uint), mesh->indices,
                     GL_STATIC_DRAW);
        vb->count = mesh->indices_count;
    }
    else
    {
        vb->count = mesh->vertices_count;
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    vb->mode = mode;
}

void vb_cleanup(VertexBuffer* vb)
{
    glDeleteVertexArrays(1, &vb->vao);
    glDeleteBuffers(1, &vb->vbo);
    if (vb->ebo != 0)
        glDeleteBuffers(1, &vb->ebo);
}

void vb_draw(const VertexBuffer* vb)
{
    glBindVertexArray(vb->vao);
    if (vb->ebo != 0)
        glDrawElements(vb->mode, vb->count, GL_UNSIGNED_INT, NULL);
    else
        glDrawArrays(vb->mode, 0, vb->count);
}