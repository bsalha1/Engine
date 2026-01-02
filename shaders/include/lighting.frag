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

/**
 * Computes the shadow component for a fragment, averaging over a 3x3 grid
 * of adjacent texels.
 *
 * @param shadow_map_sampler The shadow map sampler.
 * @param frag_pos_light_space The fragment position in light space.
 *
 * @return The shadow factor.
 */
float compute_shadow_component(sampler2D shadow_map_sampler, vec4 frag_pos_light_space)
{
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w * 0.5 + 0.5;
    float closest_depth = texture(shadow_map_sampler, proj_coords.xy).r;
    float current_depth = proj_coords.z;

    const float bias = 0.005;
    float shadow = 0.0;
    if (proj_coords.z <= 1.0)
    {
        vec2 texel_size = 1.0 / textureSize(shadow_map_sampler, 0);
        for(int x = -1; x <= 1; ++x)
        {
            for(int y = -1; y <= 1; ++y)
            {
                float pcf_depth = texture(shadow_map_sampler, proj_coords.xy + vec2(x, y) * texel_size).r; 
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
 * @param material The material properties of the surface.
 * @param shadow_map_sampler The shadow map sampler.
 * @param normal The normal vector at the fragment.
 * @param frag_pos_light_space The fragment position in light space.
 * @param view_direction The view direction vector.
 *
 * @return The computed light component.
 */
vec3 compute_directional_component(
    DirectionalLight light,
    Material material,
    sampler2D shadow_map_sampler,
    vec3 normal,
    vec4 frag_pos_light_space,
    vec3 view_direction)
{
    vec3 light_direction = -light.direction;
    vec3 halfway_direction = normalize(light_direction + view_direction);

    /*
     * Compute ambient light component.
     */
    vec3 ambient_light = light.ambient * material.ambient;

    /*
     * Compute diffuse light component.
     */
    float diff = max(dot(normal, light_direction), 0.0);
    vec3 diffuse_light = diff * light.diffuse * material.diffuse;

    /*
     * Compute specular light component.
     */
    float shine = pow(max(dot(normal, halfway_direction), 0.0), material.shininess);
    vec3 specular_light = shine * light.specular * material.specular;

    float shadow = compute_shadow_component(shadow_map_sampler, frag_pos_light_space);

    return (ambient_light + (1.0 - shadow) * (diffuse_light + specular_light));
}

/**
 * Computes the component of light contributed by a point light source.
 *
 * @param light The point light source.
 * @param normal The normal vector at the fragment.
 * @param frag_pos The position of the fragment in world coordinates.
 * @param view_direction The view direction vector.
 *
 * @return The computed light component.
 */
vec3 compute_point_component(
    PointLight light,
    Material material,
    vec3 normal,
    vec3 frag_pos,
    vec3 view_direction)
{
    vec3 light_direction = normalize(light.position - frag_pos);
    vec3 halfway_direction = normalize(light_direction + view_direction);

    /*
     * Compute ambient light component.
     */
    vec3 ambient_light = light.ambient * material.ambient;

    /*
     * Compute diffuse light component.
     */
    float diff = max(dot(normal, light_direction), 0.0);
    vec3 diffuse_light = diff * light.diffuse * material.diffuse;

    /*
     * Compute specular light component.
     */
    float shine = pow(max(dot(normal, halfway_direction), 0.0), material.shininess);
    vec3 specular_light = shine * light.specular * material.specular;

    /*
     * Compute attenuation.
     */
    float distance = length(light.position - frag_pos);
    const float constant = 1.0;
    const float linear = 0.007;
    const float quadratic = 0.002;
    float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

    return attenuation * (ambient_light + diffuse_light + specular_light);
}
