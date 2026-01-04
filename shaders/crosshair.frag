#version 460 core

out vec4 color;

uniform vec2 u_window_size;
uniform float u_size;
uniform float u_thickness;
uniform vec3 u_color;

void main()
{
    vec2 center = u_window_size * 0.5;
    vec2 frag = gl_FragCoord.xy;

    float dx = abs(frag.x - center.x);
    float dy = abs(frag.y - center.y);

    float horiz = step(dx, u_size) * step(dy, u_thickness);
    float vert = step(dy, u_size) * step(dx, u_thickness);

    float alpha = clamp(horiz + vert, 0.0, 0.5);

    if (alpha == 0.0)
    {
        discard;
    }

    color = vec4(u_color, alpha);
}