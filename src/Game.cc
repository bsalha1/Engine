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
            const std::array<TexturedVertex3d, 36> vertices = {{
            /*                         position,              texture */
                {glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 0.0f)},

                {glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec2(0.0f, 0.0f)},

                {glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 0.0f)},

                {glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 0.0f)},

                {glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 1.0f)},

                {glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec2(0.0f, 1.0f)},
                {glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec2(1.0f, 1.0f)},
                {glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 0.0f)},
                {glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec2(0.0f, 0.0f)},
                {glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec2(0.0f, 1.0f)}
            }};
            /* clang-format on */

            chaser_vertex_array.create(vertices.size());
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
                offsetof(TexturedVertex3d, position);
            static constexpr GLuint position_coord_attrib_end_offset =
                position_coord_attrib_start_offset + sizeof(TexturedVertex3d::position);
            static constexpr GLuint position_coord_attrib_size = sizeof(float);
            static constexpr GLuint position_coord_attrib_count =
                (position_coord_attrib_end_offset -
                 position_coord_attrib_start_offset) /
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
                offsetof(TexturedVertex3d, texture);
            static constexpr GLuint texture_coord_attrib_end_offset =
                texture_coord_attrib_start_offset + sizeof(TexturedVertex3d::texture);
            static constexpr GLuint texture_coord_attrib_size = sizeof(float);
            static constexpr GLuint texture_coord_attrib_count =
                (texture_coord_attrib_end_offset - texture_coord_attrib_start_offset) /
                texture_coord_attrib_size;
            glVertexAttribPointer(texture_coord_attrib_index,
                                  texture_coord_attrib_count,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(TexturedVertex3d),
                                  (GLvoid *)texture_coord_attrib_start_offset);
            glEnableVertexAttribArray(texture_coord_attrib_index);
        }

        LOG("Compiling shaders\n");
        ASSERT_RET_IF_NOT(basic_textured_shader.compile("basic_textured"), false);
        ASSERT_RET_IF_NOT(terrain_shader.compile("terrain"), false);
        ASSERT_RET_IF_NOT(skybox_shader.compile("skybox"), false);
        ASSERT_RET_IF_NOT(light_shader.compile("light"), false);

        LOG("Loading textures\n");
        ASSERT_RET_IF_NOT(chaser_texture.load_from_file("textures/obama.png", 0),
                          false);
        ASSERT_RET_IF_NOT(dirt_texture.load_from_file("textures/dirt.jpg", 1), false);

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

            terrain_vertex_array.create(vertices.size());
            terrain_vertex_array.bind();

            GLuint terrain_vertex_buffer_array_obj;
            glGenBuffers(1, &terrain_vertex_buffer_array_obj);
            glBindBuffer(GL_ARRAY_BUFFER, terrain_vertex_buffer_array_obj);
            glBufferData(GL_ARRAY_BUFFER,
                         vertices.size() * sizeof(TexturedVector3dNormal),
                         vertices.data(),
                         GL_STATIC_DRAW);

            /*
             * Position coordinate attribute.
             */
            static constexpr GLuint position_attrib_index = 0;
            static constexpr GLuint position_attrib_start_offset =
                offsetof(TexturedVector3dNormal, position);
            static constexpr GLuint position_attrib_end_offset =
                position_attrib_start_offset + sizeof(TexturedVector3dNormal::position);
            static constexpr GLuint position_attrib_size = sizeof(float);
            static constexpr GLuint position_attrib_count =
                (position_attrib_end_offset - position_attrib_start_offset) /
                position_attrib_size;
            glVertexAttribPointer(position_attrib_index,
                                  position_attrib_count,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(TexturedVector3dNormal),
                                  (GLvoid *)position_attrib_start_offset);
            glEnableVertexAttribArray(position_attrib_index);

            /*
             * Normal vector attribute.
             */
            static constexpr GLuint norm_attrib_index = 1;
            static constexpr GLuint norm_attrib_start_offset =
                offsetof(TexturedVector3dNormal, norm);
            static constexpr GLuint norm_attrib_end_offset =
                norm_attrib_start_offset + sizeof(TexturedVector3dNormal::norm);
            static constexpr GLuint norm_attrib_size = sizeof(float);
            static constexpr GLuint norm_attrib_count =
                (norm_attrib_end_offset - norm_attrib_start_offset) / norm_attrib_size;
            glVertexAttribPointer(norm_attrib_index,
                                  norm_attrib_count,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(TexturedVector3dNormal),
                                  (GLvoid *)norm_attrib_start_offset);
            glEnableVertexAttribArray(norm_attrib_index);

            /*
             * Texture coordinate attribute.
             */
            static constexpr GLuint texture_attrib_index = 2;
            static constexpr GLuint texture_attrib_start_offset =
                offsetof(TexturedVector3dNormal, texture);
            static constexpr GLuint texture_attrib_end_offset =
                texture_attrib_start_offset + sizeof(TexturedVector3dNormal::texture);
            static constexpr GLuint texture_attrib_size = sizeof(float);
            static constexpr GLuint texture_attrib_count =
                (texture_attrib_end_offset - texture_attrib_start_offset) /
                texture_attrib_size;
            glVertexAttribPointer(texture_attrib_index,
                                  texture_attrib_count,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(TexturedVector3dNormal),
                                  (GLvoid *)texture_attrib_start_offset);
            glEnableVertexAttribArray(texture_attrib_index);

            terrain_index_buffer.create(indices.data(), indices.size());
        }

        LOG("Loading skybox\n");
        {
            ASSERT_RET_IF_NOT(skybox_texture.load_from_file("textures/skybox/", ".jpg"),
                              false);

            static const std::array<Vertex3d, 36> skybox_vertices = {
                /* clang-format off */
                glm::vec3(-0.5f, -0.5f, -0.5f),
                glm::vec3( 0.5f, -0.5f, -0.5f),
                glm::vec3( 0.5f,  0.5f, -0.5f),
                glm::vec3( 0.5f,  0.5f, -0.5f),
                glm::vec3(-0.5f,  0.5f, -0.5f),
                glm::vec3(-0.5f, -0.5f, -0.5f),

                glm::vec3(-0.5f, -0.5f,  0.5f),
                glm::vec3( 0.5f, -0.5f,  0.5f),
                glm::vec3( 0.5f,  0.5f,  0.5f),
                glm::vec3( 0.5f,  0.5f,  0.5f),
                glm::vec3(-0.5f,  0.5f,  0.5f),
                glm::vec3(-0.5f, -0.5f,  0.5f),

                glm::vec3(-0.5f,  0.5f,  0.5f),
                glm::vec3(-0.5f,  0.5f, -0.5f),
                glm::vec3(-0.5f, -0.5f, -0.5f),
                glm::vec3(-0.5f, -0.5f, -0.5f),
                glm::vec3(-0.5f, -0.5f,  0.5f),
                glm::vec3(-0.5f,  0.5f,  0.5f),

                glm::vec3( 0.5f,  0.5f,  0.5f),
                glm::vec3( 0.5f,  0.5f, -0.5f),
                glm::vec3( 0.5f, -0.5f, -0.5f),
                glm::vec3( 0.5f, -0.5f, -0.5f),
                glm::vec3( 0.5f, -0.5f,  0.5f),
                glm::vec3( 0.5f,  0.5f,  0.5f),

                glm::vec3(-0.5f, -0.5f, -0.5f),
                glm::vec3( 0.5f, -0.5f, -0.5f),
                glm::vec3( 0.5f, -0.5f,  0.5f),
                glm::vec3( 0.5f, -0.5f,  0.5f),
                glm::vec3(-0.5f, -0.5f,  0.5f),
                glm::vec3(-0.5f, -0.5f, -0.5f),

                glm::vec3(-0.5f,  0.5f, -0.5f),
                glm::vec3( 0.5f,  0.5f, -0.5f),
                glm::vec3( 0.5f,  0.5f,  0.5f),
                glm::vec3( 0.5f,  0.5f,  0.5f),
                glm::vec3(-0.5f,  0.5f,  0.5f),
                glm::vec3(-0.5f,  0.5f, -0.5f),
                /* clang-format on */
            };

            skybox_vertex_array.create(skybox_vertices.size());
            skybox_vertex_array.bind();

            GLuint skybox_buffer_obj;
            glGenBuffers(1, &skybox_buffer_obj);
            glBindBuffer(GL_ARRAY_BUFFER, skybox_buffer_obj);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(skybox_vertices),
                         skybox_vertices.data(),
                         GL_STATIC_DRAW);

            /*
             * Position coordinate attribute.
             */
            static constexpr GLuint position_attrib_index = 0;
            static constexpr GLuint position_attrib_start_offset =
                offsetof(Vertex3d, position);
            static constexpr GLuint position_attrib_end_offset =
                position_attrib_start_offset + sizeof(Vertex3d::position);
            static constexpr GLuint position_attrib_size = sizeof(float);
            static constexpr GLuint position_attrib_count =
                (position_attrib_end_offset - position_attrib_start_offset) /
                position_attrib_size;
            glVertexAttribPointer(position_attrib_index,
                                  position_attrib_count,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(Vertex3d),
                                  (GLvoid *)position_attrib_start_offset);
            glEnableVertexAttribArray(position_attrib_index);
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
     * Run the game.
     */
    bool Game::run()
    {
        LOG("Entering main loop\n");

        static constexpr float fov_deg = 90.f;
        static constexpr const float far_clip = 5000.f;
        const float aspect = static_cast<float>(window_width) / window_height;
        const float near_clip = 0.001f;
        const glm::mat4 projection =
            glm::perspective(glm::radians(fov_deg), aspect, near_clip, far_clip);

        terrain_shader.use();

        const glm::vec3 light_color_rgb = glm::vec3(0xFF, 0xDF, 0x22);

        /*
         * Initialize point light.
         */
        glm::vec3 point_light_position = glm::vec3(150.f, 100.f, 120.f);
        ASSERT_RET_IF_NOT(terrain_shader.set_Uniform3f("u_point_light.color",
                                                       light_color_rgb / 255.f),
                          false);
        /*
         * Initialize directional light.
         */
        ASSERT_RET_IF_NOT(terrain_shader.set_Uniform3f("u_directional_light.color",
                                                       light_color_rgb / 255.f),
                          false);

        /*
         * Assign texture samplers.
         */
        ASSERT_RET_IF_NOT(terrain_shader.set_Uniform1i("u_texture_sampler",
                                                       dirt_texture.get_slot()),
                          false);
        dirt_texture.use();

        basic_textured_shader.use();
        ASSERT_RET_IF_NOT(basic_textured_shader.set_Uniform1i(
                              "u_texture_sampler", chaser_texture.get_slot()),
                          false);
        chaser_texture.use();

        /*
         * Set day length and compute the rotational speed.
         */
        static constexpr float day_length_s = 10.f;
        static constexpr float rotational_angular_speed =
            2 * glm::pi<float>() / day_length_s;
        static constexpr float tilt = glm::radians<float>(23.5f);
        const glm::vec3 rotation_axis = glm::vec3(glm::sin(tilt), glm::cos(tilt), 0.f);

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
        skybox_shader.use();
        ASSERT_RET_IF_NOT(skybox_shader.set_Uniform1f("u_sun_angular_radius",
                                                      sun_angular_radius),
                          false);
        ASSERT_RET_IF_NOT(skybox_shader.set_Uniform3f("u_sun_position",
                                                      sun_position_skybox_model_space),
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

            time_since_start = (frame_start_time - start_time).count() / 1e9;

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
                const glm::mat4 view =
                    glm::lookAt(player_position, player_position + direction, head);

                const float orbital_angle = rotational_angular_speed * time_since_start;

                /*
                 * Draw the chasers.
                 */

                basic_textured_shader.use();

                /*
                 * Draw a chaser at the origin.
                 */
                {
                    const glm::mat4 model_view_projection =
                        projection * view * glm::mat4(1.0f);
                    ASSERT_RET_IF_NOT(basic_textured_shader.set_UniformMatrix4fv(
                                          "u_model_view_projection",
                                          &model_view_projection[0][0]),
                                      false);

                    chaser_vertex_array.draw();
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
                    chaser_position += direction_to_player_xz * chaser_move_impulse *
                                       static_cast<float>(dt);

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
                        projection * view * chaser_model;
                    ASSERT_RET_IF_NOT(basic_textured_shader.set_UniformMatrix4fv(
                                          "u_model_view_projection",
                                          &model_view_projection[0][0]),
                                      false);

                    chaser_vertex_array.draw();
                }

                /*
                 * Update and draw point light.
                 */
                {
                    static float light_velocity = 20.f;

                    /*
                     * Update point light position.
                     */
                    if (point_light_position.y <
                        get_terrain_height(point_light_position.x,
                                           point_light_position.z) +
                            5.f)
                    {
                        light_velocity = 20.f;
                    }
                    if (point_light_position.y >
                        get_terrain_height(point_light_position.x,
                                           point_light_position.z) +
                            100.f)
                    {
                        light_velocity = -20.f;
                    }
                    point_light_position.y += light_velocity * dt;

                    const glm::mat4 point_light_model =
                        glm::translate(glm::mat4(1.f), point_light_position);

                    const glm::mat4 point_light_model_view_projection =
                        projection * view * point_light_model;

                    ASSERT_RET_IF_NOT(basic_textured_shader.set_UniformMatrix4fv(
                                          "u_model_view_projection",
                                          &point_light_model_view_projection[0][0]),
                                      false);

                    chaser_vertex_array.draw();
                }

                /*
                 * Draw the terrain.
                 */
                {
                    /*
                     * Compute the directional light direction relative to the terrain
                     * by converting the sun's position from skybox model space to the
                     * terrain model space.
                     */
                    const glm::mat4 terrain_model_matrix = glm::mat4(1.0f);

                    const glm::mat4 terrain_model_matrix_rotated =
                        glm::rotate(terrain_model_matrix, orbital_angle, rotation_axis);

                    const glm::vec3 sun_position_terrain_model_space = glm::vec3(
                        terrain_model_matrix_rotated * sun_position_skybox_model_space);

                    const glm::vec3 directional_light_direction =
                        -glm::normalize(sun_position_terrain_model_space);

                    terrain_vertex_array.bind();
                    terrain_shader.use();

                    /*
                     * Compute sun brightness based on its elevation angle.
                     */

                    const float sun_top_y = sun_position_terrain_model_space.y +
                                            sun_radius_skybox_model_space;
                    const float sun_distance_xz =
                        glm::sqrt(sun_position_terrain_model_space.x *
                                      sun_position_terrain_model_space.x +
                                  sun_position_terrain_model_space.z *
                                      sun_position_terrain_model_space.z);
                    const float sine_of_elevation_angle = sun_top_y / sun_distance_xz;

                    static constexpr float brightness_falloff_factor = 0.1f;
                    const float sun_brightness =
                        sine_of_elevation_angle <= 0.f
                            ? 0.f
                            : glm::exp(-brightness_falloff_factor /
                                       sine_of_elevation_angle);
                    const glm::vec3 directional_light_color =
                        (light_color_rgb / 255.f) * sun_brightness;

                    ASSERT_RET_IF_NOT(terrain_shader.set_UniformMatrix4fv(
                                          "u_model", &terrain_model_matrix[0][0]),
                                      false);
                    ASSERT_RET_IF_NOT(terrain_shader.set_UniformMatrix4fv("u_view",
                                                                          &view[0][0]),
                                      false);
                    ASSERT_RET_IF_NOT(terrain_shader.set_UniformMatrix4fv(
                                          "u_projection", &projection[0][0]),
                                      false);
                    ASSERT_RET_IF_NOT(
                        terrain_shader.set_Uniform3f("u_point_light.position",
                                                     point_light_position),
                        false);
                    ASSERT_RET_IF_NOT(
                        terrain_shader.set_Uniform3f("u_directional_light.direction",
                                                     directional_light_direction),
                        false);
                    ASSERT_RET_IF_NOT(
                        terrain_shader.set_Uniform3f("u_directional_light.color",
                                                     directional_light_color),
                        false);
                    ASSERT_RET_IF_NOT(terrain_shader.set_Uniform3f("u_camera_position",
                                                                   player_position),
                                      false);

                    terrain_index_buffer.draw();
                }

                /*
                 * Lastly, draw the skybox.
                 */
                {
                    glDepthFunc(GL_LEQUAL);

                    skybox_texture.use();

                    /*
                     * Use the player's view but remove translation and add rotation to
                     * emulate the planet rotating.
                     */
                    const glm::mat4 view_skybox = glm::rotate(
                        glm::mat4(glm::mat3(view)), orbital_angle, rotation_axis);

                    skybox_shader.use();
                    skybox_shader.set_UniformMatrix4fv("u_view", &view_skybox[0][0]);
                    skybox_shader.set_UniformMatrix4fv("u_projection",
                                                       &projection[0][0]);

                    skybox_vertex_array.draw();

                    glDepthFunc(GL_LESS);
                }
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