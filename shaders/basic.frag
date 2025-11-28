#version 460 core

layout(location = 0) out vec3 color;

in vec3 fragment_color;

void main()
{
    color = fragment_color;
}