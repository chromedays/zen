
#define POSITION_MAP u_samplers[0]
#define NORMAL_MAP u_samplers[1]
#define COLOR_MAP u_samplers[2]

in VertexOut
{
    vec2 uv;
};

out vec4 out_color;

void main()
{
    vec3 pos = texture(POSITION_MAP, uv).xyz;
    vec3 normal = texture(NORMAL_MAP, uv).xyz;
    vec3 color = texture(COLOR_MAP, uv).xyz;

    out_color = vec4(calc_phong(normal, pos), 1);
    //out_color = vec4(normal, 1);
}
