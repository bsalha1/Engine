#pragma once

#include "assert_util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <unordered_map>

namespace Engine
{
    class Shader
    {
    public:
        void use() const;

        bool set_mat4(const std::string &uniform_name, const glm::mat4 &value);

        bool set_int(const std::string &uniform_name, const GLint value);

        bool set_uint(const std::string &uniform_name, const GLuint value);

        bool set_bool(const std::string &uniform_name, const bool value);

        bool set_vec2(const std::string &uniform_name, const glm::vec2 &value);

        bool set_vec3(const std::string &uniform_name, const glm::vec3 &value);

        bool set_float(const std::string &uniform_name, const float value);

    private:
        /**
         * OpenGL shader ID, set by ShaderLoader::load_shader().
         */
        GLuint shader_id;

        /**
         * Cache of uniform locations.
         */
        std::unordered_map<std::string, GLint> uniform_location_cache;

        bool get_uniform_location(const std::string &uniform_name, GLint &location);

        friend class ShaderLoader;
    };
}