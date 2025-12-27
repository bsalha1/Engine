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
    struct Vertex3d
    {
        glm::vec3 position;
    };
    static_assert(sizeof(Vertex3d) == 3 * sizeof(float));

    struct TexturedVertex3d
    {
        glm::vec3 position;
        glm::vec2 texture;
    };
    static_assert(sizeof(TexturedVertex3d) == 5 * sizeof(float));

    struct TexturedVertex2d
    {
        glm::vec2 position;
        glm::vec2 texture;
    };
    static_assert(sizeof(TexturedVertex2d) == 4 * sizeof(float));

    /**
     * A vertex with a position and a normal vector.
     */
    struct TexturedVector3dNormal
    {
        glm::vec3 position;
        glm::vec3 norm = glm::vec3(0.f);
        glm::vec2 texture;
    };
    static_assert(sizeof(TexturedVector3dNormal) == 8 * sizeof(float));

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
        exposure(1.0f),
        gamma(1.0f),
        sharpness(1.0f),
        chaser_position(0.f, 0.f, 0.f),
        point_light_position(150.f, 100.f, 120.f)
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
                                                    heightmap + num_rows * num_cols *
                                                                    dimensions);

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
                            height_kerneled =
                                glm::clamp(height_kerneled, 0, num_cols - 1);
                            row_kerneled = glm::clamp(row_kerneled, 0, num_rows - 1);

                            const float kern = kernel[kernel_z + 1][kernel_x + 1];
                            sum += heightmap_original[(row_kerneled * num_cols +
                                                       height_kerneled) *
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
         * Now that we know the aspect ratio, set up the projection matrix.
         */
        static constexpr float fov_deg = 75.f;
        static constexpr const float far_clip = 5000.f;
        const float aspect = static_cast<float>(window_width) / window_height;
        const float near_clip = 0.001f;
        projection =
            glm::perspective(glm::radians(fov_deg), aspect, near_clip, far_clip);

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
        LOG("Creating screen buffer\n");
        {
            /* clang-format off */
            const std::array<TexturedVertex2d, 6> vertices = {{
            /*                  position,              texture */
                {glm::vec2(-1.0f,  1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec2(-1.0f, -1.0f), glm::vec2(0.0f, 0.0f)},
                {glm::vec2( 1.0f, -1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec2(-1.0f,  1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec2( 1.0f, -1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec2( 1.0f,  1.0f), glm::vec2(1.0f, 1.0f)},
            }};
            /* clang-format on */

            quad_textured_vertex_array.create(vertices.data(), vertices.size());
            quad_textured_vertex_array.setup_vertex_attrib(0,
                                                           &TexturedVertex2d::position);
            quad_textured_vertex_array.setup_vertex_attrib(1,
                                                           &TexturedVertex2d::texture);

            glGenFramebuffers(1, &screen_frame_buffer);
            glBindFramebuffer(GL_FRAMEBUFFER, screen_frame_buffer);

            /*
             * Create textures to hold the color and brightness buffers.
             */
            screen_color_texture.create(window_width,
                                        window_height,
                                        GL_COLOR_ATTACHMENT0,
                                        0 /* slot */,
                                        GL_REPEAT);
            screen_bloom_texture.create(window_width,
                                        window_height,
                                        GL_COLOR_ATTACHMENT1,
                                        1 /* slot */,
                                        GL_CLAMP_TO_EDGE);

            /*
             * Create a render buffer to hold the depth and stencil buffer.
             */
            GLuint depth_stencil_render_buffer;
            glGenRenderbuffers(1, &depth_stencil_render_buffer);
            glBindRenderbuffer(GL_RENDERBUFFER, depth_stencil_render_buffer);
            glRenderbufferStorage(
                GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, window_width, window_height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                      GL_DEPTH_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER,
                                      depth_stencil_render_buffer);

            ASSERT_RET_IF_NOT(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                                  GL_FRAMEBUFFER_COMPLETE,
                              false);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            /*
             * Create ping-pong frame buffers for blurring the brightness texture.
             */
            for (uint8_t i = 0; i < ping_pong_frame_buffer.size(); i++)
            {
                glGenFramebuffers(1, &ping_pong_frame_buffer[i]);
                glBindFramebuffer(GL_FRAMEBUFFER, ping_pong_frame_buffer[i]);

                ping_pong_texture[i].create(screen_bloom_texture.get_width(),
                                            screen_bloom_texture.get_height(),
                                            GL_COLOR_ATTACHMENT0,
                                            screen_bloom_texture.get_slot(),
                                            GL_CLAMP_TO_EDGE);

                ASSERT_RET_IF_NOT(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                                      GL_FRAMEBUFFER_COMPLETE,
                                  false);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        /*
         * Create chaser buffers.
         */
        LOG("Creating entity buffers\n");
        {
            /* clang-format off */
            const std::array<TexturedVertex3d, 36> vertices = {{
            /*                         position,              texture */
                {glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 0.0f)},

                {glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec2(0.0f, 0.0f)},

                {glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 0.0f)},

                {glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 0.0f)},

                {glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 1.0f)},

                {glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec2(0.0f, 1.0f)}
            }};
            /* clang-format on */

            chaser_vertex_array.create(vertices.data(), vertices.size());
            chaser_vertex_array.setup_vertex_attrib(0, &TexturedVertex3d::position);
            chaser_vertex_array.setup_vertex_attrib(1, &TexturedVertex3d::texture);
        }

        LOG("Compiling shaders\n");
        ASSERT_RET_IF_NOT(basic_textured_shader.compile(
                              {{"basic_textured.vert", GL_VERTEX_SHADER},
                               {"basic_textured.frag", GL_FRAGMENT_SHADER}}),
                          false);
        ASSERT_RET_IF_NOT(terrain_shader.compile({
                              {"terrain.vert", GL_VERTEX_SHADER},
                              {"terrain.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        ASSERT_RET_IF_NOT(skybox_shader.compile({
                              {"skybox.vert", GL_VERTEX_SHADER},
                              {"skybox.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        ASSERT_RET_IF_NOT(light_shader.compile({
                              {"light.vert", GL_VERTEX_SHADER},
                              {"light.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        ASSERT_RET_IF_NOT(screen_shader.compile({
                              {"screen.vert", GL_VERTEX_SHADER},
                              {"screen.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        ASSERT_RET_IF_NOT(gaussian_blur_shader.compile({
                              {"gaussian_blur.vert", GL_VERTEX_SHADER},
                              {"gaussian_blur.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);

        LOG("Loading textures\n");
        ASSERT_RET_IF_NOT(
            chaser_texture.create_from_file("textures/obama.png", 0 /* slot */), false);
        ASSERT_RET_IF_NOT(
            dirt_texture.create_from_file("textures/dirt.jpg", 0 /* slot */), false);

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
            const unsigned int num_indices = (terrain_num_rows - 1) *
                                             (terrain_num_cols - 1) *
                                             num_indices_per_vertex;

            /*
             * Draw one copy of the texture per cell.
             */
            const float texture_col_scale =
                1.f / static_cast<float>(terrain_num_cols) * dirt_texture.get_width();
            const float texture_row_scale =
                1.f / static_cast<float>(terrain_num_rows) * dirt_texture.get_height();

            std::vector<TexturedVector3dNormal> vertices;
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
                    const uint8_t y =
                        heightmap[(terrain_num_cols * row + col) * terrain_channels];

                    TexturedVector3dNormal &vertex = vertices.emplace_back();

                    vertex.position = {
                        col - terrain_x_middle,
                        y * y_scale + y_bottom,
                        row - terrain_z_middle,
                    };

                    vertex.texture = {
                        col * texture_col_scale,
                        row * texture_row_scale,
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
             * Compute normals by averaging face normals of adjacent faces.
             */
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

                TexturedVector3dNormal &v0 = vertices[indices[i + 0]];
                TexturedVector3dNormal &v1 = vertices[indices[i + 1]];
                TexturedVector3dNormal &v2 = vertices[indices[i + 2]];

                const glm::vec3 e1 = v1.position - v0.position;
                const glm::vec3 e2 = v2.position - v0.position;

                const glm::vec3 face_normal = glm::normalize(glm::cross(e1, e2));

                v0.norm += face_normal;
                v1.norm += face_normal;
                v2.norm += face_normal;
            }

            /*
             * Normalize the normals to average them.
             */
            for (TexturedVector3dNormal &vertex : vertices)
            {
                vertex.norm = glm::normalize(vertex.norm);
            }

            terrain_vertex_array.create(vertices.data(), vertices.size());
            terrain_vertex_array.setup_vertex_attrib(0,
                                                     &TexturedVector3dNormal::position);
            terrain_vertex_array.setup_vertex_attrib(1, &TexturedVector3dNormal::norm);
            terrain_vertex_array.setup_vertex_attrib(2,
                                                     &TexturedVector3dNormal::texture);
            terrain_index_buffer.create(indices.data(), indices.size());
        }

        LOG("Loading skybox\n");
        {
            ASSERT_RET_IF_NOT(skybox_texture.create_from_file("textures/skybox/",
                                                              ".jpg",
                                                              0 /* slot */),
                              false);

            static const std::array<Vertex3d, 36> skybox_vertices = {
                /* clang-format off */
                glm::vec3(-1.0f, -1.0f, -1.0f),
                glm::vec3( 1.0f, -1.0f, -1.0f),
                glm::vec3( 1.0f,  1.0f, -1.0f),
                glm::vec3( 1.0f,  1.0f, -1.0f),
                glm::vec3(-1.0f,  1.0f, -1.0f),
                glm::vec3(-1.0f, -1.0f, -1.0f),

                glm::vec3(-1.0f, -1.0f,  1.0f),
                glm::vec3( 1.0f, -1.0f,  1.0f),
                glm::vec3( 1.0f,  1.0f,  1.0f),
                glm::vec3( 1.0f,  1.0f,  1.0f),
                glm::vec3(-1.0f,  1.0f,  1.0f),
                glm::vec3(-1.0f, -1.0f,  1.0f),

                glm::vec3(-1.0f,  1.0f,  1.0f),
                glm::vec3(-1.0f,  1.0f, -1.0f),
                glm::vec3(-1.0f, -1.0f, -1.0f),
                glm::vec3(-1.0f, -1.0f, -1.0f),
                glm::vec3(-1.0f, -1.0f,  1.0f),
                glm::vec3(-1.0f,  1.0f,  1.0f),

                glm::vec3( 1.0f,  1.0f,  1.0f),
                glm::vec3( 1.0f,  1.0f, -1.0f),
                glm::vec3( 1.0f, -1.0f, -1.0f),
                glm::vec3( 1.0f, -1.0f, -1.0f),
                glm::vec3( 1.0f, -1.0f,  1.0f),
                glm::vec3( 1.0f,  1.0f,  1.0f),

                glm::vec3(-1.0f, -1.0f, -1.0f),
                glm::vec3( 1.0f, -1.0f, -1.0f),
                glm::vec3( 1.0f, -1.0f,  1.0f),
                glm::vec3( 1.0f, -1.0f,  1.0f),
                glm::vec3(-1.0f, -1.0f,  1.0f),
                glm::vec3(-1.0f, -1.0f, -1.0f),

                glm::vec3(-1.0f,  1.0f, -1.0f),
                glm::vec3( 1.0f,  1.0f, -1.0f),
                glm::vec3( 1.0f,  1.0f,  1.0f),
                glm::vec3( 1.0f,  1.0f,  1.0f),
                glm::vec3(-1.0f,  1.0f,  1.0f),
                glm::vec3(-1.0f,  1.0f, -1.0f),
                /* clang-format on */
            };

            skybox_vertex_array.create(skybox_vertices.data(), skybox_vertices.size());
            skybox_vertex_array.setup_vertex_attrib(0, &Vertex3d::position);
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

            screen_shader.use();

            /*
             * Add display settings.
             */
            ImGui::SliderFloat("exposure", &exposure, 0.0, 10.0);
            ASSERT_RET_IF_NOT(screen_shader.set_Uniform1f("u_exposure", exposure),
                              false);

            ImGui::SliderFloat("gamma", &gamma, 0.0, 10.0);
            ASSERT_RET_IF_NOT(screen_shader.set_Uniform1f("u_gamma", gamma), false);

            ImGui::SliderFloat("sharpness", &sharpness, 1.0, 1000.0);
            ASSERT_RET_IF_NOT(screen_shader.set_Uniform1f("u_sharpness", sharpness),
                              false);

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
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("%.3f ms (%.0f FPS)", 1000.f / io.Framerate, io.Framerate);
        ImGui::Text("state: %s", state_to_string(state));
        ImGui::Text("player_state: %s", player_state_to_string(player_state));
        ImGui::Text("player_position: (%.2f, %.2f, %.2f)",
                    player_position.x,
                    player_position.y,
                    player_position.z);
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
            player_velocity += glm::normalize(move_direction) * player_move_impulse *
                               static_cast<float>(dt);
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
     * @brief Draw objects which only contribute to color, not bloom.
     *
     * @param view View matrix.
     * @param chaser_model Model matrix of the chasing chaser.
     * @param terrain_model Model matrix of the terrain.
     * @param directional_light_direction Directional light direction.
     * @param sun_brightness Brightness of the sun.
     *
     * @return True on success, otherwise false.
     */
    bool Game::draw_non_blooming_objects(const glm::mat4 view,
                                         const glm::mat4 &chaser_model,
                                         const glm::mat4 &terrain_model,
                                         const glm::vec3 &directional_light_direction,
                                         const float sun_brightness)
    {
        /*
         * Draw things that only contribute to color, not bloom.
         */
        const std::array<GLenum, 1> buffers = {
            screen_color_texture.get_attachment(),
        };
        glDrawBuffers(buffers.size(), buffers.data());

        /*
         * Draw the chasers.
         */

        basic_textured_shader.use();
        chaser_texture.use();

        /*
         * Draw a stationary chaser at the origin.
         */
        {
            const glm::mat4 model_view_projection = projection * view * glm::mat4(1.0f);
            ASSERT_RET_IF_NOT(basic_textured_shader.set_UniformMatrix4fv(
                                  "u_model_view_projection",
                                  &model_view_projection[0][0]),
                              false);

            chaser_vertex_array.draw();
        }

        /*
         * Draw chaser which chases.
         */
        {
            const glm::mat4 model_view_projection = projection * view * chaser_model;
            ASSERT_RET_IF_NOT(basic_textured_shader.set_UniformMatrix4fv(
                                  "u_model_view_projection",
                                  &model_view_projection[0][0]),
                              false);

            chaser_vertex_array.draw();
        }

        /*
         * Draw the terrain.
         */
        {
            terrain_vertex_array.bind();
            dirt_texture.use();
            terrain_shader.use();

            const glm::vec3 directional_light_color = sun_color * sun_brightness;

            ASSERT_RET_IF_NOT(terrain_shader.set_UniformMatrix4fv("u_model",
                                                                  &terrain_model[0][0]),
                              false);
            ASSERT_RET_IF_NOT(
                terrain_shader.set_UniformMatrix4fv("u_view", &view[0][0]), false);
            ASSERT_RET_IF_NOT(terrain_shader.set_UniformMatrix4fv("u_projection",
                                                                  &projection[0][0]),
                              false);
            ASSERT_RET_IF_NOT(terrain_shader.set_Uniform3f("u_point_light.position",
                                                           point_light_position),
                              false);
            ASSERT_RET_IF_NOT(
                terrain_shader.set_Uniform3f("u_directional_light.direction",
                                             directional_light_direction),
                false);
            ASSERT_RET_IF_NOT(terrain_shader.set_Uniform3f("u_directional_light.color",
                                                           directional_light_color),
                              false);
            ASSERT_RET_IF_NOT(terrain_shader.set_Uniform3f("u_camera_position",
                                                           player_position),
                              false);

            terrain_index_buffer.draw();
        }

        return true;
    }

    /**
     * @brief Draw objects which contribute to both color and bloom.
     *
     * @param view View matrix.
     *
     * @return True on success, otherwise false.
     */
    bool Game::draw_blooming_objects(const glm::mat4 view)
    {
        /*
         * Draw things which contribute both color and bloom.
         */
        const std::array<GLenum, 2> buffers = {
            screen_color_texture.get_attachment(),
            screen_bloom_texture.get_attachment(),
        };
        glDrawBuffers(buffers.size(), buffers.data());

        light_shader.use();

        /*
         * Draw point light.
         */
        const glm::mat4 point_light_model =
            glm::translate(glm::mat4(1.f), point_light_position);

        const glm::mat4 point_light_model_view_projection =
            projection * view * point_light_model;

        ASSERT_RET_IF_NOT(
            light_shader.set_UniformMatrix4fv("u_model_view_projection",
                                              &point_light_model_view_projection[0][0]),
            false);

        chaser_vertex_array.draw();

        return true;
    }

    /**
     * @brief Draw the skybox.
     *
     * @param view View matrix.
     * @param orbital_angle Orbital angle of the skybox.
     *
     * @return True on success, otherwise false.
     */
    bool Game::draw_skybox(const glm::mat4 view, const float orbital_angle)
    {
        /*
         * Draw to both color and bloom buffers.
         */
        const std::array<GLenum, 2> buffers = {
            screen_color_texture.get_attachment(),
            screen_bloom_texture.get_attachment(),
        };
        glDrawBuffers(buffers.size(), buffers.data());

        glDepthFunc(GL_LEQUAL);

        skybox_texture.use();

        /*
         * Use the player's view but remove translation and add rotation to
         * emulate the planet rotating.
         */
        const glm::mat4 view_skybox =
            glm::rotate(glm::mat4(glm::mat3(view)), orbital_angle, rotation_axis);

        skybox_shader.use();
        ASSERT_RET_IF_NOT(
            skybox_shader.set_UniformMatrix4fv("u_view", &view_skybox[0][0]), false);
        ASSERT_RET_IF_NOT(skybox_shader.set_UniformMatrix4fv("u_projection",
                                                             &projection[0][0]),
                          false);

        skybox_vertex_array.draw();

        glDepthFunc(GL_LESS);

        return true;
    }

    /**
     * @brief Draw the scene.
     *
     * @param chaser_model Model matrix of the chasing chaser.
     * @param terrain_model Model matrix of the terrain.
     * @param directional_light_direction Directional light direction.
     * @param orbital_angle Orbital angle of the skybox.
     * @param sun_brightness Brightness of the sun.
     *
     * @return True on success, otherwise false.
     */
    bool Game::draw(const glm::mat4 &chaser_model,
                    const glm::mat4 &terrain_model,
                    const glm::vec3 &directional_light_direction,
                    const float orbital_angle,
                    const float sun_brightness)
    {
        /*
         * Compute view looking at the direction our mouse is pointing.
         */
        const glm::mat4 view =
            glm::lookAt(player_position, player_position + direction, head);

        /*
         * Clear both the color and bloom buffers.
         */
        const std::array<GLenum, 2> buffers = {
            screen_color_texture.get_attachment(),
            screen_bloom_texture.get_attachment(),
        };
        glDrawBuffers(buffers.size(), buffers.data());
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /*
         * Draw objects which do not contribute to bloom.
         */
        ASSERT_RET_IF_NOT(draw_non_blooming_objects(view,
                                                    chaser_model,
                                                    terrain_model,
                                                    directional_light_direction,
                                                    sun_brightness),
                          false);

        /*
         * Draw objects which contribute to bloom.
         */
        ASSERT_RET_IF_NOT(draw_blooming_objects(view), false);

        /*
         * Lastly, draw the skybox.
         */
        ASSERT_RET_IF_NOT(draw_skybox(view, orbital_angle), false);

        return true;
    }

    /**
     * Run the game.
     */
    bool Game::run()
    {
        LOG("Entering main loop\n");

        /*
         * Initialize terrain shader uniforms.
         */
        terrain_shader.use();
        ASSERT_RET_IF_NOT(terrain_shader.set_Uniform3f("u_point_light.color",
                                                       point_light_color),
                          false);
        ASSERT_RET_IF_NOT(terrain_shader.set_Uniform1i("u_texture_sampler",
                                                       dirt_texture.get_slot()),
                          false);

        /*
         * Initialize basic textured shader uniforms.
         */
        basic_textured_shader.use();
        ASSERT_RET_IF_NOT(basic_textured_shader.set_Uniform1i(
                              "u_texture_sampler", chaser_texture.get_slot()),
                          false);

        /*
         * Assign gaussian blur shader texture sampler. This is the same as
         * ping_pong_texture so we don't need to spend time switching slots.
         */
        gaussian_blur_shader.use();
        ASSERT_RET_IF_NOT(gaussian_blur_shader.set_Uniform1i(
                              "u_texture_sampler", screen_bloom_texture.get_slot()),
                          false);

        /*
         * Initialize screen shader uniforms.
         */
        screen_shader.use();
        ASSERT_RET_IF_NOT(screen_shader.set_Uniform1i("u_color_texture_sampler",
                                                      screen_color_texture.get_slot()),
                          false);
        ASSERT_RET_IF_NOT(screen_shader.set_Uniform1i("u_bloom_texture_sampler",
                                                      screen_bloom_texture.get_slot()),
                          false);
        ASSERT_RET_IF_NOT(screen_shader.set_Uniform1f("u_exposure", exposure), false);
        ASSERT_RET_IF_NOT(screen_shader.set_Uniform1f("u_gamma", gamma), false);
        ASSERT_RET_IF_NOT(screen_shader.set_Uniform1f("u_sharpness", sharpness), false);

        /*
         * Set day length and compute the rotational speed.
         */
        static constexpr float day_length_s = 10.f;
        static constexpr float rotational_angular_speed =
            2 * glm::pi<float>() / day_length_s;

        /*
         * Relative to the terrain, the skybox spins around it. We draw a sun
         * on the skybox in its model space so that it rotates with it with an
         * elevation angle above the orbital plane.
         */
        static constexpr float sun_angular_radius = glm::radians<float>(5.f);
        const float sun_radius_skybox_model_space = glm::sin(sun_angular_radius);
        static constexpr float sun_orbital_elevation_angle = glm::radians<float>(10.f);
        const glm::vec4 sun_position_skybox_model_space =
            glm::vec4(0.f,
                      glm::sin(sun_orbital_elevation_angle),
                      glm::cos(sun_orbital_elevation_angle),
                      0.f);

        /*
         * Initialize skybox shader uniforms.
         */
        skybox_shader.use();
        ASSERT_RET_IF_NOT(skybox_shader.set_Uniform1f("u_sun_angular_radius",
                                                      sun_angular_radius),
                          false);
        ASSERT_RET_IF_NOT(skybox_shader.set_Uniform3f("u_sun_position",
                                                      sun_position_skybox_model_space),
                          false);
        ASSERT_RET_IF_NOT(skybox_shader.set_Uniform3f("u_sun_color", sun_color), false);
        ASSERT_RET_IF_NOT(skybox_shader.set_Uniform1i("u_texture_sampler",
                                                      skybox_texture.get_slot()),
                          false);

        /*
         * Loop until the user closes the window or state gets set to QUIT by the
         * program.
         */
        std::chrono::steady_clock::time_point start_time =
            std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point frame_start_time = start_time;
        while (state != State::QUIT && !glfwWindowShouldClose(window))
        {
            /*
             * Compute how much time has passed since the last frame.
             */
            const uint64_t dt_ns =
                (std::chrono::steady_clock::now() - frame_start_time).count();
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

                const float orbital_angle = rotational_angular_speed * time_since_start;

                /*
                 * Update chaser position to move towards player position on X-Z
                 * plane and face them.
                 */
                const glm::vec3 direction_to_player_xz =
                    glm::normalize(glm::vec3(player_position.x - chaser_position.x,
                                             0.f,
                                             player_position.z - chaser_position.z));
                static constexpr float chaser_move_impulse = 1.f;
                chaser_position += direction_to_player_xz * chaser_move_impulse *
                                   static_cast<float>(dt);

                glm::mat4 chaser_model =
                    glm::translate(glm::mat4(1.f), chaser_position);

                chaser_model = glm::rotate(chaser_model,
                                           glm::radians<float>(180.f) +
                                               std::atan2(direction_to_player_xz.x,
                                                          direction_to_player_xz.z),
                                           glm::vec3(0.f, 1.f, 0.f));

                static float light_velocity = 20.f;

                /*
                 * Update point light position.
                 */
                if (point_light_position.y <
                    get_terrain_height(point_light_position.x, point_light_position.z) +
                        5.f)
                {
                    light_velocity = 20.f;
                }
                if (point_light_position.y >
                    get_terrain_height(point_light_position.x, point_light_position.z) +
                        100.f)
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

                const glm::vec3 sun_position_terrain_model_space = glm::vec3(
                    terrain_model_matrix_rotated * sun_position_skybox_model_space);

                const glm::vec3 directional_light_direction =
                    -glm::normalize(sun_position_terrain_model_space);

                /*
                 * Compute sun brightness based on its elevation angle.
                 */
                const float sun_top_y =
                    sun_position_terrain_model_space.y + sun_radius_skybox_model_space;
                const float sun_distance =
                    glm::sqrt(sun_top_y * sun_top_y +
                              sun_position_terrain_model_space.x *
                                  sun_position_terrain_model_space.x +
                              sun_position_terrain_model_space.z *
                                  sun_position_terrain_model_space.z);
                const float sine_of_elevation_angle = sun_top_y / sun_distance;

                static constexpr float brightness_falloff_factor = 0.1f;
                const float sun_brightness = sine_of_elevation_angle <= 0.f
                                                 ? 0.f
                                                 : glm::exp(-brightness_falloff_factor /
                                                            sine_of_elevation_angle);

                /*
                 * Set frame buffer to the screen frame buffer.
                 */
                glBindFramebuffer(GL_FRAMEBUFFER, screen_frame_buffer);

                /*
                 * Draw scene into color buffer and brightness buffers.
                 */
                ASSERT_RET_IF_NOT(draw(chaser_model,
                                       terrain_model,
                                       directional_light_direction,
                                       orbital_angle,
                                       sun_brightness),
                                  false);

                /*
                 * Apply a gaussian blur to the brightness texture to simulate bloom.
                 */
                uint8_t horizontal = 1;
                bool first_iteration = true;
                uint8_t passes = 10;
                gaussian_blur_shader.use();
                for (uint8_t i = 0; i < passes; ++i)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER,
                                      ping_pong_frame_buffer[horizontal]);
                    ASSERT_RET_IF_NOT(gaussian_blur_shader.set_Uniform1i("u_horizontal",
                                                                         horizontal),
                                      false);

                    horizontal = 1 ^ horizontal;

                    if (first_iteration)
                    {
                        screen_bloom_texture.use();
                    }
                    else
                    {
                        ping_pong_texture[horizontal].use();
                    }

                    quad_textured_vertex_array.bind();
                    quad_textured_vertex_array.draw();

                    if (first_iteration)
                    {
                        first_iteration = false;
                    }
                }

                /*
                 * Go back to default frame buffer and draw the screen texture over a
                 * quad.
                 */
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                screen_shader.use();
                quad_textured_vertex_array.bind();
                ping_pong_texture[horizontal].use();
                screen_shader.set_Uniform1i("u_bloom_texture_sampler",
                                            ping_pong_texture[horizontal].get_slot());
                screen_color_texture.use();
                quad_textured_vertex_array.draw();
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