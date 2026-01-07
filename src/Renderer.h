#pragma once

#include "CubemapTexture.h"
#include "FramebufferTexture.h"
#include "Model.h"
#include "TexturedMaterial.h"

#include <GL/glew.h>
#include <glm/ext/quaternion_float.hpp>
#include <glm/mat4x4.hpp>
#include <memory>

namespace Engine
{
    class Renderer
    {
    public:
        Renderer();

        /**
         * @brief Transform of an object.
         */
        struct Transform
        {
            glm::vec3 position;
            glm::quat rotation;
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
         * @brief Terrain object has a material, normal map, and drawable component.
         */
        struct Terrain
        {
            const TexturedMaterial &material;
            const Texture &normal_map;
            const Drawable &drawable;
        };

        /**
         * @brief A regular object has a material, transform, and drawable component.
         */
        struct RegularObject
        {
            const TexturedMaterial &material;
            const Texture &normal_map;
            const Transform &transform;
            const Drawable &drawable;
        };

        /**
         * @brief A point light object has a transform and drawable component. It does
         * not have a material.
         */
        struct PointLightObject
        {
            const glm::vec3 &color;
            const Transform &transform;
            const Drawable &drawable;
        };

        /**
         * @brief A directional light object has a direction and color.
         */
        struct DirectionalLightObject
        {
            const glm::vec3 &direction;
            const glm::vec3 &color;
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

        /**
         * @brief A model object has a model and transform.
         */
        struct ModelObject
        {
            const Model &model;
            const Transform &transform;
        };

        bool init(const int _window_width, const int _window_height);

        bool set_terrain(const Terrain &_terrain);

        void add_regular_object(const RegularObject &object);

        void add_point_light_object(const PointLightObject &object);

        void add_directional_light_object(const DirectionalLightObject &object);

        void add_debug_object(const DebugObject &object);

        void add_model_object(const ModelObject &object);

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
        FramebufferTexture screen_color_texture =
            FramebufferTexture(TextureSlot::DIFFUSE, GL_COLOR_ATTACHMENT0);
        FramebufferTexture screen_bloom_texture =
            FramebufferTexture(TextureSlot::BLOOM, GL_COLOR_ATTACHMENT1);
        /**
         * @}
         */

        /**
         * Terrain.
         * @{
         */
        Shader terrain_shader;
        std::unique_ptr<Terrain> terrain;
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
        std::array<FramebufferTexture, 2> ping_pong_texture = {
            FramebufferTexture(TextureSlot::BLOOM, GL_COLOR_ATTACHMENT0),
            FramebufferTexture(TextureSlot::BLOOM, GL_COLOR_ATTACHMENT0),
        };
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
        CubemapTexture skybox_texture = CubemapTexture(TextureSlot::DIFFUSE);
        /**
         * @}
         */

        /**
         * Shadows.
         * @{
         */
        Shader depth_shader;
        FramebufferTexture shadow_map_texture =
            FramebufferTexture(TextureSlot::SHADOW, GL_DEPTH_ATTACHMENT);
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

        /**
         * UI.
         * @{
         */
        Shader crosshair_shader;
        /**
         * @}
         */

        /**
         * Model objects.
         * @{
         */
        Shader model_shader;
        std::vector<ModelObject> model_objects;
        /**
         * @}
         */
    };
}