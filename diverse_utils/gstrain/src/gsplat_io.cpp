#include "gsplat_io.hpp"
#include <filesystem>
#include <format>
#include <glm/glm.hpp>
#include <tiny_gsplat.hpp>
#include "gsplat_cluster.hpp"
#if defined(USE_HIP) || defined(USE_CUDA)
#include <rasterizer/gsplat-cuda/ops.h>
#endif
#if defined(USE_MPS)
#include <rasterizer/gsplat-metal/ops.h>
#endif

extern torch::Device device;

auto load_gsplat_model(const std::string& filepath,
    torch::Tensor& means,
    torch::Tensor& opacities,
    torch::Tensor& scales,
    torch::Tensor& quats,
    torch::Tensor& featuresDc,
    torch::Tensor& featuresRest,
    bool& mip_antialiased)->void{
    std::vector<tinygsplat::RichPoint> points;
    auto ext = std::filesystem::path(filepath).extension().string();
    try{
        if (ext == ".ply")
        {
            //compressed
            if (filepath.find(".compressed") != std::string::npos)
                tinygsplat::load_compress_ply(filepath, points,mip_antialiased);
            else if(filepath.find(".reduced") != std::string::npos)
                tinygsplat::load_reduced_ply(filepath, points);
            else
                tinygsplat::load_ply(filepath, points,mip_antialiased);
        }
        else if (ext == ".splat")
        {
            tinygsplat::load_splat(filepath, points);
        }
        else if (filepath.find(".dvsplat") != std::string::npos)
            tinygsplat::load_dvs_splat(filepath, points);
        else if (filepath.find(".spz") != std::string::npos)
        {
            tinygsplat::load_spz_splats(filepath, points,mip_antialiased);
        }
        else if (ext == ".sog")
        {
            tinygsplat::load_sog(filepath, points);
        }
        else {
            std::cout << "unsupport splat file format \n";
            throw std::runtime_error("unsupport splat file format");
        }
    }
    catch (...) {
        std::cout << "load error \n";
    }
       
    auto count = points.size();
    // Resize our SoA data
    struct Shs {
        float shs[48];
    };
    std::vector<glm::vec3> pos(count);
    std::vector<Shs> shs(count);
    std::vector<glm::vec3> scas(count);
    std::vector<float> opacity(count);
    std::vector<glm::vec4> rot(count);

    // Copy data from AoS to SoA
    for (int i = 0; i < count; ++i)
    {
        pos[i] = points[i].pos;
        scas[i] = points[i].scale;
        opacity[i] = points[i].opacity;
        rot[i] = points[i].rot;
        shs[i].shs[0] = points[i].shs[0];
        shs[i].shs[1] = points[i].shs[1];
        shs[i].shs[2] = points[i].shs[2];
        for (int j = 1; j < 16; j++)
        {
            shs[i].shs[j * 3 + 0] = points[i].shs[(j - 1) + 3];
            shs[i].shs[j * 3 + 1] = points[i].shs[(j - 1) + 18];
            shs[i].shs[j * 3 + 2] = points[i].shs[(j - 1) + 33];
        }
    }

    const auto pointType = torch::TensorOptions().dtype(torch::kFloat32);
    means = torch::from_blob((float*)pos.data(), { static_cast<long>(pos.size()), 3 }, pointType).to(device).contiguous().set_requires_grad(true);
    scales = torch::from_blob((float*)scas.data(), { static_cast<long>(scas.size()), 3 }, pointType).to(device).contiguous().set_requires_grad(true);
    quats = torch::from_blob((float*)rot.data(), { static_cast<long>(rot.size()), 4 }, pointType).to(device).contiguous().set_requires_grad(true);
    opacities = torch::from_blob((float*)opacity.data(), { static_cast<long>(opacity.size()), 1 }, pointType).to(device).contiguous().set_requires_grad(true);
    // features
    auto features = torch::from_blob((float*)shs.data(), { static_cast<long>(count), 16, 3}, pointType);
    featuresDc = features.index({ torch::indexing::Slice(), torch::indexing::Slice(0,1), torch::indexing::Slice()}).contiguous().to(device).set_requires_grad(true);
    featuresRest = features.index({ torch::indexing::Slice(),  torch::indexing::Slice(1, torch::indexing::None), torch::indexing::Slice() }).contiguous().to(device).set_requires_grad(true);
}

auto save_splat_models(const std::string& filename,
                       const torch::Tensor& means,
                       const torch::Tensor& opacities,
                       const torch::Tensor& scales,
                       const torch::Tensor& quats,
                       const torch::Tensor& sh0,
                       const torch::Tensor& shn,
                       bool mip_antialiased,
                       bool quantised,
                       bool halfFloat)->void {
    auto p = std::filesystem::path(filename);
    auto folder_path = p.parent_path();
    folder_path = folder_path.empty() ? std::filesystem::current_path() : folder_path;
    auto exist = std::filesystem::exists(folder_path);
    if (!exist)
        exist = std::filesystem::create_directory(folder_path);
    if(!means.defined() || means.size(0) == 0) return;
    torch::Tensor meansCpu = means.cpu();
    torch::Tensor opacitiesCpu = opacities.cpu();
    torch::Tensor scalesCpu = scales.cpu();
    torch::Tensor quatsCpu = quats.cpu();
    // Match Inria's version
    int numPoints = means.size(0);
    auto sh0Cpu = sh0.cpu();
    auto shnCpu = shn.cpu();
    if (exist && meansCpu.size(0) > 0)
    {
        std::vector<glm::vec3> pos(numPoints);
        memcpy(pos.data(), meansCpu.contiguous().data_ptr<float>(), pos.size() * sizeof(glm::vec3));

        std::vector<glm::vec3> scale(numPoints);
        memcpy(scale.data(), scalesCpu.contiguous().data_ptr<float>(), scale.size() * sizeof(glm::vec3));

        std::vector<std::array<float, 3>> shs_0(numPoints);
        memcpy(shs_0.data(), sh0Cpu.contiguous().data_ptr<float>(), shs_0.size() * sizeof(float) * 3);

        std::vector<std::array<float, 45>> shs_n(numPoints);
        memcpy(shs_n.data(), shnCpu.contiguous().data_ptr<float>(), shs_n.size() * sizeof(float) * 45);

        std::vector<glm::vec4> rot(numPoints);
        memcpy(rot.data(), quatsCpu.contiguous().data_ptr<float>(), rot.size() * sizeof(glm::vec4));

        std::vector<float> opacity(numPoints);
        memcpy(opacity.data(), opacitiesCpu.contiguous().data_ptr<float>(), opacity.size() * sizeof(float));
        if (p.extension() == ".ply")
        {
            if (p.string().find(".compressed") != std::string::npos)
            {
                tinygsplat::save_compress_ply(p.string(), pos, scale, shs_0, shs_n, rot, opacity,mip_antialiased);
            }
            else
                tinygsplat::save_ply(p.string(), pos, scale, shs_0, shs_n, rot, opacity,mip_antialiased);
        }
        else if (p.extension() == ".splat")
        {
            tinygsplat::save_splat(p.string(), pos, scale, shs_0, shs_n, rot, opacity);
        }
        else if (p.extension() == ".spz")
        {
            tinygsplat::save_spz_splats(p.string(), pos, scale, shs_0, shs_n, rot, opacity,mip_antialiased);
        }
        else if (p.extension() == ".sog")
        {
            tinygsplat::save_sog(p.string(), pos, scale, shs_0, shs_n, rot, opacity);
        }
        std::cout << "save splat to file " << p.string() << std::endl;
    }
    else
    {
        std::cout << "saved file directory not exist!!\n";
    }
}

