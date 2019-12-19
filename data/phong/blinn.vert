out VertexOut
{
    vec3 pos_world;
    vec3 normal_world;
    vec2 uv;
};

void main()
{
    pos_world = model_to_world_point(v_pos).xyz;
    normal_world = v_normal;
    uv = v_uv;
    gl_Position = transformed_vertex();
}