#include "Game.h"

#include "assert_util.h"
#include "log.h"

#include <GLFW/glfw3.h>
#include <array>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <execinfo.h>
#include <fstream>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <sstream>
#include <stb/stb_image.h>
#include <string>
#include <unistd.h>

using namespace std::chrono_literals;

namespace Engine
{
    struct __attribute__((packed)) TexturedVertex3d
    {
        float x;
        float y;
        float z;

        float texture_x;
        float texture_y;
    };

    struct __attribute__((packed)) Vertex3d
    {
        float x;
        float y;
        float z;
    };

    static void gl_debug_message_callback(GLenum source,
                                          GLenum type,
                                          GLuint id,
                                          GLenum severity,
                                          GLsizei length,
                                          const GLchar *message,
                                          const void *user_param)
    {
        if (type == GL_DEBUG_TYPE_ERROR)
        {
            LOG_ERROR("OpenGL error: %s\n", message);

            /*
             * Dump stack trace so we can trace back where the error occurred.
             */
            void *bt[128];
            const int bt_size = backtrace(bt, 128);
            char **bt_syms = backtrace_symbols(bt, bt_size);
            if (bt_syms == nullptr)
            {
                LOG_ERROR("Failed to get stack trace symbols\n");
            }
            else
            {
                LOG_ERROR("Stack trace:\n");
                for (int i = 0; i < bt_size; i++)
                {
                    LOG_ERROR("  %s\n", bt_syms[i]);
                }

                free(bt_syms);
            }

            /*
             * Stop the run loop.
             */
            Game *game = reinterpret_cast<Game *>(const_cast<void *>(user_param));
            game->quit();
        }

#ifdef DEBUG
        else
        {
            LOG("OpenGL debug: %s\n", message);
        }
#endif
    }

    /**
     * Constructor.
     */
    Game::Game():
        window(nullptr),
        state(State::RUNNING),
        state_prev(State::RUNNING),
        window_center_x(0),
        window_center_y(0),
        is_on_ground(true),
        time_on_ground(0.f),
        friction_coeff(friction_coeff_ground),
        player_state(PlayerState::WALKING),
        player_move_impulse(move_impulse_walking),
        player_height(height_standing),
        fly_key_pressed_prev(false),
        crouch_button_pressed_prev(false),
        player_position(0.f, player_height, -10.f),
        player_velocity(0.f, 0.f, 0.f),
        last_crouch_time(std::chrono::steady_clock::now()),
        escape_pressed_prev(false),
        mouse_prev_set(false),
        mouse_x_prev(0.0),
        mouse_y_prev(0.0),
        horizontal_angle(0.f),
        vertical_angle(0.f),
        direction(0.f, 0.f, 0.f),
        right(0.f, 0.f, 0.f),
        forwards(0.f, 0.f, 0.f),
        head(0.f, 0.f, 0.f),
        chaser_position(0.f, 0.f, 0.f)
    {}

    /**
     * Create and initialize instance of a Game.
     *
     * @return Instance of game on success, otherwise nullptr.
     */
    std::unique_ptr<Game> Game::create()
    {
        std::unique_ptr<Game> new_game(new Game());
        ASSERT_RET_IF_NOT(new_game->init(), nullptr);
        return new_game;
    }

    /**
     * Initialize the game.
     *
     * @return True on success, otherwise false.
     */
    bool Game::_init()
    {
        LOG("Creating window\n");

        glfwWindowHint(GLFW_SAMPLES, 4);               /* 4x antialiasing */
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); /* OpenGL 4.6 */
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        /*
         * Create window.
         */
        window = glfwCreateWindow(960, 720, "Game", nullptr, nullptr);
        ASSERT_RET_IF(window == nullptr, false);
        glfwMakeContextCurrent(window);

        /*
         * Get the actual window's size. Window managers can disobey our request.
         */
        glfwGetFramebufferSize(window, &window_width, &window_height);
        window_center_x = window_width / 2;
        window_center_y = window_height / 2;

        /*
         * Hide the cursor and move it to the center of the window.
         */
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPos(window, window_center_x, window_center_y);

        /*
         * Enable blending for transparent/translucent textures.
         */
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);

        LOG("Initializing GLEW\n");
        ASSERT_RET_IF_GLEW_NOT_OK(glewInit(), false);

        LOG("OpenGL version: %s\n", glGetString(GL_VERSION));

        /*
         * Enable debug message callback.
         */
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(gl_debug_message_callback, this);

        /*
         * Draw fragments closer to camera over the fragments behind.
         */
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        LOG("Creating entity buffers\n");

        /*
         * Create chaser buffers.
         */
        {
            /* clang-format off */
            const std::array<TexturedVertex3d, 8> vertices = {{
            /*      x,    y,   z,  texture_x, texture_y        */
                {-1.f, -1.f, 1.f,        0.f,       0.f}, /* 0 */
                { 1.f, -1.f, 1.f,        1.f,       0.f}, /* 1 */
                { 1.f,  1.f, 1.f,        1.f,       1.f}, /* 2 */
                {-1.f,  1.f, 1.f,        0.f,       1.f}, /* 3 */
                {-1.f, -1.f, -1.f,       0.f,       0.f}, /* 4 */
                { 1.f, -1.f, -1.f,       1.f,       0.f}, /* 5 */
                { 1.f,  1.f, -1.f,       1.f,       1.f}, /* 6 */
                {-1.f,  1.f, -1.f,       0.f,       1.f}, /* 7 */
            }};
            /* clang-format on */

            chaser_vertex_array.create();
            chaser_vertex_array.bind();

            GLuint chaser_buffer_obj;
            glGenBuffers(1, &chaser_buffer_obj);
            glBindBuffer(GL_ARRAY_BUFFER, chaser_buffer_obj);
            glBufferData(
                GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);

            /*
             * Position coordinate attribute.
             */
            static constexpr GLuint position_coord_attrib_index = 0;
            static constexpr GLuint position_coord_attrib_start_offset =
                offsetof(TexturedVertex3d, x);
            static constexpr GLuint position_coord_attrib_end_offset =
                offsetof(TexturedVertex3d, z);
            static constexpr GLuint position_coord_attrib_size = sizeof(float);
            static constexpr GLuint position_coord_attrib_count =
                (position_coord_attrib_end_offset - position_coord_attrib_start_offset +
                 position_coord_attrib_size) /
                position_coord_attrib_size;
            glVertexAttribPointer(position_coord_attrib_index,
                                  position_coord_attrib_count,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(TexturedVertex3d),
                                  (GLvoid *)position_coord_attrib_start_offset);
            glEnableVertexAttribArray(position_coord_attrib_index);

            /*
             * Texture coordinate attribute.
             */
            static constexpr GLuint texture_coord_attrib_index = 1;
            static constexpr GLuint texture_coord_attrib_start_offset =
                offsetof(TexturedVertex3d, texture_x);
            static constexpr GLuint texture_coord_attrib_end_offset =
                offsetof(TexturedVertex3d, texture_y);
            static constexpr GLuint texture_coord_attrib_size = sizeof(float);
            static constexpr GLuint texture_coord_attrib_count =
                (texture_coord_attrib_end_offset - texture_coord_attrib_start_offset +
                 texture_coord_attrib_size) /
                texture_coord_attrib_size;
            glVertexAttribPointer(texture_coord_attrib_index,
                                  texture_coord_attrib_count,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(TexturedVertex3d),
                                  (GLvoid *)texture_coord_attrib_start_offset);
            glEnableVertexAttribArray(texture_coord_attrib_index);

            const std::array<unsigned int, 36> indices = {
                /* +Z face */
                0,
                1,
                2,
                2,
                3,
                0,

                /* -Z face */
                4,
                5,
                6,
                6,
                7,
                4,

                /* +X face */
                1,
                5,
                6,
                6,
                2,
                1,

                /* -X face */
                4,
                0,
                3,
                3,
                7,
                4,

                /* +Y face */
                3,
                2,
                6,
                6,
                7,
                3,

                /* -Y face */
                0,
                1,
                5,
                5,
                4,
                0,
            };

            chaser_index_buffer.create(reinterpret_cast<const void *>(indices.data()),
                                       sizeof(indices));
        }

        LOG("Compiling shaders\n");
        ASSERT_RET_IF_NOT(basic_shader.compile("basic"), false);
        ASSERT_RET_IF_NOT(heightmap_shader.compile("heightmap"), false);

        LOG("Loading textures\n");
        {
            GLuint texture_obj;
            glGenTextures(1, &texture_obj);
            glBindTexture(GL_TEXTURE_2D, texture_obj);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            stbi_set_flip_vertically_on_load(1);
            int width;
            int length;
            int channels;
            uint8_t *texture_buffer =
                stbi_load("textures/obama.png", &width, &length, &channels, 0);
            if (texture_buffer == nullptr)
            {
                LOG_ERROR("Failed to load texture\n");
                return false;
            }

            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA8,
                         width,
                         length,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         texture_buffer);

            stbi_image_free(texture_buffer);

            glActiveTexture(GL_TEXTURE0);
        }

        LOG("Loading terrain heightmaps\n");
        {
            stbi_set_flip_vertically_on_load(0);
            int terrain_length;
            int terrain_channels;
            uint8_t *heightmap = stbi_load("terrain/iceland_heightmap.png",
                                           &terrain_width,
                                           &terrain_length,
                                           &terrain_channels,
                                           0);
            if (heightmap == nullptr)
            {
                LOG_ERROR("Failed to load heightmap\n");
                return false;
            }

            /*
             * Load the heightmap in with (x,z) = (0,0) being in the middle and y going
             * from y_bottom to y_top.
             *
             * Lay out the vertices like (x,y,z):
             *
             * (0,height(0,0),0)   (1,height(1,0),0)   (2,height(2,0),0)
             * 0-------------------2 . . .       . . . 4
             * |                  /.                 . .
             * |               /   .               .   .
             * |            /      .             .     .
             * |         /
             * |      /            .      .            .
             * |   /               .    .              .
             * |/                  .  .                .
             * 1 . . .       . . . 3 . . .             5
             * (0,height(0,1),1)   (1,height(1,1),1)   (2,height(2,1),1)
             *
             */
            static constexpr unsigned int num_indices_per_vertex = 2;
            terrain_x_middle = terrain_length / 2.f;
            terrain_z_middle = terrain_width / 2.f;
            static constexpr float y_top = 64.f;
            static constexpr float y_bottom = 0.f;
            float y_scale = y_top / 0xFF;

            const unsigned int num_vertices = terrain_length * terrain_width;
            const unsigned int num_indices =
                (terrain_length - 1) * terrain_width * num_indices_per_vertex;

            std::vector<Vertex3d> vertices;
            vertices.reserve(num_vertices);

            std::vector<unsigned int> indices;
            indices.reserve(num_indices);

            xz_to_height_map.reserve(num_vertices);

            for (int x = 0; x < terrain_length; x++)
            {
                for (int z = 0; z < terrain_width; z++)
                {
                    const uint8_t y =
                        heightmap[(terrain_width * x + z) * terrain_channels];

                    const float x_coord = x - terrain_x_middle;
                    const float y_coord = y * y_scale + y_bottom;
                    const float z_coord = z - terrain_z_middle;
                    vertices.push_back({.x = x_coord, .y = y_coord, .z = z_coord});

                    xz_to_height_map.push_back(y_coord);

                    if (x != terrain_length - 1)
                    {
                        for (unsigned int index = 0; index < num_indices_per_vertex;
                             index++)
                        {
                            indices.push_back(terrain_width * (x + index) + z);
                        }
                    }
                }
            }

            stbi_image_free(heightmap);

            terrain_vertex_array.create();
            terrain_vertex_array.bind();

            GLuint terrain_vertex_buffer_array_obj;
            glGenBuffers(1, &terrain_vertex_buffer_array_obj);
            glBindBuffer(GL_ARRAY_BUFFER, terrain_vertex_buffer_array_obj);
            glBufferData(GL_ARRAY_BUFFER,
                         vertices.size() * sizeof(Vertex3d),
                         vertices.data(),
                         GL_STATIC_DRAW);

            /*
             * Position coordinate attribute.
             */
            static constexpr GLuint position_coord_attrib_index = 0;
            static constexpr GLuint position_coord_attrib_start_offset =
                offsetof(Vertex3d, x);
            static constexpr GLuint position_coord_attrib_end_offset =
                offsetof(Vertex3d, z);
            static constexpr GLuint position_coord_attrib_size = sizeof(float);
            static constexpr GLuint position_coord_attrib_count =
                (position_coord_attrib_end_offset - position_coord_attrib_start_offset +
                 position_coord_attrib_size) /
                position_coord_attrib_size;
            glVertexAttribPointer(position_coord_attrib_index,
                                  position_coord_attrib_count,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(Vertex3d),
                                  (GLvoid *)position_coord_attrib_start_offset);
            glEnableVertexAttribArray(position_coord_attrib_index);

            terrain_index_buffer.create(indices.data(), indices.size());
        }

        LOG("Initializing GUI\n");
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 460");

        return true;
    }

    /**
     * Initialize the game.
     *
     * @return True on success, otherwise false.
     */
    bool Game::init()
    {
        LOG("Initializing GLFW\n");
        ASSERT_RET_IF_NOT(glfwInit(), false);

        if (!_init())
        {
            glfwTerminate();
            return false;
        }

        return true;
    }

    /**
     * @return Terrain height at given (x, z) world coordinates.
     *
     * @param x X world coordinate.
     * @param z Z world coordinate.
     */
    float Game::get_terrain_height(const float x, const float z) const
    {
        /*
         * Convert from world coordinates to heightmap coordinates. To be smooth,
         * linearly interpolate the height between the 3 vertices of the triangle the
         * point is in.
         *
         *     |
         *     |   0--2
         *     |   |P/|
         *     |   |/ |
         *     |   1--3
         * x ---------------
         *     |
         *     z
         */
        const float x_terrain = x + terrain_x_middle;
        const float z_terrain = z + terrain_z_middle;

        const int cell_x_left = static_cast<int>(std::floor(x + terrain_x_middle));
        const int cell_x_right = static_cast<int>(std::ceil(x + terrain_x_middle));
        const int cell_z_down = static_cast<int>(std::floor(z + terrain_z_middle));
        const int cell_z_up = static_cast<int>(std::ceil(z + terrain_z_middle));

        const float dx = x_terrain - cell_x_left;
        const float dz = z_terrain - cell_z_down;

        const float y2 = xz_to_height_map[terrain_width * cell_x_right + cell_z_up];

        /*
         * In bottom-right triangle.
         */
        if (dx > dz)
        {
            const float y0 =
                xz_to_height_map[terrain_width * cell_x_left + cell_z_down];
            const float y3 =
                xz_to_height_map[terrain_width * cell_x_right + cell_z_down];
            const float x_slope = (y3 - y0);
            const float z_slope = (y2 - y3);
            return y0 + x_slope * dx + z_slope * dz;
        }

        /*
         * In top-left triangle.
         */
        else
        {
            const float y0 =
                xz_to_height_map[terrain_width * cell_x_left + cell_z_down];
            const float y1 = xz_to_height_map[terrain_width * cell_x_left + cell_z_up];
            const float x_slope = (y2 - y1);
            const float z_slope = (y1 - y0);
            return y0 + x_slope * dx + z_slope * dz;
        }
    }

    /**
     * Process menu input.
     */
    void Game::process_menu()
    {
        const bool escape_pressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        const bool escape_pressed_rising_edge = escape_pressed && !escape_pressed_prev;
        escape_pressed_prev = escape_pressed;
        if (escape_pressed_rising_edge)
        {
            if (state == State::PAUSED)
            {
                state = State::RUNNING;

                /*
                 * Put the mouse back to where it was before pausing and hide it again.
                 */
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                glfwSetCursorPos(window, mouse_x_prev, mouse_y_prev);
            }
            else
            {
                state = State::PAUSED;

                /*
                 * Show the mouse cursor and put it in the middle of window.
                 */
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                glfwSetCursorPos(window, window_center_x, window_center_y);
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        /*
         * Configure the pause menu if paused.
         */
        if (state == State::PAUSED)
        {
            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->GetCenter(),
                                    ImGuiCond_Always,
                                    ImVec2(0.5f, 0.5f));
            ImGui::Begin("Pause Menu",
                         nullptr,
                         ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings);
            ImGui::Text("Press ESC to unpause");
            ImGui::Separator();
            if (ImGui::Button("Settings"))
            {
                LOG("Pause Menu -> Settings Menu\n");
            }
            ImGui::Separator();
            if (ImGui::Button("Quit"))
            {
                LOG("Pause Menu -> Quitting\n");
                quit();
            }
            ImGui::End();
        }

        ImGuiIO &io = ImGui::GetIO();

        /*
         * Show FPS window in top left.
         */
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::Begin("Stats",
                     nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("%.3f ms (%.0f FPS)", 1000.f / io.Framerate, io.Framerate);
        ImGui::Text("state: %s", state_to_string(state));
        ImGui::Text("player state: %s", player_state_to_string(player_state));
        ImGui::Text("player_position: (%.2f, %.2f, %.2f)",
                    player_position.x,
                    player_position.y,
                    player_position.z);
        ImGui::Text("player_velocity: (%.2f, %.2f, %.2f) (%.2f m/s)",
                    player_velocity.x,
                    player_velocity.y,
                    player_velocity.z,
                    glm::length(player_velocity));
        ImGui::Text("move_impulse: %.2f", player_move_impulse);
        ImGui::Text("friction_coeff: %.2f", friction_coeff);
        ImGui::End();
    }

    /**
     * Update view based on mouse movement.
     */
    void Game::update_view()
    {
        /*
         * Get mouse player_position relative to top-left pixel of the window.
         */
        double mouse_x;
        double mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);

        /*
         * Generate horizontal and vertical viewing angles from mouse player_position.
         */
        if (!mouse_prev_set)
        {
            mouse_x_prev = mouse_x;
            mouse_y_prev = mouse_y;
            mouse_prev_set = true;
        }
        else
        {
            const double x_offset = mouse_x - mouse_x_prev;
            const double y_offset = mouse_y_prev - mouse_y;
            mouse_x_prev = mouse_x;
            mouse_y_prev = mouse_y;

            static constexpr float mouse_speed = 0.5f;
            horizontal_angle -= mouse_speed * dt * x_offset;
            vertical_angle += mouse_speed * dt * y_offset;

            /*
             * Clamp the vertical angle since we have a neck.
             */
            static constexpr float max_vertical_angle = glm::radians<float>(90.f);
            static constexpr float min_vertical_angle = -max_vertical_angle;
            if (vertical_angle > max_vertical_angle)
            {
                vertical_angle = max_vertical_angle;
            }
            else if (vertical_angle < min_vertical_angle)
            {
                vertical_angle = min_vertical_angle;
            }
        }

        /*
         * Get vector pointing at target.
         */
        direction.x = cos(vertical_angle) * sin(horizontal_angle);
        direction.y = sin(vertical_angle);
        direction.z = cos(vertical_angle) * cos(horizontal_angle);

        /*
         * Get vector pointing to the right.
         */
        right.x = -cos(horizontal_angle);
        right.z = sin(horizontal_angle);

        /*
         * Get vector pointing forwards, 90deg counter-clockwise from the vector
         * pointing right on the X-Z plane.
         */
        forwards.x = right.z;
        forwards.z = -right.x;

        /*
         * Get vector pointing head, perpendicular to right and where we are
         * looking.
         */
        head = glm::cross(right, direction);
    }

    /**
     * Update a grounded player's state.
     *
     * @param fly_key_pressed Whether the fly key is currently pressed.
     *
     * @return True if the player state was changed, otherwise false.
     */
    bool Game::update_player_state_grounded(const bool fly_key_pressed)
    {
        if (!is_on_ground)
        {
            player_state = PlayerState::MIDAIR;
            return true;
        }

        if (fly_key_pressed && !fly_key_pressed_prev)
        {
            player_state = PlayerState::FLYING;
            return true;
        }

        static constexpr float time_on_ground_to_jump = 0.1f;
        const bool can_jump = time_on_ground >= time_on_ground_to_jump;
        const bool jump_button_pressed =
            glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (can_jump && jump_button_pressed)
        {
            player_velocity.y += move_impulse_jump * dt;
            player_state = PlayerState::MIDAIR;
            return true;
        }

        return false;
    }

    /**
     * Update player position based on keyboard input.
     */
    void Game::update_player_position()
    {
        /*
         * Determine which direction to move into.
         */
        glm::vec3 move_direction(0.f, 0.f, 0.f);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            move_direction = forwards;
        }
        else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            move_direction = -forwards;
        }

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            move_direction += right;
        }
        else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            move_direction -= right;
        }

        /*
         * Get next player state.
         */
        const bool crouch_button_pressed =
            glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        const bool fly_key_pressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;

        switch (player_state)
        {
        case PlayerState::WALKING:
        {
            if (update_player_state_grounded(fly_key_pressed))
            {
                break;
            }

            const bool sprint_button_pressed =
                glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
            const bool in_sprintable_direction =
                move_direction.x * forwards.x + move_direction.z * forwards.z > 0.f;

            /*
             * While sprint button is being pressed and the player is on the ground,
             * set them to sprinting.
             */
            if (sprint_button_pressed && in_sprintable_direction)
            {
                player_state = PlayerState::SPRINTING;
            }

            /*
             * Crouch if crouch button is pressed.
             */
            else if (crouch_button_pressed && !crouch_button_pressed_prev)
            {
                player_state = PlayerState::CROUCHING;
            }

            break;
        }

        case PlayerState::SPRINTING:
        {
            if (update_player_state_grounded(fly_key_pressed))
            {
                break;
            }

            /*
             * If sprinting but the sprint button is no longer being pressed or the
             * player is no longer moving in a sprintable direction, restore the player
             * to walking.
             */
            const bool sprint_button_pressed =
                glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
            const bool in_sprintable_direction =
                move_direction.x * forwards.x + move_direction.z * forwards.z > 0.f;
            if (!sprint_button_pressed || !in_sprintable_direction)
            {
                player_state = PlayerState::WALKING;
            }

            break;
        }

        case PlayerState::CROUCHING:
        {
            if (update_player_state_grounded(fly_key_pressed))
            {
                break;
            }

            /*
             * Uncrouch if crouch button is pressed again.
             */
            const bool crouch_button_pressed =
                glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
            if (crouch_button_pressed && !crouch_button_pressed_prev)
            {
                player_state = PlayerState::WALKING;
            }
            crouch_button_pressed_prev = crouch_button_pressed;

            break;
        }

        case PlayerState::FLYING:
            if (fly_key_pressed && !fly_key_pressed_prev)
            {
                player_state =
                    is_on_ground ? PlayerState::WALKING : PlayerState::MIDAIR;
                break;
            }

            break;

        case PlayerState::MIDAIR:
            if (is_on_ground)
            {
                player_state = PlayerState::WALKING;
                break;
            }

            if (fly_key_pressed && !fly_key_pressed_prev)
            {
                player_state = PlayerState::FLYING;
                break;
            }

            break;

        default:
            LOG_ERROR("Unknown player state: %u\n", player_state);
        }

        crouch_button_pressed_prev = crouch_button_pressed;
        fly_key_pressed_prev = fly_key_pressed;

        /*
         * Set player parameters based on current state.
         */
        switch (player_state)
        {
        case PlayerState::WALKING:
            player_height = height_standing;
            player_move_impulse = move_impulse_walking;
            friction_coeff = friction_coeff_ground;
            break;

        case PlayerState::SPRINTING:
            player_height = height_standing;
            player_move_impulse = move_impulse_sprinting;
            friction_coeff = friction_coeff_ground;
            break;

        case PlayerState::CROUCHING:
            player_height = height_crouching;
            player_move_impulse = move_impulse_crouching;
            friction_coeff = friction_coeff_ground;

            /*
             * Snap the player's Y position to the crouching position
             * so it doesn't count as being MIDAIR.
             */
            player_position.y = terrain_height + height_crouching;
            break;

        case PlayerState::FLYING:
            player_height = height_standing;
            player_move_impulse = move_impulse_flying;
            friction_coeff = friction_coeff_flying;

            /*
             * If player presses space, go up.
             */
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            {
                player_velocity.y += player_move_impulse * dt;
            }

            /*
             * If player presses left shift, go down.
             */
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            {
                player_velocity.y -= player_move_impulse * dt;
            }

            break;

        case PlayerState::MIDAIR:
            player_height = height_standing;
            player_move_impulse = move_impulse_midair;
            friction_coeff = friction_coeff_air;

            /*
             * Apply gravity.
             */
            player_velocity.y -= acceleration_gravity * dt;

            break;

        default:
            LOG_ERROR("Unknown player state: %u\n", player_state);
        }

        /*
         * Cache the camera Y position when on the ground.
         */
        on_ground_camera_y = player_height + terrain_height;

        /*
         * Add player move impulse.
         */
        if (move_direction.x != 0.f || move_direction.y != 0.f ||
            move_direction.z != 0.f)
        {
            player_velocity +=
                glm::normalize(move_direction) * player_move_impulse * dt;
        }

        /*
         * Apply friction.
         */
        player_velocity -= player_velocity * friction_coeff * dt;

        /*
         * Update player position.
         */
        player_position += player_velocity * dt;

        /*
         * Don't let the player go outside the world.
         */
        if (player_position.y < on_ground_camera_y)
        {
            player_position.y = on_ground_camera_y;
        }
        // else if (player_position.y > )
    }

    /**
     * @param state State.
     *
     * @return String representation of the state.
     */
    const char *Game::state_to_string(const State state)
    {
        switch (state)
        {
        case State::RUNNING:
            return "RUNNING";
        case State::PAUSED:
            return "PAUSED";
        case State::QUIT:
            return "QUIT";
        default:
            return "UNKNOWN";
        }
    }

    /**
     * @param state State.
     *
     * @return String representation of the state.
     */
    const char *Game::player_state_to_string(const PlayerState state)
    {
        switch (state)
        {
        case PlayerState::WALKING:
            return "WALKING";
        case PlayerState::CROUCHING:
            return "CROUCHING";
        case PlayerState::SPRINTING:
            return "SPRINTING";
        case PlayerState::FLYING:
            return "FLYING";
        case PlayerState::MIDAIR:
            return "MIDAIR";
        default:
            return "UNKNOWN";
        }
    }

    /**
     * Run the game.
     */
    bool Game::run()
    {
        LOG("Entering main loop\n");

        const float fov_deg = 65.f;
        const float far_clip = 1000.f;
        const float aspect = static_cast<float>(window_width) / window_height;
        const float near_clip = 0.1f;
        const glm::mat4 perspective =
            glm::perspective(glm::radians(fov_deg), aspect, near_clip, far_clip);

        /*
         * Get reference to the model view projection.
         */
        const GLint model_view_projection_object =
            glGetUniformLocation(basic_shader.id(), "model_view_projection");
        if (model_view_projection_object == -1)
        {
            LOG_ERROR("Failed to get model_view_projection uniform location\n");
            return false;
        }

        /*
         * Get reference to the texture object and set it to slot 0.
         */
        const GLint texture_sampler_object =
            glGetUniformLocation(basic_shader.id(), "texture_sampler");
        if (texture_sampler_object == -1)
        {
            LOG_ERROR("Failed to get texture_sampler uniform location\n");
            return false;
        }
        basic_shader.use();
        glUniform1i(texture_sampler_object, 0);

        /*
         * Loop until the user closes the window or state gets set to QUIT by the
         * program.
         */
        std::chrono::steady_clock::time_point frame_start_time =
            std::chrono::steady_clock::now();
        while (state != State::QUIT && !glfwWindowShouldClose(window))
        {
            /*
             * Compute how much time has passed since the last frame.
             */
            const uint64_t dt_ns =
                (std::chrono::steady_clock::now() - frame_start_time).count();
            dt = dt_ns / 1e9f;

            frame_start_time = std::chrono::steady_clock::now();

            /*
             * Clear both the color and depth buffers.
             */
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            /*
             * Process menu.
             */
            process_menu();

            /*
             * If not paused, run gameplay.
             */
            if (state != State::PAUSED)
            {
                /*
                 * Cache variables used multiple times.
                 */
                terrain_height =
                    get_terrain_height(player_position.x, player_position.z);

                /*
                 * Cache whether player is on the ground.
                 */
                is_on_ground = player_position.y <= on_ground_camera_y;

                /*
                 * Update time the player has been on the ground.
                 */
                time_on_ground = is_on_ground ? time_on_ground + dt : 0.f;

                /*
                 * Update view based on mouse movement.
                 */
                update_view();

                /*
                 * Update player_position based on keyboard input.
                 */
                update_player_position();

                /*
                 * Compute view looking at the direction our mouse is pointing.
                 */
                const glm::mat4 camera_view =
                    glm::lookAt(player_position, player_position + direction, head);

                /*
                 * Draw the chasers.
                 */

                chaser_vertex_array.bind();
                basic_shader.use();

                /*
                 * Draw a chaser at the origin.
                 */
                {
                    const glm::mat4 model_view_projection =
                        perspective * camera_view * glm::mat4(1.0f);
                    glUniformMatrix4fv(model_view_projection_object,
                                       1,
                                       GL_FALSE,
                                       &model_view_projection[0][0]);
                    glDrawElements(GL_TRIANGLES,
                                   chaser_index_buffer.get_count(),
                                   decltype(chaser_index_buffer)::IndexGLtype,
                                   nullptr);
                }

                /*
                 * Make this one chase the player.
                 */
                {
                    /*
                     * Update chaser position to move towards player position on X-Z
                     * plane.
                     */
                    const glm::vec3 direction_to_player_xz = glm::normalize(
                        glm::vec3(player_position.x - chaser_position.x,
                                  0.f,
                                  player_position.z - chaser_position.z));
                    static constexpr float chaser_move_impulse = 1.f;
                    chaser_position +=
                        direction_to_player_xz * chaser_move_impulse * dt;

                    /*
                     * Create model matrix for the chaser at its position.
                     */
                    glm::mat4 chaser_model =
                        glm::translate(glm::mat4(1.f), chaser_position);

                    /*
                     * Set the chaser's angle to face us.
                     */
                    chaser_model = glm::rotate(chaser_model,
                                               glm::radians<float>(180.f) +
                                                   std::atan2(direction_to_player_xz.x,
                                                              direction_to_player_xz.z),
                                               glm::vec3(0.f, 1.f, 0.f));

                    const glm::mat4 model_view_projection =
                        perspective * camera_view * chaser_model;

                    glUniformMatrix4fv(model_view_projection_object,
                                       1,
                                       GL_FALSE,
                                       &model_view_projection[0][0]);
                    glDrawElements(GL_TRIANGLES,
                                   chaser_index_buffer.get_count(),
                                   decltype(chaser_index_buffer)::IndexGLtype,
                                   nullptr);
                }

                /*
                 * Draw the terrain.
                 */

                terrain_vertex_array.bind();
                heightmap_shader.use();

                const glm::mat4 model_view_projection =
                    perspective * camera_view * glm::mat4(1.0f);
                glUniformMatrix4fv(model_view_projection_object,
                                   1,
                                   GL_FALSE,
                                   &model_view_projection[0][0]);

                glDrawElements(GL_TRIANGLE_STRIP,
                               terrain_index_buffer.get_count(),
                               decltype(terrain_index_buffer)::IndexGLtype,
                               nullptr);
            }

            /*
             * Render GUI.
             */
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            /*
             * Swap front and back buffers.
             */
            glfwSwapBuffers(window);

            /*
             * Poll for and process events.
             */
            glfwPollEvents();

            /*
             * Log state transitions for debug.
             */
            if (state != state_prev)
            {
                LOG("State transition: %s -> %s\n",
                    state_to_string(state_prev),
                    state_to_string(state));
            }
            state_prev = state;
        }

        LOG("Exited main loop\n");

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);

        glfwTerminate();

        return true;
    }

    /**
     * Stop running the game.
     */
    void Game::quit()
    {
        state = State::QUIT;
    }
}