#version 460 core

out vec4 color;

/**
 * Variables from vertex shader.
 */
in vec2 v_texture_coord;

uniform sampler2D texture_sampler;

void main()
{
    color = texture(texture_sampler, v_texture_coord);
}