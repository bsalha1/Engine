#pragma once

#include "TextureSlot.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>

namespace Engine
{
    class FramebufferTexture
    {
    public:
        FramebufferTexture(const TextureSlot _slot, const GLenum _attachment);

        bool create(const GLsizei width,
                    const GLsizei height,
                    const GLint internal_format,
                    const GLint format,
                    const GLenum min_filter,
                    const GLenum max_filter,
                    const GLint wrap_mode,
                    const glm::vec4 *border_color = nullptr);

        void use() const;

        GLuint get_attachment() const;

    private:
        /**
         * OpenGL texture ID.
         */
        GLuint texture_id;

        /**
         * Texture slot.
         */
        TextureSlot slot;

        /**
         * Color attachment.
         */
        GLenum attachment;
    };
}