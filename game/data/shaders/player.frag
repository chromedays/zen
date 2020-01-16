#version 460 core

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

out vec4 out_color;

void main()
{
    out_color = u_color;
}