#version 460 core

out vec4 color;

/**
 * Variables from vertex shader.
 */
in vec3 v_texture_coord;

uniform samplerCube u_texture_sampler;

void main()
{
    color = texture(u_texture_sampler, v_texture_coord);
    // color = vec4(1.0, 0.0, 0.0, 1.0);
}