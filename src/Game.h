#pragma once

#include "CubemapTexture.h"
#include "FramebufferTexture.h"
#include "IndexBuffer.h"
#include "MenuManager.h"
#include "Model.h"
#include "PauseMenu.h"
#include "Renderer.h"
#include "Shader.h"
#include "Texture.h"
#include "TextureSlot.h"
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

        /**
         * @brief Keyboard inputs.
         */
        struct UserInputs
        {
            bool forwards;
            bool backwards;
            bool left;
            bool right;

            bool jump;
            bool jump_rising_edge;

            bool crouch;
            bool crouch_rising_edge;

            bool sprint;
            bool sprint_rising_edge;

            bool fly;
            bool fly_rising_edge;

            bool left_click;
            bool left_click_rising_edge;
        };

        /**
         * @brief Projectile structure.
         */
        struct Projectile
        {
            glm::vec3 color;
            Renderer::Transform transform;
            Drawable *drawable;
            glm::vec3 velocity;
            glm::vec3 rotation_axis;
            float angular_velocity;
            float time_to_live;
        };

        Game();

        bool _init();

        bool init();

        float cell_to_height(const int cell_x, const int cell_z) const;

        float get_terrain_height(const float x, const float z) const;

        bool process_menu();

        void update_stats();

        void update_view();

        bool update_player_movement_state_grounded();

        void apply_player_movement_state_grounded();

        void get_user_inputs();

        void update_player_position();

        void update_player_actions();

        /**
         * Window handle.
         */
        GLFWwindow *window = nullptr;

        /**
         * Shader for an object which is textured and reacts to lighting.
         */
        Shader lit_textured_shader;

        /**
         * Game state.
         */
        State state = State::RUNNING;
        State state_prev = State::RUNNING;
        static const char *state_to_string(const State state);
        static const char *player_movement_state_to_string(const PlayerMovementState state);

        /**
         * Window dimensions in pixels.
         */
        int window_width = 0;
        int window_height = 0;

        /**
         * Coordinate of window center on X axis.
         */
        int window_center_x = 0;

        /**
         * Coordinate of window center on Y axis.
         */
        int window_center_y = 0;

        /**
         * Player movement.
         */
        PlayerMovementState player_movement_state = PlayerMovementState::WALKING;
        static constexpr float acceleration_gravity = 10.f;
        bool is_on_ground = true;
        static constexpr float friction_coeff_ground = 10.f;
        static constexpr float friction_coeff_air = 0.05f;
        static constexpr float friction_coeff_flying = 5.f;
        float friction_coeff = friction_coeff_ground;
        static constexpr float move_impulse_walking = 30.0f;
        static constexpr float move_impulse_sprinting = 100.0f;
        static constexpr float move_impulse_crouching = 15.0f;
        static constexpr float move_impulse_midair = 1.0f;
        static constexpr float move_impulse_flying = 150.0f;
        static constexpr float move_impulse_jump = 4000.0f;
        float player_move_impulse = move_impulse_walking;

        /**
         * Player height.
         */
        static constexpr float height_standing = 2.f;
        static constexpr float height_crouching = 1.f;
        float player_height = height_standing;

        /**
         * User inputs from peripherals i.e. mouse and keyboard.
         */
        UserInputs user_inputs = {};

        /**
         * Player position.
         */
        glm::vec3 player_position = glm::vec3(0.f, player_height, 0.f);

        /**
         * Player velocity.
         */
        glm::vec3 player_velocity = glm::vec3(0.f);

        /**
         * Player speed.
         */
        float player_speed = 0.f;

        /**
         * Time since last frame in seconds.
         */
        double dt = 0.0;

        /**
         * Whether the escape key was pressed in the previous frame.
         */
        bool escape_pressed_prev = false;

        /**
         * View state.
         * @{
         */

        /**
         * Whether the mouse position was set in the previous frame.
         */
        bool mouse_prev_set = false;

        /**
         * Mouse position in the previous frame.
         *   @{
         */
        double mouse_x_prev = 0.0;
        double mouse_y_prev = 0.0;
        /**
         *   @}
         */

        /**
         * Viewing angles.
         *   @{
         */
        float horizontal_angle = 0.f;
        float vertical_angle = 0.f;
        /**
         *   @}
         */

        /**
         * Vector pointing at what player is looking at.
         */
        glm::vec3 direction = glm::vec3(0.f);

        /**
         * Vector pointing to the right of the player.
         */
        glm::vec3 right = glm::vec3(0.f);

        /**
         * Vector pointing forwards in the X-Z plane.
         */
        glm::vec3 forwards = glm::vec3(0.f);

        /**
         * Vector along player's body pointing to their head.
         */
        glm::vec3 head = glm::vec3(0.f);

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
        glm::vec3 chaser_position = glm::vec3(0.f, 0.f, 10.f);
        VertexArray chaser_vertex_array;

        Texture chaser_normal_map = Texture(TextureSlot::NORMAL);
        TexturedMaterial chaser_textured_material = TexturedMaterial(glm::vec3(0.2f), /* ambient */
                                                                     glm::vec3(0.2f), /* diffuse */
                                                                     glm::vec3(8.f),  /* specular */
                                                                     512.f /* shininess */
        );
        /**
         * @}
         */

        /**
         * Terrain.
         * @{
         */
        std::vector<float> xz_to_height_map;
        int terrain_num_cols = 0;
        int terrain_x_middle = 0;
        int terrain_z_middle = 0;
        VertexArray terrain_vertex_array;
        IndexBuffer terrain_index_buffer = IndexBuffer(terrain_vertex_array);
        float terrain_height = 0.f;
        float on_ground_camera_y = 0.f;

        Texture dirt_normal_map = Texture(TextureSlot::NORMAL);
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
        float orbital_angle = glm::pi<float>();
        glm::vec3 point_light_position = glm::vec3(150.f, 100.f, 120.f);
        /**
         * @}
         */

        /**
         * Statistics.
         * @{
         */
        static constexpr size_t num_stats_frames = 1024;
        std::array<double, num_stats_frames> stats_dt_buffer = {};
        size_t stats_dt_buffer_idx = 0;
        size_t stats_frames = 0;
        double stats_dt_sum = 0.0;
        double stats_dt_sq_sum = 0.0;
        GLint stats_free_vram_MB = 0;
        GLint stats_total_vram_MB = 0;
        unsigned long stats_ram_usage_MB = 0;
        /**
         * @}
         */

        /**
         * Menus.
         * @{
         */
        MenuManager menu_manager;
        PauseMenu pause_menu;
        /**
         * @}
         */

        /**
         * Projectiles.
         * @{
         */
        std::vector<Projectile> projectiles;
        /**
         * @}
         */

        /**
         * Models.
         * @{
         */
        Model backpack_model;
        /**
         * @}
         */
    };
}
