#version 460 core

layout(location = 0) in vec3 l_position;

/**
 * Variables going to fragment shader.
 */
out vec3 v_texture_coord;

uniform mat4 u_projection;
uniform mat4 u_view;

void main()
{
    /*
     * Pass the position of the vertex as the texture coordinate.
     */
    v_texture_coord = l_position;

    vec4 pos = u_projection * u_view * vec4(l_position, 1.0);

    gl_Position = pos;
    gl_Position.z = gl_Position.w;

    // gl_Position = vec4(pos.xy, pos.w, pos.w);
    // gl_Position = vec4(l_position.xy, 1.0, 1.0);
}
