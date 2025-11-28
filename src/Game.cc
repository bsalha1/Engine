#include "Game.h"

#include "assert_util.h"
#include "glm/ext/vector_float3.hpp"
#include "log.h"

#include <array>
#include <chrono>
#include <execinfo.h>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>
#include <string>

namespace Engine
{
    static constexpr unsigned int window_width = 640;
    static constexpr unsigned int window_height = 480;

    struct Vertex3d
    {
        float x;
        float y;
        float z;
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

            char message[1024];
            ASSERT_RET_IF(length > sizeof(message), false);

            glGetShaderInfoLog(shader_id, length, &length, message);
            LOG_ERROR("Failed to compile shader: %s", message);

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
            // Capture stack
            void *bt[128];
            int bt_size = backtrace(bt, 128);
            char **bt_syms = backtrace_symbols(bt, bt_size);

            fprintf(stderr, "Stack trace:\n");
            for (int i = 0; i < bt_size; i++)
                fprintf(stderr, "  %s\n", bt_syms[i]);

            free(bt_syms);

            Game *game = reinterpret_cast<Game *>(const_cast<void *>(user_param));
            LOG_ERROR("OpenGL error: %s\n", message);
            game->stop_run();
        }
        else
        {
            LOG("OpenGL debug: %s\n", message);
        }
    }

    /**
     * Constructor.
     */
    Game::Game(): window(nullptr), program_id(0), should_run(true)
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

        window =
            glfwCreateWindow(window_width, window_height, "Game", nullptr, nullptr);
        ASSERT_RET_IF(window == nullptr, false);
        glfwMakeContextCurrent(window);

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
            /*
             *     |
             *  3  -  2
             *     |
             * -|-----|-
             *     |
             *  0  -  1
             *     |
             */

            const std::array<Vertex3d, 8> positions = {{
                {-1, -1, 1}, /* 0 */
                {1, -1, 1},  /* 1 */
                {1, 1, 1},   /* 2 */
                {-1, 1, 1},  /* 3 */

                {-1, -1, -1}, /* 4 */
                {1, -1, -1},  /* 5 */
                {1, 1, -1},   /* 6 */
                {-1, 1, -1},  /* 7 */
            }};

            GLuint vertex_array_obj;
            glGenVertexArrays(1, &vertex_array_obj);
            glBindVertexArray(vertex_array_obj);

            GLuint buffer_obj;
            glGenBuffers(1, &buffer_obj);
            glBindBuffer(GL_ARRAY_BUFFER, buffer_obj);
            glBufferData(
                GL_ARRAY_BUFFER, sizeof(positions), positions.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex3d), 0);
            glEnableVertexAttribArray(0);
        }

        /*
         * Create index buffer.
         */
        {
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
                0,
                1,
                5,
                5,
                4,
                0,

                /* -Y face */
                3,
                2,
                6,
                6,
                7,
                3,
            };

            GLuint buffer_obj;
            glGenBuffers(1, &buffer_obj);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_obj);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         sizeof(indices),
                         indices.data(),
                         GL_STATIC_DRAW);
        }

        LOG("Compiling shaders\n");

        {
            std::string vertex_shader_src;
            ASSERT_RET_IF_NOT(get_shader_src("shaders/basic.vert", vertex_shader_src),
                              false);
            std::string fragment_shader_src;
            ASSERT_RET_IF_NOT(get_shader_src("shaders/basic.frag", fragment_shader_src),
                              false);

            ASSERT_RET_IF_NOT(
                create_shader(program_id, vertex_shader_src, fragment_shader_src),
                false);
            glUseProgram(program_id);
        }

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
     * Run the game.
     */
    bool Game::run()
    {
        LOG("Entering main loop\n");

        /*
         * Camera view.
         */
        glm::mat4 camera_view = glm::lookAt(glm::vec3(3, 3, 3), /* eye */
                                            glm::vec3(0, 0, 0), /* center */
                                            glm::vec3(0, 1, 0)  /* up */
        );

        /*
         * Place model at origin.
         */
        const glm::mat4 model = glm::mat4(1.0f);

        /*
         * Rotate about the Y axis.
         */
        const glm::vec3 rotation_axis = glm::vec3(0, 1, 0);

        /*
         * Perspective.
         */
        const glm::mat4 perspective =
            glm::perspective(glm::radians(45.0f),
                             static_cast<float>(window_width) /
                                 static_cast<float>(window_height),
                             0.1f,
                             100.0f);

        /*
         * Get reference to the model view projection.
         */
        const GLuint model_view_projection_object =
            glGetUniformLocation(program_id, "model_view_projection");

        /*
         * Loop until the user closes the window.
         */
        using namespace std::chrono_literals;
        static constexpr std::chrono::seconds stats_period = 1s;
        std::chrono::steady_clock::time_point last_stats_time =
            std::chrono::steady_clock::now();
        const std::chrono::steady_clock::time_point game_start_time =
            std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point frame_start_time =
            std::chrono::steady_clock::now();
        while (should_run && !glfwWindowShouldClose(window))
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
            const float dt = dt_ns / 1e9f;

            frame_start_time = std::chrono::steady_clock::now();

            /*
             * Rotate the camera view by dt radians.
             */
            camera_view = glm::rotate(camera_view, dt, rotation_axis);

            /*
             * Update model view projection.
             */
            const glm::mat4 model_view_projection = perspective * camera_view * model;
            glUniformMatrix4fv(model_view_projection_object,
                               1,
                               GL_FALSE,
                               &model_view_projection[0][0]);

            /*
             * Clear both the color and depth buffers.
             */
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            /*
             * Draw the scene.
             */
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

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
            if (now_time - last_stats_time > stats_period)
            {
                last_stats_time = now_time;
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
    void Game::stop_run()
    {
        should_run = false;
    }
}