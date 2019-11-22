#ifndef RESOURCE_H
#define RESOURCE_H

#include "glad/glad_wgl.h"

GLuint rc_shader_make_from_source(const char* vs_src,
                                  const char* fs_src,
                                  const char** includes,
                                  int includes_count);

#endif // RESOURCE_H
