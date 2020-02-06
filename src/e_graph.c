#include "example.h"
#include "resource.h"
#include "app.h"
#include <himath.h>
#include <glad/glad.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>

#define MAX_DEGREE 20
#define CURVE_POINTS_COUNT 100
#define POINT_RADIUS 10.f

#if 0
    Plotter p;

    begin(&p);
    {
        set_axis_attribs(&(AxisAttribs){
            .x = { .name = "X", .scale = 10, },
            .y = { .name = "Y", .scale = 10, },
        });
        lines(xs, ys, &(PlotAttribs){.color = 0xFFFFFFFF});
        points(control_xs, control_ys, &(PlotAttribs){.color = 0xFF00FFFF});
        enable_grid(true);
    }
    end(&p);
#endif

typedef enum PlotType_
{
    PlotType_Lines,
    PlotType_Points,
} PlotType;

typedef struct PointsBuffer_
{
    struct PointsBuffer_* next;

    PlotType type;
    FVec4 color;
    float thickness;
    FVec3* points;
    int points_count;
} PointsBuffer;

typedef struct Axis_
{
    const char* name;
    float range_min;
    float range_max;
} Axis;

typedef struct Plotter_
{
    PointsBuffer* buffers;
    Axis axes[2];
    IVec2 canvas_size;
    bool should_draw_grid;
} Plotter;

typedef struct AxisAttribs_
{
    struct
    {
        const char* name;
        float range_min;
        float range_max;
    } attribs[2];
} AxisAttribs;

static void plt_set_axis_attribs(Plotter* p, const AxisAttribs* attribs)
{
    for (int axis = 0; axis < 2; axis++)
    {
        p->axes[axis].name = attribs->attribs[axis].name;
        p->axes[axis].range_min = attribs->attribs[axis].range_min;
        p->axes[axis].range_max = attribs->attribs[axis].range_max;
    }
}

typedef struct PlotAttribs_
{
    FVec4 color;
    float thickness;
} PlotAttribs;

static void plt_append_points_buffer(Plotter* p,
                                     PlotType type,
                                     const FVec3* points,
                                     int points_count,
                                     const PlotAttribs* attribs)
{
    uint8_t* buf =
        (uint8_t*)malloc(sizeof(PointsBuffer) + points_count * sizeof(FVec3));
    PointsBuffer* header = (PointsBuffer*)buf;
    header->next = p->buffers;
    header->type = type;
    header->color = attribs->color;
    header->thickness = attribs->thickness;
    header->points = (FVec3*)(buf + sizeof(PointsBuffer));
    header->points_count = points_count;
    memcpy(header->points, points, points_count * sizeof(FVec3));
    p->buffers = header;
}

static void plt_points(Plotter* p,
                       const FVec3* points,
                       int points_count,
                       const PlotAttribs* attribs)
{
    plt_append_points_buffer(p, PlotType_Points, points, points_count, attribs);
}

static void plt_lines(Plotter* p,
                      const FVec3* points,
                      int points_count,
                      const PlotAttribs* attribs)
{
    plt_append_points_buffer(p, PlotType_Lines, points, points_count, attribs);
}

static void plt_enable_grid(Plotter* p, bool enable)
{
    p->should_draw_grid = enable;
}

static void plt_init(Plotter* p)
{
    *p = (Plotter){0};
    plt_set_axis_attribs(
        p, &(AxisAttribs){
               .attribs =
                   {
                       {.name = "X", .range_min = 0, .range_max = 1},
                       {.name = "Y", .range_min = 0, .range_max = 1},
                   },
           });

    p->canvas_size = (IVec2){100, 100};
}

static void plt_cleanup(Plotter* p)
{
    PointsBuffer* curr = p->buffers;
    while (curr)
    {
        PointsBuffer* tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    *p = (Plotter){0};
}

typedef struct PlotRenderer_
{
    VertexBuffer point_vb;

    GLuint lines_vao;
    GLuint lines_vbo;

    GLuint unlit_shader;
} PlotRenderer;

static void plt_renderer_init(Example* e, PlotRenderer* r)
{
    *r = (PlotRenderer){0};
    Mesh point_mesh = rc_mesh_make_sphere(0.5f, 32, 32);
    r_vb_init(&r->point_vb, &point_mesh, GL_TRIANGLES);

    glGenVertexArrays(1, &r->lines_vao);
    glBindVertexArray(r->lines_vao);
    glGenBuffers(1, &r->lines_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->lines_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FVec3), (GLvoid*)0);

    r->unlit_shader = e_shader_load(e, "unlit");
}

static void plt_renderer_cleanup(PlotRenderer* r)
{
    glDeleteProgram(r->unlit_shader);

    glDeleteBuffers(1, &r->lines_vbo);
    glDeleteVertexArrays(1, &r->lines_vao);
    r_vb_cleanup(&r->point_vb);
    *r = (PlotRenderer){0};
}

static void plt_draw(const Example* e, const Plotter* p, const PlotRenderer* r)
{
    glEnable(GL_SCISSOR_TEST);
    glViewport(0, 0, p->canvas_size.x, p->canvas_size.y);
    glScissor(0, 0, p->canvas_size.x, p->canvas_size.y);
    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(r->unlit_shader);

    ExamplePerFrameUBO per_frame = {0};
    per_frame.view =
        mat4_lookat((FVec3){0, 0, 50}, (FVec3){0, 0, 0}, (FVec3){0, 1, 0});
    FVec2 graph_min = {
        p->axes[0].range_min,
        p->axes[1].range_min,
    };
    FVec2 graph_max = {
        p->axes[0].range_max,
        p->axes[1].range_max,
    };

    FVec2 canvas_min = {0};
    FVec2 canvas_max = {
        (float)p->canvas_size.x,
        (float)p->canvas_size.y,
    };
    Mat4 graph_to_canvas = mat4_identity();
    graph_to_canvas.mm[0][0] =
        (canvas_max.x - canvas_min.x) / (graph_max.x - graph_min.x);
    graph_to_canvas.mm[3][0] =
        (graph_max.x * canvas_min.x - canvas_max.x * graph_min.x) /
        (graph_max.x - graph_min.x);
    graph_to_canvas.mm[1][1] =
        (canvas_max.y - canvas_min.y) / (graph_max.y - graph_min.y);
    graph_to_canvas.mm[3][1] =
        (graph_max.y * canvas_min.y - canvas_max.y * graph_min.y) /
        (graph_max.y - graph_min.y);

#if 0
    per_frame.proj =
        mat4_ortho(p->axes[0].range_max, p->axes[0].range_min,
                   p->axes[1].range_max, p->axes[1].range_min, 0.1f, 100);
#else
    per_frame.proj = mat4_ortho(canvas_max.x, canvas_min.x, canvas_max.y,
                                canvas_min.y, 0.1f, 100);
    per_frame.proj = mat4_mul(&per_frame.proj, &graph_to_canvas);
#endif
    e_apply_per_frame_ubo(e, &per_frame);

    int total_line_points_count = 0;
    {
        PointsBuffer* curr = p->buffers;
        while (curr)
        {
            if (curr->type == PlotType_Lines)
            {
                total_line_points_count += curr->points_count;
            }
            curr = curr->next;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, r->lines_vbo);
    glBufferData(GL_ARRAY_BUFFER, total_line_points_count * sizeof(FVec3), NULL,
                 GL_DYNAMIC_DRAW);
    {
        int offset = 0;
        PointsBuffer* curr = p->buffers;
        while (curr)
        {
            if (curr->type == PlotType_Lines)
            {
                glBufferSubData(GL_ARRAY_BUFFER, offset,
                                curr->points_count * sizeof(FVec3),
                                curr->points);
                offset += curr->points_count * sizeof(FVec3);
            }
            curr = curr->next;
        }
    }

    glBindVertexArray(r->lines_vao);
    glDrawArrays(GL_LINE_STRIP, 0, total_line_points_count);

    {
        FVec2 graph_size = {
            fabsf(p->axes[0].range_max - p->axes[0].range_min),
            fabsf(p->axes[1].range_max - p->axes[1].range_min),
        };

        PointsBuffer* curr = p->buffers;
        while (curr != NULL)
        {
            if (curr->type == PlotType_Points)
            {
                for (int i = 0; i < curr->points_count; i++)
                {
                    const FVec3* point = &curr->points[i];
                    ExamplePerObjectUBO per_object = {0};
                    Mat4 trans = mat4_translation(*point);
                    Mat4 scale = mat4_scalev((FVec3){
                        curr->thickness * graph_size.x /
                            (float)p->canvas_size.x,
                        curr->thickness * graph_size.y /
                            (float)p->canvas_size.y,
                        1,
                    });
                    per_object.model = mat4_mul(&trans, &scale);
                    per_object.color = (FVec3){
                        curr->color.x,
                        curr->color.y,
                        curr->color.z,
                    };
                    e_apply_per_object_ubo(e, &per_object);
                    r_vb_draw(&r->point_vb);
                }
            }
            curr = curr->next;
        }
    }
}

typedef struct Graph_ Graph;

typedef struct Method_
{
    const char* name;
    float (*call)(Graph*, float);
} Method;

typedef struct Graph_
{
    Mesh point_mesh;
    VertexBuffer point_vb;

    Mesh canvas_mesh;
    VertexBuffer canvas_vb;

    FVec3 curve_points[CURVE_POINTS_COUNT];
    uint curve_vao;
    uint curve_vbo;

    GLuint unlit_shader;

    FVec2 graph_min;
    FVec2 graph_max;

    FVec2 world_min;
    FVec2 world_max;

    float coeffs[MAX_DEGREE + 1];
    int degree;

    Method methods[2];
    int current_method_index;

    int dragged_point_index;
    bool is_dragging;

    PlotRenderer plot_renderer;
} Graph;

static int g_combs[MAX_DEGREE + 1][MAX_DEGREE + 1];

static int comb(int n, int c)
{
    if (c == 0 || c == n)
        return 1;
    if (g_combs[n][c] != 0)
        return g_combs[n][c];
    g_combs[n][c] = comb(n - 1, c - 1) + comb(n - 1, c);
    return g_combs[n][c];
}

static float calc_polynomial_bb(Graph* s, float t)
{
    float result = 0;
    int d = s->degree;
    for (int i = 0; i <= d; i++)
    {
        result += s->coeffs[i] * (float)comb(d, i) *
                  powf(1.f - t, (float)(d - i)) * powf(t, (float)i);
    }

    return result;
}

static float
    calc_polynomial_nli_rec(const Graph* s,
                            float (*values)[MAX_DEGREE + 1][MAX_DEGREE + 1],
                            int begin,
                            int end,
                            float t)
{
    if ((end - begin) == 1)
        return (1.f - t) * s->coeffs[begin] + t * s->coeffs[end];

    if ((*values)[begin][end] != FLT_MAX)
        return (*values)[begin][end];

    float result =
        (1.f - t) * calc_polynomial_nli_rec(s, values, begin, end - 1, t) +
        t * calc_polynomial_nli_rec(s, values, begin + 1, end, t);

    (*values)[begin][end] = result;

    return result;
}

static float calc_polynomial_nli(Graph* s, float t)
{
    int d = s->degree;

    float values[MAX_DEGREE + 1][MAX_DEGREE + 1];
    for (int i = 0; i <= MAX_DEGREE; i++)
    {
        for (int j = 0; j <= MAX_DEGREE; j++)
            values[i][j] = FLT_MAX;
    }

    return calc_polynomial_nli_rec(s, &values, 0, d, t);
}

static void set_world_bound(Graph* s, const Input* input)
{
    s->world_min = (FVec2){300, 100};
    s->world_max = (FVec2){(float)input->window_size.x - 100,
                           (float)input->window_size.y - 100};
}

static float graph_to_world_x(const Graph* s, float gx)
{
    float wx = (float)(s->world_max.x - s->world_min.x) /
                   (s->graph_max.x - s->graph_min.x) * gx +
               s->world_min.x;
    return wx;
}

static float graph_to_world_y(const Graph* s, float gy)
{
    float wy = (s->world_max.y - s->world_min.y) /
                   (s->graph_max.y - s->graph_min.y) * gy +
               (s->world_min.y + (s->world_max.y - s->world_min.y) * 0.5f);
    return wy;
}

static FVec2 graph_to_world(const Graph* s, FVec2 gp)
{
    FVec2 wp = {
        graph_to_world_x(s, gp.x),
        graph_to_world_y(s, gp.y),
    };
    return wp;
}

static float world_to_graph_y(const Graph* s, float wy)
{
    wy = HIMATH_CLAMP(wy, s->world_min.y, s->world_max.y) - s->world_min.y;
    float gy = (s->graph_max.y - s->graph_min.y) /
                   (s->world_max.y - s->world_min.y) * wy -
               3.f;
    return gy;
}

static FVec2 world_to_screen(const Input* input, FVec2 wp)
{
    FVec2 sp = {(float)wp.x, (float)input->window_size.y - wp.y};
    return sp;
}

static FVec2 screen_to_world(const Input* input, FVec2 sp)
{
    FVec2 wp = {(float)sp.x, (float)input->window_size.y - sp.y};
    return wp;
}

static FVec2 get_world_mouse_pos(const Input* input)
{
    FVec2 wmp = screen_to_world(
        input, (FVec2){(float)input->mouse_pos.x, (float)input->mouse_pos.y});
    return wmp;
}

static FVec2 get_coeff_graph_pos(const Graph* s, int index)
{
    float step = 1.f / (float)s->degree;
    FVec2 result = {(float)index * step, s->coeffs[index]};
    return result;
}

static int get_point_index_colliding_mouse(const Graph* s, const Input* input)
{
    int index = -1;
    for (int i = 0; i < ARRAY_LENGTH(s->coeffs); i++)
    {
        FVec2 gp = get_coeff_graph_pos(s, i);
        FVec2 wp = graph_to_world(s, gp);
        FVec2 wmp = get_world_mouse_pos(input);
        FVec2 d = fvec2_sub(wp, wmp);
        if ((d.x * d.x + d.y * d.y) <=
            (POINT_RADIUS * 1.5f) * (POINT_RADIUS * 1.5f))
        {
            index = i;
            break;
        }
    }

    return index;
}

EXAMPLE_INIT_FN_SIG(graph)
{
    Example* e = e_example_make("graph", Graph);
    Graph* s = (Graph*)e->scene;

    s->point_mesh = rc_mesh_make_sphere(0.5f, 32, 32);
    r_vb_init(&s->point_vb, &s->point_mesh, GL_TRIANGLES);

    {
        Vertex canvas_vertices[4] = {
            {{0, 0, 0}},
            {{1, 0, 0}},
            {{1, 1, 0}},
            {{0, 1, 0}},
        };
        uint canvas_indices[6] = {0, 1, 2, 2, 3, 0};

        s->canvas_mesh = rc_mesh_make_raw2(ARRAY_LENGTH(canvas_vertices),
                                           ARRAY_LENGTH(canvas_indices),
                                           canvas_vertices, canvas_indices);
    }
    r_vb_init(&s->canvas_vb, &s->canvas_mesh, GL_TRIANGLES);

    glGenVertexArrays(1, &s->curve_vao);
    glBindVertexArray(s->curve_vao);
    glGenBuffers(1, &s->curve_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s->curve_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s->curve_points), NULL,
                 GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FVec3), (GLvoid*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    s->unlit_shader = e_shader_load(e, "unlit");

    s->graph_min = (FVec2){0, -3};
    s->graph_max = (FVec2){1, 3};

    set_world_bound(s, input);

    s->degree = 1;

    s->methods[0] = (Method){"BB-form", &calc_polynomial_bb};
    s->methods[1] = (Method){"NLI", &calc_polynomial_nli};

    s->dragged_point_index = -1;

    plt_renderer_init(e, &s->plot_renderer);

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(graph)
{
    Example* e = (Example*)udata;
    Graph* s = (Graph*)e->scene;

    plt_renderer_cleanup(&s->plot_renderer);

    glDeleteProgram(s->unlit_shader);
    glDeleteBuffers(1, &s->curve_vbo);
    glDeleteVertexArrays(1, &s->curve_vao);
    r_vb_cleanup(&s->canvas_vb);
    rc_mesh_cleanup(&s->canvas_mesh);
    r_vb_cleanup(&s->point_vb);
    rc_mesh_cleanup(&s->point_mesh);
    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(graph)
{
    Example* e = (Example*)udata;
    Graph* s = (Graph*)e->scene;

    set_world_bound(s, input);

    if (!s->is_dragging && input->mouse_down[0])
    {
        s->dragged_point_index = get_point_index_colliding_mouse(s, input);
        if (s->dragged_point_index >= 0)
            s->is_dragging = true;
    }

    if (s->is_dragging && !input->mouse_down[0])
    {
        s->is_dragging = false;
        s->dragged_point_index = -1;
    }

    if (s->is_dragging)
    {
        s->coeffs[s->dragged_point_index] =
            world_to_graph_y(s, get_world_mouse_pos(input).y);
    }

    igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_Once, (ImVec2){0, 0});
    igBegin("Settings", NULL,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_AlwaysAutoResize);
    igText("Degree");
    igSameLine(0, -1);
    igSliderInt("##Degree", &s->degree, 1, MAX_DEGREE, "%d");
    igText("Method");
    igSameLine(0, -1);
    if (igBeginCombo("##Method", s->methods[s->current_method_index].name, 0))
    {
        for (int i = 0; i < ARRAY_LENGTH(s->methods); i++)
        {
            bool is_selected = i == s->current_method_index;
            if (igSelectable(s->methods[i].name, is_selected, 0,
                             (ImVec2){0, 0}))
            {
                s->current_method_index = i;
            }
        }
        igEndCombo();
    }
    igEnd();

    {
        igSetNextWindowSize((ImVec2){30, 10}, ImGuiCond_Always);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground;

        FVec2 p = world_to_screen(input, graph_to_world(s, s->graph_min));
        p.y -= 20;
        p.x -= 30;
        igSetNextWindowPos((ImVec2){p.x, p.y}, ImGuiCond_Once, (ImVec2){0, 0});
        igBegin("-3", NULL, flags);
        igText("-3");
        igEnd();

        p = world_to_screen(
            input, graph_to_world(s, (FVec2){s->graph_min.x, s->graph_max.y}));
        p.y -= 10;
        p.x -= 30;
        igSetNextWindowPos((ImVec2){p.x, p.y}, ImGuiCond_Once, (ImVec2){0, 0});
        igBegin(" 3", NULL, flags);
        igText(" 3");
        igEnd();

        p = world_to_screen(input, graph_to_world(s, s->graph_min));
        p.x -= 10;
        igSetNextWindowPos((ImVec2){p.x, p.y}, ImGuiCond_Once, (ImVec2){0, 0});
        igBegin("0", NULL, flags);
        igText("0");
        igEnd();

        p = world_to_screen(
            input, graph_to_world(s, (FVec2){s->graph_max.x, s->graph_min.y}));
        p.x -= 10;
        igSetNextWindowPos((ImVec2){p.x, p.y}, ImGuiCond_Once, (ImVec2){0, 0});
        igBegin("1", NULL, flags);
        igText("1");
        igEnd();
    }

#if 0
    ExamplePerFrameUBO per_frame = {0};
    per_frame.view =
        mat4_lookat((FVec3){0, 0, 50}, (FVec3){0, 0, 0}, (FVec3){0, 1, 0});
    per_frame.proj = mat4_ortho((float)input->window_size.x, 0,
                                (float)input->window_size.y, 0, 0.1f, 100);
    e_apply_per_frame_ubo(e, &per_frame);

    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(s->unlit_shader);
    {
        ExamplePerObjectUBO per_object = {0};
        Mat4 trans = mat4_translation(
            (FVec3){HIMATH_MIN(s->world_max.x, s->world_min.x),
                    HIMATH_MIN(s->world_max.y, s->world_min.y), -1});
        Mat4 scale =
            mat4_scalev((FVec3){fabsf(s->world_max.x - s->world_min.x),
                                fabsf(s->world_max.y - s->world_min.y)});
        per_object.model = mat4_mul(&trans, &scale);
        per_object.color = (FVec3){0.3f, 0.3f, 0.3f};
        e_apply_per_object_ubo(e, &per_object);
        r_vb_draw(&s->canvas_vb);
    }

    int hovering_point_index = get_point_index_colliding_mouse(s, input);
    for (int i = 0; i <= s->degree; i++)
    {
        FVec2 gp = get_coeff_graph_pos(s, i);
        FVec2 wp = graph_to_world(s, gp);

        ExamplePerObjectUBO per_object = {0};
        Mat4 trans = mat4_translation((FVec3){wp.x, wp.y, 0});
        Mat4 scale = mat4_scale(POINT_RADIUS * 2.f);
        per_object.model = mat4_mul(&trans, &scale);
        if (s->is_dragging && (i == s->dragged_point_index))
            per_object.color = (FVec3){0.6f, 0, 0};
        else if (i == hovering_point_index)
            per_object.color = (FVec3){1, 0, 0};
        else
            per_object.color = (FVec3){0, 1, 0};
        e_apply_per_object_ubo(e, &per_object);
        r_vb_draw(&s->point_vb);
    }

    for (int i = 0; i < ARRAY_LENGTH(s->curve_points); i++)
    {
        float step_x = 1.f / (ARRAY_LENGTH(s->curve_points) - 1);
        float t = (float)i * step_x;
        float p = s->methods[s->current_method_index].call(s, t);

        FVec2 gp = {0};
        gp.x = t;
        gp.y = p;
        FVec2 wp = graph_to_world(s, gp);
        s->curve_points[i] = (FVec3){wp.x, wp.y, 1};
    }
    {
        ExamplePerObjectUBO per_object = {0};
        per_object.model = mat4_identity();
        per_object.color = (FVec3){1, 0, 1};
        e_apply_per_object_ubo(e, &per_object);
    }
    glBindBuffer(GL_ARRAY_BUFFER, s->curve_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(s->curve_points),
                    s->curve_points);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(s->curve_vao);
    glDrawArrays(GL_LINE_STRIP, 0, ARRAY_LENGTH(s->curve_points));
    glBindVertexArray(0);
#endif

    Plotter plotter;
    plt_init(&plotter);
    plt_set_axis_attribs(&plotter, &(AxisAttribs){
                                       .attribs = {{
                                                       .name = "X",
                                                       .range_min = 0,
                                                       .range_max = 1,
                                                   },
                                                   {
                                                       .name = "Y",
                                                       .range_min = -1,
                                                       .range_max = 1,
                                                   }},
                                   });

    for (int i = 0; i < ARRAY_LENGTH(s->curve_points); i++)
    {
        float step_x = 1.f / (ARRAY_LENGTH(s->curve_points) - 1);
        float t = (float)i * step_x;
        float p = s->methods[s->current_method_index].call(s, t);

        FVec2 gp = {0};
        gp.x = t;
        gp.y = p;
        // FVec2 wp = graph_to_world(s, gp);
        s->curve_points[i] = (FVec3){gp.x, gp.y, 0};
    }

    plt_set_axis_attribs(
        &plotter, &(AxisAttribs){
                      .attribs =
                          {
                              {.name = "X", .range_min = 0, .range_max = 1},
                              {.name = "Y", .range_min = -3, .range_max = 3},
                          },
                  });

    plt_points(&plotter, s->curve_points, ARRAY_LENGTH(s->curve_points),
               &(PlotAttribs){
                   .color = (FVec4){1, 0, 1, 1},
                   .thickness = 10,
               });

    plotter.canvas_size = input->window_size;
    plt_draw(e, &plotter, &s->plot_renderer);

    plt_cleanup(&plotter);
}
