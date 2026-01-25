#version 460 core

#include "include/per_frame_ubo.glsl"

layout(triangles) in;
layout(line_strip, max_vertices = 18) out;

in vec3 v_position_world_coords[];
in mat3 v_tangent_bitangent_normal[];

out vec3 g_color;

uniform float u_magnitude;
uniform bool u_only_show_normals;

void generate_line(uint index, uint tangent_bitangent_normal_index)
{
    const vec3 colors[3] = vec3[3](vec3(1,0,0), vec3(0,1,0), vec3(0,0,1));
    g_color = colors[tangent_bitangent_normal_index];

    vec3 normal = normalize(v_tangent_bitangent_normal[index][tangent_bitangent_normal_index]);
    vec3 base = v_position_world_coords[index];
    vec3 end = base + normal * u_magnitude;

    gl_Position = per_frame_ubo.camera_projection * per_frame_ubo.camera_view * vec4(base, 1.0);
    EmitVertex();

    gl_Position = per_frame_ubo.camera_projection * per_frame_ubo.camera_view * vec4(end, 1.0);
    EmitVertex();

    EndPrimitive();
}

void main()
{
    if (u_only_show_normals)
    {
        for (uint triangle = 0; triangle < 3; ++triangle)
        {
            generate_line(triangle, 2);
        }
    }
    else
    {
        for (uint triangle = 0; triangle < 3; ++triangle)
        {
            for (uint tangent_bitangent_normal_index = 0; tangent_bitangent_normal_index < 3; ++tangent_bitangent_normal_index)
            {
                generate_line(triangle, tangent_bitangent_normal_index);
            }
        }
    }
}