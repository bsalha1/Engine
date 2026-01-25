#version 460 core

#include "include/per_frame_ubo.glsl"

layout(location = 0) in vec3 l_position;
layout(location = 1) in vec3 l_normal;
layout(location = 2) in vec2 l_texture_coord;
layout(location = 3) in vec4 l_tangent_handedness;

out vec3 v_position_world_coords;
out mat3 v_tangent_bitangent_normal;
out vec2 v_texture_coord;

uniform mat4 u_model;

mat3 compute_tbn_matrix(mat4 model, vec3 vertex_normal, vec4 tangent_handedness)
{
    mat3 normal_matrix = mat3(transpose(inverse(model)));
    vec3 normal = normalize(normal_matrix * vertex_normal);
    vec3 tangent = normalize(normal_matrix * tangent_handedness.xyz);
    vec3 bitangent = tangent_handedness.w * normalize(cross(normal, tangent));

    return mat3(tangent, bitangent, normal);
}

void main()
{
    const vec4 position_four_vector = vec4(l_position, 1.0);

    gl_Position = per_frame_ubo.camera_projection * per_frame_ubo.camera_view * u_model * position_four_vector;

    v_texture_coord = l_texture_coord;

    /*
     * Position of vertex in world space.
     */
    v_position_world_coords = vec3(u_model * position_four_vector);

    /*
     * Compute TBN matrix in world space.
     */
    v_tangent_bitangent_normal = compute_tbn_matrix(u_model, l_normal, l_tangent_handedness);
}
