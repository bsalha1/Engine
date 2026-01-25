#include "Renderer.h"

#include "FramebufferTexture.h"
#include "ShaderLoader.h"
#include "TextureSlot.h"
#include "TexturedMaterial.h"
#include "Vertex.h"
#include "VertexArray.h"
#include "math_util.h"

#include <GL/glew.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine
{
    static constexpr GLsizei shadow_map_resolution = 2048;
    static constexpr float one_over_shadow_map_resolution =
        1.0f / static_cast<float>(shadow_map_resolution);

    /**
     * Relative to the terrain, the skybox spins around it. We draw a sun
     * on the skybox in its model space so that it rotates with it with an
     * elevation angle above the orbital plane.
     */
    static constexpr float sun_angular_radius = glm::radians<float>(5.f);
    const float sun_radius_skybox_model_space = glm::sin(sun_angular_radius);
    static constexpr float sun_orbital_elevation_angle = glm::radians<float>(10.f);
    const glm::vec4 sun_position_skybox_model_space =
        glm::vec4(0.f,
                  glm::sin(sun_orbital_elevation_angle),
                  glm::cos(sun_orbital_elevation_angle),
                  0.f);

    /**
     * @return Model matrix for the transform.
     */
    glm::mat4 Renderer::Transform::model() const
    {
        const glm::mat4 model_matrix =
            glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
        return glm::scale(model_matrix, scale);
    }

    /**
     * @return Model matrix for the transform.
     */
    glm::mat4 Renderer::TranslateTransform::model() const
    {
        return glm::translate(glm::mat4(1.0f), position);
    }

    /**
     * @brief Constructor.
     */
    Renderer::Renderer(): exposure(1.4f), gamma(0.5f), sharpness(1.0f)
    {}

    /**
     * @brief Initialize the renderer.
     *
     * @param _window_width Window width.
     * @param _window_height Window height.
     *
     * @return True if successful, otherwise false.
     */
    bool Renderer::init(const int _window_width, const int _window_height)
    {
        window_width = _window_width;
        window_height = _window_height;

        LOG("OpenGL version: %s\n", glGetString(GL_VERSION));
        LOG("Vendor: %s\n", glGetString(GL_VENDOR));
        LOG("Renderer: %s\n", glGetString(GL_RENDERER));

        /*
         * Enable blending for transparent/translucent textures.
         */
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);

        /*
         * Enable anti-aliasing.
         */
        glEnable(GL_MULTISAMPLE);

        /*
         * Draw fragments closer to camera over the fragments behind.
         */
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        aspect_ratio = static_cast<float>(window_width) / window_height;
        projection = glm::perspective(fov_rads, aspect_ratio, camera_near_clip, camera_far_clip);

        LOG("Creating screen quad...\n");
        {
            /* clang-format off */
            const std::array<TexturedVertex2d, 6> vertices = {{
            /*                  position,              texture */
                {glm::vec2(-1.0f,  1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec2(-1.0f, -1.0f), glm::vec2(0.0f, 0.0f)},
                {glm::vec2( 1.0f, -1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec2(-1.0f,  1.0f), glm::vec2(0.0f, 1.0f)},
                {glm::vec2( 1.0f, -1.0f), glm::vec2(1.0f, 0.0f)},
                {glm::vec2( 1.0f,  1.0f), glm::vec2(1.0f, 1.0f)},
            }};
            /* clang-format on */

            std::unique_ptr<VertexArray> screen_vertex_array = std::make_unique<VertexArray>();

            screen_vertex_array->create(vertices.data(), vertices.size());
            TexturedVertex2d::setup_vertex_array_attribs(*screen_vertex_array);
            screen = std::move(screen_vertex_array);

            glGenFramebuffers(1, &screen_frame_buffer);
            glBindFramebuffer(GL_FRAMEBUFFER, screen_frame_buffer);

            /*
             * Create textures to hold the color and brightness buffers.
             */
            screen_color_texture.create(window_width,
                                        window_height,
                                        GL_RGBA16F /* internal_format */,
                                        GL_RGBA /* format */,
                                        GL_LINEAR /* min_filter */,
                                        GL_LINEAR /* max_filter */,
                                        GL_REPEAT /* wrap_mode */);
            screen_bloom_texture.create(window_width,
                                        window_height,
                                        GL_RGBA16F /* internal_format */,
                                        GL_RGBA /* format */,
                                        GL_LINEAR /* min_filter */,
                                        GL_LINEAR /* max_filter */,
                                        GL_CLAMP_TO_EDGE /* wrap_mode */);

            /*
             * Create a render buffer to hold the depth and stencil buffer.
             */
            GLuint depth_stencil_render_buffer;
            glGenRenderbuffers(1, &depth_stencil_render_buffer);
            glBindRenderbuffer(GL_RENDERBUFFER, depth_stencil_render_buffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, window_width, window_height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                      GL_DEPTH_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER,
                                      depth_stencil_render_buffer);

            ASSERT_RET_IF_NOT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                              false);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            /*
             * Create ping-pong frame buffers for blurring the bloom texture.
             */
            for (uint8_t i = 0; i < ping_pong_frame_buffer.size(); i++)
            {
                glGenFramebuffers(1, &ping_pong_frame_buffer[i]);
                glBindFramebuffer(GL_FRAMEBUFFER, ping_pong_frame_buffer[i]);

                ping_pong_texture[i].create(window_width,
                                            window_height,
                                            GL_RGBA16F /* internal_format */,
                                            GL_RGBA /* format */,
                                            GL_LINEAR /* min_filter */,
                                            GL_LINEAR /* max_filter */,
                                            GL_CLAMP_TO_EDGE /* wrap_mode */);

                ASSERT_RET_IF_NOT(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                                      GL_FRAMEBUFFER_COMPLETE,
                                  false);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        LOG("Loading skybox\n");
        {
            ASSERT_RET_IF_NOT(skybox_texture.create_from_file("textures/skybox/", ".jpg"), false);

            static const std::array<Vertex3d, 36> cube_vertices = {
                /* clang-format off */
                glm::vec3(-1, -1, -1),
                glm::vec3( 1, -1, -1),
                glm::vec3( 1,  1, -1),
                glm::vec3( 1,  1, -1),
                glm::vec3(-1,  1, -1),
                glm::vec3(-1, -1, -1),

                glm::vec3(-1, -1,  1),
                glm::vec3( 1, -1,  1),
                glm::vec3( 1,  1,  1),
                glm::vec3( 1,  1,  1),
                glm::vec3(-1,  1,  1),
                glm::vec3(-1, -1,  1),

                glm::vec3(-1,  1,  1),
                glm::vec3(-1,  1, -1),
                glm::vec3(-1, -1, -1),
                glm::vec3(-1, -1, -1),
                glm::vec3(-1, -1,  1),
                glm::vec3(-1,  1,  1),

                glm::vec3( 1,  1,  1),
                glm::vec3( 1,  1, -1),
                glm::vec3( 1, -1, -1),
                glm::vec3( 1, -1, -1),
                glm::vec3( 1, -1,  1),
                glm::vec3( 1,  1,  1),

                glm::vec3(-1, -1, -1),
                glm::vec3( 1, -1, -1),
                glm::vec3( 1, -1,  1),
                glm::vec3( 1, -1,  1),
                glm::vec3(-1, -1,  1),
                glm::vec3(-1, -1, -1),

                glm::vec3(-1,  1, -1),
                glm::vec3( 1,  1, -1),
                glm::vec3( 1,  1,  1),
                glm::vec3( 1,  1,  1),
                glm::vec3(-1,  1,  1),
                glm::vec3(-1,  1, -1),
                /* clang-format on */
            };

            std::unique_ptr<VertexArray> cube_vertex_array = std::make_unique<VertexArray>();
            cube_vertex_array->create(cube_vertices.data(), cube_vertices.size());
            Vertex3d::setup_vertex_array_attribs(*cube_vertex_array);
            cube = std::move(cube_vertex_array);
        }

        LOG("Creating shadow map frame buffers\n");
        {
            glGenFramebuffers(shadow_map_frame_buffers.size(), shadow_map_frame_buffers.data());
            for (size_t shadow_map_idx = 0; shadow_map_idx < shadow_map_frame_buffers.size();
                 shadow_map_idx++)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, shadow_map_frame_buffers[shadow_map_idx]);
                const glm::vec4 border_color(1.0f, 1.0f, 1.0f, 1.0f);
                ASSERT_RET_IF_NOT(shadow_map_textures[shadow_map_idx].create(
                                      shadow_map_resolution,
                                      shadow_map_resolution,
                                      GL_DEPTH_COMPONENT /* internal_format */,
                                      GL_DEPTH_COMPONENT /* format */,
                                      GL_NEAREST /* min_filter */,
                                      GL_NEAREST /* max_filter */,
                                      GL_CLAMP_TO_BORDER /* wrap_mode */,
                                      &border_color /* border_color */),
                                  false);
                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);

                ASSERT_RET_IF_NOT(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                                      GL_FRAMEBUFFER_COMPLETE,
                                  false);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }

        LOG("Creating uniform buffers\n");
        {
            glCreateBuffers(1, &per_frame_uniform_buffer_id);
            glNamedBufferData(per_frame_uniform_buffer_id,
                              sizeof(PerFrameUniformBuffer),
                              nullptr,
                              GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER, 0 /* binding */, per_frame_uniform_buffer_id);
        }

        LOG("Initializing shaders\n");

        /*
         * Create a temporary shader loader to load all shaders.
         */
        ShaderLoader shader_loader;

        /*
         * Initialize screen shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(screen_shader,
                                                    {
                                                        {"screen.vert", GL_VERTEX_SHADER},
                                                        {"screen.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);

        screen_shader.use();
        ASSERT_RET_IF_NOT(screen_shader.set_float("u_exposure", exposure), false);
        ASSERT_RET_IF_NOT(screen_shader.set_float("u_gamma", gamma), false);
        ASSERT_RET_IF_NOT(screen_shader.set_float("u_sharpness", sharpness), false);

        /*
         * Initialize gaussian blur shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(gaussian_blur_shader,
                                                    {
                                                        {"gaussian_blur.vert", GL_VERTEX_SHADER},
                                                        {"gaussian_blur.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);
        gaussian_blur_shader.use();

        /*
         * Initialize skybox shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(skybox_shader,
                                                    {
                                                        {"skybox.vert", GL_VERTEX_SHADER},
                                                        {"skybox.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);
        skybox_shader.use();
        ASSERT_RET_IF_NOT(skybox_shader.set_float("u_sun_angular_radius", sun_angular_radius),
                          false);
        ASSERT_RET_IF_NOT(skybox_shader.set_vec3("u_sun_position", sun_position_skybox_model_space),
                          false);

        /*
         * Initialize regular object shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(regular_object_shader,
                                                    {
                                                        {"regular_object.vert", GL_VERTEX_SHADER},
                                                        {"regular_object.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);
        regular_object_shader.use();

        /*
         * Initialize regular object TBN visualizer shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(regular_object_tbn_visualizer_shader,
                                                    {
                                                        {"regular_object.vert", GL_VERTEX_SHADER},
                                                        {"tbn_visualizer.geom", GL_GEOMETRY_SHADER},
                                                        {"tbn_visualizer.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);
        regular_object_tbn_visualizer_shader.use();

        /*
         * Initialize point light shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(point_light_shader,
                                                    {
                                                        {"point_light.vert", GL_VERTEX_SHADER},
                                                        {"point_light.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);

        /*
         * Initialize depth shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(depth_shader,
                                                    {
                                                        {"depth.vert", GL_VERTEX_SHADER},
                                                        {"depth.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);

        /*
         * Initialize debug shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(debug_shader,
                                                    {
                                                        {"debug.vert", GL_VERTEX_SHADER},
                                                        {"debug.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);

        /*
         * Initialize terrain TBN visualizer shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(terrain_tbn_visualizer_shader,
                                                    {
                                                        {"terrain.vert", GL_VERTEX_SHADER},
                                                        {"tbn_visualizer.geom", GL_GEOMETRY_SHADER},
                                                        {"tbn_visualizer.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);
        terrain_tbn_visualizer_shader.use();
        ASSERT_RET_IF_NOT(terrain_tbn_visualizer_shader.set_mat4("u_model", glm::mat4(1.f)), false);

        /*
         * Initialize terrain shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(terrain_shader,
                                                    {
                                                        {"terrain.vert", GL_VERTEX_SHADER},
                                                        {"terrain.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);
        terrain_shader.use();
        ASSERT_RET_IF_NOT(terrain_shader.set_mat4("u_model", glm::mat4(1.f)), false);

        /*
         * Initialize crosshair shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(crosshair_shader,
                                                    {
                                                        {"crosshair.vert", GL_VERTEX_SHADER},
                                                        {"crosshair.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);
        crosshair_shader.use();
        ASSERT_RET_IF_NOT(crosshair_shader.set_vec2("u_window_size",
                                                    glm::vec2(window_width, window_height)),
                          false);
        ASSERT_RET_IF_NOT(crosshair_shader.set_float("u_size", 10.f), false);
        ASSERT_RET_IF_NOT(crosshair_shader.set_float("u_thickness", 1.f), false);
        ASSERT_RET_IF_NOT(crosshair_shader.set_vec3("u_color", glm::vec3(1.0f)), false);

        /*
         * Initialize model shader.
         */
        ASSERT_RET_IF_NOT(shader_loader.load_shader(model_shader,
                                                    {
                                                        {"regular_object.vert", GL_VERTEX_SHADER},
                                                        {"model.frag", GL_FRAGMENT_SHADER},
                                                    }),
                          false);
        model_shader.use();
        ASSERT_RET_IF_NOT(model_shader.set_float("u_material_shininess", 4.f), false);

        return true;
    }

    /**
     * @brief Set the terrain to be rendered.
     *
     * @param _terrain Terrain to set.
     *
     * @return True on success, otherwise false.
     */
    bool Renderer::set_terrain(const Terrain &_terrain)
    {
        terrain_shader.use();
        ASSERT_RET_IF_NOT(_terrain.material.apply(terrain_shader), false);

        terrain = std::make_unique<Terrain>(_terrain);

        return true;
    }

    /**
     * @brief Add a regular object to be rendered.
     *
     * @param object Regular object to add.
     */
    void Renderer::add_regular_object(const RegularObject &object)
    {
        regular_objects.push_back(object);
    }

    /**
     * @brief Add a point light object to be rendered.
     *
     * @param object Point light object to add.
     */
    void Renderer::add_point_light_object(const PointLightObject &object)
    {
        point_light_objects.push_back(object);
    }

    /**
     * @brief Add a directional light object to be rendered.
     *
     * @param object Directional light object to add.
     */
    void Renderer::add_directional_light_object(const DirectionalLightObject &object)
    {
        directional_light_objects.push_back(object);
    }

    /**
     * @brief Add a spot light object to be rendered.
     *
     * @param object Spot light object to add.
     */
    void Renderer::add_spot_light_object(const SpotLightObject &object)
    {
        spot_light_objects.push_back(object);
    }

    /**
     * @brief Add a debug object to be rendered.
     *
     * @param object Debug object to add.
     */
    void Renderer::add_debug_object(const DebugObject &object)
    {
        debug_objects.push_back(object);
    }

    /**
     * @brief Add a model object to be rendered.
     *
     * @param object Model object to add.
     */
    void Renderer::add_model_object(const ModelObject &object)
    {
        model_objects.push_back(object);
    }

    /**
     * @brief Render the shadow map.
     *
     * @param camera_view Camera view matrix.
     * @param light_view Light view matrix.
     * @param shadow_map_idx Index of the shadow map to render.
     * @param[out] light_view_projection Light view projection matrix.
     *
     * @return True on success, otherwise false.
     */
    bool Renderer::render_shadow_map(const glm::mat4 &camera_view,
                                     const glm::mat4 &light_view,
                                     const size_t shadow_map_idx,
                                     glm::mat4 &light_view_projection)
    {
        const float far_clip = shadow_map_ranges[shadow_map_idx];
        const float tan_fov = tan(fov_rads * 0.5f);

        /*
         * Obtain the 8 corners of the frustrum in view space.
         */
        const float near_clip = 0.001f;
        const float h0 = tan_fov * near_clip;
        const float w0 = h0 * aspect_ratio;
        const float h1 = tan_fov * far_clip;
        const float w1 = h1 * aspect_ratio;
        const std::array<glm::vec3, 8> corners = {
            /* clang-format off */
            /* Near plane. */
            glm::vec3(-w0, -h0, -near_clip),
            glm::vec3(w0, -h0, -near_clip),
            glm::vec3(w0, h0, -near_clip),
            glm::vec3(-w0, h0, -near_clip),

            /* Far plane. */
            glm::vec3(-w1, -h1, -far_clip),
            glm::vec3(w1, -h1, -far_clip),
            glm::vec3(w1, h1, -far_clip),
            glm::vec3(-w1, h1, -far_clip),
            /* clang-format on */
        };

        /*
         * Transform view space corners to light space.
         */
        float left = std::numeric_limits<float>::max();
        float right = std::numeric_limits<float>::min();
        float bottom = std::numeric_limits<float>::max();
        float top = std::numeric_limits<float>::min();
        float near = std::numeric_limits<float>::max();
        float far = std::numeric_limits<float>::min();
        const glm::mat4 inverse_camera_view = glm::inverse(camera_view);
        for (size_t i = 0; i < corners.size(); i++)
        {
            const glm::vec4 world_space_corner_four_vector =
                inverse_camera_view * glm::vec4(corners[i], 1.f);
            const glm::vec3 world_space_corner =
                glm::vec3(world_space_corner_four_vector) / world_space_corner_four_vector.w;

            const glm::vec4 light_space_corner_four_vector =
                light_view * glm::vec4(world_space_corner, 1.0);
            const glm::vec3 light_space_corner =
                glm::vec3(light_space_corner_four_vector) / light_space_corner_four_vector.w;

            left = MIN(left, light_space_corner.x);
            right = MAX(right, light_space_corner.x);
            bottom = MIN(bottom, light_space_corner.y);
            top = MAX(top, light_space_corner.y);
            near = MIN(near, -light_space_corner.z);
            far = MAX(far, -light_space_corner.z);
        }

        /*
         * We still want to render shadows for stuff that is just outside of the view, so add a bit
         * of padding in every direction.
         */
        static constexpr float padding = 30.f;
        left -= padding;
        right += padding;
        bottom -= padding;
        top += padding;
        near -= padding;
        far += padding;

        /*
         * Snap the orthographic projection to texel sized increments to prevent
         * shimmering when the camera or light moves.
         */
        const float light_frustrum_width = right - left;
        const float light_frustrum_height = top - bottom;
        const float texel_size_x = light_frustrum_width * one_over_shadow_map_resolution;
        const float texel_size_y = light_frustrum_height * one_over_shadow_map_resolution;
        left = std::floor(left / texel_size_x) * texel_size_x;
        right = std::floor(right / texel_size_x) * texel_size_x;
        bottom = std::floor(bottom / texel_size_y) * texel_size_y;
        top = std::floor(top / texel_size_y) * texel_size_y;

        const glm::mat4 light_projection = glm::ortho(left, right, bottom, top, near, far);

        /*
         * Get the light's view projection matrix which models can be multiplied by
         * to transform from world space to light space. This rotates the
         * orthographic projection box to emit from the light's point of view.
         */
        light_view_projection = light_projection * light_view;

        glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);
        glBindFramebuffer(GL_FRAMEBUFFER, shadow_map_frame_buffers[shadow_map_idx]);
        glClear(GL_DEPTH_BUFFER_BIT);

        depth_shader.use();
        ASSERT_RET_IF_NOT(depth_shader.set_mat4("u_light_view_projection", light_view_projection),
                          false);

        glCullFace(GL_FRONT);

        /*
         * Draw regular objects into shadow map.
         */
        for (const RegularObject &object : regular_objects)
        {
            ASSERT_RET_IF_NOT(depth_shader.set_mat4("u_model", object.transform.model()), false);
            object.drawable.draw();
        }

        /*
         * Draw models into shadow map.
         */
        for (const ModelObject &object : model_objects)
        {
            ASSERT_RET_IF_NOT(depth_shader.set_mat4("u_model", object.transform.model()), false);
            object.model.draw_no_textures();
        }

        /*
         * Draw terrain into shadow map.
         */
        if (likely(terrain))
        {
            ASSERT_RET_IF_NOT(depth_shader.set_mat4("u_model", glm::mat4(1)), false);
            terrain->drawable.draw();
        }

        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return true;
    }

    /**
     * @brief Render the scene.
     *
     * @param camera_view Camera view matrix.
     * @param skybox_view Skybox view matrix.
     * @param camera_position Camera position in world space.
     *
     * @return True if successful, otherwise false.
     */
    bool Renderer::render(const glm::mat4 &camera_view,
                          const glm::mat4 &skybox_view,
                          const glm::vec3 &camera_position,
                          const glm::vec3 &camera_direction)
    {
        /*
         * For now, only one directional light is supported.
         */
        ASSERT_RET_IF_NOT(directional_light_objects.size() == 1, false);

        /*
         * If the directional light is shining, render the depth map. It is a bit more
         * sensicle to make this a function of the light's position above the horizon,
         * but for now we rely on the color already being a function of it.
         *
         * We place `likely` here since the shadow rendering code is the heaviest part
         * so it saves cycles when the light is shining.
         */
        std::array<glm::mat4, num_shadow_maps> light_view_projections = {};
        if (likely(directional_light_objects[0].color != glm::vec3(0.0f)))
        {
            /*
             * Create a view matrix casting from the light to the camera center.
             */
            const glm::mat4 light_view =
                glm::lookAt(camera_position -
                                directional_light_objects[0].direction * 1000.f, /* eye */
                            camera_position,                                     /* center */
                            glm::vec3(0.0f, 1.0f, 0.0f) /* up */);

            /*
             * Rendering a wide shadow map reduces the resolution of the shadows, but rendering
             * a tight shadow map prevents shadows from being rendered for objects a bit far away.
             * To wrangle these constraints, we render 3 different shadow maps. One tight, high
             * resolution shadow map for nearby objects, one medium range and one far range with
             * poor resolution.
             */
            for (size_t shadow_map_idx = 0; shadow_map_idx < shadow_map_frame_buffers.size();
                 shadow_map_idx++)
            {
                ASSERT_RET_IF_NOT(render_shadow_map(camera_view,
                                                    light_view,
                                                    shadow_map_idx,
                                                    light_view_projections[shadow_map_idx]),
                                  false);
            }
        }

        /*
         * Render scene into the screen frame buffer.
         */
        glViewport(0, 0, window_width, window_height);
        glBindFramebuffer(GL_FRAMEBUFFER, screen_frame_buffer);
        {
            const std::array<GLenum, 2> buffers = {
                screen_color_texture.get_attachment(),
                screen_bloom_texture.get_attachment(),
            };
            glDrawBuffers(buffers.size(), buffers.data());
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        /*
         * Render debug objects.
         */
        if (unlikely(debug_objects.empty()))
        {
            {
                const std::array<GLenum, 1> buffers = {
                    screen_color_texture.get_attachment(),
                };
                glDrawBuffers(buffers.size(), buffers.data());
            }
            debug_shader.use();
            for (const DebugObject &object : debug_objects)
            {
                ASSERT_RET_IF_NOT(debug_shader.set_mat4("u_model", object.transform.model()),
                                  false);
                ASSERT_RET_IF_NOT(debug_shader.set_vec3("u_color", object.color), false);
                object.drawable.draw();
            }
        }

        /*
         * Update per-frame uniform buffer.
         */
        PerFrameUniformBuffer per_frame_uniform_buffer = {
            .camera_view = camera_view,
            .camera_projection = projection,
            .camera_position = glm::vec4(camera_position, 0.0f),
            .shadow_map_ranges =
                glm::vec4(shadow_map_ranges[0], shadow_map_ranges[1], shadow_map_ranges[2], 0.f),
            .num_point_lights = static_cast<uint32_t>(point_light_objects.size()),
            .num_spot_lights = static_cast<uint32_t>(spot_light_objects.size()),
            .camera_near_clip = camera_near_clip,
            .camera_far_clip = camera_far_clip,
            .light_view_projections = light_view_projections,
            .directional_light =
                {
                    .direction = glm::vec4(directional_light_objects[0].direction, 0.0f),
                    .ambient = glm::vec4(directional_light_objects[0].color, 0.0f),
                    .diffuse = glm::vec4(directional_light_objects[0].color, 0.0f),
                    .specular = glm::vec4(directional_light_objects[0].color, 0.0f),
                },
        };

        ASSERT_RET_IF(point_light_objects.size() > PerFrameUniformBuffer::max_point_lights, false);

        for (size_t point_light_idx = 0; point_light_idx < point_light_objects.size();
             point_light_idx++)
        {
            const PointLightObject &point_light_object = point_light_objects[point_light_idx];
            per_frame_uniform_buffer.point_lights[point_light_idx] = {
                .position = glm::vec4(point_light_object.transform.position, 0.0f),
                .ambient = glm::vec4(0.f),
                .diffuse = glm::vec4(point_light_object.color, 0.0f),
                .specular = glm::vec4(point_light_object.color, 0.0f),
            };
        }

        ASSERT_RET_IF(spot_light_objects.size() > PerFrameUniformBuffer::max_spot_lights, false);

        for (size_t spot_light_idx = 0; spot_light_idx < spot_light_objects.size();
             spot_light_idx++)
        {
            const SpotLightObject &spot_light_object = spot_light_objects[spot_light_idx];
            per_frame_uniform_buffer.spot_lights[spot_light_idx] = {
                .position = glm::vec4(spot_light_object.position, 0.0f),
                .direction = glm::vec4(spot_light_object.direction, 0.0f),
                .ambient = glm::vec4(0.0f),
                .diffuse = glm::vec4(spot_light_object.color, 0.0f),
                .specular = glm::vec4(spot_light_object.color, 0.0f),
                .inner_cut_off = spot_light_object.inner_cut_off,
                .outer_cut_off = spot_light_object.outer_cut_off,
            };
        }

        glNamedBufferSubData(per_frame_uniform_buffer_id,
                             0,
                             sizeof(PerFrameUniformBuffer),
                             &per_frame_uniform_buffer);

        /*
         * Render regular objects.
         */
        {
            const std::array<GLenum, 1> buffers = {
                screen_color_texture.get_attachment(),
            };
            glDrawBuffers(buffers.size(), buffers.data());
        }
        regular_object_shader.use();

        shadow_map_textures[0].use();
        shadow_map_textures[1].use();
        shadow_map_textures[2].use();
        for (const RegularObject &object : regular_objects)
        {
            ASSERT_RET_IF_NOT(regular_object_shader.set_mat4("u_model", object.transform.model()),
                              false);
            object.material.apply(regular_object_shader);
            object.normal_map.use();
            object.drawable.draw();
        }

        /*
         * Render model objects.
         */

        shadow_map_textures[0].use();
        shadow_map_textures[1].use();
        shadow_map_textures[2].use();
        model_shader.use();
        for (const ModelObject &object : model_objects)
        {
            ASSERT_RET_IF_NOT(model_shader.set_mat4("u_model", object.transform.model()), false);
            object.model.draw();
        }

        /*
         * Render terrain.
         */
        if (likely(terrain))
        {
            {
                const std::array<GLenum, 1> buffers = {
                    screen_color_texture.get_attachment(),
                };
                glDrawBuffers(buffers.size(), buffers.data());
            }

            shadow_map_textures[0].use();
            shadow_map_textures[1].use();
            shadow_map_textures[2].use();
            terrain_shader.use();

            terrain->normal_map.use();
            terrain->material.apply(terrain_shader);
            terrain->drawable.draw();
        }

        /*
         * Render point light objects.
         */
        {
            const std::array<GLenum, 2> buffers = {
                screen_color_texture.get_attachment(),
                screen_bloom_texture.get_attachment(),
            };
            glDrawBuffers(buffers.size(), buffers.data());
        }
        point_light_shader.use();
        for (const PointLightObject &object : point_light_objects)
        {
            ASSERT_RET_IF_NOT(point_light_shader.set_mat4("u_model", object.transform.model()),
                              false);
            object.drawable.draw();
        }

        /*
         * Render tangent bitangent normal debug visualization.
         */
        if (unlikely(visualize_tbn))
        {
            const std::array<GLenum, 1> buffers = {
                screen_color_texture.get_attachment(),
            };
            glDrawBuffers(buffers.size(), buffers.data());

            /*
             * Note: This shader also works for the model objects since they use the same vertex
             * shader.
             */
            regular_object_tbn_visualizer_shader.use();

            ASSERT_RET_IF_NOT(regular_object_tbn_visualizer_shader.set_bool(
                                  "u_only_show_normals", visualize_tbn_only_show_normals),
                              false);
            ASSERT_RET_IF_NOT(regular_object_tbn_visualizer_shader.set_float(
                                  "u_magnitude", visualize_tbn_magnitude),
                              false);

            for (const RegularObject &object : regular_objects)
            {
                ASSERT_RET_IF_NOT(regular_object_tbn_visualizer_shader.set_mat4(
                                      "u_model", object.transform.model()),
                                  false);
                object.drawable.draw();
            }

            for (const ModelObject &object : model_objects)
            {
                ASSERT_RET_IF_NOT(regular_object_tbn_visualizer_shader.set_mat4(
                                      "u_model", object.transform.model()),
                                  false);
                object.model.draw();
            }

            if (likely(terrain))
            {
                terrain_tbn_visualizer_shader.use();
                ASSERT_RET_IF_NOT(terrain_tbn_visualizer_shader.set_bool(
                                      "u_only_show_normals", visualize_tbn_only_show_normals),
                                  false);
                ASSERT_RET_IF_NOT(terrain_tbn_visualizer_shader.set_float("u_magnitude",
                                                                          visualize_tbn_magnitude),
                                  false);
                ASSERT_RET_IF_NOT(terrain_tbn_visualizer_shader.set_mat4("u_model", glm::mat4(1.f)),
                                  false);
                terrain->drawable.draw();
            }
        }

        /*
         * Render skybox.
         */
        glDepthFunc(GL_LEQUAL);

        skybox_texture.use();

        skybox_shader.use();
        ASSERT_RET_IF_NOT(skybox_shader.set_mat4("u_view", skybox_view), false);
        ASSERT_RET_IF_NOT(skybox_shader.set_vec3("u_sun_color", directional_light_objects[0].color),
                          false);
        cube->draw();

        glDepthFunc(GL_LESS);

        /*
         * Apply a gaussian blur to the bloom texture.
         */
        uint8_t horizontal = 1;
        bool first_iteration = true;
        static constexpr uint8_t passes = 10;
        gaussian_blur_shader.use();
        for (uint8_t i = 0; i < passes; ++i)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, ping_pong_frame_buffer[horizontal]);
            ASSERT_RET_IF_NOT(gaussian_blur_shader.set_int("u_horizontal", horizontal), false);

            horizontal = 1 ^ horizontal;

            if (unlikely(first_iteration))
            {
                screen_bloom_texture.use();
                first_iteration = false;
            }
            else
            {
                ping_pong_texture[horizontal].use();
            }

            screen->draw();
        }

        /*
         * Go back to default frame buffer and draw the screen texture over a
         * quad.
         */
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        screen_shader.use();
        ping_pong_texture[horizontal].use();
        screen_color_texture.use();
        screen->draw();

        /*
         * Draw crosshair.
         */
        glDisable(GL_DEPTH_TEST);
        crosshair_shader.use();
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glEnable(GL_DEPTH_TEST);

        /*
         * Clear object buffers.
         */
        regular_objects.clear();
        point_light_objects.clear();
        directional_light_objects.clear();
        spot_light_objects.clear();
        debug_objects.clear();
        model_objects.clear();

        return true;
    }

    /**
     * @brief Set exposure.
     *
     * @param _exposure Exposure value.
     */
    bool Renderer::set_exposure(const float _exposure)
    {
        exposure = _exposure;
        screen_shader.use();
        ASSERT_RET_IF_NOT(screen_shader.set_float("u_exposure", _exposure), false);
        return true;
    }

    /**
     * @brief Set gamma.
     *
     * @param _gamma Gamma value.
     */
    bool Renderer::set_gamma(const float _gamma)
    {
        gamma = _gamma;
        screen_shader.use();
        ASSERT_RET_IF_NOT(screen_shader.set_float("u_gamma", _gamma), false);
        return true;
    }

    /**
     * @brief Set sharpness.
     *
     * @param _sharpness Sharpness value.
     *
     * @return True on success, otherwise false.
     */
    bool Renderer::set_sharpness(const float _sharpness)
    {
        sharpness = _sharpness;
        screen_shader.use();
        ASSERT_RET_IF_NOT(screen_shader.set_float("u_sharpness", _sharpness), false);
        return true;
    }

    /**
     * @return Exposure value.
     */
    float Renderer::get_exposure() const
    {
        return exposure;
    }

    /**
     * @return Gamma value.
     */
    float Renderer::get_gamma() const
    {
        return gamma;
    }

    /**
     * @return Sharpness value.
     */
    float Renderer::get_sharpness() const
    {
        return sharpness;
    }
}