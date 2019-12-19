#version 460 core

layout (location = 0) in vec3 v_pos;
layout (location = 1) in vec2 v_uv;
layout (location = 2) in vec3 v_normal;

layout (binding = 0, std140) uniform PerFrame
{
    mat4 u_view;
    mat4 u_proj;
    float t;
};

layout (binding = 1, std140) uniform PerObject
{
    mat4 u_model;
    vec3 u_color;
};

#define MAX_SAMPLERS_COUNT 16
layout (binding = 2) uniform sampler2D u_samplers[MAX_SAMPLERS_COUNT];

mat4 get_vp()
{
    return u_proj * u_view;
}

mat4 get_mvp()
{
    return u_proj * u_view * u_model;
}

vec3 get_unlit_color()
{
    return u_color;
}

vec4 get_transformed_vertex()
{
    return get_mvp() * vec4(v_pos, 1);
}