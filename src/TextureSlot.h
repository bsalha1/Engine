#pragma once

#include <cstdint>

namespace Engine
{
    /**
     * @brief Top-level texture slot organization.
     *
     * To prevent setting shader uniforms every time a texture is used, we fix the texture slots for
     * different types of textures and set the uniforms once on initialization. This enum defines
     * those slots.
     */
    enum TextureSlot : uint8_t
    {
        /**
         * Do not change these - as we use them as array indices in Models.
         * @{
         */
        DIFFUSE = 0,
        SPECULAR,
        NUM_MODEL_TEXTURE_TYPES,
        /**
         * @}
         */

        NORMAL = NUM_MODEL_TEXTURE_TYPES,
        SHADOW,
        BLOOM,
        NUM_TEXTURE_SLOTS,
    };
}