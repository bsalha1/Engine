#include "Shader.h"

#include "assert_util.h"

#include <fstream>
#include <sstream>

namespace Engine
{
    namespace
    {
        /**
         * Cache of shader includes to avoid redundant file reads.
         */
        std::unordered_map<std::string, std::string> shader_include_cache;

        /**
         * Base path for shader files.
         */
        const std::string base_path = "shaders/";
    }

    /**
     * @brief Compile shader program and import into OpenGL.
     *
     * @param descriptors List of shader descriptors.
     *
     * @return True on success, otherwise false.
     */
    bool Shader::compile(const std::initializer_list<Descriptor> descriptors)
    {
        shader_id = glCreateProgram();
        ASSERT_RET_IF(shader_id == 0, false);

        for (const Descriptor &descriptor : descriptors)
        {
            std::string src;
            const std::string file_name = base_path + std::string(descriptor.file_name);
            LOG("Compiling shader %s\n", file_name.c_str());
            ASSERT_RET_IF_NOT(get_shader_src(file_name, src), false);

            LOG_DEBUG("Shader source:\n%s\n", src.c_str());

            GLuint shader_type_id;
            ASSERT_RET_IF_NOT(compile_shader(shader_type_id, descriptor.type, src),
                              false);

            glAttachShader(shader_id, shader_type_id);
            glDeleteShader(shader_type_id);
        }

        glLinkProgram(shader_id);

        GLint linked = 0;
        glGetProgramiv(shader_id, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            char message[4096];
            glGetProgramInfoLog(shader_id, sizeof(message), nullptr, message);
            LOG_ERROR("Program link error: %s\n", message);
            return false;
        }

        glValidateProgram(shader_id);

        return true;
    }

    /**
     * @brief Use the shader program.
     */
    void Shader::use() const
    {
        glUseProgram(shader_id);
    }

    /**
     * @brief Get the location of a uniform variable in the shader.
     *
     * @param uniform_name Name of the uniform variable.
     * @param[out] location Output location of the uniform variable.
     *
     * @return True on success, otherwise false.
     */
    bool Shader::get_uniform_location(const std::string &uniform_name, GLint &location)
    {
        if (uniform_location_cache.find(uniform_name) == uniform_location_cache.end())
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
     * @brief Set a mat4 variable in the shader.
     *
     * @param uniform_name Name of the uniform variable.
     * @param value Pointer to the matrix data.
     *
     * @return True on success, otherwise false.
     */
    bool Shader::set_mat4(const std::string &uniform_name, const glm::mat4 &value)
    {
        GLint location;
        ASSERT_RET_IF_NOT(get_uniform_location(uniform_name, location), false);
        glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
        return true;
    }

    /**
     * @brief Set a integer variable in the shader.
     *
     * @param uniform_name Name of the uniform variable.
     * @param value Integer value.
     *
     * @return True on success, otherwise false.
     */
    bool Shader::set_int(const std::string &uniform_name, const GLint value)
    {
        GLint location;
        ASSERT_RET_IF_NOT(get_uniform_location(uniform_name, location), false);
        glUniform1i(location, value);

        return true;
    }

    /**
     * @brief Set a vec3 variable in the shader.
     *
     * @param uniform_name Name of the uniform variable.
     * @param value Vector.
     *
     * @return True on success, otherwise false.
     */
    bool Shader::set_vec3(const std::string &uniform_name, const glm::vec3 &value)
    {
        GLint location;
        ASSERT_RET_IF_NOT(get_uniform_location(uniform_name, location), false);
        glUniform3f(location, value.x, value.y, value.z);

        return true;
    }

    /**
     * @brief Set a float variable in the shader.
     *
     * @param uniform_name Name of the uniform variable.
     * @param value Float value.
     *
     * @return True on success, otherwise false.
     */
    bool Shader::set_float(const std::string &uniform_name, const float value)
    {
        GLint location;
        ASSERT_RET_IF_NOT(get_uniform_location(uniform_name, location), false);
        glUniform1f(location, value);

        return true;
    }

    bool Shader::get_shader_src_helper(const std::string &file_path,
                                       std::string &shader_src,
                                       const bool is_include)
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

                std::string include_src;
                const std::string include_path = base_path + include_file;

                LOG("Adding include: %s\n", include_path.c_str());

                /*
                 * If we have already loaded this include, use the cached version.
                 */
                if (shader_include_cache.find(include_path) !=
                    shader_include_cache.end())
                {
                    include_src = shader_include_cache.at(include_path);
                }
                /*
                 * Otherwise, load it from file and add to cache.
                 */
                else
                {
                    ASSERT_RET_IF_NOT(
                        get_shader_src_helper(include_path, include_src, true), false);
                    shader_include_cache[include_path] = include_src;
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
     * @param file_path Path to the shader source file.
     * @param[out] shader_src Output shader source code.
     *
     * @return True on success, otherwise false.
     */
    bool Shader::get_shader_src(const std::string &file_path, std::string &shader_src)
    {
        return get_shader_src_helper(file_path, shader_src, false /* is_include */);
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
    bool
    Shader::compile_shader(GLuint &shader_id, const GLuint type, const std::string &src)
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
}