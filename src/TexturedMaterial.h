#pragma once

#include "Shader.h"
#include "Texture.h"
#include "assert_util.h"

#include <glm/vec3.hpp>

namespace Engine
{
    class TexturedMaterial: public Texture
    {
    public:
        TexturedMaterial(const glm::vec3 &_ambient,
                         const glm::vec3 &_diffuse,
                         const glm::vec3 &_specular,
                         const float _shininess):
            ambient(_ambient), diffuse(_diffuse), specular(_specular), shininess(_shininess)
        {}

        /**
         * @brief Apply the material properties and texture to the given shader.
         *
         * @param shader Shader to apply the material to.
         *
         * @return True on success, otherwise false.
         */
        bool apply(Shader &shader) const
        {
            Texture::use();

            ASSERT_RET_IF_NOT(shader.set_vec3("u_material.ambient", ambient), false);
            ASSERT_RET_IF_NOT(shader.set_vec3("u_material.diffuse", diffuse), false);
            ASSERT_RET_IF_NOT(shader.set_vec3("u_material.specular", specular), false);
            ASSERT_RET_IF_NOT(shader.set_float("u_material.shininess", shininess), false);

            return true;
        }

        /**
         * @brief Do not allow callers to use the texture without applying the material.
         */
        void use() const = delete;

    private:
        /**
         * Ambient color of the material.
         */
        const glm::vec3 ambient;

        /**
         * Diffuse color of the material.
         */
        const glm::vec3 diffuse;

        /**
         * Specular color of the material.
         */
        const glm::vec3 specular;

        /**
         * Shininess factor of the material.
         */
        const float shininess;
    };
}