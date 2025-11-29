#pragma once

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>
#include <memory>

namespace Engine
{
    class Game
    {
    public:
        static std::unique_ptr<Game> create();

        bool run();

        void stop_run();

    private:
        Game();

        bool _init();

        bool init();

        void set_crouching();

        void set_standing();

        GLFWwindow *window;

        GLuint program_id;

        bool should_run;

        int window_center_x;

        int window_center_y;

        static constexpr float standing_move_speed = 1.5f;
        static constexpr float crouching_move_speed = 0.5f;
        float move_speed;

        static constexpr float standing_height = 3.f;
        static constexpr float crouching_height = 1.5f;
        float height;

        glm::vec3 position;
    };
}
