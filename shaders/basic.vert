#version 460 core

layout(location = 0) in vec4 position;
out vec3 fragment_color;
uniform mat4 model_view_projection;

void main()
{
    gl_Position = model_view_projection * position;
    fragment_color = normalize(position.xyz + (1.1, 1.1, 1.1));
}

