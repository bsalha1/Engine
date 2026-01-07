#pragma once

#include "Drawable.h"
#include "VertexArray.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace Engine
{
    class IndexBuffer: public Drawable
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

        IndexBuffer() = delete;

        IndexBuffer(const VertexArray &_vertex_array);

        void create(const void *items, const size_t _count);

        void draw() const override;

    private:
        /**
         * Reference to vertex array to index into.
         */
        const VertexArray &vertex_array;

        /**
         * OpenGL index buffer object ID.
         */
        GLuint index_buffer_obj;

        /**
         * Number of indices.
         */
        size_t count;
    };
}