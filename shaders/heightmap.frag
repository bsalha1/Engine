#version 460 core

out vec4 color;

/**
 * Variables from vertex shader.
 */
in vec4 v_position;

void main()
{
    color = vec4(0.0, v_position.y / 64.0, 0.0, 1.0);
}