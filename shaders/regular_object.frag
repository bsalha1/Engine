#version 460 core

#include "include/lighting.frag"

out vec4 color;

in vec3 v_position_world_coords;
in mat3 v_tangent_bitangent_norm;
in vec2 v_texture_coord;
in vec3 v_view_direction;
in vec4 v_frag_pos_light_space;

uniform sampler2D u_texture_sampler;
uniform sampler2D u_normal_map_sampler;
uniform sampler2D u_shadow_map_sampler;

uniform PointLight u_point_light;
uniform DirectionalLight u_directional_light;
uniform Material u_material;

void main()
{
    const vec3 texture_color = texture(u_texture_sampler, v_texture_coord).rgb;
    vec3 normal = texture(u_normal_map_sampler, v_texture_coord).rgb;
    normal = normal * 2.0 - 1.0;
    normal = normalize(v_tangent_bitangent_norm * normal);

    vec3 result = compute_directional_component(
        u_directional_light,
        u_material,
        u_shadow_map_sampler,
        normal,
        v_frag_pos_light_space,
        v_view_direction);

    result += compute_point_component(
        u_point_light,
        u_material,
        normal,
        v_position_world_coords,
        v_view_direction);

    color = vec4(result * texture_color, 1.0);
}