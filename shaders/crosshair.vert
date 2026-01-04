#version 460 core

void main()
{
    const vec2 positions[4] = vec2[](
        vec2(-1, -1),
        vec2( 1, -1),
        vec2( 1,  1),
        vec2(-1,  1)
    );

    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}