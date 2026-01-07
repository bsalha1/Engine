#include "FramebufferTexture.h"

#include "assert_util.h"

#include <stb/stb_image.h>
#include <string>

namespace Engine
{
    /**
     * @brief Constructor.
     */
    FramebufferTexture::FramebufferTexture(const TextureSlot _slot, const GLenum _attachment):
        texture_id(0), slot(_slot), attachment(_attachment)
    {}

    /**
     * @brief Create a texture to be used as a framebuffer color attachment.
     *
     * @param width FramebufferTexture width in pixels.
     * @param height FramebufferTexture height in pixels.
     * @param internal_format Internal format of the texture.
     * @param format Format of the texture.
     * @param min_filter Minification filter.
     * @param max_filter Magnification filter.
     * @param wrap_mode Wrapping mode.
     * @param border_color Border color when using GL_CLAMP_TO_BORDER wrap mode.
     *
     * @return True on success, otherwise false.
     */
    bool FramebufferTexture::create(const GLsizei width,
                                    const GLsizei height,
                                    const GLint internal_format,
                                    const GLint format,
                                    const GLenum min_filter,
                                    const GLenum max_filter,
                                    const GLint wrap_mode,
                                    const glm::vec4 *border_color)
    {
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, max_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);

        if (wrap_mode == GL_CLAMP_TO_BORDER)
        {
            ASSERT_RET_IF(border_color == nullptr, false);
            std::array<float, 4> border_color_array = {
                border_color->r, border_color->g, border_color->b, border_color->a};
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color_array.data());
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture_id, 0);

        LOG("Created framebuffer texture id: %u, slot: %u, attachment: %u\n",
            texture_id,
            slot,
            attachment - GL_COLOR_ATTACHMENT0);

        return true;
    }

    /**
     * @brief Use the texture.
     */
    void FramebufferTexture::use() const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, texture_id);
    }

    /**
     * @return The texture attachment.
     */
    GLuint FramebufferTexture::get_attachment() const
    {
        return attachment;
    }
}