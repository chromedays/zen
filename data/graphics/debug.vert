out VertexOut
{
    vec3 color;
};

void main()
{
    color = (normalize(v_pos) + vec3(1)) * 0.5;
    gl_Position = transformed_vertex();
}