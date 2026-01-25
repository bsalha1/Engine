#include "include/lighting_types.glsl"
#include "include/texture_slots.glsl"

float linearize_depth(float depth, float near, float far)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

/**
 * Computes the shadow component for a fragment, averaging over a 3x3 grid
 * of adjacent texels.
 *
 * @param shadow_map_samplers The shadow map samplers.
 * @param light_view_projections The light view projection matrices.
 * @param frag_pos_world_space The fragment position in world space.
 * @param normal The normal vector at the fragment.
 * @param light_direction The direction vector from the fragment to the light.
 * @param camera_near_clip The camera's near clipping plane distance.
 * @param camera_far_clip The camera's far clipping plane distance.
 * @param shadow_map_ranges The distance ranges for each shadow map.
 *
 * @return The shadow factor.
 */
float compute_shadow_component(
    sampler2D shadow_map_samplers[NUM_SHADOW_MAPS],
    mat4 light_view_projections[NUM_SHADOW_MAPS],
    vec3 frag_pos_world_space,
    vec3 normal,
    vec3 light_direction,
    float camera_near_clip,
    float camera_far_clip,
    vec2 shadow_map_ranges)
{
    /*
     * Get distance from camera to fragment in world space.
     */
    float distance_from_camera = linearize_depth(gl_FragCoord.z, camera_near_clip, camera_far_clip);

    /*
     * Select appropriate shadow map based on distance from camera.
     */
    uint shadow_map_idx = 0;
    if (distance_from_camera < shadow_map_ranges.x)
    {
        shadow_map_idx = 0;
    }
    else if (distance_from_camera < shadow_map_ranges.y)
    {
        shadow_map_idx = 1;
    }
    else
    {
        shadow_map_idx = 2;
    }

    /*
     * Get fragment position in light space and transform to [0,1] range.
     */
    mat4 light_view_projection = light_view_projections[shadow_map_idx];
    vec4 frag_pos_light_space = light_view_projection * vec4(frag_pos_world_space, 1.0);
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w * 0.5 + 0.5;

    /*
     * If fragment is outside light's orthographic frustum, return no shadow.
     */
    if (proj_coords.x < 0.0 || proj_coords.x > 1.0 ||
        proj_coords.y < 0.0 || proj_coords.y > 1.0 ||
        proj_coords.z < 0.0 || proj_coords.z > 1.0)
    {
        return 0.0;
    }

    /*
     * Get distance from light to fragment in light space.
     */
    float distance_from_light = proj_coords.z;

    float bias = max(0.005 * (1.0 - dot(normal, light_direction)), 0.005);
    float shadow = 0.0;
    vec2 texel_size = 1.0 / textureSize(shadow_map_samplers[shadow_map_idx], 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcf_depth = texture(shadow_map_samplers[shadow_map_idx], proj_coords.xy + vec2(x, y) * texel_size).r;
            shadow += distance_from_light - bias > pcf_depth ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

/**
 * Computes the component of light contributed by a directional light source.
 *
 * @param light The directional light source.
 * @param material The material properties of the surface.
 * @param shadow_map_samplers The array of shadow map samplers.
 * @param light_view_projections The array of light view projection matrices.
 * @param shadow_map_sampler The shadow map sampler.
 * @param normal The normal vector at the fragment.
 * @param frag_pos_world_space The fragment position in world space.
 * @param view_direction The view direction vector.
 * @param camera_near_clip The camera's near clipping plane distance.
 * @param camera_far_clip The camera's far clipping plane distance.
 * @param shadow_map_ranges The distance ranges for each shadow map.
 *
 * @return The computed light component.
 */
vec3 compute_directional_component(
    DirectionalLight light,
    Material material,
    sampler2D shadow_map_samplers[NUM_SHADOW_MAPS],
    mat4 light_view_projections[NUM_SHADOW_MAPS],
    vec3 normal,
    vec3 frag_pos_world_space,
    vec3 view_direction,
    float camera_near_clip,
    float camera_far_clip,
    vec2 shadow_map_ranges)
{
    vec3 light_direction = -light.direction.xyz;
    vec3 halfway_direction = normalize(light_direction + view_direction);

    /*
     * Compute ambient light component.
     */
    vec3 ambient_light = light.ambient.rgb * material.ambient;

    /*
     * Compute diffuse light component.
     */
    float diff = max(dot(normal, light_direction), 0.0);
    vec3 diffuse_light = diff * light.diffuse.rgb * material.diffuse;

    /*
     * Compute specular light component.
     */
    float shine = pow(max(dot(normal, halfway_direction), 0.0), material.shininess);
    vec3 specular_light = shine * light.specular.rgb * material.specular;

    float shadow = compute_shadow_component(
        shadow_map_samplers,
        light_view_projections,
        frag_pos_world_space,
        normal,
        light_direction,
        camera_near_clip,
        camera_far_clip,
        shadow_map_ranges);

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
    vec3 light_direction = normalize(light.position.xyz - frag_pos);
    vec3 halfway_direction = normalize(light_direction + view_direction);

    /*
     * Compute ambient light component.
     */
    vec3 ambient_light = light.ambient.rgb * material.ambient;

    /*
     * Compute diffuse light component.
     */
    float diff = max(dot(normal, light_direction), 0.0);
    vec3 diffuse_light = diff * light.diffuse.rgb * material.diffuse;

    /*
     * Compute specular light component.
     */
    float shine = pow(max(dot(normal, halfway_direction), 0.0), material.shininess);
    vec3 specular_light = shine * light.specular.rgb * material.specular;

    /*
     * Compute attenuation.
     */
    float distance = length(light.position.xyz - frag_pos);
    const float constant = 1.0;
    const float linear = 0.007;
    const float quadratic = 0.002;
    float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

    return attenuation * (ambient_light + diffuse_light + specular_light);
}


/**
 * Computes the component of light contributed by a spot light source.
 *
 * @param light The spot light source.
 * @param normal The normal vector at the fragment.
 * @param frag_pos The position of the fragment in world coordinates.
 * @param view_direction The view direction vector.
 *
 * @return The computed light component.
 */
vec3 compute_spot_component(
    SpotLight light,
    Material material,
    vec3 normal,
    vec3 frag_pos,
    vec3 view_direction)
{
    /*
     * Compute ambient light component.
     */
    vec3 ambient_light = light.ambient.rgb * material.ambient;

    /*
     * Compute intensity based on spot light cone angles.
     */
    vec3 light_direction = normalize(light.position.xyz - frag_pos);
    float theta = dot(light_direction, normalize(-light.direction.xyz));
    float epsilon = light.outer_cut_off - light.inner_cut_off;
    float intensity = clamp((theta - light.inner_cut_off) / epsilon, 0.0, 1.0);

    /*
     * Compute diffuse light component.
     */
    float diff = max(dot(normal, light_direction), 0.0);
    vec3 diffuse_light = intensity * diff * light.diffuse.rgb * material.diffuse;

    /*
     * Compute specular light component.
     */
    vec3 halfway_direction = normalize(light_direction + view_direction);
    float shine = pow(max(dot(normal, halfway_direction), 0.0), material.shininess);
    vec3 specular_light = intensity * shine * light.specular.rgb * material.specular;

    /*
     * Compute attenuation.
     */
    float distance = length(light.position.xyz - frag_pos);
    const float constant = 1.0;
    const float linear = 0.007;
    const float quadratic = 0.002;
    float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

    return attenuation * (ambient_light + diffuse_light + specular_light);
}
