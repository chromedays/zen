struct PhongLight
{
    int type;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 pos_or_dir;
    float inner_angle;
    float outer_angle;
    float falloff;
    float linear;
    float quadratic;
};

#define MAX_PHONG_LIGHTS_COUNT 100

layout (binding = 0, std140) uniform PerFrame
{
    mat4 u_view;
    mat4 u_proj;
    PhongLight u_phong_lights[MAX_PHONG_LIGHTS_COUNT];
    int u_phong_lights_count;
    vec3 u_view_pos;
    float t;// TODO: Rename to u_t
};

layout (binding = 1, std140) uniform PerObject
{
    mat4 u_model;
    vec3 u_color;
};

#define MAX_SAMPLERS_COUNT 16
layout (binding = 0) uniform sampler2D u_samplers[MAX_SAMPLERS_COUNT];

vec4 model_to_world_point(vec3 p)
{
    return u_model * vec4(p, 1);
}

vec3 model_to_world_vector(vec3 v)
{
    return mat3(transpose(inverse(u_model))) * v;
}

vec3 model_to_view_vector(vec3 v)
{
    return mat3(transpose(inverse(u_view * u_model))) * v;
}

mat4 mvp()
{
    return u_proj * u_view * u_model;
}

vec3 unlit_color()
{
    return u_color;
}

float phong_attenutation(PhongLight light, vec3 pos)
{
    float distance = length(light.pos_or_dir - pos);
    float attenuation =
        1.0 / (1.0 +
               light.linear * distance +
               light.quadratic * distance * distance);
    return attenuation;
}

// TODO: Needs more tests
vec3 phong_directional_light_color(PhongLight light,
                                   vec3 normal,
                                   vec3 view_pos, vec3 frag_pos,
                                   vec3 ka, vec3 kd, vec3 ks,
                                   float ns)
{
    vec3 l = -normalize(light.pos_or_dir);
    vec3 n = normalize(normal);
    vec3 ambient = light.ambient * ka;
    vec3 diffuse = light.diffuse * kd * max(dot(n, l), 0);

    vec3 v = normalize(view_pos - frag_pos);
    vec3 r = reflect(-l, n);
    float spec = pow(max(dot(v, r), 0), ns);
    vec3 specular = light.specular * ks * spec;

    return ambient + diffuse + specular;
}

vec3 phong_point_light_color(PhongLight light,
                             vec3 normal, vec3 view_pos, vec3 frag_pos,
                             vec3 ka, vec3 kd, vec3 ks, float ns)
{
    vec3 l = normalize(light.pos_or_dir - frag_pos);
    vec3 n = normalize(normal);
    vec3 ambient = light.ambient * ka;
    vec3 diffuse = light.diffuse * kd * max(dot(n, l), 0);

    vec3 v = normalize(view_pos - frag_pos);
    vec3 r = reflect(-l, n);
    float spec = pow(max(dot(v, r), 0), ns);
    vec3 specular = light.specular * ks * spec;

    float att = phong_attenutation(light, frag_pos);

    return (ambient + diffuse + specular) * att;
}

vec3 calc_phong(vec3 normal, vec3 frag_pos)
{
    vec3 n = normalize(normal);

    vec3 color = vec3(0);

    for (int i = 0; i < u_phong_lights_count; i++)
    {
        PhongLight light = u_phong_lights[i];
        vec3 ka = vec3(0);
        vec3 kd = vec3(0.9, 0.9, 0.9);
        vec3 ks = vec3(0.9, 0.9, 0.9);
        //vec3 kd = texture(DIFFUSE_MAP, uv).rgb;
        //vec3 ks = texture(SPECULAR_MAP, uv).rgb;
        float ns = 32;
        if (light.type == 1)
        {
            color += phong_directional_light_color(
                light, n, u_view_pos, frag_pos,
                ka, kd, ks, ns);
        }
        else if (light.type == 2)
        {
            color += phong_point_light_color(
                light, n, u_view_pos, frag_pos,
                ka, kd, ks, ns);
        }
    }

    return color;
}