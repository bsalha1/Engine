#version 460 core

#include "include/per_frame_ubo.glsl"

layout(location = 0) in vec3 l_position;
layout(location = 1) in vec3 l_norm;
layout(location = 2) in vec2 l_texture_coord;
layout(location = 3) in vec4 l_tangent_handedness;

/**
 * Variables going to fragment shader.
 */
out vec3 v_position_world_coords;
out vec3 v_normal;
out vec4 v_tangent_handedness;
out vec2 v_texture_coord;
out vec4 v_frag_pos_light_space;

uniform mat4 u_model;
uniform mat4 u_projection;

void main()
{
    const vec4 position_four_vector = vec4(l_position, 1.0);

    gl_Position = u_projection * per_frame_ubo.camera_view * u_model * position_four_vector;

    v_texture_coord = l_texture_coord;

    /*
     * Position of vertex in world space.
     */
    v_position_world_coords = vec3(u_model * position_four_vector);

    /*
     * Transform normal and tangent to world space.
     */
    mat3 normal_mat = transpose(inverse(mat3(u_model)));
    v_normal = normalize(normal_mat * l_norm);
    v_tangent_handedness = vec4(normalize(normal_mat * l_tangent_handedness.xyz), l_tangent_handedness.w);

    /*
     * Compute position of vertex in light space for shadow mapping.
     */
    v_frag_pos_light_space = per_frame_ubo.light_view_projection * position_four_vector;
}
