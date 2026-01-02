#version 460 core

layout(location = 0) in vec3 l_position;
layout(location = 1) in vec3 l_normal;

/**
 * Variables going to fragment shader.
 */
out vec3 v_position_world_coords;
out vec3 v_normal;
out vec3 v_view_direction;
out vec4 v_frag_pos_light_space;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_camera_position;
uniform mat4 u_light_view_projection;

void main()
{
    v_normal = l_normal;

    vec4 position_four_vector = vec4(l_position, 1.0);

    gl_Position = u_projection * u_view * u_model * position_four_vector;

    /*
     * Position of vertex in world space.
     */
    v_position_world_coords = vec3(u_model * position_four_vector);

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
