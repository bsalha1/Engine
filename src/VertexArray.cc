
#include "VertexArray.h"

namespace Engine
{
    VertexArray::VertexArray(): vertex_array_id(0)
    {}

    /**
     * @brief Bind the vertex array object.
     */
    void VertexArray::bind() const
    {
        LOG_DEBUG("Binding VAO %u\n", vertex_array_id);
        glBindVertexArray(vertex_array_id);
    }

    /**
     * @brief Draw the vertex array.
     */
    void VertexArray::draw() const
    {
        LOG_DEBUG("Drawing VAO %u\n", vertex_array_id);
        bind();
        glDrawArrays(GL_TRIANGLES, 0, num_vertices);
    }
}