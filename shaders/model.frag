#version 460 core

#include "include/lighting.frag"
#include "include/per_frame_ubo.glsl"

out vec4 color;

in vec3 v_position_world_coords;
in mat3 v_tangent_bitangent_normal;
in vec4 v_frag_pos_light_space;
in vec2 v_texture_coord;

struct ModelMaterial
{
    sampler2D diffuse_texture_sampler;
    sampler2D specular_texture_sampler;
    sampler2D normal_texture_sampler;
    float shininess;
};

uniform ModelMaterial u_material;
uniform sampler2D u_shadow_map_sampler;

void main()
{
    vec3 diffuse_color = texture(u_material.diffuse_texture_sampler, v_texture_coord).rgb;
    vec3 specular_color = texture(u_material.specular_texture_sampler, v_texture_coord).rgb;

    vec3 normal_from_texture = texture(u_material.normal_texture_sampler, v_texture_coord).rgb * 2.0 - 1.0;
    vec3 normal_world_space = normalize(v_tangent_bitangent_normal * normal_from_texture);

    /*
     * Translate model textures into a material.
     */
    Material material = Material(
        diffuse_color, /* ambient */
        diffuse_color, /* diffuse */
        specular_color, /* specular */
        u_material.shininess /* shininess */
    );

    /*
     * Compute unit vector pointing from vertex to camera to pass to fragment
     * shader to do lighting.
     */
    vec3 view_direction = normalize(per_frame_ubo.camera_position - v_position_world_coords);

    vec3 result = compute_directional_component(
        per_frame_ubo.directional_light,
        material,
        u_shadow_map_sampler,
        normal_world_space,
        v_frag_pos_light_space,
        view_direction);

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