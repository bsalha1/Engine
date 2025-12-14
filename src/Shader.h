#pragma once

#include "assert_util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <glm/vec3.hpp>
#include <sstream>
#include <string>
#include <unordered_map>

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

        /**
         * @brief Get the location of a uniform variable in the shader.
         *
         * @param uniform_name Name of the uniform variable.
         * @param[out] location Output location of the uniform variable.
         *
         * @return True on success, otherwise false.
         */
        bool get_uniform_location(const std::string &uniform_name, GLint &location)
        {
            if (uniform_location_cache.find(uniform_name) ==
                uniform_location_cache.end())
            {
                location = glGetUniformLocation(shader_id, uniform_name.c_str());
                ASSERT_RET_IF(location == -1, false);
                uniform_location_cache[uniform_name] = location;
            }
            else
            {
                location = uniform_location_cache.at(uniform_name);
            }

            return true;
        }

        /**
         * @brief Set a UniformMatrix4fv variable in the shader.
         *
         * @param uniform_name Name of the uniform variable.
         * @param value Pointer to the matrix data.
         *
         * @return True on success, otherwise false.
         */
        bool set_UniformMatrix4fv(const std::string &uniform_name, const GLfloat *value)
        {
            use();

            GLint location;
            ASSERT_RET_IF_NOT(get_uniform_location(uniform_name, location), false);
            glUniformMatrix4fv(location, 1, GL_FALSE, value);

            return true;
        }

        /**
         * @brief Set a Uniform1i variable in the shader.
         *
         * @param uniform_name Name of the uniform variable.
         * @param value Pointer to the integer data.
         *
         * @return True on success, otherwise false.
         */
        bool set_Uniform1i(const std::string &uniform_name, const GLint value)
        {
            use();

            GLint location;
            ASSERT_RET_IF_NOT(get_uniform_location(uniform_name, location), false);
            glUniform1i(location, value);

            return true;
        }

        /**
         * @brief Set a UniformMatrix4fv variable in the shader.
         *
         * @param uniform_name Name of the uniform variable.
         * @param value Pointer to the matrix data.
         *
         * @return True on success, otherwise false.
         */
        bool set_Uniform3f(const std::string &uniform_name, const glm::vec3 &value)
        {
            use();

            GLint location;
            ASSERT_RET_IF_NOT(get_uniform_location(uniform_name, location), false);
            glUniform3f(location, value.x, value.y, value.z);

            return true;
        }

    private:
        /**
         * OpenGL shader ID.
         */
        GLuint shader_id;

        /**
         * Cache of uniform locations.
         */
        std::unordered_map<std::string, GLint> uniform_location_cache;

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
    };
}