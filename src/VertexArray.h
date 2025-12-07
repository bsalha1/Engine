#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace Engine
{
    class VertexArray
    {
    public:
        VertexArray(): vertex_array_obj(0)
        {}

        void create()
        {
            glGenVertexArrays(1, &vertex_array_obj);
        }

        void bind() const
        {
            glBindVertexArray(vertex_array_obj);
        }

    private:
        GLuint vertex_array_obj;
    };
}