#pragma once

#include "CubemapTexture.h"
#include "FramebufferTexture.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Texture.h"
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

        enum class PlayerState : uint8_t
        {
            WALKING,
            CROUCHING,
            SPRINTING,
            FLYING,
            MIDAIR,
        };

        Game();

        bool _init();

        bool init();

        float cell_to_height(const int cell_x, const int cell_z) const;

        float get_terrain_height(const float x, const float z) const;

        bool process_menu();

        void update_view();

        bool update_player_state_grounded(const bool fly_key_pressed);

        void update_player_position();

        bool draw_non_blooming_objects(const glm::mat4 view,
                                       const glm::mat4 &chaser_model,
                                       const glm::mat4 &terrain_model,
                                       const glm::vec3 &directional_light_direction,
                                       const float sun_brightness);

        bool draw_blooming_objects(const glm::mat4 view);

        bool draw_skybox(const glm::mat4 view, const float orbital_angle);

        bool draw(const glm::mat4 &chaser_model,
                  const glm::mat4 &terrain_model,
                  const glm::vec3 &directional_light_direction,
                  const float orbital_angle,
                  const float sun_brightness);

        /**
         * Window handle.
         */
        GLFWwindow *window;

        /**
         * Basic textured shader program ID.
         */
        Shader basic_textured_shader;

        /**
         * Heightmap shader program ID.
         */
        Shader terrain_shader;

        /**
         * Game state.
         */
        State state;
        State state_prev;
        static const char *state_to_string(const State state);
        static const char *player_state_to_string(const PlayerState state);

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

        glm::mat4 projection;

        PlayerState player_state;

        /**
         * Player movement.
         */
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
         * Time on ground in seconds.
         */
        float time_on_ground;

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
        bool crouch_button_pressed_prev;

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
         * Screen buffer.
         * @{
         */
        float exposure;
        float gamma;
        float sharpness;
        Shader screen_shader;
        VertexArray quad_textured_vertex_array;
        GLuint screen_frame_buffer;
        FramebufferTexture screen_color_texture;
        FramebufferTexture screen_bloom_texture;

        Shader gaussian_blur_shader;
        std::array<GLuint, 2> ping_pong_frame_buffer;
        std::array<FramebufferTexture, 2> ping_pong_texture;
        /**
         * @}
         */

        /**
         * Chaser entity.
         * @{
         */
        glm::vec3 chaser_position;
        Texture chaser_texture;
        VertexArray chaser_vertex_array;
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
        Texture dirt_texture;
        IndexBuffer terrain_index_buffer;
        VertexArray terrain_vertex_array;
        float terrain_height;
        float on_ground_camera_y;
        /**
         * @}
         */

        /**
         * Skybox.
         * @{
         */
        VertexArray skybox_vertex_array;
        Shader skybox_shader;
        CubemapTexture skybox_texture;

        static constexpr float tilt = glm::radians<float>(23.5f);
        const glm::vec3 rotation_axis = glm::vec3(glm::sin(tilt), glm::cos(tilt), 0.f);
        /**
         * @}
         */

        /**
         * Lighting.
         * @{
         */
        static constexpr glm::vec3 point_light_color =
            glm::vec3(0xFF, 0xDF, 0x22) / 255.f;
        static constexpr glm::vec3 sun_color = glm::vec3(1.0f, 0.95f, 0.85f);
        Shader light_shader;
        glm::vec3 point_light_position;
        /**
         * @}
         */
    };
}
