#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 norm;

/**
 * Variables going to fragment shader.
 */
out vec3 v_position_world_coords;
out vec3 v_norm;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    const vec4 position_four_vector = vec4(position, 1.0);
    gl_Position = projection * view * model * position_four_vector;
    v_position_world_coords = (model * position_four_vector).xyz;

    /*
     * Transform normal vector to world space.
     */
    v_norm = normalize(mat3(transpose(inverse(model))) * norm);
}
