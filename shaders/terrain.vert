#version 460 core

#include "include/per_frame_ubo.glsl"

layout(location = 0) in vec3 l_position;
layout(location = 1) in vec3 l_normal;

out vec3 v_position_world_coords;
out mat3 v_tangent_bitangent_normal;

uniform mat4 u_model;

void main()
{
    /*
     * For now, the terrain is only a height-map, so we can assume that the
     * tangent points in the X direction. Use Gram-Schmidt orthonormalization
     * to make sure the tangent is orthogonal to the normal.
     */
    mat3 normal_matrix = mat3(u_model);
    vec3 normal = normalize(normal_matrix * l_normal);
    vec3 tangent = normalize(normal_matrix * vec3(1.0, 0.0, 0.0));
    tangent = normalize(tangent - normal * dot(normal, tangent));
    vec3 bitangent = cross(normal, tangent);

    v_tangent_bitangent_normal = mat3(tangent, bitangent, normal);

    vec4 position_four_vector = vec4(l_position, 1.0);

    gl_Position = per_frame_ubo.camera_projection * per_frame_ubo.camera_view * u_model * position_four_vector;

    /*
     * Position of vertex in world space.
     */
    v_position_world_coords = vec3(u_model * position_four_vector);
}
