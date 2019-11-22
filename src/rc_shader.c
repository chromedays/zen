#include "resource.h"
#include "debug.h"
#include <stdlib.h>

static GLuint
    rc_shader_compile(GLenum type, const char** sources, int sources_count);
static GLuint rc_shader_combine(GLuint vs, GLuint fs);

GLuint rc_shader_make_from_source(const char* vs_src,
                                  const char* fs_src,
                                  const char** includes,
                                  int includes_count)
{
    GLuint result = 0;

    int sources_count = includes_count + 1;
    const char** sources =
        (const char**)malloc(sources_count * sizeof(const char*));
    memcpy((void*)sources, (void*)includes,
           includes_count * sizeof(const char*));
    sources[includes_count] = vs_src;

    GLuint vs = rc_shader_compile(GL_VERTEX_SHADER, sources, sources_count);
    if (vs)
    {
        sources[includes_count] = fs_src;
        GLuint fs =
            rc_shader_compile(GL_FRAGMENT_SHADER, sources, sources_count);
        if (fs)
        {
            result = rc_shader_combine(vs, fs);
            glDeleteShader(vs);
            glDeleteShader(fs);
        }
        else
        {
            glDeleteShader(vs);
        }
    }

    free((void*)sources);

    return result;
}

static GLuint
    rc_shader_compile(GLenum type, const char** sources, int sources_count)
{
    GLuint result = 0;

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, sources_count, sources, NULL);
    glCompileShader(shader);

    GLint compile_result;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_result);
    if (compile_result == GL_TRUE)
    {
        result = shader;
    }
    else
    {
        PRINT("Failed to compile shader!\n");
        GLint info_log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
        if (info_log_length > 0)
        {
            GLchar* log_buffer =
                (GLchar*)malloc((info_log_length + 1) * sizeof(GLchar));
            glGetShaderInfoLog(shader, info_log_length, NULL, log_buffer);
            PRINT(log_buffer);
            free(log_buffer);
        }

        glDeleteShader(shader);
    }

    return result;
}

static GLuint rc_shader_combine(GLuint vs, GLuint fs)
{
    GLuint result = 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDetachShader(program, vs);
    glDetachShader(program, fs);

    GLint link_result;
    glGetProgramiv(program, GL_LINK_STATUS, &link_result);
    if (link_result == GL_TRUE)
    {
        result = program;
    }
    else
    {
        PRINT("Failed to link program!\n");
        GLint info_log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
        if (info_log_length > 0)
        {
            GLchar* log_buffer =
                (GLchar*)malloc((info_log_length + 1) * sizeof(GLchar));
            glGetProgramInfoLog(program, info_log_length, NULL, log_buffer);
            PRINT(log_buffer);
            free(log_buffer);
        }

        glDeleteProgram(program);
    }

    return result;
}
