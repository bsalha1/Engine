#version 460 core

out vec4 o_color;

in vec2 v_texture_coord;

uniform sampler2D u_texture_sampler;

uniform bool u_horizontal;
uniform float u_weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    const vec2 tex_offset = 1.0 / textureSize(u_texture_sampler, 0);
    vec3 result = texture(u_texture_sampler, v_texture_coord).rgb * u_weight[0];
    if(u_horizontal)
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(u_texture_sampler, v_texture_coord + vec2(tex_offset.x * i, 0.0)).rgb * u_weight[i];
            result += texture(u_texture_sampler, v_texture_coord - vec2(tex_offset.x * i, 0.0)).rgb * u_weight[i];
        }
    }
    else
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(u_texture_sampler, v_texture_coord + vec2(0.0, tex_offset.y * i)).rgb * u_weight[i];
            result += texture(u_texture_sampler, v_texture_coord - vec2(0.0, tex_offset.y * i)).rgb * u_weight[i];
        }
    }

    o_color = vec4(result, 1.0);
}