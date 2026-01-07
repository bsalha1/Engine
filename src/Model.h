#pragma once

#include "IndexBuffer.h"
#include "Texture.h"
#include "Vertex.h"
#include "VertexArray.h"

#include <assimp/scene.h>
#include <vector>

namespace Engine
{
    /**
     * @brief Mesh for a model.
     */
    class Mesh
    {
    public:
        void create_buffers(const std::vector<TexturedVertex3dNormalTangent> &vertices,
                            const std::vector<unsigned int> &indices);

        void draw() const;

        void draw_no_textures() const;

        void set_texture(const TextureSlot slot, Texture *texture);

    private:
        /**
         * Vertex array object.
         */
        VertexArray vertex_array;

        /**
         * Index buffer object.
         */
        IndexBuffer index_buffer = IndexBuffer(vertex_array);

        /**
         * Textures.
         */
        std::array<Texture *, TextureSlot::NUM_MODEL_TEXTURE_TYPES> textures;
    };

    /**
     * @brief 3D model consisting of multiple meshes.
     */
    class Model
    {
    public:
        bool load(const std::string &path);

        void draw() const;

        void draw_no_textures() const;

    private:
        bool load_material_textures(Mesh &mesh,
                                    const aiMaterial *material,
                                    const aiTextureType type,
                                    const std::string &directory);

        bool load_mesh(const aiMesh *mesh, const aiScene *scene, const std::string &directory);

        bool load_node(const aiNode *node, const aiScene *scene, const std::string &directory);

        size_t get_num_meshes(const aiNode *root_node);

        /**
         * Meshes comprising the model.
         */
        std::vector<Mesh> meshes;

        /**
         * Map from path to texture. Used to deduplicate textures. Meshes contain references to the
         * textures stored in this container, so this also provides storage.
         */
        std::unordered_map<std::string, Texture> path_to_texture;
    };
}