out VertexOut
{
    vec2 uv;
    vec3 color;
};

void main()
{
    uv = v_uv;
    color = (v_pos + vec3(1)) * 0.5;
    gl_Position = get_transformed_vertex();
}