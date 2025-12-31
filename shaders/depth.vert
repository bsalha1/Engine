#version 460 core

layout (location = 0) in vec3 l_position;

uniform mat4 u_light_view_projection;
uniform mat4 u_model;

void main()
{
    gl_Position = u_light_view_projection * u_model * vec4(l_position, 1.0);
}