#include "Game.h"

#include "GLFW/glfw3.h"
#include "assert_util.h"
#include "log.h"

#include <array>
#include <chrono>
#include <fstream>
#include <ratio>
#include <sstream>
#include <string>

namespace Engine
{
    struct Vertex2d
    {
        float x;
        float y;
    };

    static bool get_shader_src(const std::string &file_path, std::string &shader_src)
    {
        const std::ifstream file(file_path);
        ASSERT_RET_IF_NOT(file, false);

        std::stringstream buffer;
        buffer << file.rdbuf();
        shader_src = buffer.str();

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

    /**
     * Constructor.
     */
    Game::Game(): window(nullptr), program_id(0)
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
        window = glfwCreateWindow(640, 480, "Game", nullptr, nullptr);
        if (window == nullptr)
        {
            ASSERT_PRINT("Failed to initialize window");
            end();
            return false;
        }
        glfwMakeContextCurrent(window);

        // glfwSwapInterval(1);

        LOG("Initializing GLEW\n");
        ASSERT_RET_IF_GLEW_NOT_OK(glewInit(), false);

        LOG("OpenGL version: %s\n", glGetString(GL_VERSION));

        LOG("Allocating buffers\n");

        /*
         * Create vertex buffer.
         */
        {
            const std::array<Vertex2d, 6> positions = {{
                {-0.5f, -0.5f}, /* 0 */
                {0.5f, -0.5f},  /* 1 */
                {0.5f, 0.5f},   /* 2 */
                {-0.5f, 0.5f},  /* 3 */
            }};

            GLuint buffer;
            glGenBuffers(1, &buffer);
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glBufferData(
                GL_ARRAY_BUFFER, sizeof(positions), positions.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(Vertex2d), 0);
            glEnableVertexAttribArray(0);
        }

        /*
         * Create index buffer.
         */
        {
            const std::array<unsigned int, 6> indices = {0, 1, 2, 2, 3, 0};

            GLuint buffer;
            glGenBuffers(1, &buffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
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
            end();
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

        const GLint u_Color = glGetUniformLocation(program_id, "u_Color");
        ASSERT_RET_IF(u_Color == -1, false);

        float r = 0.f;
        float dr = 0.005f;

        /*
         * Loop until the user closes the window.
         */
        using namespace std::chrono_literals;
        static constexpr std::chrono::seconds stats_period = 1s;
        std::chrono::steady_clock::time_point last_stats_time =
            std::chrono::steady_clock::now();
        const std::chrono::steady_clock::time_point game_start_time =
            std::chrono::steady_clock::now();
        while (!glfwWindowShouldClose(window))
        {
            const std::chrono::steady_clock::time_point frame_start_time =
                std::chrono::steady_clock::now();

            glClear(GL_COLOR_BUFFER_BIT);

            glUniform4f(u_Color, r, 0.f, 1.f - r, 1.f);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

            if (r > 1.f)
            {
                dr = -0.005f;
            }
            else if (r < 0.f)
            {
                dr = 0.005f;
            }
            r += dr;

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

        return true;
    }

    /**
     * End the game.
     */
    void Game::end()
    {
        // glDeleteProgram(program_id)
        glfwTerminate();
    }
}