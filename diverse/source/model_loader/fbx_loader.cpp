#include "assets/model_asset.h"
#include "assets/material_asset.h"
#include "assets/texture_importer.h"
#include "model_loader_utils.h"
#include "assets/mesh_geometry_utils.h"
#include "maths/transform.h"
#include "engine/file_system.h"
#include "backend/drs_rhi/gpu_texture.h"
#include "utility/string_utils.h"
#include "core/profiler.h"
#include <ModelLoaders/OpenFBX/ofbx.h>
#include "utility/thread_pool.h"

const uint32_t MAX_PATH_LENGTH = 260;

namespace diverse
{
    std::string m_FBXModelDirectory;

    enum class Orientation
    {
        Y_UP,
        Z_UP,
        Z_MINUS_UP,
        X_MINUS_UP,
        X_UP
    };

    Orientation orientation = Orientation::Y_UP;
    float fbx_scale = 1.f;

    static ofbx::Vec3 operator-(const ofbx::Vec3& a, const ofbx::Vec3& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    static ofbx::Vec2 operator-(const ofbx::Vec2& a, const ofbx::Vec2& b)
    {
        return { a.x - b.x, a.y - b.y };
    }

    glm::vec3 FixOrientation(const glm::vec3& v)
    {
        switch (orientation)
        {
        case Orientation::Y_UP:
            return glm::vec3(v.x, v.y, v.z);
        case Orientation::Z_UP:
            return glm::vec3(v.x, v.z, -v.y);
        case Orientation::Z_MINUS_UP:
            return glm::vec3(v.x, -v.z, v.y);
        case Orientation::X_MINUS_UP:
            return glm::vec3(v.y, -v.x, v.z);
        case Orientation::X_UP:
            return glm::vec3(-v.y, v.x, v.z);
        }
        return glm::vec3(v.x, v.y, v.z);
    }

    glm::quat FixOrientation(const glm::quat& v)
    {
        switch (orientation)
        {
        case Orientation::Y_UP:
            return glm::quat(v.x, v.y, v.z, v.w);
        case Orientation::Z_UP:
            return glm::quat(v.x, v.z, -v.y, v.w);
        case Orientation::Z_MINUS_UP:
            return glm::quat(v.x, -v.z, v.y, v.w);
        case Orientation::X_MINUS_UP:
            return glm::quat(v.y, -v.x, v.z, v.w);
        case Orientation::X_UP:
            return glm::quat(-v.y, v.x, v.z, v.w);
        }
        return glm::quat(v.x, v.y, v.z, v.w);
    }

    static void computeTangents(ofbx::Vec3* out, int vertex_count, const ofbx::Vec3* vertices, const ofbx::Vec3* normals, const ofbx::Vec2* uvs)
    {
        for (int i = 0; i < vertex_count; i += 3)
        {
            const ofbx::Vec3 v0 = vertices[i + 0];
            const ofbx::Vec3 v1 = vertices[i + 1];
            const ofbx::Vec3 v2 = vertices[i + 2];
            const ofbx::Vec2 uv0 = uvs[i + 0];
            const ofbx::Vec2 uv1 = uvs[i + 1];
            const ofbx::Vec2 uv2 = uvs[i + 2];

            const ofbx::Vec3 dv10 = v1 - v0;
            const ofbx::Vec3 dv20 = v2 - v0;
            const ofbx::Vec2 duv10 = uv1 - uv0;
            const ofbx::Vec2 duv20 = uv2 - uv0;

            const float dir = duv20.x * duv10.y - duv20.y * duv10.x < 0 ? -1.f : 1.f;
            ofbx::Vec3 tangent;
            tangent.x = (dv20.x * duv10.y - dv10.x * duv20.y) * dir;
            tangent.y = (dv20.y * duv10.y - dv10.y * duv20.y) * dir;
            tangent.z = (dv20.z * duv10.y - dv10.z * duv20.y) * dir;
            const float l = 1 / sqrtf(float(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z));
            tangent.x *= l;
            tangent.y *= l;
            tangent.z *= l;
            out[i + 0] = tangent;
            out[i + 1] = tangent;
            out[i + 2] = tangent;
        }
    }

    glm::vec2 ToLumosVector(const ofbx::Vec2& vec)
    {
        return glm::vec2(float(vec.x), float(vec.y));
    }

    glm::vec3 ToLumosVector(const ofbx::Vec3& vec)
    {
        return glm::vec3(float(vec.x), float(vec.y), float(vec.z));
    }

    glm::vec4 ToLumosVector(const ofbx::Vec4& vec)
    {
        return glm::vec4(float(vec.x), float(vec.y), float(vec.z), float(vec.w));
    }

    glm::vec4 ToLumosVector(const ofbx::Color& vec)
    {
        return glm::vec4(float(vec.r), float(vec.g), float(vec.b), 1.0f);
    }

    glm::quat ToLumosQuat(const ofbx::Quat& quat)
    {
        return glm::quat(float(quat.x), float(quat.y), float(quat.z), float(quat.w));
    }

    bool IsMeshInvalid(const ofbx::Mesh* aMesh)
    {
        return aMesh->getGeometry()->getVertexCount() == 0;
    }

    AssetHandle<TextureAsset> LoadTexture(const ofbx::Material* material, ofbx::Texture::TextureType type)
    {
        const ofbx::Texture* ofbxTexture = material->getTexture(type);
        if (!ofbxTexture)
            return {};

        ofbx::DataView filename = ofbxTexture->getRelativeFileName();
        if (filename == "")
            filename = ofbxTexture->getFileName();

        char filePath[MAX_PATH_LENGTH];
        filename.toString(filePath);

        std::string stringFilepath = std::string(filePath);
        stringFilepath = m_FBXModelDirectory + "/" + stringutility::back_slashes_2_slashes(stringFilepath);

        if (!FileSystem::file_exists(stringFilepath))
        {
            stringFilepath = stringutility::get_file_name(stringFilepath);
            stringFilepath = m_FBXModelDirectory + "/" + stringFilepath;
        }
        if (!FileSystem::file_exists(stringFilepath))
        {
            stringFilepath = stringutility::get_file_name(stringFilepath);
            stringFilepath = m_FBXModelDirectory + "/textures/" + stringFilepath;
        }

        if (FileSystem::file_exists(stringFilepath))
            return import_and_register_texture(stringFilepath);
        return {};
    }

    std::shared_ptr<MaterialAsset> LoadMaterial(const ofbx::Material* material, bool animated)
    {
        (void)animated;
        auto pbrMaterial = std::make_shared<MaterialAsset>();
        pbrMaterial->id = GenerateAssetId();
        pbrMaterial->is_valid = true;

        MaterialProperties properties;
        properties.base_color_mult = ToLumosVector(material->getDiffuseColor());
        properties.metalness_factor = material->getSpecularColor().r;

        float roughness = 1.0f - maths::Sqrt(float(material->getShininess()) / 100.0f);
        properties.roughness_mult = roughness;

        pbrMaterial->albedo = LoadTexture(material, ofbx::Texture::TextureType::DIFFUSE);
        pbrMaterial->normal = LoadTexture(material, ofbx::Texture::TextureType::NORMAL);
        pbrMaterial->metallic = LoadTexture(material, ofbx::Texture::TextureType::SPECULAR);
        pbrMaterial->roughness = LoadTexture(material, ofbx::Texture::TextureType::SHININESS);
        pbrMaterial->emissive = LoadTexture(material, ofbx::Texture::TextureType::EMISSIVE);
        pbrMaterial->ao = LoadTexture(material, ofbx::Texture::TextureType::AMBIENT);

        if (!pbrMaterial->normal.is_valid())
            properties.normal_map_factor = 0.0f;
        if (!pbrMaterial->metallic.is_valid())
            properties.metallic_map_factor = 0.0f;
        if (!pbrMaterial->roughness.is_valid())
            properties.roughness_map_factor = 0.0f;
        if (!pbrMaterial->emissive.is_valid())
            properties.emissive_map_factor = 0.0f;
        if (!pbrMaterial->ao.is_valid())
            properties.ao_map_factor = 0.0f;

        pbrMaterial->properties = properties;
        return pbrMaterial;
    }

    maths::Transform GetTransform(const ofbx::Object* mesh)
    {
        auto transform = maths::Transform();

        ofbx::Vec3 p = mesh->getLocalTranslation();

        glm::vec3 pos = (glm::vec3(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)));
        transform.set_local_position(FixOrientation(pos));

        ofbx::Vec3 r = mesh->getLocalRotation();
        glm::vec3 rot = FixOrientation(glm::vec3(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.z)));
        transform.set_local_orientation(glm::quat(glm::vec3(rot.x, rot.y, rot.z)));

        ofbx::Vec3 s = mesh->getLocalScaling();
        glm::vec3 scl = glm::vec3(static_cast<float>(s.x), static_cast<float>(s.y), static_cast<float>(s.z));
        transform.set_local_scale(scl);

        if (mesh->getParent())
        {
            transform.set_world_matrix(GetTransform(mesh->getParent()).get_world_matrix());
        }
        else
            transform.set_world_matrix(glm::mat4(1.0f));

        return transform;
    }

    std::shared_ptr<MeshAsset> LoadMesh(const ofbx::Mesh* fbxMesh, int32_t triangleStart, int32_t triangleEnd)
    {
        const int32_t firstVertexOffset = triangleStart * 3;
        const int32_t lastVertexOffset = triangleEnd * 3;
        const int vertexCount = lastVertexOffset - firstVertexOffset + 3;

        auto geom = fbxMesh->getGeometry();
        auto numIndices = geom->getIndexCount();
        int vertex_count = geom->getVertexCount();
        const ofbx::Vec3* vertices = geom->getVertices();
        const ofbx::Vec3* normals = geom->getNormals();
        const ofbx::Vec3* tangents = geom->getTangents();
        // const ofbx::Vec3* bitangents   = geom->getBitangents();
        const ofbx::Vec4* colours = geom->getColors();
        const ofbx::Vec2* uvs = geom->getUVs();
        const int* materials = geom->getMaterials();
        std::vector<Vertex> tempvertices(vertex_count);
        std::vector<uint32_t> indicesArray(numIndices);
        ofbx::Vec3* generatedTangents = nullptr;

        int indexCount = 0;
        auto indices = geom->getFaceIndices();

        auto transform = GetTransform(fbxMesh);

        for (int i = 0; i < vertexCount; i++)
        {
            ofbx::Vec3 cp = vertices[i + firstVertexOffset];

            auto& vertex = tempvertices[i];
            vertex.Position = transform.get_world_matrix() * glm::vec4(float(cp.x), float(cp.y), float(cp.z), 1.0f);
            FixOrientation(vertex.Position);

            if (normals)
                vertex.Normal = transform.get_world_matrix() * glm::normalize(glm::vec4(float(normals[i + firstVertexOffset].x), float(normals[i + firstVertexOffset].y), float(normals[i + firstVertexOffset].z), 1.0f));
            // vertex.Normal = transform.get_world_matrix().ToMatrix3().Inverse().Transpose() * (glm::vec3(float(normals[i].x), float(normals[i].y), float(normals[i].z))).Normalised();
            if (uvs)
                vertex.TexCoords = glm::vec2(float(uvs[i + firstVertexOffset].x), 1.0f - float(uvs[i + firstVertexOffset].y));
            if (colours)
                vertex.Colours = glm::vec4(float(colours[i + firstVertexOffset].x), float(colours[i + firstVertexOffset].y), float(colours[i + firstVertexOffset].z), float(colours[i + firstVertexOffset].w));

            FixOrientation(vertex.Normal);
            FixOrientation(vertex.Tangent);
        }

        for (int i = 0; i < vertexCount; i++)
        {
            indexCount++;

            int index = (i % 3 == 2) ? (-indices[i] - 1) : indices[i];
            indicesArray[i] = i; // index;
        }

        const ofbx::Material* material = nullptr;
        if (fbxMesh->getMaterialCount() > 0)
        {
            if (geom->getMaterials())
                material = fbxMesh->getMaterial(geom->getMaterials()[triangleStart]);
            else
                material = fbxMesh->getMaterial(0);
        }

        mesh_geometry::generate_tangents_bitangents(tempvertices.data(), uint32_t(vertexCount), indicesArray.data(), uint32_t(indicesArray.size()));

        auto mesh = make_mesh_asset(indicesArray, tempvertices);
        mesh->name = fbxMesh->name;
        return mesh;
    }

    glm::mat4 FbxMatrixToLM(const ofbx::Matrix& mat)
    {
        glm::mat4 result;
        for (int32_t i = 0; i < 4; i++)
            for (int32_t j = 0; j < 4; j++)
                result[i][j] = (float)mat.m[i * 4 + j];
        return result;
    }

    glm::mat4 GetOffsetMatrix(const ofbx::Mesh* mesh, const ofbx::Object* node)
    {
        auto* skin = mesh ? mesh->getGeometry()->getSkin() : nullptr;
        if (skin)
        {
            for (int i = 0, c = skin->getClusterCount(); i < c; i++)
            {
                const ofbx::Cluster* cluster = skin->getCluster(i);
                if (cluster->getLink() == node)
                {
                    return FbxMatrixToLM(cluster->getTransformLinkMatrix());
                }
            }
        }
        return FbxMatrixToLM(node->getGlobalTransform());
    }

    bool ModelAsset::load_fbx(const std::string& path)
    {
        DS_PROFILE_FUNCTION();
        std::string err;
        std::string pathCopy = path;
        stringutility::back_slashes_2_slashes(pathCopy);
        m_FBXModelDirectory = pathCopy.substr(0, pathCopy.find_last_of('/'));

        std::string name = m_FBXModelDirectory.substr(m_FBXModelDirectory.find_last_of('/') + 1);

        std::string ext = stringutility::get_file_extension(path);
        int64_t size = FileSystem::get_file_size(path);
        auto data = FileSystem::read_file(path);

        if (data == nullptr)
        {
            DS_LOG_WARN("Failed to load fbx file");
            return false;
        }
        const bool ignoreGeometry = false;
        const uint64_t flags = ignoreGeometry ? (uint64_t)ofbx::LoadFlags::IGNORE_GEOMETRY : (uint64_t)ofbx::LoadFlags::TRIANGULATE;

        ofbx::IScene* scene = ofbx::load(data, uint32_t(size), flags);
        const ofbx::GlobalSettings* settings = scene->getGlobalSettings();

        err = ofbx::getError();

        if (!err.empty() || !scene)
        {
            DS_LOG_CRITICAL(err);
            return false;
        }

        switch (settings->UpAxis)
        {
        case ofbx::UpVector_AxisX:
            orientation = Orientation::X_UP;
            break;
        case ofbx::UpVector_AxisY:
            orientation = Orientation::Y_UP;
            break;
        case ofbx::UpVector_AxisZ:
            orientation = Orientation::Z_UP;
            break;
        }

        int meshCount = scene->getMeshCount();
        std::mutex slot_mutex;
        parallel_for<size_t>(0, meshCount, [this, scene, &slot_mutex](size_t i){
            const ofbx::Mesh* fbxMesh = (const ofbx::Mesh*)scene->getMesh(i);

            const auto geometry = fbxMesh->getGeometry();
            const auto trianglesCount = geometry->getVertexCount() / 3;

            if (IsMeshInvalid(fbxMesh))
                return;

            auto add_loaded_mesh = [&](const std::shared_ptr<MeshAsset>& mesh, int32_t triangle_start) {
                if (!mesh)
                    return;
                std::shared_ptr<MaterialAsset> mat = create_default_material();
                if (fbxMesh->getMaterialCount() > 0)
                {
                    const ofbx::Material* fbx_mat = nullptr;
                    if (geometry->getMaterials())
                        fbx_mat = fbxMesh->getMaterial(geometry->getMaterials()[triangle_start]);
                    else
                        fbx_mat = fbxMesh->getMaterial(0);
                    if (fbx_mat)
                        mat = LoadMaterial(fbx_mat, false);
                }
                std::lock_guard<std::mutex> lock(slot_mutex);
                add_slot(mesh, mat);
            };

            if (fbxMesh->getMaterialCount() < 2 || !geometry->getMaterials())
            {
                add_loaded_mesh(LoadMesh(fbxMesh, 0, trianglesCount - 1), 0);
            }
            else
            {
                const auto materials = geometry->getMaterials();
                int32_t rangeStart = 0;
                int32_t rangeStartMaterial = materials[rangeStart];
                for (int32_t triangleIndex = 1; triangleIndex < trianglesCount; triangleIndex++)
                {
                    if (rangeStartMaterial != materials[triangleIndex])
                    {
                        add_loaded_mesh(LoadMesh(fbxMesh, rangeStart, triangleIndex - 1), rangeStart);
                        rangeStart = triangleIndex;
                        rangeStartMaterial = materials[triangleIndex];
                    }
                }
                add_loaded_mesh(LoadMesh(fbxMesh, rangeStart, trianglesCount - 1), rangeStart);
            }
        });
        return !slots.empty();
    }

}
