#version 460 core

out vec4 color;

/**
 * Variables from vertex shader.
 */
in vec3 v_position_world_coords;
in vec3 v_norm;
in vec2 v_texture_coord;
in vec3 v_view_direction;
in vec4 v_frag_pos_light_space;

uniform sampler2D u_texture_sampler;
uniform sampler2D u_shadow_map_sampler;

/**
 * A point light source.
 */
struct PointLight
{
    /**
     * Position of the light source in world coordinates.
     */
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

/**
 * A light source assumed infinitely far away.
 */
struct DirectionalLight
{
    /**
     * Direction of the light.
     */
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

/**
 * Material properties of a surface.
 */
struct Material
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

uniform PointLight u_point_light;
uniform DirectionalLight u_directional_light;
uniform Material u_material;

float compute_shadow_component(vec4 frag_pos_light_space)
{
    const vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w * 0.5 + 0.5;
    const float closest_depth = texture(u_shadow_map_sampler, proj_coords.xy).r;
    const float current_depth = proj_coords.z;

    const float bias = 0.005;
    float shadow = 0.0;
    if (proj_coords.z <= 1.0)
    {
        const vec2 texel_size = 1.0 / textureSize(u_shadow_map_sampler, 0);
        for(int x = -1; x <= 1; ++x)
        {
            for(int y = -1; y <= 1; ++y)
            {
                const float pcf_depth = texture(u_shadow_map_sampler, proj_coords.xy + vec2(x, y) * texel_size).r; 
                shadow += current_depth - bias > pcf_depth ? 1.0 : 0.0;        
            }
        }
        shadow /= 9.0;
    }

    return shadow;
}

/**
 * Computes the component of light contributed by a directional light source.
 *
 * @param light The directional light source.
 * @param normal The normal vector at the fragment.
 *
 * @return The computed light component.
 */
vec3 compute_directional_component(DirectionalLight light, vec3 normal, vec3 view_direction)
{
    const vec3 light_direction = -light.direction;
    const vec3 halfway_direction = normalize(light_direction + view_direction);

    /*
     * Compute ambient light component.
     */
    const vec3 ambient_light = light.ambient * u_material.ambient;

    /*
     * Compute diffuse light component.
     */
    const float diff = max(dot(normal, light_direction), 0.0);
    const vec3 diffuse_light = diff * light.diffuse * u_material.diffuse;

    /*
     * Compute specular light component.
     */
    const float shine = pow(max(dot(normal, halfway_direction), 0.0), u_material.shininess);
    const vec3 specular_light = shine * light.specular * u_material.specular;

    float shadow = compute_shadow_component(v_frag_pos_light_space);

    return (ambient_light + (1.0 - shadow) * (diffuse_light + specular_light));
}

/**
 * Computes the component of light contributed by a point light source.
 *
 * @param light The point light source.
 * @param normal The normal vector at the fragment.
 * @param frag_pos The position of the fragment in world coordinates.
 *
 * @return The computed light component.
 */
vec3 compute_point_component(PointLight light, vec3 normal, vec3 frag_pos, vec3 view_direction)
{
    const vec3 light_direction = normalize(light.position - frag_pos);
    const vec3 halfway_direction = normalize(light_direction + view_direction);

    /*
     * Compute ambient light component.
     */
    const vec3 ambient_light = light.ambient * u_material.ambient;

    /*
     * Compute diffuse light component.
     */
    const float diff = max(dot(normal, light_direction), 0.0);
    const vec3 diffuse_light = diff * light.diffuse * u_material.diffuse;

    /*
     * Compute specular light component.
     */
    const float shine = pow(max(dot(normal, halfway_direction), 0.0), u_material.shininess);
    const vec3 specular_light = shine * light.specular * u_material.specular;

    /*
     * Compute attenuation.
     */
    const float distance = length(light.position - frag_pos);
    const float constant = 1.0;
    const float linear = 0.007;
    const float quadratic = 0.002;
    const float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

    return attenuation * (ambient_light + diffuse_light + specular_light);
}

void main()
{
    const vec3 texture_color = texture(u_texture_sampler, v_texture_coord).rgb;

    vec3 result = compute_directional_component(u_directional_light, v_norm, v_view_direction);
    result += compute_point_component(u_point_light, v_norm, v_position_world_coords, v_view_direction);

    color = vec4(result * texture_color, 1.0);
}