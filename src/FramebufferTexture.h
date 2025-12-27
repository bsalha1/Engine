#pragma once

#include "assert_util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <string>

namespace Engine
{
    class FramebufferTexture
    {
    public:
        FramebufferTexture()
        {}

        /**
         * @brief Create a texture to be used as a framebuffer color attachment.
         *
         * @param width FramebufferTexture width in pixels.
         * @param height FramebufferTexture height in pixels.
         */
        void create(const GLsizei _width,
                    const GLsizei _height,
                    const GLenum _attachment,
                    const GLenum _slot,
                    const GLint wrap_mode)
        {
            width = _width;
            height = _height;
            attachment = _attachment;
            slot = _slot;

            glGenTextures(1, &texture_id);
            glBindTexture(GL_TEXTURE_2D, texture_id);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA16F,
                         width,
                         height,
                         0,
                         GL_RGBA,
                         GL_FLOAT,
                         nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);

            glFramebufferTexture2D(
                GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture_id, 0);

            LOG("Created framebuffer texture id: 0x%x, slot: %u, attachment: %u\n",
                texture_id,
                slot,
                attachment - GL_COLOR_ATTACHMENT0);
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
         * @return The texture attachment.
         */
        GLuint get_attachment() const
        {
            return attachment;
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
        GLenum attachment;
        uint8_t slot;
        int width;
        int height;
    };
}