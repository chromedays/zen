out VertexOut
{
    vec3 pos_world;
    vec3 normal_world;
};

void main()
{
    pos_world = model_to_world_point(v_pos).xyz;
    normal_world = model_to_world_vector(v_normal);
    gl_Position = transformed_vertex();
}