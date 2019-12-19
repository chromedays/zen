in VertexOut
{
    vec2 uv;
    vec3 color;
};

out vec3 out_color;

#define NORMAL_MAP u_samplers[0]

void main()
{
    vec3 l = normalize(vec3(cos(t), 1, sin(t)));
    vec3 n = texture(NORMAL_MAP, uv).xyz - vec3(0.5);
    out_color = vec3(1, 1, 1) * max(dot(l, n), 0);
}