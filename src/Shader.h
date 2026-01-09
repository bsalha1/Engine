#pragma once

#include "assert_util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Engine
{
    class Shader
    {
    public:
        struct Descriptor
        {
            const char *file_name;
            const GLuint type;
        };

        bool compile(const std::initializer_list<Descriptor> descriptors);

        void use() const;

        bool set_mat4(const std::string &uniform_name, const glm::mat4 &value);

        bool set_int(const std::string &uniform_name, const GLint value);

        bool set_vec2(const std::string &uniform_name, const glm::vec2 &value);

        bool set_vec3(const std::string &uniform_name, const glm::vec3 &value);

        bool set_float(const std::string &uniform_name, const float value);

    private:
        /**
         * OpenGL shader ID.
         */
        GLuint shader_id;

        /**
         * Cache of uniform locations.
         */
        std::unordered_map<std::string, GLint> uniform_location_cache;

        bool get_uniform_location(const std::string &uniform_name, GLint &location);

        static bool get_shader_src_helper(const std::string &file_path,
                                          std::string &shader_src,
                                          const bool is_include,
                                          std::unordered_set<std::string> &included_files);

        static bool get_shader_src(const std::string &file_path, std::string &shader_src);

        /**
         * @brief Compile a shader of given type from source code.
         *
         * @param shader_id Output OpenGL shader ID.
         * @param type Shader type (e.g., GL_VERTEX_SHADER, GL_FRAGMENT_SHADER).
         * @param src Shader source code.
         *
         * @return True on success, otherwise false.
         */
        static bool compile_shader(GLuint &shader_id, const GLuint type, const std::string &src);
    };
}