#include "Texture.h"

#include "assert_util.h"

#include <stb/stb_image.h>

namespace Engine
{
    /**
     * @brief Constructor.
     */
    Texture::Texture(const TextureSlot _slot): texture_id(0), slot(_slot)
    {}

    /**
     * @brief Load texture from file into the given slot.
     *
     * @param file_name Path to the texture file.
     * @param invert_green Whether to invert the green channel (useful for normal maps).
     *
     * @return True on success, otherwise false.
     */
    bool Texture::create_from_file(const std::string &file_name, const bool invert_green)
    {
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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
        int width, height, channels;
        uint8_t *texture_buffer = stbi_load(file_name.c_str(), &width, &height, &channels, 0);
        ASSERT_RET_IF_NOT(texture_buffer, false);
        const GLenum internal_format = (channels == 4) ? GL_RGBA8 : GL_RGB8;
        const GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
        if (invert_green)
        {
            for (int i = 0; i < width * height; i++)
            {
                texture_buffer[i * channels + 1] = 255 - texture_buffer[i * channels + 1];
            }
        }
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

        LOG("Created texture %s id: %u, slot: %u\n", file_name.c_str(), texture_id, slot);

        return true;
    }

    /**
     * @brief Use the texture.
     */
    void Texture::use() const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, texture_id);
    }
}