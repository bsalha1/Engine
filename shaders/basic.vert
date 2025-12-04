#version 460 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texture_coord;

/**
 * Variables going to fragment shader.
 */
out vec2 v_texture_coord;

uniform mat4 model_view_projection;

void main()
{
    gl_Position = model_view_projection * position;
    v_texture_coord = texture_coord;
}
