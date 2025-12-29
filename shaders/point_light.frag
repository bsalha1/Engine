#version 460 core

layout (location = 0) out vec4 o_color;
layout (location = 1) out vec4 o_bloom;

void main()
{
    o_color = vec4(1.0, 1.0, 1.0, 1.0);
    o_bloom = vec4(5.0 * o_color.rgb, 1.0);
}