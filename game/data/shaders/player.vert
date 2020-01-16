#version 460 core
layout (location = 0) in vec3 v_pos;
layout (location = 1) in vec2 v_uv;
layout (location = 2) in vec3 v_normal;

layout (binding = 0) uniform PerFrame
{
    mat4 u_view;
    mat4 u_proj;
};

layout (binding = 1) uniform PerObject
{
    mat4 u_model;
    vec4 u_color;
};

void main()
{
    mat4 mvp = u_proj * u_view * u_model;
    gl_Position = mvp * vec4(v_pos, 1);
}