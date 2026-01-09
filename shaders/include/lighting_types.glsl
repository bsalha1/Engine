/**
 * A point light source.
 *
 * Note: vec4s used for 16-byte alignment in a std140 block.
 */
struct PointLight
{
    vec4 position;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
};

/**
 * A light source assumed infinitely far away.
 *
 * Note: vec4s used for 16-byte alignment in a std140 block.
 */
struct DirectionalLight
{
    vec4 direction;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
};

/**
 * Material properties of a surface.
 */
struct Material
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};