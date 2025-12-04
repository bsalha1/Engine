#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace Engine
{
    class IndexBuffer
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

        IndexBuffer(): index_buffer_obj(0), count(0)
        {}

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

        inline size_t get_count() const
        {
            return count;
        }

    private:
        GLuint index_buffer_obj;
        size_t count;
    };
}