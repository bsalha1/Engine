#version 460 core

layout(location = 0) in vec3 l_position;
layout(location = 1) in vec2 l_texture_coord;

out vec2 v_texture_coord;

void main()
{
    gl_Position = vec4(l_position.x, l_position.y, 0, 1.0);
    v_texture_coord = l_texture_coord;
}
