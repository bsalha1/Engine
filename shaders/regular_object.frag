#version 460 core

#include "include/lighting.frag"

out vec4 color;

in vec3 v_position_world_coords;
in vec3 v_normal;
in vec4 v_tangent_handedness;
in vec2 v_texture_coord;
in vec3 v_view_direction;
in vec4 v_frag_pos_light_space;

uniform sampler2D u_texture_sampler;
uniform sampler2D u_normal_map_sampler;
uniform sampler2D u_shadow_map_sampler;

#define MAX_POINT_LIGHTS 64

uniform int u_num_point_lights;
uniform PointLight u_point_lights[MAX_POINT_LIGHTS];
uniform DirectionalLight u_directional_light;
uniform Material u_material;

void main()
{
    vec3 texture_color = texture(u_texture_sampler, v_texture_coord).rgb;
    vec3 normal_from_texture = texture(u_normal_map_sampler, v_texture_coord).rgb * 2.0 - 1.0;

    vec3 normal = normalize(v_normal);
    vec3 tangent = normalize(v_tangent_handedness.xyz);
    vec3 bitangent = v_tangent_handedness.w * normalize(cross(normal, tangent));

    mat3 tangent_bitangent_norm = mat3(tangent, bitangent, normal);
    normal = normalize(tangent_bitangent_norm * normal_from_texture);

    vec3 result = compute_directional_component(
        u_directional_light,
        u_material,
        u_shadow_map_sampler,
        normal,
        v_frag_pos_light_space,
        v_view_direction);

    for (int i = 0; i < u_num_point_lights; i++)
    {
        result += compute_point_component(
            u_point_lights[i],
            u_material,
            normal,
            v_position_world_coords,
            v_view_direction);
    }

    color = vec4(result * texture_color, 1.0);
}