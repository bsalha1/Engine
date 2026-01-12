#pragma once

#include "Shader.h"
#include "assert_util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Engine
{
    class ShaderLoader
    {
    public:
        ShaderLoader() = default;

        struct Descriptor
        {
            const std::string file_name;
            const GLenum type;
        };

        bool load_shader(Shader &shader, const std::initializer_list<Descriptor> &descriptors);

    private:
        /**
         * Cache of shader source code already read.
         * - key: file path, relative to execution cwd.
         * - value: shader source code.
         */
        std::unordered_map<std::string, std::string> shader_src_cache;

        /**
         * Base path for shader files, relative to execution cwd.
         */
        const std::string base_path = "shaders/";

        bool get_shader_src_helper(const std::string &file_path,
                                   std::string &shader_src,
                                   const bool is_include,
                                   std::unordered_set<std::string> &included_files);

        bool get_shader_src(const std::string &file_name, std::string &shader_src);

        bool compile_shader(GLuint &shader_id, const GLenum type, const std::string &src);

        friend class Shader;
    };
}