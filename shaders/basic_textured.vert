#version 460 core

layout(location = 0) in vec3 l_position;
layout(location = 1) in vec2 l_texture_coord;

/**
 * Variables going to fragment shader.
 */
out vec2 v_texture_coord;

uniform mat4 u_model_view_projection;

void main()
{
    const vec4 position_four_vector = vec4(l_position, 1.0);
    gl_Position = u_model_view_projection * position_four_vector;
    v_texture_coord = l_texture_coord;
}
