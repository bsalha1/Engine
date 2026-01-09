#include "IndexBuffer.h"

#include "log.h"

namespace Engine
{
    /**
     * @brief Constructor.
     *
     * @param _vertex_array Reference to vertex array to index into.
     */
    IndexBuffer::IndexBuffer(const VertexArray &_vertex_array):
        vertex_array(_vertex_array), index_buffer_obj(0), count(0)
    {}

    /**
     * @brief Create index buffer from given items.
     *
     * @param items Pointer to the index items.
     * @param _count Number of indices.
     */
    void IndexBuffer::create(const void *items, const size_t _count)
    {
        count = _count;

        glGenBuffers(1, &index_buffer_obj);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_obj);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, _count * sizeof(IndexType), items, GL_STATIC_DRAW);

        LOG_DEBUG("Created EBO %u: %zu indices\n", index_buffer_obj, count);
    }

    /**
     * @brief Draw the vertices using this buffer together with the vertex buffer.
     */
    void IndexBuffer::draw() const
    {
        vertex_array.bind();
        glDrawElements(GL_TRIANGLES, count, IndexGLtype, nullptr);
    }
}