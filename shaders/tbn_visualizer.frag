#version 460 core

out vec4 color;

in vec3 g_color;

void main()
{
    color = vec4(g_color, 1.0);
}