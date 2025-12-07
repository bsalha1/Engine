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
        player_move_speed(move_speed_walking),
        player_height(height_standing),
        player_flying(false),
        player_position(0.f, player_height, -10.f),
        player_velocity(0.f, 0.f, 0.f),
        last_jump_time(std::chrono::steady_clock::now()),
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
        int window_width;
        int window_height;
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
            int width;
            int length;
            int channels;
            uint8_t *height = stbi_load(
                "terrain/iceland_heightmap.png", &width, &length, &channels, 0);
            if (height == nullptr)
            {
                LOG_ERROR("Failed to load heightmap\n");
                return false;
            }

            /*
             * Load the heightmap in with (x,z) = (0,0) being in the middle and y going
             * from -16 to 64.
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
            const float x_middle = length / 2.f;
            const float z_middle = width / 2.f;
            static constexpr float y_top = 64.f;
            static constexpr float y_bottom = -16.0f;
            float y_scale = y_top / 0xFF;

            const unsigned int num_vertices = length * width;
            const unsigned int num_indices =
                (length - 1) * width * num_indices_per_vertex;
            std::unique_ptr<Vertex3d[]> vertices(new Vertex3d[num_vertices]);
            std::unique_ptr<unsigned int[]> indices(new unsigned int[num_indices]);
            for (int x = 0; x < length; x++)
            {
                for (int z = 0; z < width; z++)
                {
                    const uint8_t y = height[(width * x + z) * channels];

                    vertices[width * x + z] = {.x = x - x_middle,
                                               .y = y * y_scale + y_bottom,
                                               .z = z - z_middle};

                    if (x != length - 1)
                    {
                        for (unsigned int index = 0; index < num_indices_per_vertex;
                             index++)
                        {
                            indices[(x * width + z) * num_indices_per_vertex + index] =
                                width * (x + index) + z;
                        }
                    }
                }
            }

            stbi_image_free(height);

            terrain_vertex_array.create();
            terrain_vertex_array.bind();

            GLuint terrain_vertex_chaser_buffer_obj;
            glGenBuffers(1, &terrain_vertex_chaser_buffer_obj);
            glBindBuffer(GL_ARRAY_BUFFER, terrain_vertex_chaser_buffer_obj);
            glBufferData(GL_ARRAY_BUFFER,
                         length * width * sizeof(vertices[0]),
                         &vertices[0],
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

            terrain_index_buffer.create(indices.get(), num_indices);
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
     * Make player crouch.
     */
    void Game::set_crouching()
    {
        player_height = height_crouching;
        player_move_speed = move_speed_crouching;
    }

    /**
     * Make player stand.
     */
    void Game::set_standing()
    {
        player_height = height_standing;
        player_move_speed = move_speed_walking;
    }

    /**
     * Make player sprint.
     */
    void Game::set_sprinting()
    {
        player_height = height_standing;
        player_move_speed = move_speed_sprinting;
    }

    /**
     * Make player walk.
     */
    void Game::set_walking()
    {
        player_height = height_standing;
        player_move_speed = move_speed_walking;
    }

    /**
     * @return Whether player is crouching.
     */
    bool Game::is_crouching() const
    {
        return player_height == height_crouching;
    }

    /**
     * @return Whether player is on the ground.
     */
    bool Game::is_on_ground() const
    {
        return player_position.y <= player_height;
    }

    /**
     * @return Whether player is sprinting.
     */
    bool Game::is_sprinting() const
    {
        return player_move_speed == move_speed_sprinting;
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
        ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                    player_position.x,
                    player_position.y,
                    player_position.z);
        ImGui::Text("Velocity: (%.2f, %.2f, %.2f)",
                    player_velocity.x,
                    player_velocity.y,
                    player_velocity.z);
        ImGui::End();
    }

    /**
     * Process jump or crouch input.
     */
    void Game::process_jump_crouch()
    {
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        {
            player_flying = true;

            /*
             * Weird to crouch when flying.
             */
            set_standing();

            player_move_speed = move_speed_flying;
        }

        if (player_flying)
        {
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            {
                player_velocity.y = player_move_speed;
            }
            else if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            {
                player_velocity.y = -player_move_speed;
            }

            player_velocity.y -= friction_coeff * player_velocity.y;
        }

        /*
         * If the player is off the ground, make gravity pull them down and do not allow
         * jumping or crouching.
         */
        else if (!is_on_ground())
        {
            static constexpr float acceleration_gravity = 10.f;
            player_velocity.y -= acceleration_gravity * dt;
        }
        /*
         * Otherwise, if the player is on the ground, jump when they press space or
         * crouch when they press left shift.
         */
        else
        {
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            {
                static constexpr std::chrono::milliseconds jump_cooldown = 500ms;
                if (std::chrono::steady_clock::now() - last_jump_time > jump_cooldown)
                {
                    last_jump_time = std::chrono::steady_clock::now();

                    player_velocity.y = 8.f;

                    /*
                     * If player is jumping from a crouch, they should land
                     * standing, otherwise it'll be hard on the knees.
                     */
                    set_standing();
                }
            }
            else if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            {
                static constexpr std::chrono::milliseconds crouch_cooldown = 250ms;
                if (std::chrono::steady_clock::now() - last_crouch_time >
                    crouch_cooldown)
                {
                    last_crouch_time = std::chrono::steady_clock::now();

                    if (is_crouching())
                    {
                        set_standing();
                    }
                    else
                    {
                        set_crouching();
                    }
                }
            }
            else
            {
                player_velocity.y = 0.f;
            }
        }
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
     * Update player position based on keyboard input.
     */
    void Game::update_player_position()
    {
        /*
         * Determine which direction to move into.
         */
        glm::vec3 direction(0.f, 0.f, 0.f);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            direction += forwards;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            direction -= forwards;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            direction += right;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            direction -= right;
        }

        const bool sprint_button_pressed =
            glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
        const bool in_sprintable_direction =
            direction.x * forwards.x + direction.z * forwards.z > 0.f;

        /*
         * While sprint button is being pressed and the player is on the ground,
         * set them to sprinting.
         */
        if (sprint_button_pressed && in_sprintable_direction)
        {
            if (is_on_ground())
            {
                set_sprinting();
            }
        }

        /*
         * If sprinting but the sprint button is no longer being pressed or the player
         * is no longer moving in a sprintable direction, restore the player to walking.
         */
        else if (is_sprinting())
        {
            set_walking();
        }

        /*
         * Update player velocity.
         */
        if (direction.x != 0.f || direction.y != 0.f || direction.z != 0.f)
        {
            player_velocity += glm::normalize(direction) * player_move_speed;
        }

        /*
         * If the player is moving, make friction slow them down. Note that this also
         * applies while in the air which is not realistic but feels better for
         * gameplay.
         */
        if (std::fabs(player_velocity.x) > 0.f || std::fabs(player_velocity.z) > 0.f)
        {
            player_velocity.x -= friction_coeff * player_velocity.x;
            player_velocity.z -= friction_coeff * player_velocity.z;
        }

        player_position += player_velocity * dt;

        /*
         * Don't let the player go under the ground.
         */
        if (player_position.y < player_height)
        {
            player_position.y = player_height;
        }
    }

    /**
     * @param state State.
     *
     * @return String representation of the state.
     */
    const char *Game::state_to_string(const Game::State state)
    {
        switch (state)
        {
        case Game::State::RUNNING:
            return "RUNNING";
        case Game::State::PAUSED:
            return "PAUSED";
        case Game::State::QUIT:
            return "QUIT";
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
        const float aspect = 4.f / height_standing;
        const float near_clip = 1.f;
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
                 * Process jumps or crouches.
                 */
                process_jump_crouch();

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
                    static constexpr float chaser_move_speed = 1.f;
                    chaser_position += direction_to_player_xz * chaser_move_speed * dt;

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