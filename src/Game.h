#pragma once

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <memory>

namespace Engine
{
    class Game
    {
    public:
        static std::unique_ptr<Game> create();

        bool run();

        void end();

    private:
        Game();

        bool _init();

        bool init();

        GLFWwindow *window;

        GLuint program_id;
    };
}
