layout(location = 0) in vec3 vPosition;

void main()
{
    gl_Position = get_mvp() * vec4( vPosition, 1.0f );
}