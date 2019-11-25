#version 430 core

struct Light
{
    vec3 ia;
    vec3 id;
    vec3 is;
    vec3 pos;
    vec3 dir;
    float inner_angle;
    float outer_angle;
    float falloff;
    float ns; // Should've belonged to material
    float linear;
    float quadratic;
};

layout(std140) uniform PerFrame
{
    mat4 view;
    mat4 proj;
    vec3 global_ambient;
    vec3 fog;
    float z_near;
    float z_far;
    int directional_lights_count;
    int point_lights_count;
    int spotlights_count;
    Light directional_lights[16];
    Light point_lights[16];
    Light spotlights[16];
    int use_gpu_uv;
    int uv_mapping_type;
    int uv_entity_type;
} u_per_frame;

layout(std140) uniform PerObject
{
    mat4 model;
    vec3 ambient;
    float ns;
} u_per_object;

uniform sampler2D u_tex0;
uniform sampler2D u_tex1;

mat4 get_mv()
{
    return u_per_frame.view * u_per_object.model;
}

mat4 get_mvp()
{
    return u_per_frame.proj * u_per_frame.view * u_per_object.model;
}

vec3 get_reflection(vec3 l, vec3 n)
{
    return 2.0 * dot(l, n) * n - l;
}

float get_attenuation(Light light, vec3 pos)
{
    float distance = length(light.pos - pos);
    float attenuation = 1.0 / (1.0 + light.linear * distance + light.quadratic * (distance * distance));   
    return attenuation;
}

vec3 calc_directional_light(Light light, vec3 normal, vec3 frag_pos, vec3 ka, vec3 kd, vec3 ks, float ns)
{
    vec3 l = -light.dir;
    vec3 n = normal;
    vec3 ambient = light.ia * ka;
    vec3 diffuse = light.id * kd * max(dot(n, l), 0);

    vec3 view_dir = normalize(-frag_pos);
    vec3 r = get_reflection(l, n);
    float spec = pow(max(dot(view_dir, r), 0.0), ns);
    vec3 specular = light.is * ks * spec;

    vec3 color = ambient + diffuse + specular;
    return color;
}

vec3 calc_point_light(Light light, vec3 normal, vec3 frag_pos, vec3 ka, vec3 kd, vec3 ks, float ns)
{
    float attenuation = get_attenuation(light, frag_pos);

    vec3 l = normalize(light.pos - frag_pos);
    vec3 n = normal;
    vec3 ambient = light.ia * ka;
    vec3 diffuse = light.id * kd * max(dot(n, l), 0.0);

    vec3 view_dir = normalize(-frag_pos);
    vec3 r = get_reflection(l, n);
    float spec = pow(max(dot(view_dir, r), 0.0), ns);
    vec3 specular = light.is * ks * spec;

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    vec3 color = ambient + diffuse + specular;
    return color;
}

vec3 calc_spotlight(Light light, vec3 normal, vec3 frag_pos, vec3 ka, vec3 kd, vec3 ks, float ns)
{
    vec3 l = -light.dir;
    vec3 d = normalize(frag_pos - light.pos);
    vec3 n = normal;

    float attenuation = get_attenuation(light, frag_pos);
    vec3 color = vec3(0, 0, 0);

    float spotlight_effect = 0;

    float frag_angle = dot(light.dir, d);
    if (frag_angle >= cos(light.outer_angle))
    {
        if (frag_angle >= cos(light.inner_angle))
        {
            spotlight_effect = 1;
        }
        else
        {
            float falloff = max(light.falloff, 1.0);
            spotlight_effect = pow((frag_angle - cos(light.outer_angle)) / (cos(light.inner_angle) - cos(light.outer_angle)), falloff);
        }
    }

    vec3 ambient = light.ia * ka;
    vec3 diffuse = light.id * kd * max(dot(n, l), 0.0);

    vec3 view_dir = normalize(-frag_pos);
    vec3 r = get_reflection(l, n);
    float spec = pow(max(dot(view_dir, r), 0.0), ns);
    vec3 specular = light.is * ks * spec;

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    color = ambient + spotlight_effect * (diffuse + specular);

    return color;
}

vec3 calc_directional_light_blinn(Light light, vec3 normal, vec3 frag_pos, vec3 ka, vec3 kd, vec3 ks, float ns)
{
    vec3 l = -light.dir;
    vec3 n = normal;
    vec3 ambient = light.ia * ka;
    vec3 diffuse = light.id * kd * max(dot(n, l), 0);

    vec3 view_dir = normalize(-frag_pos);
    vec3 h = normalize(l + view_dir);
    float spec = pow(max(dot(n, h), 0.0), ns);
    vec3 specular = light.is * ks * spec;

    vec3 color = ambient + diffuse + specular;
    return color;
}

vec3 calc_point_light_blinn(Light light, vec3 normal, vec3 frag_pos, vec3 ka, vec3 kd, vec3 ks, float ns)
{
    float attenuation = get_attenuation(light, frag_pos);

    vec3 l = normalize(light.pos - frag_pos);
    vec3 n = normal;
    vec3 ambient = light.ia * ka;
    vec3 diffuse = light.id * kd * max(dot(n, l), 0.0);

    vec3 view_dir = normalize(-frag_pos);
    vec3 h = normalize(l + view_dir);
    float spec = pow(max(dot(n, h), 0.0), ns);
    vec3 specular = light.is * ks * spec;

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    vec3 color = ambient + diffuse + specular;
    return color;
}

vec3 calc_spotlight_blinn(Light light, vec3 normal, vec3 frag_pos, vec3 ka, vec3 kd, vec3 ks, float ns)
{
    vec3 l = -light.dir;
    vec3 d = normalize(frag_pos - light.pos);
    vec3 n = normal;

    float attenuation = get_attenuation(light, frag_pos);
    vec3 color = vec3(0, 0, 0);

    float spotlight_effect = 0;

    float frag_angle = dot(light.dir, d);
    if (frag_angle >= cos(light.outer_angle))
    {
        if (frag_angle >= cos(light.inner_angle))
        {
            spotlight_effect = 1;
        }
        else
        {
            float falloff = max(light.falloff, 1.0);
            spotlight_effect = pow((frag_angle - cos(light.outer_angle)) / (cos(light.inner_angle) - cos(light.outer_angle)), falloff);
        }
    }

    vec3 ambient = light.ia * ka;
    vec3 diffuse = light.id * kd * max(dot(n, l), 0.0);

    vec3 view_dir = normalize(-frag_pos);
    vec3 h = normalize(l + view_dir);
    float spec = pow(max(dot(n, h), 0.0), ns);
    vec3 specular = light.is * ks * spec;

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    color = ambient + spotlight_effect * (diffuse + specular);

    return color;
}

#define M_PI 3.141596

vec2 calc_uv(vec3 entity)
{
    vec2 uv = vec2(0, 0);
    if (u_per_frame.uv_mapping_type == 0)
    {
        float angle = atan(entity.z, entity.x);
        if (angle < 0)
            angle += 2 * M_PI;
        uv = vec2(angle / (2 * M_PI), (entity.y + 1) * 0.5);
    }
    else if (u_per_frame.uv_mapping_type == 1)
    {
        float theta = atan(entity.z, entity.x);
        if (theta < 0)
            theta += 2 * M_PI;
        float r = length(entity);
        float phi = acos(entity.y / r);
        uv = vec2(theta / (2 * M_PI), (M_PI - phi) / M_PI);
    }
    else if (u_per_frame.uv_mapping_type == 2)
    {
        vec3 abs_pos = abs(entity);
        if (abs_pos.x > abs_pos.y && abs_pos.x > abs_pos.z)
        {
            if (entity.x > 0)
                uv.x = -entity.z;
            else
                uv.x = entity.z;
            uv.y = entity.y;
        }
        else if (abs_pos.y > abs_pos.x && abs_pos.y > abs_pos.z)
        {
            uv.x = -entity.x;
            if (entity.y > 0)
                uv.y = entity.z;
            else
                uv.y = -entity.z;
        }
        else
        {
            if (entity.z > 0)
                uv.x = entity.x;
            else
                uv.x = -entity.x;
            uv.y = entity.y;
        }

        uv = (uv + vec2(1.0)) * 0.5;
    }

    return uv;
}