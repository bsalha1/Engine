#version 460 core

layout (location = 0) out vec4 o_color;
layout (location = 1) out vec4 o_bloom;

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

/**
 * Sun color.
 */
uniform vec3 u_sun_color;

void main()
{
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
    if (sun_presence < cos(u_sun_angular_radius))
    {
        const vec3 sky_color = texture(u_texture_sampler, v_position_model).rgb;
        o_color = vec4(sky_color, 1.0);
        o_bloom = vec4(0.0, 0.0, 0.0, 1.0);
    }
    else
    {
        o_color = vec4(u_sun_color, 1.0);
        o_bloom = vec4(2 * u_sun_color, 1.0);
    }
}