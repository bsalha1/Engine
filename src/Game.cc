#include "Game.h"

#include "assert_util.h"
#include "log.h"

#include <GLFW/glfw3.h>
#include <array>
#include <execinfo.h>
#include <fstream>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

    static bool get_shader_src(const std::string &file_path, std::string &shader_src)
    {
        const std::ifstream file(file_path);
        ASSERT_RET_IF_NOT(file, false);

        std::stringstream buffer_obj;
        buffer_obj << file.rdbuf();
        shader_src = buffer_obj.str();

        return true;
    }

    static bool
    compile_shader(GLuint &shader_id, const GLuint type, const std::string &src)
    {
        shader_id = glCreateShader(type);
        ASSERT_RET_IF(shader_id == 0, false);

        const char *_src = src.c_str();
        glShaderSource(shader_id, 1, &_src, nullptr);
        glCompileShader(shader_id);

        GLint shader_compiled;
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &shader_compiled);
        if (shader_compiled != GL_TRUE)
        {
            GLsizei length;
            glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length);

            char message[4096];
            ASSERT_RET_IF(length > sizeof(message), false);

            glGetShaderInfoLog(shader_id, length, &length, message);
            LOG_ERROR("Failed to compile shader: %s\n", message);

            glDeleteShader(shader_id);

            return false;
        }

        return true;
    }

    static bool create_shader(GLuint &program_id,
                              const std::string &vertex_shader_src,
                              const std::string &fragment_shader_src)
    {
        program_id = glCreateProgram();
        ASSERT_RET_IF(program_id == 0, false);

        GLuint vertex_shader_id;
        ASSERT_RET_IF_NOT(
            compile_shader(vertex_shader_id, GL_VERTEX_SHADER, vertex_shader_src),
            false);

        GLuint fragment_shader_id;
        ASSERT_RET_IF_NOT(
            compile_shader(fragment_shader_id, GL_FRAGMENT_SHADER, fragment_shader_src),
            false);

        glAttachShader(program_id, vertex_shader_id);
        glAttachShader(program_id, fragment_shader_id);
        glLinkProgram(program_id);
        glValidateProgram(program_id);

        glDeleteShader(vertex_shader_id);
        glDeleteShader(fragment_shader_id);

        return true;
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
        else
        {
            LOG("OpenGL debug: %s\n", message);
        }
    }

    /**
     * Constructor.
     */
    Game::Game():
        window(nullptr),
        program_id(0),
        state(State::RUNNING),
        window_center_x(0),
        window_center_y(0),
        move_speed(standing_move_speed),
        height(standing_height),
        position(0.f, height, -10.f),
        velocity(0.f, 0.f, 0.f),
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
        head(0.f, 0.f, 0.f)
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

        LOG("Allocating buffers\n");

        /*
         * Create vertex buffer.
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

            GLuint vertex_array_obj;
            glGenVertexArrays(1, &vertex_array_obj);
            glBindVertexArray(vertex_array_obj);

            GLuint buffer_obj;
            glGenBuffers(1, &buffer_obj);
            glBindBuffer(GL_ARRAY_BUFFER, buffer_obj);
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
        }

        /*
         * Create index buffer.
         */
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

        index_buffer.create(reinterpret_cast<const void *>(indices.data()),
                            sizeof(indices));

        LOG("Compiling shaders\n");

        std::string vertex_shader_src;
        ASSERT_RET_IF_NOT(get_shader_src("shaders/basic.vert", vertex_shader_src),
                          false);
        std::string fragment_shader_src;
        ASSERT_RET_IF_NOT(get_shader_src("shaders/basic.frag", fragment_shader_src),
                          false);

        ASSERT_RET_IF_NOT(
            create_shader(program_id, vertex_shader_src, fragment_shader_src), false);
        glUseProgram(program_id);

        LOG("Loading textures\n");

        GLuint texture_obj;
        glGenTextures(1, &texture_obj);
        glBindTexture(GL_TEXTURE_2D, texture_obj);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        stbi_set_flip_vertically_on_load(1);
        int texture_width;
        int texture_height;
        int channels;
        uint8_t *texture_buffer = stbi_load(
            "textures/obama.png", &texture_width, &texture_height, &channels, 0);
        if (texture_buffer == nullptr)
        {
            LOG_ERROR("Failed to load texture\n");
            return false;
        }

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     texture_width,
                     texture_height,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     texture_buffer);

        free(texture_buffer);

        glActiveTexture(GL_TEXTURE0);

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
        height = crouching_height;
        move_speed = crouching_move_speed;
        position.y = height;
    }

    /**
     * Make player stand.
     */
    void Game::set_standing()
    {
        height = standing_height;
        move_speed = standing_move_speed;
        position.y = height;
    }

    /**
     * Process menu input.
     */
    void Game::process_menu()
    {
        bool escape_pressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        const bool escape_pressed_rising_edge = escape_pressed && !escape_pressed_prev;
        if (escape_pressed_rising_edge)
        {
            if (state == State::PAUSED)
            {
                state = State::RUNNING;
                LOG("PAUSED -> RUNNING\n");

                /*
                 * Put the mouse back to where it was before pausing and hide it again.
                 */
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                glfwSetCursorPos(window, mouse_x_prev, mouse_y_prev);
            }
            else
            {
                state = State::PAUSED;
                LOG("RUNNING -> PAUSED\n");

                /*
                 * Show the mouse cursor and put it in the middle of window.
                 */
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                glfwSetCursorPos(window, window_center_x, window_center_y);
            }
        }
        escape_pressed_prev = escape_pressed;
    }

    /**
     * Process jump or crouch input.
     */
    void Game::process_jump_crouch()
    {
        /*
         * If the player is off the ground, make gravity pull them down and do not allow
         * jumping or crouching.
         */
        if (position.y > height)
        {
            static constexpr float acceleration_gravity = 10.f;
            velocity.y -= acceleration_gravity * dt;
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

                    velocity.y = 10.f;

                    /*
                     * If player is jumping from a crouch, they should land
                     * standing, otherwise it'll be hard on the knees.
                     */
                    set_standing();
                }
            }
            else if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            {
                static constexpr std::chrono::milliseconds crouch_cooldown = 500ms;
                if (std::chrono::steady_clock::now() - last_crouch_time >
                    crouch_cooldown)
                {
                    last_crouch_time = std::chrono::steady_clock::now();

                    if (height == crouching_height)
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
                velocity.y = 0.f;
            }
        }
    }

    /**
     * Update view based on mouse movement.
     */
    void Game::update_view()
    {
        /*
         * Get mouse position relative to top-left pixel of the window.
         */
        double mouse_x;
        double mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);

        /*
         * Generate horizontal and vertical viewing angles from mouse position.
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
            static constexpr float max_vertical_angle = glm::radians<float>(90);
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
     * Update position based on keyboard input.
     */
    void Game::update_position()
    {
        /*
         * Move about the X-Z plane given keyboard inputs.
         */
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            velocity += forwards * move_speed;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            velocity -= forwards * move_speed;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            velocity += right * move_speed;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            velocity -= right * move_speed;
        }

        /*
         * If the player is moving, make friction slow them down.
         */
        if (std::fabs(velocity.x) > 0.f || std::fabs(velocity.z) > 0.f)
        {
            static constexpr float friction_coeff = 0.07f;
            velocity.x -= friction_coeff * velocity.x;
            velocity.z -= friction_coeff * velocity.z;
        }

        position += velocity * dt;
    }

    /**
     * Run the game.
     */
    bool Game::run()
    {
        LOG("Entering main loop\n");

        const glm::mat4 perspective =
            glm::perspective(glm::radians<float>(65), 4.f / 3.f, 0.1f, 100.f);

        /*
         * Place model at origin.
         */
        const glm::mat4 model = glm::mat4(1.0f);

        /*
         * Get reference to the model view projection.
         */
        const GLint model_view_projection_object =
            glGetUniformLocation(program_id, "model_view_projection");
        if (model_view_projection_object == -1)
        {
            LOG_ERROR("Failed to get model_view_projection uniform location\n");
            return false;
        }

        /*
         * Get reference to the texture object.
         */
        const GLint texture_sampler_object =
            glGetUniformLocation(program_id, "texture_sampler");
        if (texture_sampler_object == -1)
        {
            LOG_ERROR("Failed to get texture_sampler uniform location\n");
            return false;
        }
        glUniform1i(texture_sampler_object, 0);

        /*
         * Set initial state.
         */

        std::chrono::steady_clock::time_point stats_time_prev =
            std::chrono::steady_clock::now();
        const std::chrono::steady_clock::time_point game_start_time =
            std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point frame_start_time =
            std::chrono::steady_clock::now();

        /*
         * Loop until the user closes the window or state gets set to QUIT by the
         * program.
         */
        while (state != State::QUIT && !glfwWindowShouldClose(window))
        {
            /*
             * Compute how much time has passed.
             */
            const std::chrono::steady_clock::duration total_time =
                std::chrono::steady_clock::now() - game_start_time;

            /*
             * Compute how much time has passed since the last frame.
             */
            const uint64_t dt_ns =
                (std::chrono::steady_clock::now() - frame_start_time).count();
            dt = dt_ns / 1e9f;

            frame_start_time = std::chrono::steady_clock::now();

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
                 * Update position based on keyboard input.
                 */
                update_position();

                /*
                 * Compute view looking at the direction our mouse is pointing.
                 */
                const glm::mat4 camera_view =
                    glm::lookAt(position, position + direction, head);

                /*
                 * Update model view projection.
                 */
                const glm::mat4 model_view_projection =
                    perspective * camera_view * model;
                glUniformMatrix4fv(model_view_projection_object,
                                   1,
                                   GL_FALSE,
                                   &model_view_projection[0][0]);
            }

            /*
             * Clear both the color and depth buffers.
             */
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            /*
             * Draw the scene.
             */
            glDrawElements(GL_TRIANGLES,
                           index_buffer.get_count(),
                           decltype(index_buffer)::IndexGLtype,
                           nullptr);

            /*
             * Swap front and back buffers.
             */
            glfwSwapBuffers(window);

            /*
             * Poll for and process events.
             */
            glfwPollEvents();

            /*
             * Display stats.
             */
            const std::chrono::steady_clock::time_point now_time =
                std::chrono::steady_clock::now();
            static constexpr std::chrono::seconds stats_period = 1s;
            if (now_time - stats_time_prev > stats_period)
            {
                stats_time_prev = now_time;
                const std::chrono::nanoseconds frame_duration_ns =
                    now_time - frame_start_time;
                const float frames_per_second = 1e9f / frame_duration_ns.count();
                LOG("%.2f fps\n", frames_per_second);
            }
        }

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