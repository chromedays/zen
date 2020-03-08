in VertexOut
{
    vec3 pos_world;
    vec3 normal_world;
    vec2 uv;
};

out vec3 out_color;

#define DIFFUSE_MAP u_samplers[0]
#define SPECULAR_MAP u_samplers[1]

void main()
{
    vec3 color = calc_phong(normal_world, pos_world);
    out_color = color;
    // out_color = (normalize(normal_world) + vec3(1)) * 0.5;
}
