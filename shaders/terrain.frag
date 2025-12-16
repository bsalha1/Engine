#version 460 core

out vec4 color;

/**
 * Variables from vertex shader.
 */
in vec3 v_position_world_coords;
in vec3 v_norm;
in vec2 v_texture_coord;
in vec3 v_camera_direction;

uniform sampler2D u_texture_sampler;

/**
 * A point light source.
 */
struct PointLight
{
    /**
     * Position of the light source in world coordinates.
     */
    vec3 position;

    /**
     * Color of the light.
     */
    vec3 color;
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

    /**
     * Color of the light.
     */
    vec3 color;
};

uniform PointLight u_point_light;
uniform DirectionalLight u_directional_light;

/**
 * Computes the component of light contributed by a directional light source.
 *
 * @param light The directional light source.
 * @param normal The normal vector at the fragment.
 * @param camera_direction The direction vector pointing from the fragment to the camera.
 *
 * @return The computed light component.
 */
vec3 compute_directional_component(DirectionalLight light, vec3 normal, vec3 camera_direction)
{
    const vec3 light_direction = -light.direction;

    /*
     * Compute ambient light component.
     */
    const float ambient_light_strength = 0.1;
    const vec3 ambient_light = ambient_light_strength * light.color;

    /*
     * Compute diffuse light component.
     */
    const float diff = max(dot(normal, light_direction), 0.0);
    const vec3 diffuse_light = diff * light.color;

    /*
     * Compute specular light component.
     */
    const float specular_light_strength = 0.5;
    const int shininess = 4;
    const vec3 reflect_direction = reflect(-light_direction, normal);
    const float shine = pow(max(dot(camera_direction, reflect_direction), 0.0), shininess);
    const vec3 specular_light = specular_light_strength * shine * light.color;

    return ambient_light + diffuse_light + specular_light;
}

/**
 * Computes the component of light contributed by a point light source.
 *
 * @param light The point light source.
 * @param normal The normal vector at the fragment.
 * @param frag_pos The position of the fragment in world coordinates.
 * @param camera_direction The direction vector pointing from the fragment to the camera.
 *
 * @return The computed light component.
 */
vec3 compute_point_component(PointLight light, vec3 normal, vec3 frag_pos, vec3 camera_direction)
{
    /*
     * Get vector pointing from the fragment to the light source.
     */
    const vec3 light_direction = normalize(light.position - frag_pos);

    /*
     * Compute ambient light component.
     */
    const float ambient_light_strength = 0.1;
    const vec3 ambient_light = ambient_light_strength * light.color;

    /*
     * Compute diffuse light component.
     */
    const float diff = max(dot(normal, light_direction), 0.0);
    const vec3 diffuse_light = diff * light.color;

    /*
     * Compute specular light component.
     */
    const float specular_light_strength = 0.5;
    const int shininess = 4;
    const vec3 reflect_direction = reflect(-light_direction, normal);
    const float shine = pow(max(dot(camera_direction, reflect_direction), 0.0), shininess);
    const vec3 specular_light = specular_light_strength * shine * light.color;

    /*
     * Compute attenuation.
     */
    const float distance = length(light.position - frag_pos);
    const float constant = 1.0;
    const float linear = 0.007;
    const float quadratic = 0.0002;
    const float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

    return attenuation * (ambient_light + diffuse_light + specular_light);
}

void main()
{
    const vec3 terrain_color = texture(u_texture_sampler, v_texture_coord).rgb;

    vec3 result = compute_directional_component(u_directional_light, v_norm, v_camera_direction);
    result += compute_point_component(u_point_light, v_norm, v_position_world_coords, v_camera_direction);

    color = vec4(result * terrain_color, 1.0);
}