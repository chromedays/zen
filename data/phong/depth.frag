out vec4 out_color;

void main()
{
    float z = gl_FragCoord.z * 2.0 - 1.0;
    float far = 100;
    float near = 0.1;
    float linear_depth = (2 * far * near) / (far + near - z * (far - near));
    out_color = vec4(vec3(linear_depth / far), 1.0);
}