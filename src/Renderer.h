#pragma once

#include "CubemapTexture.h"
#include "FramebufferTexture.h"
#include "TexturedMaterial.h"

#include <GL/glew.h>
#include <glm/mat4x4.hpp>
#include <memory>

namespace Engine
{
    class Renderer
    {
    public:
        Renderer();

        /**
         * @brief Interface for drawable objects.
         */
        class Drawable
        {
        public:
            virtual ~Drawable() = default;

            virtual void draw() const = 0;
        };

        /**
         * @brief Transform of an object.
         */
        struct Transform
        {
            glm::vec3 position;
            glm::vec3 rotation;
            glm::vec3 scale;

            glm::mat4 model() const;
        };

        /**
         * @brief Transform of an object with only translation.
         */
        struct TranslateTransform
        {
            glm::vec3 position;

            glm::mat4 model() const;
        };

        /**
         * @brief A regular object has a material, transform, and drawable component.
         */
        struct RegularObject
        {
            TexturedMaterial &material;
            Transform &transform;
            Drawable &drawable;
        };

        /**
         * @brief A point light object has a transform and drawable component. It does
         * not have a material.
         */
        struct PointLightObject
        {
            glm::vec3 &color;
            Transform &transform;
            Drawable &drawable;
        };

        /**
         * @brief A directional light object has a direction and color.
         */
        struct DirectionalLightObject
        {
            glm::vec3 &direction;
            glm::vec3 &color;
        };

        /**
         * @brief A debug object has a position and color.
         */
        struct DebugObject
        {
            const TranslateTransform &transform;
            const glm::vec3 &color;
            const Drawable &drawable;
        };

        bool init(const int _window_width, const int _window_height);

        void add_regular_object(const RegularObject &object);

        void add_point_light_object(const PointLightObject &object);

        void add_directional_light_object(const DirectionalLightObject &object);

        void add_debug_object(const DebugObject &object);

        bool render(const glm::mat4 &camera_view,
                    const glm::mat4 &skybox_view,
                    const glm::vec3 &camera_position,
                    const glm::vec3 &camera_direction);

        bool set_exposure(const float _exposure);

        bool set_gamma(const float _gamma);

        bool set_sharpness(const float _sharpness);

        float get_exposure() const;

        float get_gamma() const;

        float get_sharpness() const;

    private:
        int window_width;
        int window_height;
        glm::mat4 projection;

        /**
         * Screen quad.
         * @{
         */
        std::unique_ptr<Drawable> screen;
        float exposure;
        float gamma;
        float sharpness;
        Shader screen_shader;
        GLuint screen_frame_buffer;
        FramebufferTexture screen_color_texture;
        FramebufferTexture screen_bloom_texture;
        /**
         * @}
         */

        /**
         * Regular objects.
         * @{
         */
        Shader regular_object_shader;
        std::vector<RegularObject> regular_objects;
        /**
         * @}
         */

        /**
         * Point light objects.
         * @{
         */
        Shader point_light_shader;
        std::vector<PointLightObject> point_light_objects;
        /**
         * @}
         */

        /**
         * Directional light objects.
         */
        std::vector<DirectionalLightObject> directional_light_objects;

        /**
         * Gaussian blur.
         * @{
         */
        Shader gaussian_blur_shader;
        std::array<GLuint, 2> ping_pong_frame_buffer;
        std::array<FramebufferTexture, 2> ping_pong_texture;
        /**
         * @}
         */

        /**
         * Internal drawables.
         */
        std::unique_ptr<Drawable> cube;

        /**
         * Skybox.
         * @{
         */
        Shader skybox_shader;
        CubemapTexture skybox_texture;
        /**
         * @}
         */

        /**
         * Shadows.
         * @{
         */
        Shader depth_shader;
        FramebufferTexture shadow_map_texture;
        GLuint shadow_map_frame_buffer;
        /**
         * @}
         */

        /**
         * Debugging.
         * @{
         */
        Shader debug_shader;
        std::vector<DebugObject> debug_objects;
        /**
         * @}
         */
    };
}