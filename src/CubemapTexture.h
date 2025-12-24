
#include "assert_util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <array>
#include <stb/stb_image.h>
#include <string>

namespace Engine
{
    class CubemapTexture
    {
    public:
        /**
         * @brief Load texture from file into the given slot.
         *
         * @param file_name_prefix File name prefix.
         * @param file_name_suffix File name suffix.
         *
         * The six faces should be named as:
         * file_name_prefix + "px" + file_name_suffix
         * file_name_prefix + "nx" + file_name_suffix
         * file_name_prefix + "py" + file_name_suffix
         * file_name_prefix + "ny" + file_name_suffix
         * file_name_prefix + "pz" + file_name_suffix
         * file_name_prefix + "nz" + file_name_suffix
         *
         * Where p = positive, n = negative, x/y/z = face axial direction.
         *
         * @return True on success, otherwise false.
         */
        bool load_from_file(const std::string &file_name_prefix,
                            const std::string &file_name_suffix)
        {
            glGenTextures(1, &texture_id);
            glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id);

            const std::array<std::string, 6> faces = {
                file_name_prefix + "px" + file_name_suffix,
                file_name_prefix + "nx" + file_name_suffix,
                file_name_prefix + "py" + file_name_suffix,
                file_name_prefix + "ny" + file_name_suffix,
                file_name_prefix + "pz" + file_name_suffix,
                file_name_prefix + "nz" + file_name_suffix,
            };

            int width, height, channels;
            for (uint8_t i = 0; i < faces.size(); i++)
            {
                uint8_t *face =
                    stbi_load(faces[i].c_str(), &width, &height, &channels, 0);
                ASSERT_RET_IF_NOT(face, false);
                LOG("Loading cubemap face %s (%d x %d x %d)\n",
                    faces[i].c_str(),
                    width,
                    height,
                    channels);
                const GLenum internal_format = (channels == 4) ? GL_RGBA8 : GL_RGB8;
                const GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                             0,
                             internal_format,
                             width,
                             height,
                             0,
                             format,
                             GL_UNSIGNED_BYTE,
                             face);
                stbi_image_free(face);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            return true;
        }

        /**
         * @brief Use the texture.
         */
        void use() const
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id);
        }

    private:
        GLuint texture_id;
    };
}