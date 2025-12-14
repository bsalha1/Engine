#version 460 core

out vec4 color;

/**
 * Variables from vertex shader.
 */
in vec3 v_position_world_coords;
in vec3 v_norm;

uniform vec3 light_position;
uniform vec3 light_color;
uniform vec3 terrain_color;

void main()
{
    /*
     * Get vector pointing from the fragment to the light source.
     */
    const vec3 light_direction = normalize(light_position - v_position_world_coords);

    /*
     * Compute ambient light component.
     */
    const float ambient_light_strength = 0.1;
    const vec3 ambient_light = ambient_light_strength * light_color;

    /*
     * Compute diffuse light component.
     */
    const float diff = max(dot(v_norm, light_direction), 0.0);
    const vec3 diffuse_light = diff * light_color;

    /*
     * Combine lighting components with terrain color.
     */
    const vec3 result = (ambient_light + diffuse_light) * terrain_color;
    color = vec4(result, 1.0);
}