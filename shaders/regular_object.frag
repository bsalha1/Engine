#version 460 core

#include "include/lighting.frag"
#include "include/per_frame_ubo.glsl"

out vec4 color;

in vec3 v_position_world_coords;
in mat3 v_tangent_bitangent_normal;
in vec4 v_frag_pos_light_space;
in vec2 v_texture_coord;

uniform sampler2D u_texture_sampler;
uniform sampler2D u_normal_map_sampler;
uniform sampler2D u_shadow_map_sampler;

uniform Material u_material;

void main()
{
    vec3 texture_color = texture(u_texture_sampler, v_texture_coord).rgb;
    vec3 normal_from_texture = texture(u_normal_map_sampler, v_texture_coord).rgb * 2.0 - 1.0;

    vec3 normal_world_space = normalize(v_tangent_bitangent_normal * normal_from_texture);

    /*
     * Compute unit vector pointing from vertex to camera to pass to fragment
     * shader to do lighting.
     */
    vec3 view_direction = normalize(per_frame_ubo.camera_position - v_position_world_coords);

    vec3 result = compute_directional_component(
        per_frame_ubo.directional_light,
        u_material,
        u_shadow_map_sampler,
        normal_world_space,
        v_frag_pos_light_space,
        view_direction);

    for (uint i = 0; i < per_frame_ubo.num_point_lights; i++)
    {
        result += compute_point_component(
            per_frame_ubo.point_lights[i],
            u_material,
            normal_world_space,
            v_position_world_coords,
            view_direction);
    }

    for (uint i = 0; i < per_frame_ubo.num_spot_lights; i++)
    {
        result += compute_spot_component(
            per_frame_ubo.spot_lights[i],
            u_material,
            normal_world_space,
            v_position_world_coords,
            view_direction);
    }

    color = vec4(result * texture_color, 1.0);
}