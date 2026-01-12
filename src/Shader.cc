#include "Shader.h"

#include "assert_util.h"

namespace Engine
{
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
     * @brief Set an unsigned integer variable in the shader.
     *
     * @param uniform_name Name of the uniform variable.
     * @param value Integer value.
     *
     * @return True on success, otherwise false.
     */
    bool Shader::set_uint(const std::string &uniform_name, const GLuint value)
    {
        GLint location;
        ASSERT_RET_IF_NOT(get_uniform_location(uniform_name, location), false);
        glUniform1ui(location, value);

        return true;
    }

    /**
     * @brief Set a bool variable in the shader.
     *
     * @param uniform_name Name of the uniform variable.
     * @param value Integer value.
     *
     * @return True on success, otherwise false.
     */
    bool Shader::set_bool(const std::string &uniform_name, const bool value)
    {
        return set_uint(uniform_name, value);
    }

    /**
     * @brief Set a vec2 variable in the shader.
     *
     * @param uniform_name Name of the uniform variable.
     * @param value Vector.
     *
     * @return True on success, otherwise false.
     */
    bool Shader::set_vec2(const std::string &uniform_name, const glm::vec2 &value)
    {
        GLint location;
        ASSERT_RET_IF_NOT(get_uniform_location(uniform_name, location), false);
        glUniform2f(location, value.x, value.y);

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
}