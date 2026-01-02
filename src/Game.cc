#include "Game.h"

#include "Vertex.h"
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
    namespace
    {
        /**
         * Debug flag to pause the sun movement.
         */
        bool sun_paused = false;
    }

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
        friction_coeff(friction_coeff_ground),
        player_movement_state(PlayerMovementState::WALKING),
        player_move_impulse(move_impulse_walking),
        player_height(height_standing),
        fly_key_pressed_prev(false),
        crouch_key_pressed_prev(false),
        sprint_key_pressed_prev(false),
        jump_key_pressed_prev(false),
        player_position(0.f, player_height, 0.f),
        player_velocity(0.f, 0.f, 0.f),
        last_crouch_time(std::chrono::steady_clock::now()),
        time_since_start(0.0),
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
        chaser_position(0.f, 0.f, 10.f),
        point_light_position(150.f, 100.f, 120.f),
        orbital_angle(glm::pi<float>())
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
     * @brief Apply a Gaussian blur to the given heightmap.
     *
     * @param[inout] heightmap Heightmap data.
     * @param num_rows Number of rows in the heightmap.
     * @param num_cols Number of columns in the heightmap.
     * @param dimensions Number of dimensions in the heightmap.
     * @param iterations Number of times to apply the blur.
     */
    static void gaussian_blur(uint8_t *heightmap,
                              const int num_rows,
                              const int num_cols,
                              const int dimensions,
                              const uint8_t iterations)
    {
        for (uint8_t i = 0; i < iterations; i++)
        {
            /*
             * Copy original heightmap for reference.
             */
            std::vector<uint8_t> heightmap_original(heightmap,
                                                    heightmap + num_rows * num_cols * dimensions);

            const float kernel[3][3] = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}};

            /*
             * Apply blur.
             */
            for (int row = 0; row < num_rows; row++)
            {
                for (int col = 0; col < num_cols; col++)
                {
                    float sum = 0.f;
                    float weight_sum = 0.f;

                    for (int kernel_z = -1; kernel_z <= 1; kernel_z++)
                    {
                        for (int kernel_x = -1; kernel_x <= 1; kernel_x++)
                        {
                            int height_kerneled = col + kernel_x;
                            int row_kerneled = row + kernel_z;
                            height_kerneled = glm::clamp(height_kerneled, 0, num_cols - 1);
                            row_kerneled = glm::clamp(row_kerneled, 0, num_rows - 1);

                            const float kern = kernel[kernel_z + 1][kernel_x + 1];
                            sum += heightmap_original[(row_kerneled * num_cols + height_kerneled) *
                                                      dimensions] *
                                   kern;
                            weight_sum += kern;
                        }
                    }

                    heightmap[(row * num_cols + col) * dimensions] = sum / weight_sum;
                }
            }
        }
    }

    /**
     * Initialize the game.
     *
     * @return True on success, otherwise false.
     */
    bool Game::_init()
    {
        LOG("Creating window\n");

        glfwWindowHint(GLFW_SAMPLES, 16);              /* 16x antialiasing */
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

        /*
         * Enable anti-aliasing.
         */
        glEnable(GL_MULTISAMPLE);

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

        /*
         * Create screen frame buffer.
         */
        LOG("Initializing renderer\n");
        renderer.init(window_width, window_height);

        /*
         * Create chaser buffers.
         */
        LOG("Creating entity buffers\n");
        {
            /* clang-format off */
            const std::array<TexturedVertex3dNormalTangent, 36> vertices = {{
            /*                position,                normal,         texture,                tangent */
                /* -Z */
                {glm::vec3(-1, -1, -1), glm::vec3( 0,  0, -1), glm::vec2(0, 0), glm::vec4(-1, 0, 0, 1)},
                {glm::vec3( 1, -1, -1), glm::vec3( 0,  0, -1), glm::vec2(1, 0), glm::vec4(-1, 0, 0, 1)},
                {glm::vec3( 1,  1, -1), glm::vec3( 0,  0, -1), glm::vec2(1, 1), glm::vec4(-1, 0, 0, 1)},
                {glm::vec3( 1,  1, -1), glm::vec3( 0,  0, -1), glm::vec2(1, 1), glm::vec4(-1, 0, 0, 1)},
                {glm::vec3(-1,  1, -1), glm::vec3( 0,  0, -1), glm::vec2(0, 1), glm::vec4(-1, 0, 0, 1)},
                {glm::vec3(-1, -1, -1), glm::vec3( 0,  0, -1), glm::vec2(0, 0), glm::vec4(-1, 0, 0, 1)},

                /* +Z */
                {glm::vec3(-1, -1,  1), glm::vec3( 0,  0,  1), glm::vec2(0, 0), glm::vec4(1, 0, 0, 1)},
                {glm::vec3( 1, -1,  1), glm::vec3( 0,  0,  1), glm::vec2(1, 0), glm::vec4(1, 0, 0, 1)},
                {glm::vec3( 1,  1,  1), glm::vec3( 0,  0,  1), glm::vec2(1, 1), glm::vec4(1, 0, 0, 1)},
                {glm::vec3( 1,  1,  1), glm::vec3( 0,  0,  1), glm::vec2(1, 1), glm::vec4(1, 0, 0, 1)},
                {glm::vec3(-1,  1,  1), glm::vec3( 0,  0,  1), glm::vec2(0, 1), glm::vec4(1, 0, 0, 1)},
                {glm::vec3(-1, -1,  1), glm::vec3( 0,  0,  1), glm::vec2(0, 0), glm::vec4(1, 0, 0, 1)},

                /* -X */
                {glm::vec3(-1,  1,  1), glm::vec3(-1,  0,  0), glm::vec2(1, 0), glm::vec4(0, 0, 1, 1)},
                {glm::vec3(-1,  1, -1), glm::vec3(-1,  0,  0), glm::vec2(1, 1), glm::vec4(0, 0, 1, 1)},
                {glm::vec3(-1, -1, -1), glm::vec3(-1,  0,  0), glm::vec2(0, 1), glm::vec4(0, 0, 1, 1)},
                {glm::vec3(-1, -1, -1), glm::vec3(-1,  0,  0), glm::vec2(0, 1), glm::vec4(0, 0, 1, 1)},
                {glm::vec3(-1, -1,  1), glm::vec3(-1,  0,  0), glm::vec2(0, 0), glm::vec4(0, 0, 1, 1)},
                {glm::vec3(-1,  1,  1), glm::vec3(-1,  0,  0), glm::vec2(1, 0), glm::vec4(0, 0, 1, 1)},

                /* +X */
                {glm::vec3( 1,  1,  1), glm::vec3( 1,  0,  0), glm::vec2(1, 0), glm::vec4(0, 0, -1, 1)},
                {glm::vec3( 1,  1, -1), glm::vec3( 1,  0,  0), glm::vec2(1, 1), glm::vec4(0, 0, -1, 1)},
                {glm::vec3( 1, -1, -1), glm::vec3( 1,  0,  0), glm::vec2(0, 1), glm::vec4(0, 0, -1, 1)},
                {glm::vec3( 1, -1, -1), glm::vec3( 1,  0,  0), glm::vec2(0, 1), glm::vec4(0, 0, -1, 1)},
                {glm::vec3( 1, -1,  1), glm::vec3( 1,  0,  0), glm::vec2(0, 0), glm::vec4(0, 0, -1, 1)},
                {glm::vec3( 1,  1,  1), glm::vec3( 1,  0,  0), glm::vec2(1, 0), glm::vec4(0, 0, -1, 1)},

                /* -Y */
                {glm::vec3(-1, -1, -1), glm::vec3( 0, -1,  0), glm::vec2(0, 1), glm::vec4(1, 0, 0, 1)},
                {glm::vec3( 1, -1, -1), glm::vec3( 0, -1,  0), glm::vec2(1, 1), glm::vec4(1, 0, 0, 1)},
                {glm::vec3( 1, -1,  1), glm::vec3( 0, -1,  0), glm::vec2(1, 0), glm::vec4(1, 0, 0, 1)},
                {glm::vec3( 1, -1,  1), glm::vec3( 0, -1,  0), glm::vec2(1, 0), glm::vec4(1, 0, 0, 1)},
                {glm::vec3(-1, -1,  1), glm::vec3( 0, -1,  0), glm::vec2(0, 0), glm::vec4(1, 0, 0, 1)},
                {glm::vec3(-1, -1, -1), glm::vec3( 0, -1,  0), glm::vec2(0, 1), glm::vec4(1, 0, 0, 1)},

                /* +Y */
                {glm::vec3(-1,  1, -1), glm::vec3( 0,  1,  0), glm::vec2(0, 1), glm::vec4(1, 0, 0, 1)},
                {glm::vec3( 1,  1, -1), glm::vec3( 0,  1,  0), glm::vec2(1, 1), glm::vec4(1, 0, 0, 1)},
                {glm::vec3( 1,  1,  1), glm::vec3( 0,  1,  0), glm::vec2(1, 0), glm::vec4(1, 0, 0, 1)},
                {glm::vec3( 1,  1,  1), glm::vec3( 0,  1,  0), glm::vec2(1, 0), glm::vec4(1, 0, 0, 1)},
                {glm::vec3(-1,  1,  1), glm::vec3( 0,  1,  0), glm::vec2(0, 0), glm::vec4(1, 0, 0, 1)},
                {glm::vec3(-1,  1, -1), glm::vec3( 0,  1,  0), glm::vec2(0, 1), glm::vec4(1, 0, 0, 1)}
            }};
            /* clang-format on */

            chaser_vertex_array.create(vertices.data(), vertices.size());
            TexturedVertex3dNormalTangent::setup_vertex_array_attribs(chaser_vertex_array);
        }

        LOG("Loading textures\n");
        {
            ASSERT_RET_IF_NOT(chaser_textured_material.create_from_file("textures/snake.jpg",
                                                                        0 /* slot */),
                              false);
            ASSERT_RET_IF_NOT(chaser_normal_map.create_from_file("textures/snake_normals.jpg",
                                                                 1 /* slot */),
                              false);
            ASSERT_RET_IF_NOT(
                dirt_textured_material.create_from_file("textures/dirt.jpg", 0 /* slot */), false);
            ASSERT_RET_IF_NOT(
                dirt_normal_map.create_from_file("textures/dirt_normals.jpg", 1 /* slot */), false);
        }

        LOG("Loading terrain heightmaps\n");
        {
            stbi_set_flip_vertically_on_load(0);
            int terrain_num_rows;
            int terrain_channels;
            uint8_t *_heightmap = stbi_load("terrain/iceland_heightmap.png",
                                            &terrain_num_cols,
                                            &terrain_num_rows,
                                            &terrain_channels,
                                            0);
            ASSERT_RET_IF_NOT(_heightmap, false);

            /*
             * Wrap in RAII container for automatic freeing.
             */
            std::unique_ptr<uint8_t[]> heightmap(_heightmap);

            /*
             * First, apply a Gaussian blur to the heightmap to smooth out sharp edges.
             */
            gaussian_blur(heightmap.get(),
                          terrain_num_rows,
                          terrain_num_cols,
                          terrain_channels,
                          2 /* iterations */);

            /*
             * We need to have a right-handed coordinate system. If we choose to map the
             * heightmap image to:
             *
             *          +z
             *           o   / +y
             *           |  /
             *           | /
             *   -x o----o----o +x
             *           |
             *           |
             *           o
             *          -z
             *
             * Then Y must point into the the screen. This means that the height values
             * in the heightmap would turn to depth values which is wrong and if we try
             * to reverse those effects, we will end up flipping the X or Z axis weirdly
             * enough. Therefore, we map the heightmap image to:
             *
             *          -z
             *           o
             *           |
             *           |
             *   -x o----o----o +x
             *         / |
             *        /  |
             *    +y /   o
             *          +z
             */
            terrain_z_middle = terrain_num_rows / 2.f;
            terrain_x_middle = terrain_num_cols / 2.f;
            static constexpr float y_top = 64.f;
            static constexpr float y_bottom = -27.f;
            float y_scale = y_top / 0xFF;

            const unsigned int num_vertices = terrain_num_rows * terrain_num_cols;
            static constexpr unsigned int num_indices_per_vertex = 6;
            const unsigned int num_indices =
                (terrain_num_rows - 1) * (terrain_num_cols - 1) * num_indices_per_vertex;

            /*
             * Draw one copy of the texture per cell.
             */
            std::vector<Vertex3dNormal> vertices;
            vertices.reserve(num_vertices);

            std::vector<unsigned int> indices;
            indices.reserve(num_indices);

            xz_to_height_map.reserve(num_vertices);

            /*
             * Compute vertices and indices iterating from the top row to the bottom row
             * and from the left column to the right column.
             */
            for (int row = 0; row < terrain_num_rows; row++)
            {
                for (int col = 0; col < terrain_num_cols; col++)
                {
                    const uint8_t y = heightmap[(terrain_num_cols * row + col) * terrain_channels];

                    Vertex3dNormal &vertex = vertices.emplace_back();

                    vertex.position = {
                        col - terrain_x_middle,
                        y * y_scale + y_bottom,
                        row - terrain_z_middle,
                    };

                    xz_to_height_map.push_back(vertex.position.y);

                    /*
                     * If we are not on the bottom row or rightmost column, wind the
                     * triangles.
                     */
                    if (row != terrain_num_rows - 1 && col != terrain_num_cols - 1)
                    {
                        const int this_vertex = terrain_num_cols * row + col;
                        const int right_vertex = this_vertex + 1;
                        const int bottom_vertex = terrain_num_cols * (row + 1) + col;
                        const int bottom_right_vertex = bottom_vertex + 1;

                        indices.push_back(this_vertex);
                        indices.push_back(bottom_vertex);
                        indices.push_back(right_vertex);

                        indices.push_back(right_vertex);
                        indices.push_back(bottom_vertex);
                        indices.push_back(bottom_right_vertex);
                    }
                }
            }

            /*
             * Accumulate normals, tangents, and bitangents for each vertex in each
             * triangle.
             */
            std::vector<glm::vec3> tangents;
            std::vector<glm::vec3> bitangents;
            tangents.reserve(vertices.size());
            bitangents.reserve(vertices.size());
            for (size_t i = 0; i < indices.size(); i += 3)
            {
                /*
                 *             e2
                 *     v0------->---------v2
                 *     |                  /
                 *     |     +         /
                 *     |            /
                 * e1 \ /        /
                 *     |      /
                 *     |   /
                 *     |/
                 *     v1
                 *
                 *                     v0
                 *                    /|
                 *                 /   |
                 *         e1   /      |
                 *          /_        \ / e2
                 *        /            |
                 *     /       +       |
                 *  /                  |
                 * v1------------------v2
                 */

                const size_t idx0 = indices[i + 0];
                const size_t idx1 = indices[i + 1];
                const size_t idx2 = indices[i + 2];

                Vertex3dNormal &v0 = vertices[idx0];
                Vertex3dNormal &v1 = vertices[idx1];
                Vertex3dNormal &v2 = vertices[idx2];

                const glm::vec3 e1 = v1.position - v0.position;
                const glm::vec3 e2 = v2.position - v0.position;

                /*
                 * Compute face normal.
                 */
                const glm::vec3 face_normal = glm::normalize(glm::cross(e1, e2));
                v0.norm += face_normal;
                v1.norm += face_normal;
                v2.norm += face_normal;
            }

            /*
             * Finalize normals, tangents, and bitangents for each vertex.
             */
            for (size_t i = 0; i < vertices.size(); i += 1)
            {
                Vertex3dNormal &vertex = vertices[i];

                /*
                 * Average to produce normal.
                 */
                vertex.norm = glm::normalize(vertex.norm);
            }

            terrain_vertex_array.create(vertices.data(), vertices.size());
            Vertex3dNormal::setup_vertex_array_attribs(terrain_vertex_array);
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
     * @brief Convert a cell coordinate to the height at that cell.
     *
     * @param cell_x X cell coordinate.
     * @param cell_z Z cell coordinate.
     *
     * @return Terrain height.
     */
    float Game::cell_to_height(const int cell_x, const int cell_z) const
    {
        return xz_to_height_map[terrain_num_cols * cell_z + cell_x];
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

        const int cell_x_left = static_cast<int>(std::floor(x_terrain));
        const int cell_x_right = static_cast<int>(std::ceil(x_terrain));
        const int cell_z_down = static_cast<int>(std::floor(z_terrain));
        const int cell_z_up = static_cast<int>(std::ceil(z_terrain));

        const float dx = x_terrain - cell_x_left;
        const float dz = z_terrain - cell_z_down;

        const float y2 = cell_to_height(cell_x_right, cell_z_up);

        /*
         * In bottom-right triangle.
         */
        if (dx > dz)
        {
            const float y0 = cell_to_height(cell_x_left, cell_z_down);
            const float y3 = cell_to_height(cell_x_right, cell_z_down);
            const float x_slope = (y3 - y0);
            const float z_slope = (y2 - y3);
            return y0 + x_slope * dx + z_slope * dz;
        }

        /*
         * In top-left triangle.
         */
        else
        {
            const float y0 = cell_to_height(cell_x_left, cell_z_down);
            const float y1 = cell_to_height(cell_x_left, cell_z_up);
            const float x_slope = (y2 - y1);
            const float z_slope = (y1 - y0);
            return y0 + x_slope * dx + z_slope * dz;
        }
    }

    /**
     * Process menu input.
     */
    bool Game::process_menu()
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
            ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("Pause Menu",
                         nullptr,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings);
            ImGui::Text("Press ESC to unpause");

            /*
             * Add settings button.
             */
            ImGui::Separator();
            if (ImGui::Button("Settings"))
            {
                LOG("Pause Menu -> Settings\n");
            }

            /*
             * Add button to toggle triangle outlining for debugging.
             */
            ImGui::Separator();
            if (ImGui::Button("Outline Triangles"))
            {
                LOG("Pause Menu -> Outline Triangles\n");
                GLint gl_polygon_mode[2];
                glGetIntegerv(GL_POLYGON_MODE, gl_polygon_mode);
                if (gl_polygon_mode[0] == GL_LINE)
                {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                }
                else
                {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                }
            }

            /*
             * Add display settings.
             */
            float exposure = renderer.get_exposure();
            ImGui::SliderFloat("exposure", &exposure, 0.f, 10.f);
            ASSERT_RET_IF_NOT(renderer.set_exposure(exposure), false);

            float gamma = renderer.get_gamma();
            ImGui::SliderFloat("gamma", &gamma, 0.f, 10.f);
            ASSERT_RET_IF_NOT(renderer.set_gamma(gamma), false);

            float sharpness = renderer.get_sharpness();
            ImGui::SliderFloat("sharpness", &sharpness, 1.f, 1000.f);
            ASSERT_RET_IF_NOT(renderer.set_sharpness(sharpness), false);

            if (ImGui::Button("Pause Sun"))
            {
                sun_paused = !sun_paused;
            }

            /*
             * Add quit button.
             */
            ImGui::Separator();
            if (ImGui::Button("Quit"))
            {
                LOG("Pause Menu -> Quit\n");
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
                         ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("%.3f ms (%.0f FPS)", 1000.f / io.Framerate, io.Framerate);
        ImGui::Text("state: %s", state_to_string(state));
        ImGui::Text("player_movement_state: %s",
                    player_movement_state_to_string(player_movement_state));
        ImGui::Text("player_position: (%.2f, %.2f, %.2f)",
                    player_position.x,
                    player_position.y,
                    player_position.z);
        ImGui::Text("on_ground_camera_y: %.2f", on_ground_camera_y);
        ImGui::Text("altitude: %.2f", player_position.y - on_ground_camera_y);
        ImGui::Text("player_velocity: (%.2f, %.2f, %.2f) (%.2f m/s)",
                    player_velocity.x,
                    player_velocity.y,
                    player_velocity.z,
                    player_speed);
        ImGui::Text("move_impulse: %.2f", player_move_impulse);
        ImGui::Text("friction_coeff: %.2f", friction_coeff);
        ImGui::End();

        return true;
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
     * @brief Update a grounded player's state.
     *
     * @param fly_key_pressed Whether the fly key is currently pressed.
     *
     * @return True if the player state was changed, otherwise false.
     */
    bool Game::update_player_movement_state_grounded(const bool fly_key_pressed,
                                                     const bool jump_key_pressed)
    {
        if (fly_key_pressed && !fly_key_pressed_prev)
        {
            player_movement_state = PlayerMovementState::FLYING;
            return true;
        }

        const bool can_jump = player_position.y - on_ground_camera_y <= 0.2f;
        if (can_jump && jump_key_pressed && !jump_key_pressed_prev)
        {
            player_velocity.y += move_impulse_jump * dt;

            /*
             * If the player jumps from a crouch, stand them up.
             */
            if (player_movement_state == PlayerMovementState::CROUCHING)
            {
                player_movement_state = PlayerMovementState::WALKING;
            }
            return true;
        }

        return false;
    }

    /**
     * @brief Apply grounded movement state effects to a player on the ground.
     */
    void Game::apply_player_movement_state_grounded()
    {
        /*
         * If in the air, apply gravity and set the move impulse and friction to that of
         * air.
         */
        if (player_position.y > on_ground_camera_y)
        {
            friction_coeff = friction_coeff_air;

            player_move_impulse = move_impulse_midair;
            player_velocity.y -= acceleration_gravity * dt;
        }
        /*
         * Otherwise set friction to that of ground.
         */
        else
        {
            friction_coeff = friction_coeff_ground;
        }
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
         * Get next player movement state.
         */
        const bool crouch_key_pressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        const bool fly_key_pressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
        const bool sprint_key_pressed = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
        const bool jump_key_pressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

        switch (player_movement_state)
        {
        case PlayerMovementState::WALKING:
        {
            if (update_player_movement_state_grounded(fly_key_pressed, jump_key_pressed))
            {
                break;
            }

            const bool in_sprintable_direction =
                move_direction.x * forwards.x + move_direction.z * forwards.z > 0.f;

            /*
             * While sprint button is being pressed and the player is on the ground,
             * set them to sprinting.
             */
            if (sprint_key_pressed && !sprint_key_pressed_prev && in_sprintable_direction)
            {
                player_movement_state = PlayerMovementState::SPRINTING;
            }

            /*
             * Crouch if crouch button is pressed.
             */
            else if (crouch_key_pressed && !crouch_key_pressed_prev)
            {
                player_movement_state = PlayerMovementState::CROUCHING;
            }

            break;
        }

        case PlayerMovementState::SPRINTING:
        {
            if (update_player_movement_state_grounded(fly_key_pressed, jump_key_pressed))
            {
                break;
            }

            /*
             * If sprinting but the sprint button is no longer being pressed or the
             * player is no longer moving in a sprintable direction, restore the player
             * to walking.
             */
            const bool in_sprintable_direction =
                move_direction.x * forwards.x + move_direction.z * forwards.z > 0.f;
            if (sprint_key_pressed && !sprint_key_pressed_prev || !in_sprintable_direction)
            {
                player_movement_state = PlayerMovementState::WALKING;
            }

            /*
             * Crouch if crouch button is pressed.
             */
            else if (crouch_key_pressed && !crouch_key_pressed_prev)
            {
                player_movement_state = PlayerMovementState::CROUCHING;
            }

            break;
        }

        case PlayerMovementState::CROUCHING:
        {
            if (update_player_movement_state_grounded(fly_key_pressed, jump_key_pressed))
            {
                break;
            }

            /*
             * Sprint if sprint button is pressed while crouching.
             */
            const bool in_sprintable_direction =
                move_direction.x * forwards.x + move_direction.z * forwards.z > 0.f;
            if (sprint_key_pressed && !sprint_key_pressed_prev && in_sprintable_direction)
            {
                player_movement_state = PlayerMovementState::SPRINTING;
            }

            /*
             * Uncrouch if crouch button is pressed again.
             */
            else if (crouch_key_pressed && !crouch_key_pressed_prev)
            {
                player_movement_state = PlayerMovementState::WALKING;
            }

            break;
        }

        case PlayerMovementState::FLYING:
            if (fly_key_pressed && !fly_key_pressed_prev)
            {
                player_movement_state = PlayerMovementState::WALKING;
            }

            break;

        default:
            LOG_ERROR("Unknown player state: %u\n", player_movement_state);
        }

        crouch_key_pressed_prev = crouch_key_pressed;
        fly_key_pressed_prev = fly_key_pressed;
        sprint_key_pressed_prev = sprint_key_pressed;
        jump_key_pressed_prev = jump_key_pressed;

        /*
         * Set player parameters based on current state.
         */
        switch (player_movement_state)
        {
        case PlayerMovementState::WALKING:
            player_height = height_standing;
            on_ground_camera_y = player_height + terrain_height;

            player_move_impulse = move_impulse_walking;
            apply_player_movement_state_grounded();
            break;

        case PlayerMovementState::SPRINTING:
            player_height = height_standing;
            on_ground_camera_y = player_height + terrain_height;

            player_move_impulse = move_impulse_sprinting;
            apply_player_movement_state_grounded();
            break;

        case PlayerMovementState::CROUCHING:
            player_height = height_crouching;
            on_ground_camera_y = player_height + terrain_height;

            player_move_impulse = move_impulse_crouching;
            apply_player_movement_state_grounded();
            break;

        case PlayerMovementState::FLYING:
            player_height = height_standing;
            on_ground_camera_y = player_height + terrain_height;

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

        default:
            LOG_ERROR("Unknown player_movement_state: %u\n", player_movement_state);
        }

        /*
         * Add player move impulse.
         */
        if (move_direction.x != 0.f || move_direction.y != 0.f || move_direction.z != 0.f)
        {
            player_velocity +=
                glm::normalize(move_direction) * player_move_impulse * static_cast<float>(dt);
        }

        /*
         * Apply friction.
         */
        player_velocity -= player_velocity * friction_coeff * static_cast<float>(dt);
        player_speed = glm::length(player_velocity);

        /*
         * Update player position.
         */
        player_position += player_velocity * static_cast<float>(dt);

        /*
         * Don't let the player go outside the world.
         */

        if (player_position.y < on_ground_camera_y)
        {
            player_position.y = on_ground_camera_y;
        }

        if (player_position.x < -terrain_x_middle + 1.f)
        {
            player_position.x = -terrain_x_middle + 1.f;
        }
        else if (player_position.x > terrain_x_middle - 1.f)
        {
            player_position.x = terrain_x_middle - 1.f;
        }

        if (player_position.z < -terrain_z_middle + 1.f)
        {
            player_position.z = -terrain_z_middle + 1.f;
        }
        else if (player_position.z > terrain_z_middle - 1.f)
        {
            player_position.z = terrain_z_middle - 1.f;
        }
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
    const char *Game::player_movement_state_to_string(const PlayerMovementState state)
    {
        switch (state)
        {
        case PlayerMovementState::WALKING:
            return "WALKING";
        case PlayerMovementState::CROUCHING:
            return "CROUCHING";
        case PlayerMovementState::SPRINTING:
            return "SPRINTING";
        case PlayerMovementState::FLYING:
            return "FLYING";
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

        ASSERT_RET_IF_NOT(renderer.set_terrain({
                              .material = dirt_textured_material,
                              .normal_map = dirt_normal_map,
                              .drawable = terrain_index_buffer,
                          }),
                          false);

        /*
         * Set day length and compute the rotational speed.
         */
        static constexpr float day_length_s = 10.f;
        static constexpr float rotational_angular_speed = 2 * glm::pi<float>() / day_length_s;

        /*
         * Relative to the terrain, the skybox spins around it. We draw a sun
         * on the skybox in its model space so that it rotates with it with an
         * elevation angle above the orbital plane.
         */
        static constexpr float sun_angular_radius = glm::radians<float>(5.f);
        const float sun_radius_skybox_model_space = glm::sin(sun_angular_radius);
        static constexpr float sun_orbital_elevation_angle = glm::radians<float>(10.f);
        const glm::vec4 sun_position_skybox_model_space = glm::vec4(
            0.f, glm::sin(sun_orbital_elevation_angle), glm::cos(sun_orbital_elevation_angle), 0.f);

        /*
         * Loop until the user closes the window or state gets set to QUIT by the
         * program.
         */
        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point frame_start_time = start_time;
        while (state != State::QUIT && !glfwWindowShouldClose(window))
        {
            /*
             * Compute how much time has passed since the last frame.
             */
            const uint64_t dt_ns = (std::chrono::steady_clock::now() - frame_start_time).count();
            dt = dt_ns / 1e9;

            frame_start_time = std::chrono::steady_clock::now();

            /*
             * Process menu.
             */
            ASSERT_RET_IF_NOT(process_menu(), false);

            /*
             * If not paused, run gameplay.
             */
            if (state != State::PAUSED)
            {
                time_since_start += dt;

                /*
                 * Cache variables used multiple times.
                 */
                terrain_height = get_terrain_height(player_position.x, player_position.z);

                /*
                 * Cache whether player is on the ground.
                 */
                is_on_ground = player_position.y <= on_ground_camera_y;

                /*
                 * Update view based on mouse movement.
                 */
                update_view();

                /*
                 * Update player_position based on keyboard input.
                 */
                update_player_position();

                /*
                 * Update orbital angle. Offset by -pi so that at time 0, the sun is
                 * rising from the horizon.
                 */
                if (!sun_paused)
                {
                    orbital_angle += rotational_angular_speed * dt;
                }

                /*
                 * Update chaser position to move towards player position on X-Z
                 * plane and face them.
                 */
                glm::vec3 direction_to_player_xz;
                if (player_position.x != chaser_position.x ||
                    player_position.z != chaser_position.z)
                {
                    direction_to_player_xz =
                        glm::normalize(glm::vec3(player_position.x - chaser_position.x,
                                                 0.f,
                                                 player_position.z - chaser_position.z));
                    static constexpr float chaser_move_impulse = 5.f;
                    chaser_position +=
                        direction_to_player_xz * chaser_move_impulse * static_cast<float>(dt);
                    chaser_position.y =
                        get_terrain_height(chaser_position.x, chaser_position.z) + 1.f;
                }
                else
                {
                    direction_to_player_xz = glm::vec3(1.f, 0.f, 0.f);
                }

                glm::mat4 chaser_model = glm::translate(glm::mat4(1.f), chaser_position);

                chaser_model =
                    glm::rotate(chaser_model,
                                glm::radians<float>(180.f) +
                                    std::atan2(direction_to_player_xz.x, direction_to_player_xz.z),
                                glm::vec3(0.f, 1.f, 0.f));

                static float light_velocity = 20.f;

                /*
                 * Update point light position.
                 */
                if (point_light_position.y <
                    get_terrain_height(point_light_position.x, point_light_position.z) + 1.f)
                {
                    light_velocity = 20.f;
                }
                else if (point_light_position.y >
                         get_terrain_height(point_light_position.x, point_light_position.z) + 100.f)
                {
                    light_velocity = -20.f;
                }
                point_light_position.y += light_velocity * dt;

                /*
                 * Compute the directional light direction relative to the terrain
                 * by converting the sun's position from skybox model space to the
                 * terrain model space.
                 */
                const glm::mat4 terrain_model = glm::mat4(1.0f);

                const glm::mat4 terrain_model_matrix_rotated =
                    glm::rotate(terrain_model, orbital_angle, rotation_axis);

                const glm::vec3 sun_position_terrain_model_space =
                    glm::vec3(terrain_model_matrix_rotated * sun_position_skybox_model_space);

                glm::vec3 directional_light_direction =
                    -glm::normalize(sun_position_terrain_model_space);

                /*
                 * Compute sun brightness based on its elevation angle.
                 */
                const float sun_top_y =
                    sun_position_terrain_model_space.y + sun_radius_skybox_model_space;
                const float sun_distance = glm::sqrt(
                    sun_top_y * sun_top_y +
                    sun_position_terrain_model_space.x * sun_position_terrain_model_space.x +
                    sun_position_terrain_model_space.z * sun_position_terrain_model_space.z);
                const float sine_of_elevation_angle = sun_top_y / sun_distance;

                static constexpr float brightness_falloff_factor = 0.1f;
                const float sun_brightness =
                    sine_of_elevation_angle <= 0.f
                        ? 0.f
                        : glm::exp(-brightness_falloff_factor / sine_of_elevation_angle);

                const glm::mat4 view =
                    glm::lookAt(player_position, player_position + direction, head);

                /*
                 * Submit chaser to renderer.
                 */
                Renderer::Transform chaser_transform = {
                    .position = chaser_position,
                    .rotation =
                        glm::vec3(0.f,
                                  glm::radians<float>(180.f) + std::atan2(direction_to_player_xz.x,
                                                                          direction_to_player_xz.z),
                                  0.f),
                    .scale = glm::vec3(1.f, 1.f, 1.f),
                };
                renderer.add_regular_object({
                    .material = chaser_textured_material,
                    .normal_map = chaser_normal_map,
                    .transform = chaser_transform,
                    .drawable = chaser_vertex_array,
                });

                /*
                 * Submit floating chaser to renderer.
                 */
                Renderer::Transform floating_chaser_transform = {
                    .position = glm::vec3(0.f, 10.f, 0.f),
                    .rotation = glm::vec3(0.f, 0.f, 0.f),
                    .scale = glm::vec3(1.f, 1.f, 1.f),
                };
                renderer.add_regular_object({
                    .material = chaser_textured_material,
                    .normal_map = chaser_normal_map,
                    .transform = floating_chaser_transform,
                    .drawable = chaser_vertex_array,
                });

                /*
                 * Submit point light to renderer.
                 */
                glm::vec3 point_light_color = 10.f * glm::vec3(0xFF, 0xDF, 0x22) / 255.f;
                Renderer::Transform point_light_transform = {
                    .position = point_light_position,
                    .rotation = glm::vec3(0.f, 0.f, 0.f),
                    .scale = glm::vec3(1.f, 1.f, 1.f),
                };
                renderer.add_point_light_object({
                    .color = point_light_color,
                    .transform = point_light_transform,
                    .drawable = chaser_vertex_array,
                });

                /*
                 * Submit sun to renderer.
                 */
                static constexpr glm::vec3 sun_color = 10.f * glm::vec3(1.0f, 0.95f, 0.85f);
                glm::vec3 directional_light_color = sun_color * sun_brightness;
                renderer.add_directional_light_object({
                    .direction = directional_light_direction,
                    .color = directional_light_color,
                });

                /*
                 * Use the player's view but remove translation and add rotation to
                 * emulate the planet rotating.
                 */
                const glm::mat4 view_skybox =
                    glm::rotate(glm::mat4(glm::mat3(view)), orbital_angle, rotation_axis);

                ASSERT_RET_IF_NOT(renderer.render(view, view_skybox, player_position, direction),
                                  false);
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