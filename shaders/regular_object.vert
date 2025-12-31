#version 460 core

layout(location = 0) in vec3 l_position;
layout(location = 1) in vec3 l_norm;
layout(location = 2) in vec2 l_texture_coord;

/**
 * Variables going to fragment shader.
 */
out vec3 v_position_world_coords;
out vec3 v_norm;
out vec2 v_texture_coord;
out vec3 v_view_direction;
out vec4 v_frag_pos_light_space;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_camera_position;
uniform mat4 u_light_view_projection;

void main()
{
    const vec4 position_four_vector = vec4(l_position, 1.0);

    gl_Position = u_projection * u_view * u_model * position_four_vector;

    v_texture_coord = l_texture_coord;

    /*
     * Position of vertex in world space.
     */
    v_position_world_coords = vec3(u_model * position_four_vector);

    /*
     * Transform normal vector to world space.
     */
    v_norm = normalize(mat3(transpose(inverse(u_model))) * l_norm);

    /*
     * Compute unit vector pointing from vertex to camera to pass to fragment
     * shader to do lighting.
     */
    v_view_direction = normalize(u_camera_position - v_position_world_coords);

    /*
     * Compute position of vertex in light space for shadow mapping.
     */
    v_frag_pos_light_space = u_light_view_projection * position_four_vector;
}
