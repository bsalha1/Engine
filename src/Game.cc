#include "Game.h"

#include "assert_util.h"
#include "log.h"

#include <array>
#include <fstream>
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
    Game::Game(): window(nullptr)
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

        LOG("Initializing GLEW\n");
        ASSERT_RET_IF_GLEW_NOT_OK(glewInit(), false);

        LOG("OpenGL version: %s\n", glGetString(GL_VERSION));

        LOG("Allocating buffers\n");

        const std::array<Vertex2d, 3> positions = {{
            {-0.5f, -0.5f},
            {0.0f, 0.5f},
            {0.5f, -0.5f},
        }};
        GLuint buffer;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(
            GL_ARRAY_BUFFER, sizeof(positions), positions.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(Vertex2d), 0);
        glEnableVertexAttribArray(0);

        LOG("Compiling shaders\n");

        std::string vertex_shader_src;
        ASSERT_RET_IF_NOT(get_shader_src("shaders/basic.vert", vertex_shader_src),
                          false);
        std::string fragment_shader_src;
        ASSERT_RET_IF_NOT(get_shader_src("shaders/basic.frag", fragment_shader_src),
                          false);

        GLuint program_id;
        ASSERT_RET_IF_NOT(
            create_shader(program_id, vertex_shader_src, fragment_shader_src), false);
        glUseProgram(program_id);

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
    void Game::run()
    {
        LOG("Entering main loop\n");

        /*
         * Loop until the user closes the window.
         */
        while (!glfwWindowShouldClose(window))
        {
            glClear(GL_COLOR_BUFFER_BIT);

            glDrawArrays(GL_TRIANGLES, 0, 3);

            /*
             * Swap front and back buffers.
             */
            glfwSwapBuffers(window);

            /*
             * Poll for and process events.
             */
            glfwPollEvents();
        }
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