#version 460 core

layout(location = 0) in vec4 position;

uniform mat4 model_view_projection;

void main()
{
    gl_Position = model_view_projection * position;
}
