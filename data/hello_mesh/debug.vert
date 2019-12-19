out VertexOut
{
    vec3 color;
};

void main()
{
    color = (v_pos + vec3(1)) * 0.5;
    gl_Position = get_transformed_vertex();
}