#version 460 core

out vec4 color;

/**
 * Variables from vertex shader.
 */
in vec3 v_position_world_coords;
in vec3 v_norm;
in vec2 v_texture_coord;

uniform sampler2D texture_sampler;
uniform vec3 light_position;
uniform vec3 light_color;
uniform vec3 terrain_color;
uniform vec3 camera_position;

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
     * Compute specular light component.
     */
    const float specular_light_strength = 0.5;
    const int shininess = 4;
    const vec3 camera_direction = normalize(camera_position - v_position_world_coords);
    const vec3 reflect_direction = reflect(-light_direction, v_norm);
    const float shine = pow(max(dot(camera_direction, reflect_direction), 0.0), shininess);
    const vec3 specular_light = specular_light_strength * shine * light_color;  

    /*
     * Combine lighting components with terrain color.
     */
    const vec3 terrain_color = texture(texture_sampler, v_texture_coord).rgb;
    const vec3 result = (ambient_light + diffuse_light + specular_light) * terrain_color;
    color = vec4(result, 1.0);
}