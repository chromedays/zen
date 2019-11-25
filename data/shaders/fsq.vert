layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec2 vUV;

out VS_OUT
{
    vec2 uv;
} vs_out;

void main()
{
    vs_out.uv = vUV;
    gl_Position = vec4(vPosition, 1.0f);
}