in VS_OUT
{
    vec2 uv;
} fs_in;

out vec3 fragColor;

void main()
{
    fragColor = texture(u_tex0, fs_in.uv).xyz;
}