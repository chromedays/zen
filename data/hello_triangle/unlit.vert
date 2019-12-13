#version 460 core

layout (location = 0) in vec3 v_pos;

out VertexOut
{
    vec3 color;
};

void main()
{
    color = (v_pos + vec3(1)) * 0.5;
    gl_Position = vec4(v_pos, 1);
}