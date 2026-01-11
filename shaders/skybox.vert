#version 460 core

#include "include/per_frame_ubo.glsl"

layout(location = 0) in vec3 l_position;

/**
 * Variables going to fragment shader.
 */
out vec3 v_position_model;

uniform mat4 u_view;

void main()
{
    v_position_model = l_position;

    vec4 pos = per_frame_ubo.camera_projection * u_view * vec4(l_position, 1.0);

    /*
     * Set w component to the z component to ensure depth is at maximum value.
     */
    gl_Position = pos.xyww;
}
