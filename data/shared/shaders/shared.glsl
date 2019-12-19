#version 460 core

layout (location = 0) in vec3 v_pos;
layout (location = 1) in vec2 v_uv;
layout (location = 2) in vec3 v_normal;

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

#define MAX_PHONG_LIGHTS_COUNT 10

layout (binding = 0, std140) uniform PerFrame
{
    mat4 u_view;
    mat4 u_proj;
    PhongLight u_phong_lights[MAX_PHONG_LIGHTS_COUNT];
    int u_phong_lights_count;
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

mat4 mvp()
{
    return u_proj * u_view * u_model;
}

vec3 unlit_color()
{
    return u_color;
}

vec4 transformed_vertex()
{
    return mvp() * vec4(v_pos, 1);
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

vec3 phong_directional_light_color(PhongLight light,
                                   vec3 normal,
                                   vec3 view_pos, vec3 frag_pos,
                                   vec3 ka, vec3 kd, vec3 ks,
                                   float ns)
{
    vec3 l = -light.pos_or_dir;
    vec3 n = normal;
    vec3 ambient = light.ambient * ka;
    vec3 diffuse = light.diffuse * kd * max(dot(n, l), 0);

    vec3 v = normalize(view_pos - frag_pos);
    vec3 r = reflect(-l, n);
    float spec = pow(max(dot(v, r), 0), ns);
    vec3 specular = light.specular * ks * spec;

    return ambient + diffuse + specular;
}