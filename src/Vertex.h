#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Engine
{
    struct Vertex3d
    {
        glm::vec3 position;
    };
    static_assert(sizeof(Vertex3d) == 3 * sizeof(float));

    struct TexturedVertex3d
    {
        glm::vec3 position;
        glm::vec2 texture;
    };
    static_assert(sizeof(TexturedVertex3d) == 5 * sizeof(float));

    struct TexturedVertex2d
    {
        glm::vec2 position;
        glm::vec2 texture;
    };
    static_assert(sizeof(TexturedVertex2d) == 4 * sizeof(float));

    struct TexturedVertex3dNormal
    {
        glm::vec3 position;
        glm::vec3 norm = glm::vec3(0.f);
        glm::vec2 texture;
    };
    static_assert(sizeof(TexturedVertex3dNormal) == 8 * sizeof(float));
}