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

        /**
         * Uniform buffer objects must be aligned to 16 bytes.
         */
        static constexpr uint8_t uniform_buffer_object_alignment = 16;

        /**
         * @brief Point light uniform structure.
         */
        struct alignas(uniform_buffer_object_alignment) PointLightUniform
        {
            glm::vec4 position;
            glm::vec4 ambient;
            glm::vec4 diffuse;
            glm::vec4 specular;
        };
        static_assert(sizeof(PointLightUniform) == 64);

        /**
         * @brief Directional light uniform structure.
         */
        struct alignas(uniform_buffer_object_alignment) DirectionalLightUniform
        {
            glm::vec4 direction;
            glm::vec4 ambient;
            glm::vec4 diffuse;
            glm::vec4 specular;
        };
        static_assert(sizeof(DirectionalLightUniform) == 64);

        /**
         * @brief Per-frame uniform buffer structure.
         */
        struct alignas(uniform_buffer_object_alignment) PerFrameUniformBuffer
        {
            /**
             * Camera position in world space (stored here so point_lights is aligned on a 16-byte
             * boundary).
             */
            glm::vec3 camera_position;

            /**
             * Number of points lights in point_per_frame_ubo.
             */
            uint32_t num_point_lights;

            /**
             * Maximum number of point lights supported.
             */
            static constexpr uint8_t max_point_lights = 64;

            /**
             * Point lights array.
             */
            std::array<PointLightUniform, max_point_lights> point_lights;

            /**
             * Directional light.
             */
            DirectionalLightUniform directional_light;

            /**
             * Camera view matrix.
             */
            glm::mat4 camera_view;

            /**
             * Light view-projection matrix.
             */
            glm::mat4 light_view_projection;
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
         * Lighting.
         * @{
         */
        Shader point_light_shader;

        std::vector<PointLightObject> point_light_objects;

        std::vector<DirectionalLightObject> directional_light_objects;

        GLuint per_frame_uniform_buffer_id;
        /**
         * @}
         */

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