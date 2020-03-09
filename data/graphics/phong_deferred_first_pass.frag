in VertexOut
{
    vec3 pos_world;
    vec3 normal_world;
};

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec4 out_albedo;

void main()
{
    out_position = pos_world;
    out_normal = normalize(normal_world);
    out_albedo = vec4(1, 1, 1, 32);
}
