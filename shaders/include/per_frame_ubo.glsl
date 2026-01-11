#include "include/lighting_types.glsl"

#define MAX_POINT_LIGHTS 64
#define MAX_SPOT_LIGHTS 64

/**
 * Uniform Buffer Object (UBO) containing all data that changes per frame.
 */
layout (std140) uniform PerFrameUBO
{
    vec3 camera_position; /* Stored here so point_lights is aligned on a 16-byte boundary */
    uint num_point_lights;

    PointLight point_lights[MAX_POINT_LIGHTS];

    vec3 pad0;
    uint num_spot_lights;

    SpotLight spot_lights[MAX_SPOT_LIGHTS];

    DirectionalLight directional_light;

    mat4 camera_view;
    mat4 camera_projection;
    mat4 light_view_projection;
} per_frame_ubo;
