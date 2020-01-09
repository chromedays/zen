out VertexOut
{
    vec2 uv;
};

void main()
{
    uv = v_uv;
    gl_Position = vec4(v_pos, 1);
}