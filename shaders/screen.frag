#version 460 core

out vec4 color;

in vec2 v_texture_coord;

uniform sampler2D u_texture_sampler;
uniform float u_sharpness;
uniform float u_exposure;
uniform float u_gamma;

vec3 inversion_filter(const vec3 in_color)
{
    return vec3(1.0 - in_color);
}

vec3 greyscale_filter(const vec3 in_color)
{
    const float average = (in_color.r + in_color.g + in_color.b) / 3.0;
    return vec3(average, average, average);
}

vec3 sharpen_filter()
{
    float offset = 1.0 / u_sharpness;
    vec2 offsets[9] = vec2[](
        vec2(-offset,  offset),
        vec2( 0.0f,    offset),
        vec2( offset,  offset),
        vec2(-offset,  0.0f),
        vec2( 0.0f,    0.0f),
        vec2( offset,  0.0f),
        vec2(-offset, -offset),
        vec2( 0.0f,   -offset),
        vec2( offset, -offset)
    );

    float kernel[9] = float[](
        -1, -1, -1,
        -1,  9, -1,
        -1, -1, -1
    );

    vec3 sample_texture[9];
    for(int i = 0; i < 9; i++)
    {
        sample_texture[i] = vec3(texture(u_texture_sampler, v_texture_coord.st + offsets[i]));
    }
    vec3 col = vec3(0.0);
    for(int i = 0; i < 9; i++)
    {
        col += sample_texture[i] * kernel[i];
    }

    return col;
}


void main()
{
    const vec3 sharpened = sharpen_filter();
    vec3 mapped = vec3(1.0) - exp(-sharpened * u_exposure);
    mapped = pow(mapped, vec3(1.0 / u_gamma));
  
    color = vec4(mapped, 1.0);
}