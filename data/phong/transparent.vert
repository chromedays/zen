out VertexOut
{
    vec2 uv;
};

void main()
{
    uv = v_uv;
    gl_Position = transformed_vertex();
}