#version 460 core

#include "include/lighting.frag"
#include "include/per_frame_ubo.glsl"
#include "include/texture_slots.glsl"

out vec4 color;

in vec3 v_position_world_coords;
in mat3 v_tangent_bitangent_normal;
in vec2 v_texture_coord;

uniform float u_material_shininess;
layout(binding = TEXTURE_SLOT_DIFFUSE) uniform sampler2D u_diffuse_texture_sampler;
layout(binding = TEXTURE_SLOT_SPECULAR) uniform sampler2D u_specular_texture_sampler;
layout(binding = TEXTURE_SLOT_NORMAL) uniform sampler2D u_normal_texture_sampler;
layout(binding = TEXTURE_SLOT_SHADOW_NEAR) uniform sampler2D u_shadow_map_samplers[NUM_SHADOW_MAPS];

void main()
{
    vec3 diffuse_color = texture(u_diffuse_texture_sampler, v_texture_coord).rgb;
    vec3 specular_color = texture(u_specular_texture_sampler, v_texture_coord).rgb;

    vec3 normal_from_texture = texture(u_normal_texture_sampler, v_texture_coord).rgb * 2.0 - 1.0;
    vec3 normal_world_space = normalize(v_tangent_bitangent_normal * normal_from_texture);

    /*
     * Translate model textures into a material.
     */
    Material material = Material(
        diffuse_color, /* ambient */
        diffuse_color, /* diffuse */
        specular_color, /* specular */
        u_material_shininess /* shininess */
    );

    /*
     * Compute unit vector pointing from vertex to camera to pass to fragment
     * shader to do lighting.
     */
    vec3 view_direction = normalize(vec3(per_frame_ubo.camera_position) - v_position_world_coords);

    vec3 result = compute_directional_component(
        per_frame_ubo.directional_light,
        material,
        u_shadow_map_samplers,
        per_frame_ubo.light_view_projections,
        normal_world_space,
        v_position_world_coords,
        view_direction,
        per_frame_ubo.camera_near_clip,
        per_frame_ubo.camera_far_clip,
        vec2(per_frame_ubo.shadow_map_ranges));

    for (uint i = 0; i < per_frame_ubo.num_point_lights; i++)
    {
        result += compute_point_component(
            per_frame_ubo.point_lights[i],
            material,
            normal_world_space,
            v_position_world_coords,
            view_direction);
    }

    for (uint i = 0; i < per_frame_ubo.num_spot_lights; i++)
    {
        result += compute_spot_component(
            per_frame_ubo.spot_lights[i],
            material,
            normal_world_space,
            v_position_world_coords,
            view_direction);
    }

    color = vec4(diffuse_color * result, 1.0);
}