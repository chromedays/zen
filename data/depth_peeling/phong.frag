in VertexOut
{
    vec3 pos_world;
    vec3 normal_world;
    vec2 uv;
};

out vec4 out_color;

#define DIFFUSE_MAP u_samplers[0]
#define SPECULAR_MAP u_samplers[1]

void main()
{
    vec3 n = normalize(normal_world);

    //out_color = texture(SPECULAR_MAP, uv).rgb;
    vec3 color = vec3(0);

    for (int i = 0; i < u_phong_lights_count; i++)
    {
        PhongLight light = u_phong_lights[i];
        vec3 ka = vec3(0.1);
        //vec3 kd = texture(DIFFUSE_MAP, uv).rgb;
        //vec3 ks = texture(SPECULAR_MAP, uv).rgb;
        vec3 kd = u_color;
        vec3 ks = u_color;
        float ns = 32;
        if (light.type == 1)
        {
            color += phong_directional_light_color(
                light, n, u_view_pos, pos_world,
                ka, kd, ks, ns);
        }
        else if (light.type == 2)
        {
            color += phong_point_light_color(
                light, n, u_view_pos, pos_world,
                ka, kd, ks, ns);
        }
    }

    out_color = vec4(color, 1);
}
