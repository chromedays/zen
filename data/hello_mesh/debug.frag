#version 460 core

in VertexOut
{
    vec3 color;
};

out vec3 out_color;

void main()
{
    out_color = color;
}