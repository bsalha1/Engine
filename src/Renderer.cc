#include "Renderer.h"

#include "FramebufferTexture.h"
#include "TexturedMaterial.h"
#include "Vertex.h"
#include "VertexArray.h"

#include <GL/glew.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

namespace Engine
{
    static constexpr GLsizei shadow_map_resolution = 2048;

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
        glm::mat4 model_matrix = glm::mat4(1.0f);
        model_matrix = glm::translate(model_matrix, position);
        model_matrix =
            glm::rotate(model_matrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model_matrix =
            glm::rotate(model_matrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model_matrix =
            glm::rotate(model_matrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        model_matrix = glm::scale(model_matrix, scale);
        return model_matrix;
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
    Renderer::Renderer(): exposure(1.0f), gamma(0.5f), sharpness(1.0f)
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

        static constexpr float fov_deg = 75.f;
        static constexpr const float far_clip = 5000.f;
        const float aspect = static_cast<float>(window_width) / window_height;
        const float near_clip = 0.001f;
        projection =
            glm::perspective(glm::radians(fov_deg), aspect, near_clip, far_clip);

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

            std::unique_ptr<VertexArray> screen_vertex_array =
                std::make_unique<VertexArray>();

            screen_vertex_array->create(vertices.data(), vertices.size());
            screen_vertex_array->setup_vertex_attrib(0, &TexturedVertex2d::position);
            screen_vertex_array->setup_vertex_attrib(1, &TexturedVertex2d::texture);
            screen = std::move(screen_vertex_array);

            glGenFramebuffers(1, &screen_frame_buffer);
            glBindFramebuffer(GL_FRAMEBUFFER, screen_frame_buffer);

            /*
             * Create textures to hold the color and brightness buffers.
             */
            screen_color_texture.create(window_width,
                                        window_height,
                                        GL_COLOR_ATTACHMENT0,
                                        0 /* slot */,
                                        GL_RGBA16F /* internal_format */,
                                        GL_RGBA /* format */,
                                        GL_LINEAR /* min_filter */,
                                        GL_LINEAR /* max_filter */,
                                        GL_REPEAT /* wrap_mode */);
            screen_bloom_texture.create(window_width,
                                        window_height,
                                        GL_COLOR_ATTACHMENT1,
                                        1 /* slot */,
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
            glRenderbufferStorage(
                GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, window_width, window_height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                      GL_DEPTH_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER,
                                      depth_stencil_render_buffer);

            ASSERT_RET_IF_NOT(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                                  GL_FRAMEBUFFER_COMPLETE,
                              false);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            /*
             * Create ping-pong frame buffers for blurring the bloom texture.
             */
            for (uint8_t i = 0; i < ping_pong_frame_buffer.size(); i++)
            {
                glGenFramebuffers(1, &ping_pong_frame_buffer[i]);
                glBindFramebuffer(GL_FRAMEBUFFER, ping_pong_frame_buffer[i]);

                ping_pong_texture[i].create(screen_bloom_texture.get_width(),
                                            screen_bloom_texture.get_height(),
                                            GL_COLOR_ATTACHMENT0,
                                            screen_bloom_texture.get_slot(),
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
            ASSERT_RET_IF_NOT(skybox_texture.create_from_file("textures/skybox/",
                                                              ".jpg",
                                                              0 /* slot */),
                              false);

            static const std::array<Vertex3d, 36> skybox_vertices = {
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

            std::unique_ptr<VertexArray> cube_vertex_array =
                std::make_unique<VertexArray>();
            cube_vertex_array->create(skybox_vertices.data(), skybox_vertices.size());
            cube_vertex_array->setup_vertex_attrib(0, &Vertex3d::position);
            cube = std::move(cube_vertex_array);
        }

        /*
         * Create shadow map frame buffer.
         */
        LOG("Creating shadow map frame buffer\n");
        {
            glGenFramebuffers(1, &shadow_map_frame_buffer);
            glBindFramebuffer(GL_FRAMEBUFFER, shadow_map_frame_buffer);
            shadow_map_texture.create(shadow_map_resolution,
                                      shadow_map_resolution,
                                      GL_DEPTH_ATTACHMENT,
                                      2 /* slot */,
                                      GL_DEPTH_COMPONENT /* internal_format */,
                                      GL_DEPTH_COMPONENT /* format */,
                                      GL_NEAREST /* min_filter */,
                                      GL_NEAREST /* max_filter */,
                                      GL_CLAMP_TO_BORDER /* wrap_mode */);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);

            ASSERT_RET_IF_NOT(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                                  GL_FRAMEBUFFER_COMPLETE,
                              false);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        /*
         * Initialize screen shader.
         */
        ASSERT_RET_IF_NOT(screen_shader.compile({
                              {"screen.vert", GL_VERTEX_SHADER},
                              {"screen.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        screen_shader.use();
        ASSERT_RET_IF_NOT(screen_shader.set_int("u_color_texture_sampler",
                                                screen_color_texture.get_slot()),
                          false);
        ASSERT_RET_IF_NOT(screen_shader.set_int("u_bloom_texture_sampler",
                                                screen_bloom_texture.get_slot()),
                          false);
        ASSERT_RET_IF_NOT(screen_shader.set_float("u_exposure", exposure), false);
        ASSERT_RET_IF_NOT(screen_shader.set_float("u_gamma", gamma), false);
        ASSERT_RET_IF_NOT(screen_shader.set_float("u_sharpness", sharpness), false);

        /*
         * Initialize gaussian blur shader.
         */
        ASSERT_RET_IF_NOT(gaussian_blur_shader.compile({
                              {"gaussian_blur.vert", GL_VERTEX_SHADER},
                              {"gaussian_blur.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        gaussian_blur_shader.use();
        ASSERT_RET_IF_NOT(gaussian_blur_shader.set_int("u_texture_sampler",
                                                       screen_bloom_texture.get_slot()),
                          false);

        /*
         * Initialize cube shader.
         */
        ASSERT_RET_IF_NOT(skybox_shader.compile({
                              {"skybox.vert", GL_VERTEX_SHADER},
                              {"skybox.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        skybox_shader.use();
        ASSERT_RET_IF_NOT(skybox_shader.set_mat4("u_projection", projection), false);
        ASSERT_RET_IF_NOT(
            skybox_shader.set_float("u_sun_angular_radius", sun_angular_radius), false);
        ASSERT_RET_IF_NOT(skybox_shader.set_vec3("u_sun_position",
                                                 sun_position_skybox_model_space),
                          false);
        ASSERT_RET_IF_NOT(skybox_shader.set_int("u_texture_sampler",
                                                skybox_texture.get_slot()),
                          false);

        /*
         * Initialize regular object shader.
         */
        ASSERT_RET_IF_NOT(regular_object_shader.compile({
                              {"regular_object.vert", GL_VERTEX_SHADER},
                              {"regular_object.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        regular_object_shader.use();
        ASSERT_RET_IF_NOT(regular_object_shader.set_mat4("u_projection", projection),
                          false);
        ASSERT_RET_IF_NOT(regular_object_shader.set_int("u_texture_sampler", 0), false);
        ASSERT_RET_IF_NOT(regular_object_shader.set_int("u_normal_map_sampler", 1),
                          false);
        ASSERT_RET_IF_NOT(regular_object_shader.set_int("u_shadow_map_sampler",
                                                        shadow_map_texture.get_slot()),
                          false);

        /*
         * Initialize point light shader.
         */
        ASSERT_RET_IF_NOT(point_light_shader.compile({
                              {"point_light.vert", GL_VERTEX_SHADER},
                              {"point_light.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        point_light_shader.use();
        ASSERT_RET_IF_NOT(point_light_shader.set_mat4("u_projection", projection),
                          false);

        /*
         * Initialize depth shader.
         */
        ASSERT_RET_IF_NOT(depth_shader.compile({
                              {"depth.vert", GL_VERTEX_SHADER},
                              {"depth.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);

        /*
         * Initialize debug shader.
         */
        ASSERT_RET_IF_NOT(debug_shader.compile({
                              {"debug.vert", GL_VERTEX_SHADER},
                              {"debug.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        debug_shader.use();
        ASSERT_RET_IF_NOT(debug_shader.set_mat4("u_projection", projection), false);

        /*
         * Initialize terrain shader.
         */
        ASSERT_RET_IF_NOT(terrain_shader.compile({
                              {"terrain.vert", GL_VERTEX_SHADER},
                              {"terrain.frag", GL_FRAGMENT_SHADER},
                          }),
                          false);
        terrain_shader.use();
        ASSERT_RET_IF_NOT(terrain_shader.set_mat4("u_projection", projection), false);
        ASSERT_RET_IF_NOT(terrain_shader.set_mat4("u_model", glm::mat4(1)), false);

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
        ASSERT_RET_IF_NOT(terrain_shader.set_int("u_normal_map_sampler",
                                                 _terrain.normal_map.get_slot()),
                          false);
        ASSERT_RET_IF_NOT(terrain_shader.set_int("u_shadow_map_sampler",
                                                 shadow_map_texture.get_slot()),
                          false);

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
     * @brief Add a debug object to be rendered.
     *
     * @param object Debug object to add.
     */
    void Renderer::add_debug_object(const DebugObject &object)
    {
        debug_objects.push_back(object);
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
         * For now, only one directional and point light is supported.
         */
        ASSERT_RET_IF_NOT(directional_light_objects.size() == 1, false);
        ASSERT_RET_IF_NOT(point_light_objects.size() == 1, false);

        glm::mat4 light_view_projection;

        /*
         * If the directional light is shining, render the depth map. It is a bit more
         * sensicle to make this a function of the light's position above the horizon,
         * but for now we rely on the color already being a function of it.
         */
        if (directional_light_objects[0].color != glm::vec3(0.0f))
        {
            /*
             * The directional light is infinitely far away, but we cannot afford
             * to render an infinite area for the shadow map. Instead, we want to only
             * render what the camera can see. We can accomplish this by picking a point
             * in front of the camera, `light_target` and compute the `light_position`
             * by moving back along the light's direction and draw an orthographic
             * projection frustrum from the light, pointed at the `light_target`.
             *
             * There are a few settings that we must decide on:
             * - `shadow_frustrum_start`: How close to the light the frustrum starts.
             *   This should just start at the `light_position`, so 0.
             *
             * - `shadow_frustrum_end`: How far from the light the frustrum ends. This
             *   should be long enough to reach the ground from the `light_position`. We
             *   should eventually set this programmatically, but for now we just have a
             *   large fixed value.
             *
             * - `shadow_frustrum_width`: How wide the frustrum is in each direction
             *   from the center. This sets how much field of view resides in the
             *   frustrum. We should also set this programmatically, but for now we just
             *   make it the same as the `shadow_frustrum_end` (it is divided by 2
             *   because it extends in both left and right directions).
             *
             * - `shadow_render_distance_from_camera`: How far in front of the camera we
             *   should place the `light_target`. This should be set so that objects
             *   near the camera are rendered with shadows. We set it to be half the
             *   length of the frustrum so that it resides in the center of the
             *   frustrum.
             *
             * - `shadow_render_distance_from_light`: How far back from the
             *   `light_target` we place the `light_position`. To make it so
             *   `light_target` is in the center of the frustrum we set it to be half
             *   the length of the frustrum.
             *
             * Also important to note is that a big frustrum means more area will have
             * shadows rendered, but the resolution of the shadow map will be spread
             * thinner, leading to blocky shadows. A smaller frustrum means less area
             * will have shadows rendered, but the resolution of the shadow map will be
             * more dense, leading to sharper shadows.
             */
            static constexpr float shadow_frustrum_start = 0.f;
            static constexpr float shadow_frustrum_end = 150.f;
            static constexpr float shadow_frustrum_width = shadow_frustrum_end / 2.f;
            static constexpr float shadow_render_distance_from_camera =
                shadow_frustrum_end / 2.f;
            const float shadow_render_distance_from_light = shadow_frustrum_end / 2.f;

            const glm::vec3 light_target =
                camera_position + camera_direction * shadow_render_distance_from_camera;

            const glm::vec3 light_position =
                light_target - directional_light_objects[0].direction *
                                   shadow_render_distance_from_light;

            const glm::mat4 light_projection = glm::ortho(-shadow_frustrum_width,
                                                          shadow_frustrum_width,
                                                          -shadow_frustrum_width,
                                                          shadow_frustrum_width,
                                                          shadow_frustrum_start,
                                                          shadow_frustrum_end);

            /*
             * Get the light's view, starting at its position, looking at the target,
             * with "up" being the y axis.
             */
            const glm::mat4 light_view =
                glm::lookAt(light_position, light_target, glm::vec3(0.0f, 1.0f, 0.0f));

            /*
             * Get the light's view projection matrix which models can be multiplied by
             * to transform from world space to light space. This rotates the
             * orthographic projection box to emit from the light's point of view.
             */
            light_view_projection = light_projection * light_view;

            glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);
            glBindFramebuffer(GL_FRAMEBUFFER, shadow_map_frame_buffer);
            glClear(GL_DEPTH_BUFFER_BIT);

            depth_shader.use();
            ASSERT_RET_IF_NOT(depth_shader.set_mat4("u_light_view_projection",
                                                    light_view_projection),
                              false);

            glCullFace(GL_FRONT);

            /*
             * Draw regular objects into shadow map.
             */
            for (RegularObject &object : regular_objects)
            {
                ASSERT_RET_IF_NOT(
                    depth_shader.set_mat4("u_model", object.transform.model()), false);
                object.drawable.draw();
            }

            /*
             * Draw terrain into shadow map.
             */
            if (terrain)
            {
                ASSERT_RET_IF_NOT(depth_shader.set_mat4("u_model", glm::mat4(1)),
                                  false);
                terrain->drawable.draw();
            }

            glCullFace(GL_BACK);
        }
        else
        {
            light_view_projection = glm::mat4(1.0f);
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
        if (!debug_objects.empty())
        {
            {
                const std::array<GLenum, 1> buffers = {
                    screen_color_texture.get_attachment(),
                };
                glDrawBuffers(buffers.size(), buffers.data());
            }
            debug_shader.use();
            ASSERT_RET_IF_NOT(debug_shader.set_mat4("u_view", camera_view), false);
            for (const DebugObject &object : debug_objects)
            {
                ASSERT_RET_IF_NOT(
                    debug_shader.set_mat4("u_model", object.transform.model()), false);
                ASSERT_RET_IF_NOT(debug_shader.set_vec3("u_color", object.color),
                                  false);
                object.drawable.draw();
            }
        }

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
        ASSERT_RET_IF_NOT(regular_object_shader.set_mat4("u_view", camera_view), false);
        ASSERT_RET_IF_NOT(regular_object_shader.set_vec3("u_camera_position",
                                                         camera_position),
                          false);
        ASSERT_RET_IF_NOT(regular_object_shader.set_mat4("u_light_view_projection",
                                                         light_view_projection),
                          false);
        ASSERT_RET_IF_NOT(
            regular_object_shader.set_vec3("u_point_light.position",
                                           point_light_objects[0].transform.position),
            false);
        ASSERT_RET_IF_NOT(regular_object_shader.set_vec3("u_point_light.ambient",
                                                         point_light_objects[0].color),
                          false);
        ASSERT_RET_IF_NOT(regular_object_shader.set_vec3("u_point_light.diffuse",
                                                         point_light_objects[0].color),
                          false);
        ASSERT_RET_IF_NOT(regular_object_shader.set_vec3("u_point_light.specular",
                                                         point_light_objects[0].color),
                          false);
        ASSERT_RET_IF_NOT(
            regular_object_shader.set_vec3("u_directional_light.direction",
                                           directional_light_objects[0].direction),
            false);
        ASSERT_RET_IF_NOT(
            regular_object_shader.set_vec3("u_directional_light.ambient",
                                           directional_light_objects[0].color),
            false);
        ASSERT_RET_IF_NOT(
            regular_object_shader.set_vec3("u_directional_light.diffuse",
                                           directional_light_objects[0].color),
            false);
        ASSERT_RET_IF_NOT(
            regular_object_shader.set_vec3("u_directional_light.specular",
                                           directional_light_objects[0].color),
            false);
        shadow_map_texture.use();
        for (RegularObject &object : regular_objects)
        {
            ASSERT_RET_IF_NOT(regular_object_shader.set_mat4("u_model",
                                                             object.transform.model()),
                              false);
            object.material.apply(regular_object_shader);
            object.normal_map.use();
            object.drawable.draw();
        }

        /*
         * Render terrain.
         */
        if (terrain)
        {
            {
                const std::array<GLenum, 1> buffers = {
                    screen_color_texture.get_attachment(),
                };
                glDrawBuffers(buffers.size(), buffers.data());
            }

            shadow_map_texture.use();
            terrain_shader.use();
            ASSERT_RET_IF_NOT(terrain_shader.set_mat4("u_view", camera_view), false);
            ASSERT_RET_IF_NOT(terrain_shader.set_mat4("u_light_view_projection",
                                                      light_view_projection),
                              false);
            ASSERT_RET_IF_NOT(
                terrain_shader.set_vec3("u_camera_position", camera_position), false);
            ASSERT_RET_IF_NOT(
                terrain_shader.set_vec3("u_point_light.position",
                                        point_light_objects[0].transform.position),
                false);
            ASSERT_RET_IF_NOT(terrain_shader.set_vec3("u_point_light.ambient",
                                                      point_light_objects[0].color),
                              false);
            ASSERT_RET_IF_NOT(terrain_shader.set_vec3("u_point_light.diffuse",
                                                      point_light_objects[0].color),
                              false);
            ASSERT_RET_IF_NOT(terrain_shader.set_vec3("u_point_light.specular",
                                                      point_light_objects[0].color),
                              false);
            ASSERT_RET_IF_NOT(
                terrain_shader.set_vec3("u_directional_light.direction",
                                        directional_light_objects[0].direction),
                false);
            ASSERT_RET_IF_NOT(
                terrain_shader.set_vec3("u_directional_light.ambient",
                                        directional_light_objects[0].color),
                false);
            ASSERT_RET_IF_NOT(
                terrain_shader.set_vec3("u_directional_light.diffuse",
                                        directional_light_objects[0].color),
                false);
            ASSERT_RET_IF_NOT(
                terrain_shader.set_vec3("u_directional_light.specular",
                                        directional_light_objects[0].color),
                false);
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
        ASSERT_RET_IF_NOT(point_light_shader.set_mat4("u_view", camera_view), false);
        for (PointLightObject &object : point_light_objects)
        {
            ASSERT_RET_IF_NOT(point_light_shader.set_mat4("u_model",
                                                          object.transform.model()),
                              false);
            object.drawable.draw();
        }

        /*
         * Render skybox.
         */
        glDepthFunc(GL_LEQUAL);

        skybox_texture.use();

        skybox_shader.use();
        ASSERT_RET_IF_NOT(skybox_shader.set_mat4("u_view", skybox_view), false);
        ASSERT_RET_IF_NOT(skybox_shader.set_vec3("u_sun_color",
                                                 directional_light_objects[0].color),
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
            ASSERT_RET_IF_NOT(gaussian_blur_shader.set_int("u_horizontal", horizontal),
                              false);

            horizontal = 1 ^ horizontal;

            if (first_iteration)
            {
                screen_bloom_texture.use();
            }
            else
            {
                ping_pong_texture[horizontal].use();
            }

            screen->draw();

            if (first_iteration)
            {
                first_iteration = false;
            }
        }

        /*
         * Go back to default frame buffer and draw the screen texture over a
         * quad.
         */
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        screen_shader.use();
        ping_pong_texture[horizontal].use();
        screen_shader.set_int("u_bloom_texture_sampler",
                              ping_pong_texture[horizontal].get_slot());
        screen_color_texture.use();
        screen->draw();

        /*
         * Clear object buffers.
         */
        regular_objects.clear();
        point_light_objects.clear();
        directional_light_objects.clear();
        debug_objects.clear();

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