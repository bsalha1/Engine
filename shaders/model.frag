#version 460 core

#include "include/lighting.frag"

out vec4 color;

in vec3 v_position_world_coords;
in vec2 v_texture_coord;
in vec3 v_view_direction;
in vec3 v_norm;
in vec4 v_frag_pos_light_space;

struct ModelMaterial
{
    sampler2D diffuse_texture_sampler;
    sampler2D specular_texture_sampler;
    float shininess;
};

uniform ModelMaterial u_material;
uniform sampler2D u_shadow_map_sampler;

#define MAX_POINT_LIGHTS 64

uniform int u_num_point_lights;
uniform PointLight u_point_lights[MAX_POINT_LIGHTS];
uniform DirectionalLight u_directional_light;

void main()
{
    const vec3 diffuse_color = texture(u_material.diffuse_texture_sampler, v_texture_coord).rgb;
    const vec3 specular_color = texture(u_material.specular_texture_sampler, v_texture_coord).rgb;

    /*
     * Translate model textures into a material.
     */
    Material material = Material(
        diffuse_color, /* ambient */
        diffuse_color, /* diffuse */
        specular_color, /* specular */
        u_material.shininess /* shininess */
    );

    vec3 result = compute_directional_component(
        u_directional_light,
        material,
        u_shadow_map_sampler,
        v_norm,
        v_frag_pos_light_space,
        v_view_direction);

    for (int i = 0; i < u_num_point_lights; i++)
    {
        result += compute_point_component(
            u_point_lights[i],
            material,
            v_norm,
            v_position_world_coords,
            v_view_direction);
    }

    color = vec4(result * diffuse_color, 1.0);
}