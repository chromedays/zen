#include "graphics_bv.h"
#include "../../all.h"
#include <stdlib.h>
#include <math.h>

#define MAX_MODELS_COUNT 100
#define MAX_LIGHT_SOURCES_COUNT 100

typedef struct LightSource_
{
    FVec3 pos;
    FVec3 color;
} LightSource;

typedef struct GBuffer_
{
    IVec2 dim;
    uint framebuffer;
    uint position_texture;
    uint normal_texture;
    uint albedo_texture;
    uint depth_stencil_buffer;
} GBuffer;

typedef enum DrawMode_
{
    DrawMode_FinalScene = 0,
    DrawMode_PositionMap,
    DrawMode_NormalMap,
    DrawMode_AlbedoMap,
} DrawMode;

typedef struct transform
{
    FVec3 pos;
    FVec3 scale;
    FVec3 rot;
} transform_t;

typedef struct scene_object
{
    Mesh* mesh;
    VertexBuffer* vb;
    struct transform transform;
} scene_object_t;

static void create_point_cloud(struct scene_object* objects,
                               int objects_count,
                               float** out_points,
                               int* out_points_count)
{
    *out_points_count = 0;
    for (int i = 0; i < objects_count; i++)
    {
        struct scene_object* o = &objects[i];
        *out_points_count += o->mesh->vertices_count;
    }

    *out_points = (float*)malloc(*out_points_count * sizeof(float[3]));

    float* p = *out_points;
    for (int i = 0; i < objects_count; i++)
    {
        struct scene_object* o = &objects[i];
        for (int j = 0; j < o->mesh->vertices_count; j++)
        {
            FVec3 tp = o->mesh->vertices[j].pos;
            // TODO: Support rotation
            tp = fvec3_mul(tp, o->transform.scale);
            tp = fvec3_add(tp, o->transform.pos);
            float3_copy(p, (float*)&tp);
            p += 3;
        }
    }
}

typedef enum node_type
{
    node_type_leaf,
    node_type_node,
} node_type_t;

typedef struct node
{
    struct bvolume bv;
    enum node_type type;

    float* points_base;
    int points_count;

    struct scene_object* scene_object;

    struct node* left;
    struct node* right;
} node_t;

static int compare_points(int* axis, const float* a, const float* b)
{
    if (a[*axis] < b[*axis])
        return -1;
    else
        return 1;
}

static int partition_points(float* points, int points_count, int axis)
{
    qsort_s(points, points_count, sizeof(float[3]), &compare_points, &axis);

    return points_count / 2;
}

static void tree_cleanup(struct node* tree)
{
    if (!tree)
        return;
    tree_cleanup(tree->left);
    tree_cleanup(tree->right);

    free(tree);
}

static void top_down_bv_tree_rec(struct node** tree,
                                 float* points,
                                 int points_count,
                                 struct bvolume* bv,
                                 int depth,
                                 enum bv_type type);
static void top_down_bv_tree(struct node** tree,
                             float* points,
                             int points_count,
                             enum bv_type type)
{
    struct bvolume bv;
    bv.type = type;
    switch (type)
    {
    case bv_type_aabb:
        bv.aabb = calc_aabb(points, points_count, 0, sizeof(float[3]));
        break;
    case bv_type_sphere:
        bv.sphere = calc_bsphere(points, points_count, 0, sizeof(float[3]));
        break;
    }
    top_down_bv_tree_rec(tree, points, points_count, &bv, 0, type);
}

static void top_down_bv_tree_rec(struct node** tree,
                                 float* points,
                                 int points_count,
                                 struct bvolume* bv,
                                 int depth,
                                 enum bv_type type)
{
    if (depth >= 10)
        return;

    ASSERT(points_count > 0);
    int min_points_count_per_leaf = 500;

    struct node* node = (struct node*)calloc(sizeof(*node), 1);
    *tree = node;

    node->bv = *bv;

    if (points_count <= min_points_count_per_leaf)
    {
        node->type = node_type_leaf;
        node->points_base = points;
        node->points_count = points_count;
    }
    else
    {
        node->type = node_type_node;

        float min_total_child_volume = FLT_MAX;
        int min_axis = -1;
        struct bvolume min_left_bv;
        struct bvolume min_right_bv;
        int min_k = -1;

        for (int i = 0; i < 3; i++)
        {
            int k = partition_points(points, points_count, i);
            struct bvolume left_bv;
            struct bvolume right_bv;
            left_bv.type = right_bv.type = type;
            float left_volume = 0;
            float right_volume = 0;
            switch (type)
            {
            case bv_type_aabb:
                left_bv.aabb = calc_aabb(points, k, 0, sizeof(float[3]));
                left_volume = aabb_volume(&left_bv.aabb);
                right_bv.aabb = calc_aabb(points + k * 3, points_count - k, 0,
                                          sizeof(float[3]));
                right_volume = aabb_volume(&right_bv.aabb);
                break;
            case bv_type_sphere:
                left_bv.sphere = calc_bsphere(points, k, 0, sizeof(float[3]));
                left_volume = bsphere_volume(&left_bv.sphere);
                right_bv.sphere = calc_bsphere(points + k * 3, points_count - k,
                                               0, sizeof(float[3]));
                right_volume = bsphere_volume(&right_bv.sphere);
                break;
            }
            float total_volume = left_volume + right_volume;

            if (min_total_child_volume > total_volume)
            {
                min_total_child_volume = total_volume;
                min_axis = i;
                min_left_bv = left_bv;
                min_right_bv = right_bv;
                min_k = k;
            }
        }

        ASSERT(min_k >= 0);

        qsort_s(points, points_count, sizeof(float[3]), &compare_points,
                &min_axis);

        top_down_bv_tree_rec(&node->left, points, min_k, &min_left_bv,
                             depth + 1, type);
        top_down_bv_tree_rec(&node->right, points + min_k * 3,
                             points_count - min_k, &min_right_bv, depth + 1,
                             type);
    }
}

static void find_nodes_to_merge(struct node** nodes,
                                int nodes_count,
                                int* out_a,
                                int* out_b)
{
    float min_d_sq = FLT_MAX;
    for (int i = 0; i < nodes_count; i++)
    {
        struct node* n0 = nodes[i];
        float c0[3];

        switch (n0->bv.type)
        {
        case bv_type_aabb: float3_copy(c0, n0->bv.aabb.c); break;
        case bv_type_sphere: float3_copy(c0, n0->bv.sphere.c); break;
        }

        for (int j = 0; j < nodes_count; j++)
        {
            if (i == j)
                continue;

            struct node* n1 = nodes[j];
            float c1[3];

            switch (n1->bv.type)
            {
            case bv_type_aabb: float3_copy(c1, n1->bv.aabb.c); break;
            case bv_type_sphere: float3_copy(c1, n1->bv.sphere.c); break;
            }

            float dv[3];
            float3_sub_r(dv, c0, c1);
            float d_sq = float3_length_sq(dv);
            if (min_d_sq > d_sq)
            {
                *out_a = i;
                *out_b = j;
                min_d_sq = d_sq;
            }
        }
    }
}
static void collect_scene_objects_rec(struct node* tree,
                                      int* count,
                                      struct scene_object* out_scene_objects)
{
    if (tree->type == node_type_leaf)
    {
        out_scene_objects[(*count)++] = *tree->scene_object;
    }
    else
    {
        collect_scene_objects_rec(tree->left, count, out_scene_objects);
        collect_scene_objects_rec(tree->right, count, out_scene_objects);
    }
}

static void collect_scene_objects(struct node* tree,
                                  struct scene_object* out_scene_objects,
                                  int* out_count)
{
    *out_count = 0;
    collect_scene_objects_rec(tree, out_count, out_scene_objects);
}

static struct node* bottom_up_bv_tree(struct scene_object* objects,
                                      int objects_count,
                                      enum bv_type type)
{
    ASSERT(objects_count > 0);

    struct node** nodes =
        (struct node**)calloc(objects_count * sizeof(*nodes), 1);
    for (int i = 0; i < objects_count; i++)
    {
        struct scene_object* o = &objects[i];
        struct node* l = nodes[i] = (struct node*)calloc(sizeof(*l), 1);
        l->type = node_type_leaf;
        l->scene_object = o;
        l->bv.type = type;
        float* points;
        int points_count;
        create_point_cloud(o, 1, &points, &points_count);
        switch (type)
        {
        case bv_type_aabb:
            l->bv.aabb = calc_aabb(points, points_count, 0, sizeof(float[3]));
            break;
        case bv_type_sphere:
            l->bv.sphere =
                calc_bsphere(points, points_count, 0, sizeof(float[3]));
            break;
        }
        free(points);
    }

    struct scene_object* subtree_objects =
        malloc(objects_count * sizeof(*subtree_objects));
    int subtree_objects_count = 0;

    while (objects_count > 1)
    {
        int a, b;
        find_nodes_to_merge(nodes, objects_count, &a, &b);
        struct node* pair = calloc(sizeof(*pair), 1);
        pair->type = node_type_node;
        pair->left = nodes[a];
        pair->right = nodes[b];

        collect_scene_objects(pair, subtree_objects, &subtree_objects_count);

        float* points;
        int points_count;
        create_point_cloud(subtree_objects, subtree_objects_count, &points,
                           &points_count);

        pair->bv.type = type;
        switch (type)
        {
        case bv_type_aabb:
            pair->bv.aabb =
                calc_aabb(points, points_count, 0, sizeof(float[3]));
            break;
        case bv_type_sphere:
            pair->bv.sphere =
                calc_bsphere(points, points_count, 0, sizeof(float[3]));
            break;
        }

        free(points);

        if (a > b)
        {
            int temp = a;
            a = b;
            b = temp;
        }

        nodes[a] = pair;
        nodes[b] = nodes[objects_count - 1];

        --objects_count;
    }

    struct node* root = nodes[0];

    free(subtree_objects);
    free(nodes);

    return root;
}

static void draw_bvh_rec(Example* e,
                         struct node* tree,
                         uint shader,
                         VertexBuffer* vb,
                         int depth,
                         int highlight_depth)
{
    if (!tree)
        return;

    FVec3 color = {0.5f, 0.5f, 1};

    if (depth == highlight_depth)
    {
        struct bvolume* bv = &tree->bv;

        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glUseProgram(shader);
        Mat4 trans_mat;
        Mat4 scale_mat;
        switch (bv->type)
        {
        case bv_type_aabb: {
            struct aabb* aabb = &bv->aabb;
            FVec3 c = {aabb->c[0], aabb->c[1], aabb->c[2]};
            trans_mat = mat4_translation(c);
            scale_mat = mat4_scalev(
                (FVec3){aabb->r[0] * 2, aabb->r[1] * 2, aabb->r[2] * 2});
            break;
        }
        case bv_type_sphere: {
            struct bsphere* bsphere = &bv->sphere;
            FVec3 c = {bsphere->c[0], bsphere->c[1], bsphere->c[2]};
            trans_mat = mat4_translation(c);
            scale_mat = mat4_scale(bsphere->r * 2);

            break;
        }
        }

        Mat4 model_mat = mat4_mul(&trans_mat, &scale_mat);
        ExamplePerObjectUBO per_object = {.model = model_mat, .color = color};
        e_apply_per_object_ubo(e, &per_object);
        r_vb_draw(vb);
    }

    draw_bvh_rec(e, tree->left, shader, vb, depth + 1, highlight_depth);
    draw_bvh_rec(e, tree->right, shader, vb, depth + 1, highlight_depth);
}

static void draw_bvh(Example* e,
                     struct node* tree,
                     uint shader,
                     VertexBuffer* vb,
                     int highlight_depth)
{
    draw_bvh_rec(e, tree, shader, vb, 0, highlight_depth);
}

typedef struct GraphicsScene_
{
    Path model_file_paths[MAX_MODELS_COUNT];
    Mesh model_meshes[MAX_MODELS_COUNT];
    VertexBuffer model_vbs[MAX_MODELS_COUNT];
    int models_count;

    uint model_shader;
    uint normal_debug_shader;

    struct scene_object scene_objects[100];
    int scene_objects_count;

    Mesh aabb_mesh;
    VertexBuffer aabb_vb;
    Mesh bsphere_mesh;
    VertexBuffer bsphere_vb;

    float* scene_points;
    int scene_points_count;
    struct node* bvh_aabb;
    struct node* bvh_sphere;
    int bvh_highlight_depth;
    int bvh_type;
    enum bv_type visible_bv_type;

    Mesh light_source_mesh;
    VertexBuffer light_source_vb;
    LightSource light_sources[MAX_LIGHT_SOURCES_COUNT];
    int light_sources_count;
    uint light_source_shader;
    float light_intensity;

    FVec3 orbit_pos;
    float orbit_speed_deg;
    float orbit_radius;

    ExampleFpsCamera cam;

    float t;

    Mesh fsq_mesh;
    VertexBuffer fsq_vb;
    uint fsq_shader;
    uint fsq_target_texture;

    GBuffer gbuffer;
    uint deferred_first_pass_shader;
    uint deferred_second_pass_shader;

    DrawMode draw_mode;

    bool copy_depth;
    IVec2 orbits_count;
} GraphicsScene;

static void reconstruct_bvh(GraphicsScene* s)
{
    free(s->scene_points);
    tree_cleanup(s->bvh_aabb);
    tree_cleanup(s->bvh_sphere);
    create_point_cloud(s->scene_objects, s->scene_objects_count,
                       &s->scene_points, &s->scene_points_count);
    if (s->bvh_type == 0)
    {
        s->bvh_aabb = bottom_up_bv_tree(s->scene_objects,
                                        s->scene_objects_count, bv_type_aabb);
        s->bvh_sphere = bottom_up_bv_tree(
            s->scene_objects, s->scene_objects_count, bv_type_sphere);
    }
    else
    {
        top_down_bv_tree(&s->bvh_aabb, s->scene_points, s->scene_points_count,
                         bv_type_aabb);
        top_down_bv_tree(&s->bvh_sphere, s->scene_points, s->scene_points_count,
                         bv_type_sphere);
    }
}

static void add_random_scene_object(GraphicsScene* s)
{
    struct scene_object* o = &s->scene_objects[s->scene_objects_count++];
    int model_index = rand() % s->models_count;
    o->mesh = &s->model_meshes[model_index];
    o->vb = &s->model_vbs[model_index];
    o->transform.scale = (FVec3){1, 1, 1};
    o->transform.pos.x = (rand() % 25 - 12) * 0.1f;
    o->transform.pos.y = (rand() % 25 - 12) * 0.1f;
    o->transform.pos.z = (rand() % 25 - 12) * 0.1f;

    reconstruct_bvh(s);
}

static FILE_FOREACH_FN_DECL(push_model)
{
    GraphicsScene* s = (GraphicsScene*)udata;
    ASSERT(s->models_count < MAX_MODELS_COUNT);
    s->model_file_paths[s->models_count] = fs_path_copy(*file_path);
    Mesh* mesh = &s->model_meshes[s->models_count];
    rc_mesh_load_from_obj(mesh,
                          s->model_file_paths[s->models_count].abs_path_str);
    // Need to compute normals manually
    if (fvec3_length_sq(mesh->vertices[0].normal) == 0.f)
        rc_mesh_set_approximate_normals(mesh);
    NormalizedTransform normalized_transform =
        rc_mesh_calc_normalized_transform(mesh);
    for (int i = 0; i < mesh->vertices_count; i++)
    {
        Vertex* v = &mesh->vertices[i];
        v->pos = fvec3_mulf(v->pos, normalized_transform.scale);
        v->pos = fvec3_add(v->pos, normalized_transform.pos);
    }

    VertexBuffer* vb = &s->model_vbs[s->models_count];
    r_vb_init(vb, mesh, GL_TRIANGLES);

    ++s->models_count;
}

#if 0
static void try_switch_model(GraphicsScene* s, int new_model_index)
{
    if (s->current_model_index != new_model_index)
    {
        ASSERT((new_model_index >= 0) && (new_model_index < s->models_count));

        scene_object_t scene_objects[1] = {{
            .mesh = &s->model_mesh,
            .vb = &s->model_vb,
            .transform =
                {
                    .pos = s->model_pos,
                    .scale = {s->model_scale, s->model_scale, s->model_scale},
                },
        }};
        free(s->scene_points);
        tree_cleanup(s->bvh_aabb);
        tree_cleanup(s->bvh_sphere);
        create_point_cloud(scene_objects, ARRAY_LENGTH(scene_objects),
                           &s->scene_points, &s->scene_points_count);
        top_down_bv_tree(&s->bvh_aabb, s->scene_points, s->scene_points_count,
                         bv_type_aabb);
        top_down_bv_tree(&s->bvh_sphere, s->scene_points, s->scene_points_count,
                         bv_type_sphere);

        s->current_model_index = new_model_index;
    }
}
#endif

static float get_channel_function1(float x, float period)
{
    float coeff = (HIMATH_PI * 2.f) / period;
    return (0.5f * cosf(coeff * x) + 0.5f);
}

static float get_channel_function2(float x, float period)
{
    float coeff = (HIMATH_PI * 2.f) / period;
    return (0.5f * cosf(coeff * (x + period * 0.5f)) + 0.5f);
}

static void update_light_colors(GraphicsScene* s)
{
    // Smoothly interpolate from red to green to blue
    for (int i = 0; i < s->light_sources_count; i++)
    {
        float period = (float)(s->light_sources_count - 1) * 0.666666f;

        float rx1 = HIMATH_CLAMP((float)i, 0, period * 0.5f);
        float rx2 = HIMATH_CLAMP((float)i, period, period * 1.5f);
        float gx = HIMATH_CLAMP((float)i, 0, period);
        float bx = HIMATH_CLAMP((float)i, period * 0.5f, period * 1.5f);

        float r = get_channel_function1(rx1, period) +
                  get_channel_function2(rx2, period);
        float g = get_channel_function2(gx, period);
        float b = get_channel_function1(bx, period);

        s->light_sources[i].color.x = r * s->light_intensity;
        s->light_sources[i].color.y = g * s->light_intensity;
        s->light_sources[i].color.z = b * s->light_intensity;
    }
}

EXAMPLE_INIT_FN_SIG(graphics)
{
    Example* e = e_example_make("graphics", GraphicsScene);
    GraphicsScene* s = (GraphicsScene*)e->scene;

    Path model_root_path = fs_path_make_working_dir();
    fs_path_append2(&model_root_path, "shared", "models");
    fs_for_each_files_with_ext(model_root_path, "obj", &push_model, s);
    fs_path_cleanup(&model_root_path);

    s->model_shader = e_shader_load(e, "phong");
    s->normal_debug_shader = e_shader_load(e, "visualize_normals");

    add_random_scene_object(s);
    s->scene_objects[0].mesh = &s->model_meshes[0];
    s->scene_objects[0].vb = &s->model_vbs[0];
    s->scene_objects[0].transform.scale.x = 1;
    s->scene_objects[0].transform.scale.y = 1;
    s->scene_objects[0].transform.scale.z = 1;
    s->scene_objects_count = 1;

    s->aabb_mesh = rc_mesh_make_cube();
    r_vb_init(&s->aabb_vb, &s->aabb_mesh, GL_TRIANGLES);
    s->bsphere_mesh = rc_mesh_make_sphere(0.5f, 32, 32);
    r_vb_init(&s->bsphere_vb, &s->bsphere_mesh, GL_TRIANGLES);

    reconstruct_bvh(s);

    s->light_source_mesh = rc_mesh_make_sphere(0.05f, 32, 32);
    r_vb_init(&s->light_source_vb, &s->light_source_mesh, GL_TRIANGLES);
    s->light_sources_count = 8;

    s->light_intensity = 0.4f;

    update_light_colors(s);

    s->light_source_shader = e_shader_load(e, "light_source");

    s->orbit_speed_deg = 30;
    s->orbit_radius = 1;

    s->cam.pos.z = 2;

    Vertex fsq_vertices[4] = {
        {{-1, -1, 0}, {0, 0}},
        {{1, -1, 0}, {1, 0}},
        {{1, 1, 0}, {1, 1}},
        {{-1, 1, 0}, {0, 1}},
    };
    uint fsq_indices[6] = {0, 1, 2, 2, 3, 0};
    s->fsq_mesh =
        rc_mesh_make_raw2(ARRAY_LENGTH(fsq_vertices), ARRAY_LENGTH(fsq_indices),
                          fsq_vertices, fsq_indices);
    r_vb_init(&s->fsq_vb, &s->fsq_mesh, GL_TRIANGLES);
    s->fsq_shader = e_shader_load(e, "fsq");

    s->gbuffer.dim = input->window_size;
    glGenFramebuffers(1, &s->gbuffer.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, s->gbuffer.framebuffer);

    glGenTextures(1, &s->gbuffer.position_texture);
    glBindTexture(GL_TEXTURE_2D, s->gbuffer.position_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, s->gbuffer.dim.x,
                 s->gbuffer.dim.y, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           s->gbuffer.position_texture, 0);

    glGenTextures(1, &s->gbuffer.normal_texture);
    glBindTexture(GL_TEXTURE_2D, s->gbuffer.normal_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, s->gbuffer.dim.x,
                 s->gbuffer.dim.y, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                           s->gbuffer.normal_texture, 0);

    glGenTextures(1, &s->gbuffer.albedo_texture);
    glBindTexture(GL_TEXTURE_2D, s->gbuffer.albedo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, s->gbuffer.dim.x,
                 s->gbuffer.dim.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
                           s->gbuffer.albedo_texture, 0);

    uint attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                          GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    glGenRenderbuffers(1, &s->gbuffer.depth_stencil_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, s->gbuffer.depth_stencil_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          s->gbuffer.dim.x, s->gbuffer.dim.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, s->gbuffer.depth_stencil_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    s->fsq_target_texture = s->gbuffer.position_texture;

    s->deferred_first_pass_shader =
        e_shader_load(e, "phong_deferred_first_pass");

    s->deferred_second_pass_shader =
        e_shader_load(e, "phong_deferred_second_pass");

    s->copy_depth = true;
    s->orbits_count.x = 1;
    s->orbits_count.y = 1;

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(graphics)
{
    Example* e = (Example*)udata;
    GraphicsScene* s = (GraphicsScene*)e->scene;

    glDeleteProgram(s->deferred_second_pass_shader);
    glDeleteProgram(s->deferred_first_pass_shader);
    glDeleteFramebuffers(1, &s->gbuffer.framebuffer);
    glDeleteRenderbuffers(1, &s->gbuffer.depth_stencil_buffer);
    glDeleteTextures(1, &s->gbuffer.albedo_texture);
    glDeleteTextures(1, &s->gbuffer.normal_texture);
    glDeleteTextures(1, &s->gbuffer.position_texture);

    glDeleteProgram(s->fsq_shader);
    r_vb_cleanup(&s->fsq_vb);
    rc_mesh_cleanup(&s->fsq_mesh);

    glDeleteProgram(s->light_source_shader);
    r_vb_cleanup(&s->light_source_vb);
    rc_mesh_cleanup(&s->light_source_mesh);

    free(s->scene_points);
    tree_cleanup(s->bvh_aabb);
    tree_cleanup(s->bvh_sphere);

    glDeleteProgram(s->model_shader);
    glDeleteProgram(s->normal_debug_shader);

    for (int i = 0; i < s->models_count; i++)
    {
        r_vb_cleanup(&s->model_vbs[i]);
        rc_mesh_cleanup(&s->model_meshes[i]);
        fs_path_cleanup(&s->model_file_paths[i]);
    }
    free(e);
}

static void update_light_source_transforms(GraphicsScene* s)
{
    for (int i = 0; i < s->light_sources_count; i++)
    {
        float deg = ((360.f / (float)s->light_sources_count) * (float)i +
                     s->t * s->orbit_speed_deg);
        float rad = degtorad(deg);
        FVec3 pos = {
            .x = cosf(rad) * s->orbit_radius,
            .z = sinf(rad) * s->orbit_radius,
        };
        s->light_sources[i].pos = pos;
    }
}

void prepare_per_frame(Example* e, const GraphicsScene* s, const Input* input)
{
    ExamplePerFrameUBO per_frame = {0};
    per_frame.phong_lights_count = s->light_sources_count + 1;
    for (int i = 0; i < s->light_sources_count; i++)
    {
        FVec3 color = s->light_sources[i].color;
        per_frame.phong_lights[i].type = ExamplePhongLightType_Point;
        per_frame.phong_lights[i].pos_or_dir = s->light_sources[i].pos;
        per_frame.phong_lights[i].ambient = fvec3_mulf(color, 0.1f);
        per_frame.phong_lights[i].diffuse = color;
        per_frame.phong_lights[i].specular = color;
        per_frame.phong_lights[i].linear = 0.09f;
        per_frame.phong_lights[i].quadratic = 0.032f;
    }
    ExamplePhongLight* dir_light =
        &per_frame.phong_lights[s->light_sources_count];
    dir_light->type = ExamplePhongLightType_Directional;
    dir_light->pos_or_dir = (FVec3){1, -1, -1};
    dir_light->ambient = (FVec3){0.3f, 0.3f, 0.3f};
    dir_light->diffuse = (FVec3){0.8f, 0.8f, 0.8f};
    dir_light->specular = (FVec3){0.8f, 0.8f, 0.8f};

    per_frame.proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
    per_frame.view = mat4_lookat(
        s->cam.pos, fvec3_add(s->cam.pos, e_fpscam_get_look(&s->cam)),
        (FVec3){0, 1, 0});
    per_frame.view_pos = s->cam.pos;
    e_apply_per_frame_ubo(e, &per_frame);
}

static void draw_deferred_objects(Example* e, GraphicsScene* s)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // First pass
    glBindFramebuffer(GL_FRAMEBUFFER, s->gbuffer.framebuffer);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glUseProgram(s->deferred_first_pass_shader);
    for (int i = 0; i < s->scene_objects_count; i++)
    {
        struct scene_object* o = &s->scene_objects[i];
        struct transform* t = &o->transform;
        ExamplePerObjectUBO per_object = {0};
        Mat4 trans_mat = mat4_translation(t->pos);
        Mat4 scale_mat = mat4_scalev(t->scale);
        per_object.model = mat4_mul(&trans_mat, &scale_mat);
        e_apply_per_object_ubo(e, &per_object);
        r_vb_draw(o->vb);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Second pass
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    switch (s->draw_mode)
    {
    case DrawMode_FinalScene: {
        uint textures[] = {
            s->gbuffer.position_texture,
            s->gbuffer.normal_texture,
            s->gbuffer.albedo_texture,
        };
        glBindTextures(0, ARRAY_LENGTH(textures), textures);
        glUseProgram(s->deferred_second_pass_shader);
        r_vb_draw(&s->fsq_vb);
    }
    break;
    case DrawMode_PositionMap:
        glBindTextures(0, 1, &s->gbuffer.position_texture);
        glUseProgram(s->fsq_shader);
        r_vb_draw(&s->fsq_vb);
        break;
    case DrawMode_NormalMap:
        glBindTextures(0, 1, &s->gbuffer.normal_texture);
        glUseProgram(s->fsq_shader);
        r_vb_draw(&s->fsq_vb);
        break;
    case DrawMode_AlbedoMap:
        glBindTextures(0, 1, &s->gbuffer.albedo_texture);
        glUseProgram(s->fsq_shader);
        r_vb_draw(&s->fsq_vb);
        break;
    }

    glClear(GL_DEPTH_BUFFER_BIT);
}

static void copy_depth_buffer(const GraphicsScene* s, IVec2 window_size)
{
    // Copy depth buffer written from deferred rendering pass
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s->gbuffer.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, s->gbuffer.dim.x, s->gbuffer.dim.y, 0, 0,
                      window_size.x, window_size.y, GL_DEPTH_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

static void draw_debug_objects(Example* e, GraphicsScene* s)
{
    // Draw light sources
    for (int i = 0; i < s->light_sources_count; i++)
    {
        Mat4 trans_mat = mat4_translation(s->light_sources[i].pos);
        ExamplePerObjectUBO per_object = {
            .model = trans_mat,
            .color = s->light_sources[i].color,
        };
        e_apply_per_object_ubo(e, &per_object);
        glUseProgram(s->light_source_shader);
        r_vb_draw(&s->light_source_vb);
    }

    switch (s->visible_bv_type)
    {
    case bv_type_aabb:
        draw_bvh(e, s->bvh_aabb, s->light_source_shader, &s->aabb_vb,
                 s->bvh_highlight_depth);
        break;
    case bv_type_sphere:
        draw_bvh(e, s->bvh_sphere, s->light_source_shader, &s->bsphere_vb,
                 s->bvh_highlight_depth);
        break;
    }
}

EXAMPLE_UPDATE_FN_SIG(graphics)
{
    Example* e = (Example*)udata;
    GraphicsScene* s = (GraphicsScene*)e->scene;

    bool status = false;
    igSetNextWindowSize((ImVec2){400, (float)input->window_size.y},
                        ImGuiCond_Once);
    igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_Once, (ImVec2){0, 0});
    if (igBegin("Control Panel", &status,
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize))
    {
#if 0
        if (igCollapsingHeader("Select Model", 0))
        {
            for (int i = 0; i < s->model_file_paths_count; i++)
            {
                if (igMenuItemBool(s->model_file_paths[i].filename, NULL, false,
                                   true))
                {
                    try_switch_model(s, i);
                }
            }
        }
#endif

        if (igCollapsingHeader("View Mode", 0))
        {
            if (igMenuItemBool("Final Scene", NULL, false, true))
            {
                s->draw_mode = DrawMode_FinalScene;
            }
            if (igMenuItemBool("Position Map", NULL, false, true))
            {
                s->draw_mode = DrawMode_PositionMap;
                s->fsq_target_texture = s->gbuffer.position_texture;
            }
            if (igMenuItemBool("Normal Map", NULL, false, true))
            {
                s->draw_mode = DrawMode_NormalMap;
                s->fsq_target_texture = s->gbuffer.normal_texture;
            }
            if (igMenuItemBool("Albedo Map", NULL, false, true))
            {
                s->draw_mode = DrawMode_AlbedoMap;
                s->fsq_target_texture = s->gbuffer.albedo_texture;
            }
        }

        if (igCollapsingHeader("Scene Setup", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (igButton("Add Random Object", (ImVec2){0}))
            {
                add_random_scene_object(s);
            }
        }
        if (igCollapsingHeader("BVH", ImGuiTreeNodeFlags_DefaultOpen))
        {
            igSliderInt("Highlight Depth", &s->bvh_highlight_depth, 0, 10,
                        "%d");
            igText("BV Type (0: AABB, 1: Sphere)");
            igSliderInt("##BV type", (int*)&s->visible_bv_type, 0,
                        bv_type_count - 1, "%d");
            igText("BVH Type (0: bottom-up, 1: top-down)");
            int new_bvh_type = s->bvh_type;
            igSliderInt("##BVH type", &new_bvh_type, 0, 1, "%d");
            if (new_bvh_type != s->bvh_type)
            {
                s->bvh_type = new_bvh_type;
                reconstruct_bvh(s);
            }
        }

        if (igCollapsingHeader("Misc", 0))
        {
            if (igCheckbox("Copy Depth", &s->copy_depth))
            {
            }

            igSliderFloat("Light intensity", &s->light_intensity, 0.4f, 1,
                          "%.3f", 1);
            igSliderInt("Light sources count", &s->light_sources_count, 8, 100,
                        "%d");
            igSliderFloat("Orbit radius", &s->orbit_radius, 1, 100, "%.3f", 1);
        }
    }
    igEnd();

    e_fpscam_update(&s->cam, input, 5);

    update_light_colors(s);

    s->t += input->dt;

    update_light_source_transforms(s);
    prepare_per_frame(e, s, input);
    draw_deferred_objects(e, s);
    if (s->copy_depth)
        copy_depth_buffer(s, input->window_size);
    draw_debug_objects(e, s);
}

#define USER_INIT                                                              \
    Scene scene = {0};                                                         \
    s_init(&scene, &input);                                                    \
    s_switch_scene(&scene, EXAMPLE_LITERAL(graphics));

#define USER_UPDATE s_update(&scene);

#define USER_CLEANUP s_cleanup(&scene);

//#include "../../win32_main.inl"
