#version 460 core

#include "include/lighting.frag"

out vec4 color;

in vec3 v_position_world_coords;
in vec3 v_normal;
in vec3 v_view_direction;
in vec4 v_frag_pos_light_space;

uniform sampler2D u_texture_sampler;
uniform sampler2D u_normal_map_sampler;
uniform sampler2D u_shadow_map_sampler;

uniform PointLight u_point_light;
uniform DirectionalLight u_directional_light;
uniform Material u_material;

/**
 * @return a pseudorandom number in [0, 1].
 *
 * @param p An input number.
 */
float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

/**
 * @return a 2D vector of pseudorandom numbers in [0, 1].
 *
 * @param p A 2D input vector.
 */
vec2 hash22(vec2 p)
{
    float n = hash12(p);
    return vec2(n, hash12(p + n));
}

/**
 * @return a 2x2 rotation matrix for angle a.
 *
 * @param a The rotation angle in radians.
 */
mat2 rot(float a)
{
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

/**
 * @brief Return type for sample_stochastic.
 */
struct StochasticSample
{
    vec4 diffuse;
    vec4 normal;
};

/**
 * @brief Samples a texture using stochastic sampling.
 *
 * @param diffuse_texture_sampler The diffuse texture sampler.
 * @param normal_texture_sampler The normal texture sampler.
 * @param texture_coord The texture coordinates.
 *
 * @return The sampled color.
 */
StochasticSample sample_stochastic(sampler2D diffuse_texture_sampler, sampler2D normal_texture_sampler, vec2 texture_coord)
{
    vec2 cell = floor(texture_coord);
    vec2 f = fract(texture_coord);

    vec4 diffuse_result = vec4(0.0);
    vec4 normal_result = vec4(0.0);
    float total = 0.0;

    for (int y = 0; y <= 1; y++)
    {
        for (int x = 0; x <= 1; x++)
        {
            vec2 c = cell + vec2(x, y);

            vec2 rnd = hash22(c);
            float angle = rnd.x * 6.2831853;
            vec2 offset = rnd - 0.5;

            vec2 u = rot(angle) * (f - vec2(x, y)) + 0.5 + offset;

            float w = (1.0 - abs(float(x) - f.x))
                    * (1.0 - abs(float(y) - f.y));

            diffuse_result += texture(diffuse_texture_sampler, u) * w;
            normal_result += texture(normal_texture_sampler, u) * w;
            total += w;
        }
    }

    StochasticSample result;
    result.diffuse = diffuse_result / total;
    result.normal = normal_result / total;
    return result;
}

void main()
{
    vec2 texture_coord = v_position_world_coords.xz / 2.0;

    vec3 vertex_normal = normalize(v_normal);
    vec3 tangent = normalize(vec3(1.0, 0.0, 0.0) - vertex_normal * dot(vertex_normal, vec3(1.0, 0.0, 0.0)));
    vec3 bitangent = cross(vertex_normal, tangent);
    mat3 tangent_bitangent_normal = mat3(tangent, bitangent, vertex_normal);

    /*
     * Sample texture stochastically.
     */
    StochasticSample samp = sample_stochastic(
        u_texture_sampler,
        u_normal_map_sampler,
        texture_coord);
    vec3 texture_color = samp.diffuse.rgb;
    vec3 normal_texture_space = samp.normal.rgb * 2.0 - 1.0;
    vec3 normal_world_space = normalize(tangent_bitangent_normal * normal_texture_space);

    vec3 result = compute_directional_component(
        u_directional_light,
        u_material,
        u_shadow_map_sampler,
        normal_world_space,
        v_frag_pos_light_space,
        v_view_direction);

    result += compute_point_component(
        u_point_light,
        u_material,
        normal_world_space,
        v_position_world_coords,
        v_view_direction);

    color = vec4(result * texture_color, 1.0);
}