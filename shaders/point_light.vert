#version 460 core

layout(location = 0) in vec3 l_position;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

void main()
{
    const vec4 position_four_vector = vec4(l_position, 1.0);
    gl_Position = u_projection * u_view * u_model * position_four_vector;
}
