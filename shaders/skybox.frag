#version 460 core

out vec4 color;

/**
 * Variables from vertex shader.
 */
in vec3 v_position_model;

uniform samplerCube u_texture_sampler;
uniform float u_sun_angular_radius;

/**
 * Sun position in model space of the skybox.
 */
uniform vec3 u_sun_position;

void main()
{
    const vec3 sky_color = texture(u_texture_sampler, v_position_model).rgb;

    /*
     * For each pixel position vector, this is 1 if it's looking directly at the
     * sun, close to 1 if pointing near the sun, therefore this quantifies the
     * presence of the sun in the given pixel.
     */
    const float sun_presence = dot(normalize(v_position_model), normalize(u_sun_position));

    /*
     * Form a disk around the sun going from the point of max presence to the
     * inner edge which is interpolated smoothly to the outer edge.
     */
    const float sun_disk = smoothstep(
        cos(u_sun_angular_radius * 1.5), /* inner edge */
        cos(u_sun_angular_radius), /* outer edge */
        sun_presence               /* value */);

    const vec3 sun_color = vec3(1.0, 0.95, 0.85) * 10.0;
    color = vec4(sky_color + sun_disk * sun_color, 1.0);
}