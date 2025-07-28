#if defined(__GNUC__) && defined(_DEBUG) && defined(__OPTIMIZE__)
#warning "Undefing __OPTIMIZE__"
#undef __OPTIMIZE__
#endif

#include "assets/mesh_model.h"
#include "assets/mesh.h"
#include "assets/material.h"
#include "animation/skeleton.h"
#include "animation/animation.h"
#include "backend/drs_rhi/gpu_texture.h"
#include "maths/maths_basic_types.h"
#include "maths/transform.h"
#include "utility/string_utils.h"
#include "assets/asset_manager.h"
#include "core/profiler.h"
#include "core/hash_map.h"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14

#ifdef DS_PRODUCTION
#define TINYGLTF_NOEXCEPTION
#endif
#define TINYGLTF_NO_FS
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <ModelLoaders/tinygltf/tiny_gltf.h>
#include <image_utils.h>

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz-animation/src/animation/offline/gltf/gltf2ozz.cc>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <utility/thread_pool.h>

namespace tinygltf
{

    bool FileExists(const std::string& abs_filename, void*) {
        return std::filesystem::exists(abs_filename);
    }

    std::string ExpandFilePath(const std::string& filepath, void*) {
#ifdef _WIN32
        DWORD len = ExpandEnvironmentStringsA(filepath.c_str(), NULL, 0);
        char* str = new char[len];
        ExpandEnvironmentStringsA(filepath.c_str(), str, len);

        std::string s(str);

        delete[] str;

        return s;
#else
#endif
    }

    bool ReadWholeFile(std::vector<unsigned char>* out, std::string* err,
        const std::string& filepath, void*) {
        size_t size;
        return diverse::readByteData(filepath, *out, size);
    }

    bool WriteWholeFile(std::string* err, const std::string& filepath,
        const std::vector<unsigned char>& contents, void*) {
        return diverse::writeData(filepath, contents.data(), contents.size());
    }
    bool LoadImageData(Image* image, const int image_idx, std::string* err,
        std::string* warn, int req_width, int req_height,
        const unsigned char* bytes, int size, void* userdata)
    {
        return true;
    }

    bool WriteImageData(const std::string* basepath, const std::string* filename,
        Image* image, bool embedImages, void*)
    {
        assert(0); // TODO
        return false;
    }
}

namespace diverse
{
    std::string AlbedoTexName   = "baseColorTexture";
    std::string NormalTexName   = "normalTexture";
    std::string MetallicTexName = "metallicRoughnessTexture";
    std::string GlossTexName    = "metallicRoughnessTexture";
    std::string AOTexName       = "occlusionTexture";
    std::string EmissiveTexName = "emissiveTexture";

    struct GLTFTexture
    {
        tinygltf::Image* Image;
        tinygltf::Sampler* Sampler;
    };

    static HashMap(int, int) GLTF_COMPONENT_LENGTH_LOOKUP;
    static HashMap(int, int) GLTF_COMPONENT_BYTE_SIZE_LOOKUP;
    static bool HashMapsInitialised = false;

    std::vector<SharedPtr<Material>> LoadMaterials(tinygltf::Model& gltfModel,const std::string& basePath)
    {
        DS_PROFILE_FUNCTION();
        std::vector<SharedPtr<Material>> loadedMaterials;
        loadedMaterials.reserve(gltfModel.materials.size());
        bool animated = false;
        if(gltfModel.skins.size() >= 0)
        {
            animated = true;
        }

        auto TextureName = [&](int index)
        {
            if(index >= 0)
            {
                const tinygltf::Texture& tex = gltfModel.textures[index];
                if (tex.source != -1)
                {
                    auto Image = &gltfModel.images.at(tex.source);
                    auto image_path = std::filesystem::path(Image->uri);
                    if(image_path.empty())
						return SharedPtr<asset::Texture>();
                    if (image_path.is_relative())
                        image_path = std::filesystem::path(basePath) / image_path;
                    auto texture = ResourceManager<asset::Texture>::get().get_resource(image_path.string());
                    return texture;
                }
            }
            return SharedPtr<asset::Texture>();
        };
        auto texture_transform_to_matrix  = [](float r, const glm::vec2 & s, const glm::vec2 & o) -> std::array<f32, 6>
        {
            return {
                std::cos(r) * s[0],
                std::sin(r) * s[1],
                -std::sin(r) * s[0],
                std::cos(r) * s[1],
                o[0],
                o[1]
            };
        };

        for(tinygltf::Material& mat : gltfModel.materials)
        {
            SharedPtr<Material> pbrMaterial = createSharedPtr<Material>();
            PBRMataterialTextures textures;
            MaterialProperties properties;

            const tinygltf::PbrMetallicRoughness& pbr = mat.pbrMetallicRoughness;
            textures.albedo                           = TextureName(pbr.baseColorTexture.index);
            textures.normal                           = TextureName(mat.normalTexture.index);
            textures.ao                               = TextureName(mat.occlusionTexture.index);
            textures.emissive                         = TextureName(mat.emissiveTexture.index);
            textures.metallic                         = TextureName(pbr.metallicRoughnessTexture.index);
            if (textures.metallic)
                properties.work_flow = PBR_WORKFLOW_METALLIC_ROUGHNESS;
            else
                properties.work_flow = PBR_WORKFLOW_SEPARATE_TEXTURES;

            // metallic-roughness workflow:
            auto baseColourFactor = mat.values.find("baseColorFactor");
            auto roughnessFactor  = mat.values.find("roughnessFactor");
            auto metallicFactor   = mat.values.find("metallicFactor");
            auto emissiveFactor = mat.values.find("emissiveFactor");
            auto alphaCutoff = mat.values.find("alphaCutoff");
            auto alphaMode = mat.values.find("alphaMode");

            if(roughnessFactor != mat.values.end())
            {
                properties.roughness = static_cast<float>(roughnessFactor->second.Factor());
            }

            if(metallicFactor != mat.values.end())
            {
                properties.metalness = static_cast<float>(metallicFactor->second.Factor());
            }

            if(baseColourFactor != mat.values.end())
            {
                properties.albedoColour = glm::vec4((float)baseColourFactor->second.ColorFactor()[0], (float)baseColourFactor->second.ColorFactor()[1], (float)baseColourFactor->second.ColorFactor()[2], 1.0f);
                if(baseColourFactor->second.ColorFactor().size() > 3)
                    properties.albedoColour.w = (float)baseColourFactor->second.ColorFactor()[3];
            }
            if(emissiveFactor != mat.values.end())
            {
                properties.emissive = glm::vec3((float)emissiveFactor->second.ColorFactor()[0], (float)emissiveFactor->second.ColorFactor()[1], (float)emissiveFactor->second.ColorFactor()[2]);
            }

            // Extensions
            auto metallicGlossinessWorkflow = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
            if(metallicGlossinessWorkflow != mat.extensions.end())
            {
                if(metallicGlossinessWorkflow->second.Has("diffuseTexture"))
                {
                    int index       = metallicGlossinessWorkflow->second.Get("diffuseTexture").Get("index").Get<int>();
                    auto tex = gltfModel.textures[index];
                    auto img_path = std::filesystem::path(basePath) / gltfModel.images[tex.source].uri;
                    textures.albedo = ResourceManager<asset::Texture>::get().get_resource(img_path.string());
                    if (tex.extensions.count("KHR_texture_transform"))
                    {
                        auto offset_json = tex.extensions["KHR_texture_transform"].Get("offset");
                        auto scale_json = tex.extensions["KHR_texture_transform"].Get("scale");
                        auto offset = glm::vec2(offset_json.Get(0).Get<double>(), offset_json.Get(1).Get<double>());
                        auto scale = glm::vec2(scale_json.Get(0).Get<double>(), scale_json.Get(1).Get<double>());
                        auto rotation = tex.extensions["KHR_texture_transform"].Get("rotation").Get<double>();
                        properties.map_transforms[0] = texture_transform_to_matrix(rotation, scale, offset);
                    }
                }

                if(metallicGlossinessWorkflow->second.Has("metallicGlossinessTexture"))
                {
                    int index           = metallicGlossinessWorkflow->second.Get("metallicGlossinessTexture").Get("index").Get<int>();
                    auto tex = gltfModel.textures[index];
                    auto img_path = std::filesystem::path(basePath) / gltfModel.images[tex.source].uri;
                    textures.metallic  = ResourceManager<asset::Texture>::get().get_resource(img_path.string());
                    if (tex.extensions.count("KHR_texture_transform"))
                    {
                        auto offset_json = tex.extensions["KHR_texture_transform"].Get("offset");
                        auto scale_json = tex.extensions["KHR_texture_transform"].Get("scale");
                        auto offset = glm::vec2(offset_json.Get(0).Get<double>(), offset_json.Get(1).Get<double>());
                        auto scale = glm::vec2(scale_json.Get(0).Get<double>(), scale_json.Get(1).Get<double>());
                        auto rotation = tex.extensions["KHR_texture_transform"].Get("rotation").Get<double>();
                        properties.map_transforms[1] = texture_transform_to_matrix(rotation, scale, offset);
                    }
                }

                if(metallicGlossinessWorkflow->second.Has("diffuseFactor"))
                {
                    auto& factor              = metallicGlossinessWorkflow->second.Get("diffuseFactor");
                    properties.albedoColour.x = factor.ArrayLen() > 0 ? float(factor.Get(0).IsNumber() ? factor.Get(0).Get<double>() : factor.Get(0).Get<int>()) : 1.0f;
                    properties.albedoColour.y = factor.ArrayLen() > 1 ? float(factor.Get(1).IsNumber() ? factor.Get(1).Get<double>() : factor.Get(1).Get<int>()) : 1.0f;
                    properties.albedoColour.z = factor.ArrayLen() > 2 ? float(factor.Get(2).IsNumber() ? factor.Get(2).Get<double>() : factor.Get(2).Get<int>()) : 1.0f;
                    properties.albedoColour.w = factor.ArrayLen() > 3 ? float(factor.Get(3).IsNumber() ? factor.Get(3).Get<double>() : factor.Get(3).Get<int>()) : 1.0f;
                }
                if(metallicGlossinessWorkflow->second.Has("metallicFactor"))
                {
                    auto& factor        = metallicGlossinessWorkflow->second.Get("metallicFactor");
                    properties.metalness = factor.ArrayLen() > 0 ? float(factor.Get(0).IsNumber() ? factor.Get(0).Get<double>() : factor.Get(0).Get<int>()) : 1.0f;
                }
                if(metallicGlossinessWorkflow->second.Has("glossinessFactor"))
                {
                    auto& factor         = metallicGlossinessWorkflow->second.Get("glossinessFactor");
                    properties.roughness = 1.0f - float(factor.IsNumber() ? factor.Get<double>() : factor.Get<int>());
                }
            }
            if (mat.normalTexture.extensions.count("KHR_texture_transform"))
            {
                auto offset_json = mat.normalTexture.extensions["KHR_texture_transform"].Get("offset");
                auto scale_json = mat.normalTexture.extensions["KHR_texture_transform"].Get("scale");
                auto offset = glm::vec2(offset_json.Get(0).Get<double>(), offset_json.Get(1).Get<double>());
                auto scale = glm::vec2(scale_json.Get(0).Get<double>(), scale_json.Get(1).Get<double>());
                auto rotation = mat.normalTexture.extensions["KHR_texture_transform"].Get("rotation").Get<double>();
                properties.map_transforms[3] = texture_transform_to_matrix(rotation, scale, offset);
            }
            if (mat.emissiveTexture.extensions.count("KHR_texture_transform"))
            {
                auto offset_json = mat.emissiveTexture.extensions["KHR_texture_transform"].Get("offset");
                auto scale_json = mat.emissiveTexture.extensions["KHR_texture_transform"].Get("scale");
                auto offset = glm::vec2(offset_json.Get(0).Get<double>(), offset_json.Get(1).Get<double>());
                auto scale = glm::vec2(scale_json.Get(0).Get<double>(), scale_json.Get(1).Get<double>());
                auto rotation = mat.emissiveTexture.extensions["KHR_texture_transform"].Get("rotation").Get<double>();
                properties.map_transforms[3] = texture_transform_to_matrix(rotation, scale, offset);
            }
            if (mat.occlusionTexture.extensions.count("KHR_texture_transform"))
            {
                auto offset_json = mat.occlusionTexture.extensions["KHR_texture_transform"].Get("offset");
                auto scale_json = mat.occlusionTexture.extensions["KHR_texture_transform"].Get("scale");
                auto offset = glm::vec2(offset_json.Get(0).Get<double>(), offset_json.Get(1).Get<double>());
                auto scale = glm::vec2(scale_json.Get(0).Get<double>(), scale_json.Get(1).Get<double>());
                auto rotation = mat.occlusionTexture.extensions["KHR_texture_transform"].Get("rotation").Get<double>();
                properties.map_transforms[4] = texture_transform_to_matrix(rotation, scale, offset);
            }
            auto ext_ior = mat.extensions.find("KHR_materials_ior");
            if(ext_ior != mat.extensions.end())
            {
                // https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_ior

                if(ext_ior->second.Has("ior"))
                {
                    auto& factor = ext_ior->second.Get("ior");
                    float ior    = float(factor.IsNumber() ? factor.Get<double>() : factor.Get<int>());

                    properties.reflectance = std::sqrt(std::pow((ior - 1.0f) / (ior + 1.0f), 2.0f) / 16.0f);
                }
            }

            pbrMaterial->set_textures(textures);
            pbrMaterial->set_material_properites(properties);
            pbrMaterial->set_name(mat.name);

            if(mat.doubleSided)
                pbrMaterial->set_render_flag(Material::RenderFlags::TWOSIDED);

            if(mat.alphaMode != "OPAQUE")
                pbrMaterial->set_render_flag(Material::RenderFlags::ALPHABLEND);

            loadedMaterials.push_back(pbrMaterial);
        }

        return loadedMaterials;
    }

    std::vector<Mesh*> LoadMesh(const tinygltf::Model& model,const tinygltf::Mesh& mesh,const maths::Transform& parentTransform)
    {
        std::vector<Mesh*> meshes;

        for(auto& primitive : mesh.primitives)
        {
            std::vector<Vertex> vertices;
            std::vector<AnimVertex> animVertices;

            uint32_t vertexCount = (uint32_t)(primitive.attributes.empty() ? 0 : model.accessors.at(primitive.attributes.at("POSITION")).count);

            bool hasNormals    = false;
            bool hasTangents   = false;
            bool hasBitangents = false;
            bool hasWeights    = false;
            bool hasJoints     = false;

            if(primitive.attributes.find("NORMAL") != primitive.attributes.end())
                hasNormals = true;
            if(primitive.attributes.find("TANGENT") != primitive.attributes.end())
                hasTangents = true;
            if(primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
                hasJoints = true;
            if(primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
                hasWeights = true;
            if(primitive.attributes.find("BITANGENT") != primitive.attributes.end())
                hasBitangents = true;

            if(hasJoints || hasWeights)
                animVertices.resize(vertexCount);

            vertices.resize(vertexCount);

#define VERTEX(i, member) (hasWeights ? vertices[i].member : animVertices[i].member)

            for(auto& attribute : primitive.attributes)
            {
                // Get accessor info
                auto& accessor   = model.accessors.at(attribute.second);
                auto& bufferView = model.bufferViews.at(accessor.bufferView);
                auto& buffer     = model.buffers.at(bufferView.buffer);
                // int componentLength       = GLTF_COMPONENT_LENGTH_LOOKUP.at(accessor.type);
                // int componentTypeByteSize = GLTF_COMPONENT_BYTE_SIZE_LOOKUP.at(accessor.componentType);

                int componentLength       = 0; // GLTF_COMPONENT_LENGTH_LOOKUP.at(indexAccessor.type);
                int componentTypeByteSize = 0; // GLTF_COMPONENT_BYTE_SIZE_LOOKUP.at(indexAccessor.componentType);

                HashMapFind(&GLTF_COMPONENT_LENGTH_LOOKUP, accessor.type, &componentLength);
                HashMapFind(&GLTF_COMPONENT_BYTE_SIZE_LOOKUP, accessor.componentType, &componentTypeByteSize);

                int stride = accessor.ByteStride(bufferView);

                // Extra vertex data from buffer
                size_t bufferOffset = bufferView.byteOffset + accessor.byteOffset;
                int bufferLength    = static_cast<int>(accessor.count) * componentLength * componentTypeByteSize;
                // auto first                = buffer.data.begin() + bufferOffset;
                // auto last                 = buffer.data.begin() + bufferOffset + bufferLength;

                std::vector<uint8_t> data = std::vector<uint8_t>();
                data.resize(bufferLength);
                uint8_t* arrayData = data.data();
                memcpy(arrayData, buffer.data.data() + bufferOffset, bufferLength);

                // -------- Position attribute -----------

                if(attribute.first == "POSITION")
                {
                    size_t positionCount            = accessor.count;
                    glm::vec3* positions = reinterpret_cast<glm::vec3*>(data.data());
                    //for(auto p = 0; p < positionCount; ++p)
                    parallel_for<size_t>(0, positionCount, [&](size_t p)
                    {
                        vertices[p].Position = parentTransform.get_world_matrix() * glm::vec4(positions[p],1.0f);
                        DS_ASSERT(!glm::isinf(vertices[p].Position.x) && !glm::isinf(vertices[p].Position.y) && !glm::isinf(vertices[p].Position.z) && 
                        !glm::isnan(vertices[p].Position.x) && !glm::isnan(vertices[p].Position.y) && !glm::isnan(vertices[p].Position.z));
                    });
                }

                // -------- Normal attribute -----------

                else if(attribute.first == "NORMAL")
                {
                    size_t normalCount            = accessor.count;
                    glm::vec3* normals = reinterpret_cast<glm::vec3*>(data.data());
                    //for(auto p = 0; p < normalCount; ++p)
                    parallel_for<size_t>(0, normalCount, [&](size_t p)
                    {
                        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(parentTransform.get_world_matrix())));
                        vertices[p].Normal = normalMatrix * normals[p];
                        DS_ASSERT(!glm::isinf(vertices[p].Normal.x) && !glm::isinf(vertices[p].Normal.y) && !glm::isinf(vertices[p].Normal.z) && 
                        !glm::isnan(vertices[p].Normal.x) && !glm::isnan(vertices[p].Normal.y) && !glm::isnan(vertices[p].Normal.z));
                    });
                }

                // -------- Texcoord attribute -----------

                else if(attribute.first == "TEXCOORD_0")
                {
                    size_t uvCount            = accessor.count;
                    glm::vec2* uvs = reinterpret_cast<glm::vec2*>(data.data());
                    for(auto p = 0; p < uvCount; ++p)
                    {
                        vertices[p].TexCoords = uvs[p];
                    }
                }

                // -------- Colour attribute -----------

                else if(attribute.first == "COLOR_0")
                {
                    size_t uvCount                = accessor.count;
                    glm::vec4* colours = reinterpret_cast<glm::vec4*>(data.data());
                    for(auto p = 0; p < uvCount; ++p)
                    {
                        vertices[p].Colours = colours[p];
                    }
                }

                // -------- Tangent attribute -----------

                else if(attribute.first == "TANGENT")
                {
                    hasTangents               = true;
                    size_t uvCount            = accessor.count;
                    glm::vec3* uvs = reinterpret_cast<glm::vec3*>(data.data());
                    parallel_for<size_t>(0, uvCount, [&](size_t p)
                    {
                        vertices[p].Tangent = glm::transpose(glm::inverse(glm::mat3(parentTransform.get_world_matrix()))) * uvs[p];
                        DS_ASSERT(!glm::isinf(vertices[p].Tangent.x) && !glm::isinf(vertices[p].Tangent.y) && !glm::isinf(vertices[p].Tangent.z) && 
                        !glm::isnan(vertices[p].Tangent.x) && !glm::isnan(vertices[p].Tangent.y) && !glm::isnan(vertices[p].Tangent.z));
                    });
                }

                else if(attribute.first == "BINORMAL")
                {
                    hasBitangents             = true;
                    size_t uvCount            = accessor.count;
                    glm::vec3* uvs = reinterpret_cast<glm::vec3*>(data.data());
                    //for(auto p = 0; p < uvCount; ++p)
                    parallel_for<size_t>(0, uvCount, [&](size_t p)
                    {
                        vertices[p].Bitangent = glm::transpose(glm::inverse(glm::mat3(parentTransform.get_world_matrix()))) * uvs[p];
                        DS_ASSERT(!glm::isinf(vertices[p].Bitangent.x) && !glm::isinf(vertices[p].Bitangent.y) && !glm::isinf(vertices[p].Bitangent.z) && !glm::isnan(vertices[p].Bitangent.x) && !glm::isnan(vertices[p].Bitangent.y) && !glm::isnan(vertices[p].Bitangent.z));
                    });
                }

                // -------- Weights attribute -----------

                else if(attribute.first == "WEIGHTS_0")
                {
                    hasWeights = true;

                    size_t weightCount = accessor.count;

                    if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                    {
                        maths::Vector4Simple* weights = reinterpret_cast<maths::Vector4Simple*>(data.data());
                        for(auto p = 0; p < weightCount; ++p)
                        {
                            animVertices[p].Weights[0] = weights[p].x;
                            animVertices[p].Weights[1] = weights[p].y;
                            animVertices[p].Weights[2] = weights[p].z;
                            animVertices[p].Weights[3] = weights[p].w;
                        }
                    }
                    else if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        for(auto i = 0; i < weightCount; ++i)
                        {
                            const uint8_t& x = *(uint8_t*)((size_t)data.data() + i * stride + 0 * sizeof(uint16_t));
                            const uint8_t& y = *(uint8_t*)((size_t)data.data() + i * stride + 1 * sizeof(uint16_t));
                            const uint8_t& z = *(uint8_t*)((size_t)data.data() + i * stride + 2 * sizeof(uint16_t));
                            const uint8_t& w = *(uint8_t*)((size_t)data.data() + i * stride + 3 * sizeof(uint16_t));

                            animVertices[i].Weights[0] = (float)x / 65535.0f;
                            animVertices[i].Weights[1] = (float)y / 65535.0f;
                            animVertices[i].Weights[2] = (float)z / 65535.0f;
                            animVertices[i].Weights[3] = (float)w / 65535.0f;
                        }
                    }
                    else if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        for(auto i = 0; i < weightCount; ++i)
                        {
                            const uint8_t& x = *(uint8_t*)((size_t)data.data() + i * stride + 0);
                            const uint8_t& y = *(uint8_t*)((size_t)data.data() + i * stride + 1);
                            const uint8_t& z = *(uint8_t*)((size_t)data.data() + i * stride + 2);
                            const uint8_t& w = *(uint8_t*)((size_t)data.data() + i * stride + 3);

                            animVertices[i].Weights[0] = (float)x / 255.0f;
                            animVertices[i].Weights[1] = (float)y / 255.0f;
                            animVertices[i].Weights[2] = (float)z / 255.0f;
                            animVertices[i].Weights[3] = (float)w / 255.0f;
                        }
                    }
                    else
                    {
                        DS_LOG_WARN("Unsupported weight data type");
                    }
                }

                // -------- Joints attribute -----------

                else if(attribute.first == "JOINTS_0")
                {
                    hasJoints = true;

                    size_t jointCount = accessor.count;
                    if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        struct JointTmp
                        {
                            uint16_t ind[4];
                        };

                        uint16_t* joints = reinterpret_cast<uint16_t*>(data.data());
                        for(auto p = 0; p < jointCount; ++p)
                        {
                            const JointTmp& joint = *(const JointTmp*)(data.data() + p * stride);

                            animVertices[p].BoneInfoIndices[0] = (uint32_t)joint.ind[0];
                            animVertices[p].BoneInfoIndices[1] = (uint32_t)joint.ind[1];
                            animVertices[p].BoneInfoIndices[2] = (uint32_t)joint.ind[2];
                            animVertices[p].BoneInfoIndices[3] = (uint32_t)joint.ind[3];
                        }
                    }
                    else if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        uint8_t* joints = reinterpret_cast<uint8_t*>(data.data());
                        for(auto p = 0; p < jointCount; ++p)
                        {
                            animVertices[p].BoneInfoIndices[0] = (uint32_t)joints[p * 4];
                            animVertices[p].BoneInfoIndices[1] = (uint32_t)joints[p * 4 + 1];
                            animVertices[p].BoneInfoIndices[2] = (uint32_t)joints[p * 4 + 2];
                            animVertices[p].BoneInfoIndices[3] = (uint32_t)joints[p * 4 + 3];
                        }
                    }
                    else if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    {
                        struct JointTmp
                        {
                            uint32_t ind[4];
                        };

                        for(size_t i = 0; i < jointCount; ++i)
                        {
                            const JointTmp& joint = *(const JointTmp*)(data.data() + i * stride);

                            animVertices[i].BoneInfoIndices[0] = joint.ind[0];
                            animVertices[i].BoneInfoIndices[1] = joint.ind[1];
                            animVertices[i].BoneInfoIndices[2] = joint.ind[2];
                            animVertices[i].BoneInfoIndices[3] = joint.ind[3];
                        }
                    }
                    else
                    {
                        DS_LOG_WARN("Unsupported joint data type");
                    }
                }
            }

            // -------- Indices ----------
            std::vector<uint32_t> indices;
            if(primitive.indices >= 0)
            {
                const tinygltf::Accessor& indicesAccessor = model.accessors[primitive.indices];
                indices.resize(indicesAccessor.count);
                {
                    // Get accessor info
                    auto indexAccessor   = model.accessors.at(primitive.indices);
                    auto indexBufferView = model.bufferViews.at(indexAccessor.bufferView);
                    auto indexBuffer     = model.buffers.at(indexBufferView.buffer);

                    int componentLength       = 0; // GLTF_COMPONENT_LENGTH_LOOKUP.at(indexAccessor.type);
                    int componentTypeByteSize = 0; // GLTF_COMPONENT_BYTE_SIZE_LOOKUP.at(indexAccessor.componentType);

                    HashMapFind(&GLTF_COMPONENT_LENGTH_LOOKUP, indexAccessor.type, &componentLength);
                    HashMapFind(&GLTF_COMPONENT_BYTE_SIZE_LOOKUP, indexAccessor.componentType, &componentTypeByteSize);

                    // Extra index data
                    size_t bufferOffset = indexBufferView.byteOffset + indexAccessor.byteOffset;
                    int bufferLength    = static_cast<int>(indexAccessor.count) * componentLength * componentTypeByteSize;
       
                    std::vector<uint8_t> data = std::vector<uint8_t>();
                    data.resize(bufferLength);
                    uint8_t* arrayData = data.data();
                    MemoryCopy(arrayData, indexBuffer.data.data() + bufferOffset, bufferLength);

                    size_t indicesCount = indexAccessor.count;
                    if(componentTypeByteSize == 1)
                    {
                        uint8_t* in = reinterpret_cast<uint8_t*>(data.data());
                        for(auto iCount = 0; iCount < indicesCount; iCount++)
                        {
                            indices[iCount] = (uint32_t)in[iCount];
                        }
                    }
                    else if(componentTypeByteSize == 2)
                    {
                        uint16_t* in = reinterpret_cast<uint16_t*>(data.data());
                        for(auto iCount = 0; iCount < indicesCount; iCount++)
                        {
                            indices[iCount] = (uint32_t)in[iCount];
                        }
                    }
                    else if(componentTypeByteSize == 4)
                    {
                        auto in = reinterpret_cast<uint32_t*>(data.data());
                        for(auto iCount = 0; iCount < indicesCount; iCount++)
                        {
                            indices[iCount] = in[iCount];
                        }
                    }
                    else
                    {
                        DS_LOG_WARN("Unsupported indices data type - {}", componentTypeByteSize);
                    }
                }
            }
            else
            {
                DS_LOG_WARN("Missing Indices - Generating new");

                const auto& accessor = model.accessors[primitive.attributes.find("POSITION")->second];
                indices.reserve(accessor.count);
                //                for (auto i = 0; i < accessor.count; i++)
                //                       indices.push_back(i);

                for(size_t vi = 0; vi < accessor.count; vi += 3)
                {
                    indices.push_back(uint32_t(vi + 0));
                    indices.push_back(uint32_t(vi + 1));
                    indices.push_back(uint32_t(vi + 2));
                }
            }

            if(!hasNormals)
                Mesh::generate_normals(vertices.data(), uint32_t(vertices.size()), indices.data(), uint32_t(indices.size()));
            if(!hasTangents || !hasBitangents)
                Mesh::generate_tangents_bitangents(vertices.data(), uint32_t(vertices.size()), indices.data(), uint32_t(indices.size()));

            // Add mesh
            Mesh* lMesh;

            if(hasJoints || hasWeights)
            {
                for(size_t i = 0; i < vertices.size(); i++)
                {
                    animVertices[i].normalize_weights();
                    animVertices[i].Position  = vertices[i].Position;
                    animVertices[i].Normal    = vertices[i].Normal;
                    animVertices[i].Colours   = vertices[i].Colours;
                    animVertices[i].Tangent   = vertices[i].Tangent;
                    animVertices[i].Bitangent = vertices[i].Bitangent;
                    animVertices[i].TexCoords = vertices[i].TexCoords;
                }
                lMesh = new Mesh(indices, animVertices);
            }
            else
                lMesh = new Mesh(indices, vertices);

            meshes.emplace_back(lMesh);
        }

        return meshes;
    }

    std::mutex mesh_mutex;
    void LoadNode(MeshModel* mainModel, int nodeIndex, const glm::mat4& parentTransform,const tinygltf::Model& model,const std::vector<SharedPtr<Material>>& materials)
    {
        DS_PROFILE_FUNCTION();
        if(nodeIndex < 0)
            return;

        auto& node = model.nodes[nodeIndex];
        auto name  = node.name;

#ifdef DEBUG_PRINT_GLTF_LOADING
        DS_LOG_INFO("asset.copyright : {}", model.asset.copyright);
        DS_LOG_INFO("asset.generator : {}", model.asset.generator);
        DS_LOG_INFO("asset.version : {}", model.asset.version);
        DS_LOG_INFO("asset.minVersion : {}", model.asset.minVersion);
#endif

        maths::Transform transform;
        glm::mat4 matrix;
        glm::mat4 position = glm::mat4(1.0f);
        glm::mat4 rotation = glm::mat4(1.0f);
        glm::mat4 scale    = glm::mat4(1.0f);

        if(!node.scale.empty())
        {
            scale = glm::scale(glm::mat4(1.0), glm::vec3(static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]), static_cast<float>(node.scale[2])));
            // transform.SetLocalScale(Vec3(static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]), static_cast<float>(node.scale[2])));
        }

        if(!node.rotation.empty())
        {
            rotation = glm::toMat4(glm::quat(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2])));

            // transform.SetLocalOrientation(Quat(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2])));
        }

        if(!node.translation.empty())
        {
            // transform.SetLocalPosition(Vec3(static_cast<float>(node.translation[0]), static_cast<float>(node.translation[1]), static_cast<float>(node.translation[2])));
            position = glm::translate(glm::mat4(1.0), glm::vec3(static_cast<float>(node.translation[0]), static_cast<float>(node.translation[1]), static_cast<float>(node.translation[2])));
        }

        if(!node.matrix.empty())
        {
            float matrixData[16];
            for(int i = 0; i < 16; i++)
                matrixData[i] = float(node.matrix.data()[i]);
            matrix = glm::make_mat4(matrixData);
            transform.set_local_transform(matrix);
        }
        else
        {
            matrix = position * rotation * scale;
            transform.set_local_transform(matrix);
        }

        transform.set_world_matrix(parentTransform);

        if(node.mesh >= 0)
        {
            auto meshes = LoadMesh(model, model.meshes[node.mesh], transform);
            int subIndex = 0;
            for(auto& mesh : meshes)
            {
                auto subname = model.meshes[node.mesh].name;
                auto lMesh   = SharedPtr<Mesh>(mesh);
                lMesh->set_name(subname);

                int materialIndex = model.meshes[node.mesh].primitives[subIndex].material;
                if(materialIndex >= 0)
                    lMesh->set_material(materials[materialIndex]);
                else
                {
                    auto pbrMaterial = createSharedPtr<Material>();
                    pbrMaterial->set_name(subname.empty() ? std::format("mat_{}", subIndex) : subname);
                    lMesh->set_material(pbrMaterial);
                }
                mainModel->add_mesh(lMesh);
                subIndex++;
            }
        }

        if(!node.children.empty())
        {
            for(auto child : node.children)
            {
                LoadNode(mainModel, child, transform.get_local_matrix(), model, materials);
            }
        }
    }

    glm::mat4 ConvertToGLM2(const ozz::math::Float4x4& ozzMat)
    {
        glm::mat4 glmMat;

        // Assuming ozz::math::Float4x4 is column-major
        for(int r = 0; r < 4; r++)
        {
            float result[4];
            memcpy(result, ozzMat.cols + r, sizeof(ozzMat.cols[r]));
            float dresult[4];
            for(int j = 0; j < 4; j++)
            {
                dresult[j] = result[j];
            }
            //_mm_store_ps(result,p.cols[r]);
            auto matrix = glm::value_ptr(glmMat);
            memcpy(matrix + r * 4, dresult, sizeof(dresult));
        }

        return glmMat;
    }

    bool MeshModel::load_gltf(const std::string& path)
    {
        DS_PROFILE_FUNCTION();

        if(!HashMapsInitialised)
        {
            HashMapInit(&GLTF_COMPONENT_LENGTH_LOOKUP);
            HashMapInit(&GLTF_COMPONENT_BYTE_SIZE_LOOKUP);

            int key   = (int)TINYGLTF_TYPE_SCALAR;
            int value = 1;
            {
                HashMapInsert(&GLTF_COMPONENT_LENGTH_LOOKUP, key, value);
                key   = (int)TINYGLTF_TYPE_VEC2;
                value = 2;
                HashMapInsert(&GLTF_COMPONENT_LENGTH_LOOKUP, key, value);

                key   = (int)TINYGLTF_TYPE_VEC3;
                value = 3;
                HashMapInsert(&GLTF_COMPONENT_LENGTH_LOOKUP, key, value);

                key   = (int)TINYGLTF_TYPE_VEC4;
                value = 4;
                HashMapInsert(&GLTF_COMPONENT_LENGTH_LOOKUP, key, value);

                key   = (int)TINYGLTF_TYPE_MAT2;
                value = 4;
                HashMapInsert(&GLTF_COMPONENT_LENGTH_LOOKUP, key, value);

                key   = (int)TINYGLTF_TYPE_MAT3;
                value = 9;
                HashMapInsert(&GLTF_COMPONENT_LENGTH_LOOKUP, key, value);

                key   = (int)TINYGLTF_TYPE_MAT4;
                value = 16;
                HashMapInsert(&GLTF_COMPONENT_LENGTH_LOOKUP, key, value);
            }

            {
                key   = (int)TINYGLTF_COMPONENT_TYPE_BYTE;
                value = 1;
                HashMapInsert(&GLTF_COMPONENT_BYTE_SIZE_LOOKUP, key, value);

                key   = (int)TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                value = 1;
                HashMapInsert(&GLTF_COMPONENT_BYTE_SIZE_LOOKUP, key, value);

                key   = (int)TINYGLTF_COMPONENT_TYPE_SHORT;
                value = 2;
                HashMapInsert(&GLTF_COMPONENT_BYTE_SIZE_LOOKUP, key, value);

                key   = (int)TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                value = 2;
                HashMapInsert(&GLTF_COMPONENT_BYTE_SIZE_LOOKUP, key, value);

                key   = (int)TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                value = 4;
                HashMapInsert(&GLTF_COMPONENT_BYTE_SIZE_LOOKUP, key, value);

                key   = (int)TINYGLTF_COMPONENT_TYPE_FLOAT;
                value = 4;
                HashMapInsert(&GLTF_COMPONENT_BYTE_SIZE_LOOKUP, key, value);
            }

            HashMapsInitialised = true;
        }

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        std::string ext = stringutility::get_file_extension(path);
        tinygltf::FsCallbacks callbacks;
        callbacks.ReadWholeFile = tinygltf::ReadWholeFile;
        callbacks.WriteWholeFile = tinygltf::WriteWholeFile;
        callbacks.FileExists = tinygltf::FileExists;
        callbacks.ExpandFilePath = tinygltf::ExpandFilePath;
        loader.SetFsCallbacks(callbacks);
        loader.SetImageLoader(tinygltf::LoadImageData, nullptr);
        loader.SetImageWriter(tinygltf::WriteImageData, nullptr);

        bool ret;

        std::vector<u8> file_data;
        size_t d_size;
        ret = readByteData(path, file_data, d_size);
        std::string basedir;
        if (ret)
        {
            basedir = tinygltf::GetBaseDir(path);
            if (ext == "glb") // assume binary glTF.
            {
                DS_PROFILE_SCOPE(".glb binary loading");
                ret = loader.LoadBinaryFromMemory(
                    &model,
                    &err,
                    &warn,
                    file_data.data(),
                    static_cast<unsigned int>(file_data.size()),
                    basedir
                );
            }
            else // assume ascii glTF.
            {
                DS_PROFILE_SCOPE(".gltf loading");
                ret = loader.LoadASCIIFromString(
                    &model,
                    &err,
                    &warn,
                    reinterpret_cast<const char*>(file_data.data()),
                    static_cast<unsigned int>(file_data.size()),
                    basedir
                );
            }
        }
       
        if(!err.empty())
        {
            DS_LOG_ERROR(err.c_str());
            return false;
        }

        if(!warn.empty())
        {
            DS_LOG_ERROR(warn.c_str());
        }

        if(!ret || model.defaultScene < 0 || model.scenes.empty())
        {
            DS_LOG_ERROR("Failed to parse glTF");
            return false;
        }
        {
            DS_PROFILE_SCOPE("Parse GLTF Model");

            auto LoadedMaterials = LoadMaterials(model,basedir);

            std::string name = path.substr(path.find_last_of('/') + 1);

            const tinygltf::Scene& gltfScene = model.scenes[diverse::maths::Max(0, model.defaultScene)];
            for(size_t i = 0; i < gltfScene.nodes.size(); i++)
            //parallel_for<size_t>(0, gltfScene.nodes.size(), [&](size_t i)
            {
                LoadNode(this, gltfScene.nodes[i], glm::mat4(1.0f), model, LoadedMaterials);
            }

            auto skins = model.skins;
            if(!skins.empty() || !model.animations.empty())
            {
                using namespace ozz::animation::offline;
                GltfImporter impl;
                ozz::animation::offline::OzzImporter& importer = impl;
                OzzImporter::NodeType types                    = {};

                importer.Load(path.c_str());
                RawSkeleton* rawSkeleton = new RawSkeleton();
                importer.Import(rawSkeleton, types);

                ozz::animation::offline::SkeletonBuilder skeletonBuilder;

                skeleton = createSharedPtr<Skeleton>(skeletonBuilder(*rawSkeleton).release());
                if(skeleton->valid())
                {
                    ozz::vector<ozz::math::Float4x4> bindposes_ozz;
                    bind_poses.reserve(skeleton->get_skeleton().joint_names().size());
                    bindposes_ozz.resize(skeleton->get_skeleton().joint_names().size());

                    // convert from local space to model space
                    ozz::animation::LocalToModelJob job;
                    job.skeleton = &skeleton->get_skeleton();
                    job.input    = ozz::span(skeleton->get_skeleton().joint_rest_poses());
                    job.output   = ozz::make_span(bindposes_ozz); //, m_Skeleton->GetSkeleton().joint_names().size_bytes());

                    if(!job.Run())
                    {
                        DS_LOG_ERROR("Failed to run ozz LocalToModelJob");
                    }

                    for(auto& pose : bindposes_ozz)
                    {
                        bind_poses.push_back(glm::inverse(ConvertToGLM2(pose)));
                    }

                    ozz::animation::offline::AnimationBuilder animBuilder;
                    auto animationNames = importer.GetAnimationNames();

                    for(auto& animName : animationNames)
                    {
                        RawAnimation* rawAnimation = new RawAnimation();
                        importer.Import(animName.c_str(), skeleton->get_skeleton(), 30.0f, rawAnimation);

                        animation.push_back(createSharedPtr<Animation>(std::string(animName.c_str()), animBuilder(*rawAnimation).release(), skeleton));

                        delete rawAnimation;
                    }
                }

                delete rawSkeleton;
            }
        }
        return true;
    }
}
