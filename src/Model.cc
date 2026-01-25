#include "Model.h"

#include "assert_util.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <glm/geometric.hpp>

namespace Engine
{
    /**
     * @brief Create buffers for the mesh.
     *
     * @param vertices Vertices.
     * @param indices Indices.
     */
    void Mesh::create_buffers(const std::vector<TexturedVertex3dNormalTangent> &vertices,
                              const std::vector<unsigned int> &indices)
    {
        vertex_array.create(vertices.data(), vertices.size());
        TexturedVertex3dNormalTangent::setup_vertex_array_attribs(vertex_array);
        index_buffer.create(indices.data(), indices.size());
    }

    /**
     * @brief Draw the mesh.
     */
    void Mesh::draw() const
    {
        if (textures[TextureSlot::DIFFUSE])
        {
            textures[TextureSlot::DIFFUSE]->use();
        }
        if (textures[TextureSlot::SPECULAR])
        {
            textures[TextureSlot::SPECULAR]->use();
        }
        if (textures[TextureSlot::NORMAL])
        {
            textures[TextureSlot::NORMAL]->use();
        }

        index_buffer.draw();
    }

    /**
     * @brief Draw the mesh without binding textures - useful for depth/shadow passes.
     */
    void Mesh::draw_no_textures() const
    {
        index_buffer.draw();
    }

    /**
     * @brief Set a texture for the mesh.
     *
     * @param slot Texture slot.
     * @param texture Pointer to texture.
     */
    void Mesh::set_texture(const TextureSlot slot, Texture *texture)
    {
        textures[slot] = texture;
    }

    /**
     * @brief Load model from file.
     *
     * @param path Path to the model file.
     *
     * @return True on success, otherwise false.
     */
    bool Model::load(const std::string &path)
    {
        LOG_DEBUG("Loading model %s\n", path.c_str());

        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(path,
                                                 aiProcess_Triangulate | aiProcess_FlipUVs |
                                                     aiProcess_CalcTangentSpace);
        ASSERT_RET_IF_NOT(scene, false);
        ASSERT_RET_IF_NOT(scene->mRootNode, false);
        ASSERT_RET_IF(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE, false);

        /*
         * First, get the number of meshes. This is critical because the mesh
         * object contains IndexBuffers which contain references to the VertexArray objects,
         * and so dynamic resizing of the meshes vector would invalidate those references.
         */
        const size_t num_meshes = get_num_meshes(scene->mRootNode);
        LOG_DEBUG("Contains %zu meshes\n", num_meshes);

        meshes.reserve(num_meshes);

        const std::string directory = path.substr(0, path.find_last_of('/') + 1);
        ASSERT_RET_IF_NOT(load_node(scene->mRootNode, scene, directory), false);

        return true;
    }

    /**
     * @brief Draw each mesh of the model.
     */
    void Model::draw() const
    {
        for (const Mesh &mesh : meshes)
        {
            mesh.draw();
        }
    }

    /**
     * @brief Draw each mesh of the model without binding textures - useful for depth/shadow passes.
     */
    void Model::draw_no_textures() const
    {
        for (const Mesh &mesh : meshes)
        {
            mesh.draw_no_textures();
        }
    }

    /**
     * @brief Load material textures of a given type into the mesh.
     *
     * @param[out] mesh Mesh to load textures into.
     * @param material Material.
     * @param type Texture type.
     * @param directory Directory of the model file.
     *
     * @return True on success, otherwise false.
     */
    bool Model::load_material_textures(Mesh &mesh,
                                       const aiMaterial *material,
                                       const aiTextureType type,
                                       const std::string &directory)
    {
        /*
         * Translate texture type to texture slot.
         */
        TextureSlot slot;
        switch (type)
        {
        case aiTextureType_DIFFUSE:
            slot = TextureSlot::DIFFUSE;
            break;
        case aiTextureType_SPECULAR:
            slot = TextureSlot::SPECULAR;
            break;
        case aiTextureType_HEIGHT:
            slot = TextureSlot::NORMAL;
            break;
        default:
            ASSERT_RET_IF_NOT(false, false);
        }

        /*
         * We only support one texture per type for now.
         */
        ASSERT_RET_IF(material->GetTextureCount(type) != 1, false);

        /*
         * Get texture file path.
         */
        aiString file_path;
        ASSERT_RET_IF_NOT(material->GetTexture(type, 0 /* index */, &file_path) == aiReturn_SUCCESS,
                          false);

        Texture *texture = nullptr;

        /*
         * Check if texture was loaded before. If so, grab a reference to it.
         */
        const std::string texture_path = (directory + std::string(file_path.C_Str()));
        const decltype(path_to_texture)::iterator &it = path_to_texture.find(file_path.C_Str());
        if (it != path_to_texture.end())
        {
            LOG_DEBUG("Reusing texture %s\n", texture_path.c_str());
            texture = &it->second;
        }
        /*
         * Otherwise, create a new texture and load it from file.
         */
        else
        {
            texture = &path_to_texture.emplace(file_path.C_Str(), slot).first->second;

            /*
             * Load texture from file.
             */
            ASSERT_RET_IF_NOT(texture->create_from_file(texture_path), false);
        }

        /*
         * Set the texture on the mesh.
         */
        mesh.set_texture(slot, texture);

        return true;
    }

    /**
     * @brief Load a mesh from Assimp into our internal mesh representation.
     *
     * @param mesh Assimp mesh.
     * @param scene Assimp scene.
     * @param directory Directory of the model file.
     */
    bool Model::load_mesh(const aiMesh *mesh, const aiScene *scene, const std::string &directory)
    {
        /*
         * Load vertices.
         */
        std::vector<TexturedVertex3dNormalTangent> vertices;
        vertices.reserve(mesh->mNumVertices);
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            TexturedVertex3dNormalTangent &vertex = vertices.emplace_back();

            /*
             * Set position.
             */
            const aiVector3D &position = mesh->mVertices[i];
            vertex.position = glm::vec3(position.x, position.y, position.z);

            /*
             * Set normal.
             */
            const aiVector3D &normal = mesh->mNormals[i];
            vertex.norm = glm::vec3(normal.x, normal.y, normal.z);

            const aiVector3D &bitangent = mesh->mBitangents[i];
            const glm::vec3 _bitangent = glm::vec3(bitangent.x, bitangent.y, bitangent.z);

            /*
             * Compute tangent and use Gram-Schmidt orthonormalization to remove any normal
             * component from the tangent.
             */
            const aiVector3D &tangent = mesh->mTangents[i];
            glm::vec3 _tangent = glm::vec3(tangent.x, tangent.y, tangent.z);
            _tangent = normalize(_tangent - vertex.norm * dot(vertex.norm, _tangent));

            const float handedness =
                (glm::dot(glm::cross(vertex.norm, _tangent), _bitangent) < 0.0f) ? -1.0f : 1.0f;
            vertex.tangent = glm::vec4(_tangent, handedness);

            if (mesh->mTextureCoords[0])
            {
                const aiVector3D &texCoord = mesh->mTextureCoords[0][i];
                vertex.texture = glm::vec2(texCoord.x, texCoord.y);
            }
            else
            {
                vertex.texture = glm::vec2(0.f, 0.f);
            }
        }

        /*
         * Load indices.
         */
        std::vector<unsigned int> indices;
        for (unsigned int face_idx = 0; face_idx < mesh->mNumFaces; face_idx++)
        {
            const aiFace &face = mesh->mFaces[face_idx];
            indices.reserve(indices.size() + face.mNumIndices);
            for (unsigned int i = 0; i < face.mNumIndices; i++)
            {
                indices.push_back(face.mIndices[i]);
            }
        }

        Mesh &internal_mesh = meshes.emplace_back();

        /*
         * Load material textures.
         */
        if (mesh->mMaterialIndex >= 0)
        {
            const aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
            ASSERT_RET_IF_NOT(
                load_material_textures(internal_mesh, material, aiTextureType_DIFFUSE, directory),
                false);
            ASSERT_RET_IF_NOT(
                load_material_textures(internal_mesh, material, aiTextureType_SPECULAR, directory),
                false);
            ASSERT_RET_IF_NOT(
                load_material_textures(internal_mesh, material, aiTextureType_HEIGHT, directory),
                false);
        }

        /*
         * Create buffers on the GPU.
         */
        internal_mesh.create_buffers(vertices, indices);

        return true;
    }

    /**
     * @brief Load node and its children recursively.
     *
     * @param node Current node.
     * @param scene Scene.
     * @param directory Directory of the model file.
     *
     * @return True on success, otherwise false.
     */
    bool Model::load_node(const aiNode *node, const aiScene *scene, const std::string &directory)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
            ASSERT_RET_IF_NOT(load_mesh(mesh, scene, directory), false);
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ASSERT_RET_IF_NOT(load_node(node->mChildren[i], scene, directory), false);
        }

        return true;
    }

    /**
     * @return Number of meshes in the model.
     *
     * @param root_node Root node.
     */
    size_t Model::get_num_meshes(const aiNode *root_node)
    {
        size_t num_meshes = root_node->mNumMeshes;

        for (unsigned int i = 0; i < root_node->mNumChildren; i++)
        {
            num_meshes += root_node->mChildren[i]->mNumMeshes;
        }

        return num_meshes;
    }
}