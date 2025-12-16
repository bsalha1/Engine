#version 460 core

out vec4 color;

/**
 * Variables from vertex shader.
 */
in vec2 v_texture_coord;

uniform sampler2D u_texture_sampler;

void main()
{
    color = texture(u_texture_sampler, v_texture_coord);
}