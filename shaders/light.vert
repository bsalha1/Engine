#version 460 core

layout(location = 0) in vec3 l_position;

uniform mat4 u_model_view_projection;

void main()
{
    const vec4 position_four_vector = vec4(l_position, 1.0);
    gl_Position = u_model_view_projection * position_four_vector;
}
