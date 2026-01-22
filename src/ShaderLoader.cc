#include "ShaderLoader.h"

#include "assert_util.h"

#include <fstream>
#include <sstream>

namespace Engine
{

#ifdef NDEBUG
    static void dump_shader_src(const std::string &name, const std::string &src)
    {
        (void)src;
    }
#else
    static void dump_shader_src(const std::string &name, const std::string &src)
    {
        std::ofstream out("build/" + name);
        out << src;
    }
#endif

    /**
     * @brief Recursive shader loader source which handles includes.
     *
     * @param file_path Path to the shader source file.
     * @param[out] shader_src Output shader source code.
     * @param is_include Whether this file is being included (not the main file).
     *
     * @return True on success, otherwise false.
     */
    bool ShaderLoader::get_shader_src_helper(const std::string &file_path,
                                             std::string &shader_src,
                                             const bool is_include,
                                             std::unordered_set<std::string> &included_files)
    {
        std::ifstream file(file_path);
        ASSERT_RET_IF_NOT(file, false);

        /*
         * If this is not an include, place the first line at the beginning of the
         * source code. This must be the version directive.
         */
        if (!is_include)
        {
            ASSERT_RET_IF_NOT(std::getline(file, shader_src), false);
            ASSERT_RET_IF_NOT(shader_src.substr(0, 8) == "#version", false);
            shader_src += "\n";
        }

        std::string line;
        while (std::getline(file, line))
        {
            /*
             * Process include directives.
             */
            if (line.substr(0, 8) == "#include")
            {
                std::istringstream iss(line);
                std::string include_directive, include_file;
                iss >> include_directive >> include_file;

                /*
                 * Remove quotes from include file name.
                 */
                include_file = include_file.substr(1, include_file.size() - 2);

                /*
                 * Do not include a file which was already included. Eventually we should
                 * key off a #pragma once before doing this but yeah.
                 */
                if (included_files.find(include_file) != included_files.end())
                {
                    LOG_DEBUG("Shader include file %s already included, skipping to avoid "
                              "circular dependency.\n",
                              include_file.c_str());
                    continue;
                }

                /*
                 * Update included files set.
                 */
                included_files.insert(include_file);

                std::string include_src;
                const std::filesystem::path _include_path = base_path / include_file;
                const std::string include_path =_include_path.string();

                LOG("Adding include: %s\n", include_path.c_str());

                /*
                 * If we have already loaded this include, use the cached version.
                 */
                if (shader_src_cache.find(include_path) != shader_src_cache.end())
                {
                    include_src = shader_src_cache.at(include_path);
                }
                /*
                 * Otherwise, load it from file and add to cache.
                 */
                else
                {
                    ASSERT_RET_IF_NOT(
                        get_shader_src_helper(include_path, include_src, true, included_files),
                        false);
                    shader_src_cache[include_path] = include_src;
                }

                shader_src += include_src + "\n";
            }

            /*
             * Otherwise, if this line isn't a version directive, add it to the shader
             * source.
             */
            else if (line.substr(0, 8) != "#version")
            {
                shader_src += line + "\n";
            }
        }

        return true;
    }

    /**
     * @brief Load shader source code from file.
     *
     * @param file_name Name of the shader source file.
     * @param[out] shader_src Output shader source code.
     *
     * @return True on success, otherwise false.
     */
    bool ShaderLoader::get_shader_src(const std::string &file_name, std::string &shader_src)
    {
        const std::filesystem::path _file_path = base_path / file_name;
        const std::string file_path = std::filesystem::path(base_path / file_name).string();

        LOG("Getting source code for shader %s\n", file_path.c_str());

        /*
         * If we have already read this shader file, use the cached version.
         */
        if (shader_src_cache.find(file_path) != shader_src_cache.end())
        {
            shader_src = shader_src_cache.at(file_path);
            return true;
        }

        /*
         * Otherwise, parse the source code and add it to the cache.
         */
        std::unordered_set<std::string> included_files;
        ASSERT_RET_IF_NOT(
            get_shader_src_helper(file_path, shader_src, false /* is_include */, included_files),
            false);
        shader_src_cache[file_path] = shader_src;

        return true;
    }

    /**
     * @brief Compile a shader of given type from source code.
     *
     * @param[out] shader_id Output OpenGL shader ID.
     * @param type Shader type (e.g., GL_VERTEX_SHADER, GL_FRAGMENT_SHADER).
     * @param src Shader source code.
     *
     * @return True on success, otherwise false.
     */
    bool ShaderLoader::compile_shader(GLuint &shader_id, const GLenum type, const std::string &src)
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
            ASSERT_RET_IF(length > static_cast<int>(sizeof(message)), false);

            glGetShaderInfoLog(shader_id, length, &length, message);
            LOG_ERROR("Failed to compile shader: %s\n", message);

            glDeleteShader(shader_id);

            return false;
        }

        return true;
    }

    /**
     * @brief Load and compile a shader program from given descriptors.
     *
     * @param[out] shader Output Shader object.
     * @param descriptors List of shader descriptors (file name and type).
     *
     * @return True on success, otherwise false.
     */
    bool
    ShaderLoader::load_shader(Shader &shader, const std::initializer_list<Descriptor> &descriptors)
    {
        shader.shader_id = glCreateProgram();
        ASSERT_RET_IF(shader.shader_id == 0, false);

        LOG_DEBUG("Creating shader id: %u\n", shader.shader_id);

        for (const Descriptor &descriptor : descriptors)
        {
            std::string src;
            ASSERT_RET_IF_NOT(get_shader_src(descriptor.file_name, src), false);

            dump_shader_src(descriptor.file_name, src);

            GLuint shader_type_id;
            ASSERT_RET_IF_NOT(compile_shader(shader_type_id, descriptor.type, src), false);

            glAttachShader(shader.shader_id, shader_type_id);
            glDeleteShader(shader_type_id);
        }

        glLinkProgram(shader.shader_id);
        GLint linked = 0;
        glGetProgramiv(shader.shader_id, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            char message[4096];
            glGetProgramInfoLog(shader.shader_id, sizeof(message), nullptr, message);
            LOG_ERROR("Program link error: %s\n", message);
            return false;
        }

        glValidateProgram(shader.shader_id);

        return true;
    }
}