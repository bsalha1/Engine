#pragma once

#include "IndexBuffer.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <glm/vec3.hpp>
#include <memory>

namespace Engine
{
    class Game
    {
    public:
        static std::unique_ptr<Game> create();

        bool run();

        void quit();

    private:
        Game();

        bool _init();

        bool init();

        void set_crouching();

        void set_standing();

        void process_menu();

        void process_jump_crouch();

        void update_view();

        void update_position();

        /**
         * Window handle.
         */
        GLFWwindow *window;

        /**
         * Shader program ID.
         */
        GLuint program_id;

        /**
         * Game state.
         */
        enum class State
        {
            RUNNING,
            PAUSED,
            QUIT,
        };
        State state;
        State state_prev;
        static const char *state_to_string(const State state);

        /**
         * Coordinate of window center on X axis.
         */
        int window_center_x;

        /**
         * Coordinate of window center on Y axis.
         */
        int window_center_y;

        /**
         * Player movement speed.
         */
        static constexpr float standing_move_speed = 1.5f;
        static constexpr float crouching_move_speed = 0.5f;
        float move_speed;

        /**
         * Player height.
         */
        static constexpr float standing_height = 3.f;
        static constexpr float crouching_height = 1.5f;
        float height;

        /**
         * Player position.
         */
        glm::vec3 position;

        /**
         * Player velocity.
         */
        glm::vec3 velocity;

        /**
         * Timestamp of last jump.
         */
        std::chrono::steady_clock::time_point last_jump_time;

        /**
         * Timestamp of last crouch.
         */
        std::chrono::steady_clock::time_point last_crouch_time;

        /**
         * Time since last frame in seconds.
         */
        float dt;

        /**
         * Whether the escape key was pressed in the previous frame.
         */
        bool escape_pressed_prev;

        /**
         * View state.
         * @{
         */

        /**
         * Whether the mouse position was set in the previous frame.
         */
        bool mouse_prev_set;

        /**
         * Mouse position in the previous frame.
         *   @{
         */
        double mouse_x_prev;
        double mouse_y_prev;
        /**
         *   @}
         */

        /**
         * Viewing angles.
         *   @{
         */
        float horizontal_angle;
        float vertical_angle;
        /**
         *   @}
         */

        /**
         * Vector pointing at what player is looking at.
         */
        glm::vec3 direction;

        /**
         * Vector pointing to the right of the player.
         */
        glm::vec3 right;

        /**
         * Vector pointing forwards in the X-Z plane.
         */
        glm::vec3 forwards;

        /**
         * Vector along player's body pointing to their head.
         */
        glm::vec3 head;

        /**
         * @}
         */

        IndexBuffer index_buffer;
    };
}
