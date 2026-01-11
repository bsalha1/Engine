#version 460 core

#include "include/per_frame_ubo.glsl"

layout(location = 0) in vec3 l_position;

uniform mat4 u_model;

void main()
{
    const vec4 position_four_vector = vec4(l_position, 1.0);
    gl_Position = per_frame_ubo.camera_projection * per_frame_ubo.camera_view * u_model * position_four_vector;
}
