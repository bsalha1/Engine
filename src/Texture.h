
#include "TextureSlot.h"
#include "assert_util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <string>

namespace Engine
{
    extern uint8_t next_texture_slot;

    class Texture
    {
    public:
        Texture(): slot(TextureSlot::next_texture_slot())
        {}

        /**
         * @brief Create a texture to be used as a framebuffer color attachment.
         *
         * @param width Texture width in pixels.
         * @param height Texture height in pixels.
         */
        void create_framebuffer_texture(const GLsizei width, const GLsizei height)
        {
            glGenTextures(1, &texture_id);
            glBindTexture(GL_TEXTURE_2D, texture_id);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA16F,
                         width,
                         height,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, slot);

            glFramebufferTexture2D(
                GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
        }

        /**
         * @brief Load texture from file into the given slot.
         *
         * @param file_name Path to the texture file.
         *
         * @return True on success, otherwise false.
         */
        bool create_from_file(const std::string &file_name)
        {
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
            stbi_image_free(texture_buffer);

            glGenerateMipmap(GL_TEXTURE_2D);

            glActiveTexture(GL_TEXTURE0 + slot);

            return true;
        }

        /**
         * @brief Use the texture.
         */
        void use() const
        {
            glActiveTexture(GL_TEXTURE0 + slot);
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
        const uint8_t slot;
        int width;
        int height;
    };
}