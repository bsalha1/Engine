#version 460 core

#include "include/per_frame_ubo.glsl"

layout(location = 0) in vec3 l_position;
layout(location = 1) in vec3 l_normal;

/**
 * Variables going to fragment shader.
 */
out vec3 v_position_world_coords;
out vec3 v_normal;
out vec4 v_frag_pos_light_space;

uniform mat4 u_model;

void main()
{
    v_normal = l_normal;

    vec4 position_four_vector = vec4(l_position, 1.0);

    gl_Position = per_frame_ubo.camera_projection * per_frame_ubo.camera_view * u_model * position_four_vector;

    /*
     * Position of vertex in world space.
     */
    v_position_world_coords = vec3(u_model * position_four_vector);

    /*
     * Compute position of vertex in light space for shadow mapping.
     */
    v_frag_pos_light_space = per_frame_ubo.light_view_projection * position_four_vector;
}
