out VertexOut
{
    vec3 normal;
};

void main()
{
    gl_Position = transformed_vertex();
    normal = normalize(vec3(u_proj * vec4(model_to_view_vector(v_normal), 0)));
}