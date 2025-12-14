
#include "assert_util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <string>

namespace Engine
{
    class Texture
    {
    public:
        /**
         * @brief Load texture from file into the given slot.
         *
         * @param file_name Path to the texture file.
         * @param _slot Texture slot to load the texture into.
         *
         * @return True on success, otherwise false.
         */
        bool load_from_file(const std::string file_name, const uint8_t _slot)
        {
            slot = _slot;

            glGenTextures(1, &texture_id);
            glBindTexture(GL_TEXTURE_2D, texture_id);

            glTexParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            /*
             * Max out anisotropic filtering. We will probably want to turn this down
             * one day when clawing back performance.
             */
            float max_anistropy = 0.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anistropy);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anistropy);

            stbi_set_flip_vertically_on_load(1);
            int channels;
            uint8_t *texture_buffer =
                stbi_load(file_name.c_str(), &width, &height, &channels, 0);
            ASSERT_RET_IF_NOT(texture_buffer, false);
            const GLenum internal_format = (channels == 4) ? GL_RGBA8 : GL_RGB8;
            const GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         internal_format,
                         width,
                         height,
                         0,
                         format,
                         GL_UNSIGNED_BYTE,
                         texture_buffer);

            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(texture_buffer);

            glActiveTexture(GL_TEXTURE0 + _slot);

            return true;
        }

        /**
         * @brief Bind the texture.
         */
        void bind() const
        {
            glBindTexture(GL_TEXTURE_2D, texture_id);
        }

        /**
         * @return The texture slot.
         */
        uint8_t get_slot() const
        {
            return slot;
        }

        /**
         * @return Texture width in pixels.
         */
        int get_width() const
        {
            return width;
        }

        /**
         * @return Texture height in pixels.
         */
        int get_height() const
        {
            return height;
        }

    private:
        GLuint texture_id;
        uint8_t slot;
        int width;
        int height;
    };
}