#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace Engine
{
    class VertexArray
    {
    public:
        VertexArray(): vertex_array_id(0)
        {}

        /**
         * @brief Create a vertex array object.
         *
         * @param _num_vertices Number of vertices in the vertex array.
         */
        void create(const size_t _num_vertices)
        {
            num_vertices = _num_vertices;
            glGenVertexArrays(1, &vertex_array_id);
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
        void draw()
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