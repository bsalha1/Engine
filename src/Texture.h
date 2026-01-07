#pragma once

#include "TextureSlot.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>

namespace Engine
{
    class Texture
    {
    public:
        Texture(const TextureSlot _slot);

        bool create_from_file(const std::string &file_name, const bool invert_green = false);

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