#pragma once

#include "TextureSlot.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>

namespace Engine
{
    class CubemapTexture
    {
    public:
        CubemapTexture(const TextureSlot _slot);

        bool
        create_from_file(const std::string &file_name_prefix, const std::string &file_name_suffix);

        void use() const;

    private:
        /**
         * OpenGL texture ID.
         */
        GLuint texture_id;

        /**
         * Texture slot.
         */
        TextureSlot slot;
    };
}