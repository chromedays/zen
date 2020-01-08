vec4 transformed_vertex()
{
    return mvp() * vec4(v_pos, 1);
}
