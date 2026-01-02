#pragma once

#include "VertexArray.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Engine
{
    struct Vertex3d
    {
        glm::vec3 position;
    };
    static_assert(sizeof(Vertex3d) == 3 * sizeof(float));

    struct Vertex3dNormal
    {
        glm::vec3 position;
        glm::vec3 norm = glm::vec3(0.f);

        static void setup_vertex_array_attribs(VertexArray &vertex_array)
        {
            vertex_array.setup_vertex_attrib(0, &Vertex3dNormal::position);
            vertex_array.setup_vertex_attrib(1, &Vertex3dNormal::norm);
        }
    };
    static_assert(sizeof(Vertex3dNormal) == 6 * sizeof(float));

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

    struct Vertex3dNormalTangent
    {
        glm::vec3 position;
        glm::vec3 norm = glm::vec3(0.f);
        glm::vec4 tangent; /* (tangent.xyz, handedness) */

        static void setup_vertex_array_attribs(VertexArray &vertex_array)
        {
            vertex_array.setup_vertex_attrib(0, &Vertex3dNormalTangent::position);
            vertex_array.setup_vertex_attrib(1, &Vertex3dNormalTangent::norm);
            vertex_array.setup_vertex_attrib(2, &Vertex3dNormalTangent::tangent);
        }
    };
    static_assert(sizeof(Vertex3dNormalTangent) == 10 * sizeof(float));

    struct TexturedVertex3dNormalTangent
    {
        glm::vec3 position;
        glm::vec3 norm = glm::vec3(0.f);
        glm::vec2 texture;
        glm::vec4 tangent; /* (tangent.xyz, handedness) */

        static void setup_vertex_array_attribs(VertexArray &vertex_array)
        {
            vertex_array.setup_vertex_attrib(0, &TexturedVertex3dNormalTangent::position);
            vertex_array.setup_vertex_attrib(1, &TexturedVertex3dNormalTangent::norm);
            vertex_array.setup_vertex_attrib(2, &TexturedVertex3dNormalTangent::texture);
            vertex_array.setup_vertex_attrib(3, &TexturedVertex3dNormalTangent::tangent);
        }
    };
    static_assert(sizeof(TexturedVertex3dNormalTangent) == 12 * sizeof(float));
}