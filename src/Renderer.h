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
         * @brief A spot light object has a position, direction, and color.
         */
        struct SpotLightObject
        {
            const glm::vec3 &position;
            const glm::vec3 &direction;
            const glm::vec3 &color;
            const float inner_cut_off;
            const float outer_cut_off;
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
         * @brief Point light uniform structure.
         */
        struct alignas(uniform_buffer_object_alignment) SpotLightUniform
        {
            glm::vec4 position;
            glm::vec4 direction;
            glm::vec4 ambient;
            glm::vec4 diffuse;
            glm::vec4 specular;
            glm::vec2 pad0;
            float inner_cut_off;
            float outer_cut_off;
        };
        static_assert(sizeof(SpotLightUniform) == 96);

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
         * Number of shadow maps.
         */
        static constexpr size_t num_shadow_maps = 3;

        /**
         * @brief Per-frame uniform buffer structure.
         */
        struct alignas(uniform_buffer_object_alignment) PerFrameUniformBuffer
        {
            /**
             * Camera view matrix.
             */
            glm::mat4 camera_view;

            /**
             * Camera projection matrix.
             */
            glm::mat4 camera_projection;

            /**
             * Camera position in world space. Note that the W coordinate is unused padding.
             */
            glm::vec4 camera_position;

            /**
             * Shadow depths. Note that the W coordinate is unused padding.
             */
            glm::vec4 shadow_map_ranges;

            /**
             * Number of points lights in point_lights.
             */
            uint32_t num_point_lights;

            /**
             * Number of spot lights in spot_lights.
             */
            uint32_t num_spot_lights;

            /**
             * Near plane distance for the camera.
             */
            float camera_near_clip;

            /**
             * Far plane distance for the camera.
             */
            float camera_far_clip;

            /**
             * Light view-projection matrices for each shadow map.
             */
            std::array<glm::mat4, num_shadow_maps> light_view_projections;

            /**
             * Maximum number of point lights supported.
             */
            static constexpr uint8_t max_point_lights = 64;

            /**
             * Point lights array.
             */
            std::array<PointLightUniform, max_point_lights> point_lights;

            /**
             * Maximum number of spot lights supported.
             */
            static constexpr uint8_t max_spot_lights = 64;

            /**
             * Spot lights array.
             */
            std::array<SpotLightUniform, max_spot_lights> spot_lights;

            /**
             * Directional light.
             */
            DirectionalLightUniform directional_light;
        };

        bool init(const int _window_width, const int _window_height);

        bool set_terrain(const Terrain &_terrain);

        void add_regular_object(const RegularObject &object);

        void add_point_light_object(const PointLightObject &object);

        void add_directional_light_object(const DirectionalLightObject &object);

        void add_spot_light_object(const SpotLightObject &object);

        void add_debug_object(const DebugObject &object);

        void add_model_object(const ModelObject &object);

        bool render_shadow_map(const glm::mat4 &camera_view,
                               const glm::mat4 &light_view,
                               const size_t shadow_map_idx,
                               glm::mat4 &light_view_projection);

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

        /**
         * Camera.
         * @{
         */
        float aspect_ratio;
        static constexpr float camera_far_clip = 5000.f;
        static constexpr float camera_near_clip = 0.001f;
        static constexpr float fov_rads = glm::radians(75.f);
        glm::mat4 projection;
        /**
         * @}
         */

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

        std::vector<SpotLightObject> spot_light_objects;

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
        std::array<FramebufferTexture, num_shadow_maps> shadow_map_textures = {
            FramebufferTexture(TextureSlot::SHADOW_NEAR, GL_DEPTH_ATTACHMENT),
            FramebufferTexture(TextureSlot::SHADOW_MID, GL_DEPTH_ATTACHMENT),
            FramebufferTexture(TextureSlot::SHADOW_FAR, GL_DEPTH_ATTACHMENT),
        };
        std::array<GLuint, num_shadow_maps> shadow_map_frame_buffers;
        static constexpr std::array<float, num_shadow_maps> shadow_map_ranges = {20.f,
                                                                                 100.f,
                                                                                 500.f};
        /**
         * @}
         */

        /**
         * Debugging.
         * @{
         */
        Shader debug_shader;
        std::vector<DebugObject> debug_objects;

        Shader regular_object_tbn_visualizer_shader;
        Shader terrain_tbn_visualizer_shader;

        /**
         * Flag to render tangent, bitangent, and normal vectors for debugging.
         */
        bool visualize_tbn = false;

        /**
         * Flag to only render normals when visualizing TBNs.
         */
        bool visualize_tbn_only_show_normals = false;

        /**
         * Magnitude of TBN vectors when visualizing.
         */
        float visualize_tbn_magnitude = 0.5f;

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

        friend class DebugMenu;
    };
}