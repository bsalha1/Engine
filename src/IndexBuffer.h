#pragma once

#include "Renderer.h"
#include "VertexArray.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace Engine
{
    class IndexBuffer: public Renderer::Drawable
    {
    public:
        /**
         * Type of index.
         */
        using IndexType = unsigned int;

        /**
         * Type of index in terms of OpenGL.
         */
        static constexpr GLenum IndexGLtype = GL_UNSIGNED_INT;

        IndexBuffer(const VertexArray &_vertex_array):
            vertex_array(_vertex_array), index_buffer_obj(0), count(0)
        {}

        /**
         * @brief Create index buffer from given items.
         *
         * @param items Pointer to the index items.
         * @param _count Number of indices.
         */
        void create(const void *items, const size_t _count)
        {
            count = _count;

            glGenBuffers(1, &index_buffer_obj);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_obj);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         _count * sizeof(IndexType),
                         items,
                         GL_STATIC_DRAW);
        }

        /**
         * @brief Draw the vertices using this buffer together with the vertex buffer.
         */
        void draw() const override
        {
            vertex_array.bind();
            glDrawElements(GL_TRIANGLES, count, IndexGLtype, nullptr);
        }

    private:
        const VertexArray &vertex_array;
        GLuint index_buffer_obj;
        size_t count;
    };
}