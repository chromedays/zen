in VertexOut 
{
    vec2 uv;
};

out vec4 out_color;

void main()
{
    out_color = texture(u_samplers[0], vec2(uv.x, 1 - uv.y));
}
