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
    //out_color = texture(SPECULAR_MAP, uv).rgb;
    /*for (int i = 0; i < u_phong_lights_count; i++)
    {
        PhongLight light = u_phong_lights[i];
        phong_directional_light_color(light, )
    }*/
    out_color = normal_world;
    //out_color = vec3(0, 0.5, 0);
}
