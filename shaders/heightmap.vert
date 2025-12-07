#version 460 core

layout(location = 0) in vec4 position;

/**
 * Variables going to fragment shader.
 */
out vec4 v_position;

uniform mat4 model_view_projection;

void main()
{
    gl_Position = model_view_projection * position;
    v_position = position;
}
