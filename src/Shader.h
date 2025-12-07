#pragma once

#include "assert_util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <string>

namespace Engine
{
    class Shader
    {
    public:
        /**
         * @brief Compile shader program and import into OpenGL.
         *
         * @param name Name of the shader.
         *
         * @return True on success, otherwise false.
         */
        bool compile(const std::string &name)
        {
            std::string vertex_shader_src;
            ASSERT_RET_IF_NOT(
                get_shader_src("shaders/" + name + ".vert", vertex_shader_src), false);
            std::string fragment_shader_src;
            ASSERT_RET_IF_NOT(get_shader_src("shaders/" + name + ".frag",
                                             fragment_shader_src),
                              false);

            ASSERT_RET_IF_NOT(create_shader(vertex_shader_src, fragment_shader_src),
                              false);

            return true;
        }

        /**
         * @brief Use the shader program.
         */
        void use() const
        {
            glUseProgram(shader_id);
        }

        /**
         * @return OpenGL shader ID.
         */
        GLuint id() const
        {
            return shader_id;
        }

    private:
        /**
         * @brief Load shader source code from file.
         *
         * @param file_path Path to the shader source file.
         * @param[out] shader_src Output shader source code.
         *
         * @return True on success, otherwise false.
         */
        static bool
        get_shader_src(const std::string &file_path, std::string &shader_src)
        {
            const std::ifstream file(file_path);
            ASSERT_RET_IF_NOT(file, false);

            std::stringstream chaser_buffer_obj;
            chaser_buffer_obj << file.rdbuf();
            shader_src = chaser_buffer_obj.str();

            return true;
        }

        /**
         * @brief Compile a shader of given type from source code.
         *
         * @param shader_id Output OpenGL shader ID.
         * @param type Shader type (e.g., GL_VERTEX_SHADER, GL_FRAGMENT_SHADER).
         * @param src Shader source code.
         *
         * @return True on success, otherwise false.
         */
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

        /**
         * @brief Create shader program from vertex and fragment shader source code.
         *
         * @param vertex_shader_src Vertex shader source code.
         * @param fragment_shader_src Fragment shader source code.
         *
         * @return True on success, otherwise false.
         */
        bool create_shader(const std::string &vertex_shader_src,
                           const std::string &fragment_shader_src)
        {
            shader_id = glCreateProgram();
            ASSERT_RET_IF(shader_id == 0, false);

            GLuint vertex_shader_id;
            ASSERT_RET_IF_NOT(
                compile_shader(vertex_shader_id, GL_VERTEX_SHADER, vertex_shader_src),
                false);

            GLuint fragment_shader_id;
            ASSERT_RET_IF_NOT(compile_shader(fragment_shader_id,
                                             GL_FRAGMENT_SHADER,
                                             fragment_shader_src),
                              false);

            glAttachShader(shader_id, vertex_shader_id);
            glAttachShader(shader_id, fragment_shader_id);
            glLinkProgram(shader_id);
            glValidateProgram(shader_id);

            glDeleteShader(vertex_shader_id);
            glDeleteShader(fragment_shader_id);

            return true;
        }

        /**
         * OpenGL shader ID.
         */
        GLuint shader_id;
    };
}