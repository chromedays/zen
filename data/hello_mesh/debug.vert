#version 460 core
layout (location = 0) in vec3 v_pos;
layout (location = 1) in vec3 v_uv;
layout (location = 2) in vec3 v_normal;

layout (binding = 0, std140) uniform PerFrame
{
    mat4 mvp;
};

out VertexOut
{
    vec3 color;
};

void main()
{
    color = (v_pos + vec3(1)) * 0.5;
    gl_Position = mvp * vec4(v_pos, 1);
}