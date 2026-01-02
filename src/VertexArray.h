#pragma once

#include "Renderer.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace Engine
{
    class VertexArray: public Renderer::Drawable
    {
    public:
        VertexArray(): vertex_array_id(0)
        {}

        /**
         * @brief Create a vertex array object.
         *
         * @param vertices Vertices.
         * @param _num_vertices Number of vertices in the vertex array.
         */
        template <typename Vertex>
        void create(const Vertex *vertices, const size_t _num_vertices)
        {
            num_vertices = _num_vertices;
            glGenVertexArrays(1, &vertex_array_id);

            bind();

            GLuint buffer_id;
            glGenBuffers(1, &buffer_id);
            glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
            glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * _num_vertices, vertices, GL_STATIC_DRAW);
        }

        /**
         * @return Offset of member in vertex structure.
         */
        template <typename Vertex, typename Member>
        constexpr size_t offset_of(const Member Vertex::*member)
        {
            return reinterpret_cast<size_t>(
                &(reinterpret_cast<Vertex const volatile *>(0)->*member));
        }

        /**
         * @brief Setup a vertex attribute pointer.
         *
         * @tparam Vertex Vertex structure type.
         * @tparam Member Member type.
         *
         * @param idx Attribute index.
         * @param member Pointer to member in vertex structure.
         */
        template <typename Vertex, typename Member>
        void setup_vertex_attrib(const GLuint idx, const Member Vertex::*member)
        {
            const GLuint attrib_start_offset = offset_of(member);
            const GLuint attrib_end_offset = attrib_start_offset + sizeof(Member);
            const GLuint attrib_size = sizeof(float);
            const GLuint attrib_count = (attrib_end_offset - attrib_start_offset) / attrib_size;
            glVertexAttribPointer(idx,
                                  attrib_count,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(Vertex),
                                  reinterpret_cast<GLvoid *>(attrib_start_offset));
            glEnableVertexAttribArray(idx);
        }

        /**
         * @brief Bind the vertex array object.
         */
        void bind() const
        {
            glBindVertexArray(vertex_array_id);
        }

        /**
         * @brief Draw the vertex array.
         */
        void draw() const override
        {
            bind();
            glDrawArrays(GL_TRIANGLES, 0, num_vertices);
        }

    private:
        /**
         * OpenGL vertex array object ID.
         */
        GLuint vertex_array_id;

        /**
         * Number of vertices in the vertex array.
         */
        size_t num_vertices;
    };
}