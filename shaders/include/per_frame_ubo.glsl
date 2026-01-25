#include "include/lighting_types.glsl"
#include "include/texture_slots.glsl"

#define MAX_POINT_LIGHTS 64
#define MAX_SPOT_LIGHTS 64

/**
 * Uniform Buffer Object (UBO) containing all data that changes per frame.
 */
layout(std140) uniform PerFrameUniformBuffer
{
    mat4 camera_view;
    mat4 camera_projection;
    vec4 camera_position;
    vec4 shadow_map_ranges; /* x = near, y = mid, z = far */

    uint num_point_lights;
    uint num_spot_lights;
    float camera_near_clip;
    float camera_far_clip;

    mat4 light_view_projections[NUM_SHADOW_MAPS];
    PointLight point_lights[MAX_POINT_LIGHTS];
    SpotLight spot_lights[MAX_SPOT_LIGHTS];
    DirectionalLight directional_light;
} per_frame_ubo;
