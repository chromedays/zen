
#define GRASS_TEXTURE u_samplers[0]

in VertexOut
{
    vec2 uv;
};

out vec4 out_color;

void main()
{
    vec4 color = texture(GRASS_TEXTURE, vec2(uv.x, 1 - uv.y));
    out_color = color;
}