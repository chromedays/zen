out vec3 out_color;

in VertexOut
{
    vec2 uv;
};

void main()
{
    out_color = vec3(1, 0, 0);
    out_color = texture(u_samplers[0], uv).xyz;
}
