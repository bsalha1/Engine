#pragma once

#include "CubemapTexture.h"
#include "FramebufferTexture.h"
#include "IndexBuffer.h"
#include "Renderer.h"
#include "Shader.h"
#include "Texture.h"
#include "TexturedMaterial.h"
#include "VertexArray.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <map>
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
        enum class State : uint8_t
        {
            RUNNING,
            PAUSED,
            QUIT,
        };

        enum class PlayerMovementState : uint8_t
        {
            WALKING,
            CROUCHING,
            SPRINTING,
            FLYING,
        };

        Game();

        bool _init();

        bool init();

        float cell_to_height(const int cell_x, const int cell_z) const;

        float get_terrain_height(const float x, const float z) const;

        bool process_menu();

        void update_view();

        bool update_player_movement_state_grounded(const bool fly_key_pressed,
                                                   const bool jump_key_pressed);

        void apply_player_movement_state_grounded();

        void update_player_position();

        /**
         * Window handle.
         */
        GLFWwindow *window;

        /**
         * Shader for an object which is textured and reacts to lighting.
         */
        Shader lit_textured_shader;

        /**
         * Game state.
         */
        State state;
        State state_prev;
        static const char *state_to_string(const State state);
        static const char *
        player_movement_state_to_string(const PlayerMovementState state);

        /**
         * Window dimensions in pixels.
         */
        int window_width;
        int window_height;

        /**
         * Coordinate of window center on X axis.
         */
        int window_center_x;

        /**
         * Coordinate of window center on Y axis.
         */
        int window_center_y;

        /**
         * Player movement.
         */
        PlayerMovementState player_movement_state;
        static constexpr float acceleration_gravity = 10.f;
        bool is_on_ground;
        static constexpr float friction_coeff_ground = 10.f;
        static constexpr float friction_coeff_air = 0.05f;
        static constexpr float friction_coeff_flying = 5.f;
        float friction_coeff;
        static constexpr float move_impulse_walking = 30.0f;
        static constexpr float move_impulse_sprinting = 100.0f;
        static constexpr float move_impulse_crouching = 15.0f;
        static constexpr float move_impulse_midair = 1.0f;
        static constexpr float move_impulse_flying = 150.0f;
        static constexpr float move_impulse_jump = 4000.0f;
        float player_move_impulse;

        /**
         * Player height.
         */
        static constexpr float height_standing = 1.78f;
        static constexpr float height_crouching = 1.f;
        float player_height;

        /**
         * Rising edge detectors.
         */
        bool fly_key_pressed_prev;
        bool crouch_key_pressed_prev;
        bool sprint_key_pressed_prev;
        bool jump_key_pressed_prev;

        /**
         * Player position.
         */
        glm::vec3 player_position;

        /**
         * Player velocity.
         */
        glm::vec3 player_velocity;

        /**
         * Player speed.
         */
        float player_speed;

        /**
         * Timestamp of last crouch.
         */
        std::chrono::steady_clock::time_point last_crouch_time;

        /**
         * Time since last frame in seconds.
         */
        double dt;

        /**
         * Total time passed since the start of the game in seconds.
         */
        double time_since_start;

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

        /**
         * Renderer.
         */
        Renderer renderer;

        /**
         * Chaser entity.
         * @{
         */
        glm::vec3 chaser_position;
        VertexArray chaser_vertex_array;
        TexturedMaterial chaser_textured_material =
            TexturedMaterial(glm::vec3(0.2f), /* ambient */
                             glm::vec3(0.2f), /* diffuse */
                             glm::vec3(8.f),  /* specular */
                             512.f            /* shininess */
            );
        /**
         * @}
         */

        /**
         * Terrain.
         * @{
         */
        std::vector<float> xz_to_height_map;
        int terrain_num_cols;
        int terrain_x_middle;
        int terrain_z_middle;
        VertexArray terrain_vertex_array;
        IndexBuffer terrain_index_buffer = IndexBuffer(terrain_vertex_array);
        float terrain_height;
        float on_ground_camera_y;

        TexturedMaterial dirt_textured_material =
            TexturedMaterial(glm::vec3(0.15f, 0.12f, 0.08f), /* ambient */
                             glm::vec3(0.45f, 0.36f, 0.25f), /* diffuse */
                             glm::vec3(0.02f, 0.02f, 0.02f), /* specular */
                             4.f                             /* shininess */
            );
        /**
         * @}
         */

        /**
         * Skybox.
         * @{
         */
        static constexpr float tilt = glm::radians<float>(23.5f);
        const glm::vec3 rotation_axis = glm::vec3(glm::sin(tilt), glm::cos(tilt), 0.f);
        /**
         * @}
         */

        /**
         * Lighting.
         * @{
         */
        glm::vec3 point_light_position;
        /**
         * @}
         */
    };
}
