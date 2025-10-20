#include "backend/drs_rhi/buffer_builder.h"
#include "gaussian_model.h"
#include <sstream>
#include <glm/glm.hpp>
#include "utility/pack_utils.h"
#include "utility/file_utils.h"
#include "utility/data_view.h"
#include "utility/sh_utils.h"
#include "core/ds_log.h"
#include <tinygsplat/tiny_gsplat.hpp>
#include "utility/thread_pool.h"
namespace diverse
{
	auto sigmoid = [](const float v) {
		//return 1.0f / (1.0f + exp(-v));
		if (v > 0) {
			return 1 / (1 + exp(-v));
		}

		const float t = exp(v);
		return t / (1 + t);
	};

	GaussianModel::GaussianModel(int max_splats)
		: max_splats(max_splats)
	{
		//set_flag(AssetFlag::Loaded);
	}

	GaussianModel::GaussianModel(const std::string& filePath)
		:file_path(filePath)
	{
		std::thread t([this, filePath]() {
			load_model(filePath);
		});
		t.detach();
	}
	void GaussianModel::update_from_gpu(float* pos, float* shs, float* opacities, float* scales, float* rots)
	{
		
	}
	
	void GaussianModel::update_from_cpu(
						float* pos_d, 
						float* shs_d, 
						float* opacities_d, 
						float* scales_d, 
						float* rots_d,
						int num_gaussians)
	{
		auto device = g_device;
		pos.resize(num_gaussians);
		rot.resize(num_gaussians);
		scales.resize(num_gaussians);
		opacities.resize(num_gaussians);
		shs.resize(num_gaussians);

		memcpy(pos.data(), pos_d, num_gaussians * sizeof(glm::vec3));
		memcpy(rot.data(), rots_d, num_gaussians * sizeof(glm::vec4));
		memcpy(scales.data(), scales_d, num_gaussians * sizeof(glm::vec3));
		memcpy(opacities.data(), opacities_d, num_gaussians * sizeof(f32));
		memcpy(shs.data(), shs_d, num_gaussians * sizeof(f32) * 48);

		update_data();
	}

	void GaussianModel::update_from_pos_color(u8* pos_color_h,int num_gaussians)
	{
		if(!pos_color_h) return;
		pos.resize(num_gaussians);
		rot.resize(num_gaussians);
		scales.resize(num_gaussians);
		opacities.resize(num_gaussians);
		shs.resize(num_gaussians);
		constexpr float C0 = 0.28209479177387814f;

		struct VertexPosColor{
			glm::vec3 pos;
			glm::uvec4 c;
		};
		auto v = (VertexPosColor*)(pos_color_h);
		parallel_for<size_t>(0,num_gaussians,[&](size_t i){
			pos[i][0] = *(float*)(pos_color_h + i * 16 + 0);
			pos[i][1] = *(float*)(pos_color_h + i * 16 + 4);
			pos[i][2] = *(float*)(pos_color_h + i * 16 + 8);
			shs[i][0] = (pos_color_h[i * 16 + 12] / 255.0f - 0.5f) / C0;
			shs[i][1] = (pos_color_h[i * 16 + 13] / 255.0f - 0.5f) / C0;
			shs[i][2] = (pos_color_h[i * 16 + 14] / 255.0f - 0.5f) / C0;
		});
		update_data();
	}
	auto GaussianModel::get_feature_dc_rest(const std::vector<std::array<float,48>>& colors)->std::pair<std::vector<glm::vec3>, std::vector<std::array<glm::vec3,15>>>
	{
		auto num_gaussians = pos.size();
		std::vector<glm::vec3>	featureDc(num_gaussians);
		std::vector<std::array<glm::vec3,15>> featureRest(num_gaussians);
		parallel_for<size_t>(0, num_gaussians, [&](size_t i) {
			featureDc[i][0] = colors[i][0];
			featureDc[i][1] = colors[i][1];
			featureDc[i][2] = colors[i][2];
			for(auto j=0;j<15;j++)
			{
				featureRest[i][j][0] = colors[i][3 + j * 3];
				featureRest[i][j][1] = colors[i][3 + j * 3 + 1];
				featureRest[i][j][2] = colors[i][3 + j * 3 + 2];
			}
		});
		return {featureDc, featureRest};
	}

	void GaussianModel::create_gpu_buffer(bool compact)
	{
		auto device = get_global_device();
		const auto alignment = std::max<u64>(1, device->gpu_limits.minStorageBufferOffsetAlignment);
		std::vector<Gaussian> gaussians(pos.size());
		std::vector<PackedVertexColor>	gaussians_color(pos.size());
		std::vector<PackedVertexSH>	gaussians_sh(pos.size());
		splat_state.resize(pos.size());
		splat_select_flag.resize(pos.size());
		splat_transform_index.resize(pos.size());
		
        const u32 t11 = (1 << 11) - 1;
        const u32 t10 = (1 << 10) - 1;
		constexpr float SH_C0 = 0.28209479177387814f;
		parallel_for<size_t>(0, gaussians.size(), [&](size_t k) {
			glm::vec4 harmonics[16];

			Gaussian& gaussian = gaussians[k];
			glm::uvec2& gs_color = gaussians_color[k];
			// copy position
			gaussian.position.xyz = pos[k];
			//normalize 
			float length2 = 0;
			for (int j = 0; j < 4; j++)
				length2 += rot[k][j] * rot[k][j];
			float length = sqrt(length2);
			glm::vec4 rot_t;
			for (int j = 0; j < 4; j++)
				rot_t[j] = rot[k][j] / length;

			auto rotation0 = glm::packHalf2x16(glm::vec2(rot_t[0], rot_t[1]));
			auto rotation1 = glm::packHalf2x16(glm::vec2(rot_t[2], rot_t[3]));

			glm::vec3 scale_t;
			for (int j = 0; j < 3; j++)
				scale_t[j] = exp(scales[k][j]);
			auto scale0 = glm::packHalf2x16(glm::vec2(scale_t[0], scale_t[1]));
			auto scale1 = glm::packHalf2x16(glm::vec2(scale_t[2], sigmoid(opacities[k])));

			gaussian.rotation_scale = glm::uvec4(rotation0, rotation1, scale0, scale1);
			const float r = (shs[k][0] * SH_C0 + 0.5);
            const float g = (shs[k][1] * SH_C0 + 0.5);
            const float b = (shs[k][2] * SH_C0 + 0.5);
			gs_color.x = glm::packHalf2x16(glm::vec2(r, g));
			gs_color.y = glm::packHalf2x16(glm::vec2(b, 0));

			// extract coefficients
			std::array<float,45> c = {0};
			for (auto j = 0; j < 15; ++j) {
                c[j * 3] = shs[k][j * 3 + 3];
                c[j * 3 + 1] = shs[k][j * 3 + 4];
                c[j * 3 + 2] = shs[k][j * 3 + 5];
            }

            // calc maximum value
            auto max = c[0];
            for (auto j = 1; j < 15 * 3; ++j) {
                max = std::max(max, std::abs(c[j]));
            }

            // normalize
			if( max != 0){
				for (auto j = 0; j < 15; ++j) {
					c[j * 3 + 0] = (c[j * 3 + 0] / max);
					c[j * 3 + 1] = (c[j * 3 + 1] / max);
					c[j * 3 + 2] = (c[j * 3 + 2] / max);
				}
			}
			auto& sh1to3 = gaussians_sh[k].sh1to3;
			sh1to3.x = *(u32*)(&max);
			sh1to3.y = pack_unit_direction_11_10_11(glm::vec3(c[0],c[1],c[2]));
			sh1to3.z = pack_unit_direction_11_10_11(glm::vec3(c[3],c[4],c[5]));
			sh1to3.w = pack_unit_direction_11_10_11(glm::vec3(c[6],c[7],c[8]));

			//sh > 1
            {
				auto& sh4to7 = gaussians_sh[k].sh4to7;
				sh4to7.x = pack_unit_direction_11_10_11(glm::vec3(c[9],c[10],c[11]));
				sh4to7.y = pack_unit_direction_11_10_11(glm::vec3(c[12],c[13],c[14]));
				sh4to7.z = pack_unit_direction_11_10_11(glm::vec3(c[15],c[16],c[17]));
				sh4to7.w = pack_unit_direction_11_10_11(glm::vec3(c[18],c[19],c[20]));

				//sh > 2
                {
					auto& sh8to11 = gaussians_sh[k].sh8to11;
					sh8to11.x = pack_unit_direction_11_10_11(glm::vec3(c[21],c[22],c[23]));
					sh8to11.y = pack_unit_direction_11_10_11(glm::vec3(c[24],c[25],c[26]));
					sh8to11.z = pack_unit_direction_11_10_11(glm::vec3(c[27],c[28],c[29]));
					sh8to11.w = pack_unit_direction_11_10_11(glm::vec3(c[30],c[31],c[32]));

					auto& sh12to15 = gaussians_sh[k].sh12to15;
					sh12to15.x = pack_unit_direction_11_10_11(glm::vec3(c[33],c[34],c[35]));
					sh12to15.y = pack_unit_direction_11_10_11(glm::vec3(c[36],c[37],c[38]));
					sh12to15.z = pack_unit_direction_11_10_11(glm::vec3(c[39],c[40],c[41]));
					sh12to15.w = pack_unit_direction_11_10_11(glm::vec3(c[42],c[43],c[44]));
                }
            }
		});
		if (gaussians_buf && gaussians_buf->desc.size >= (gaussians.size() * sizeof(Gaussian)))
		{
			{
				auto data = reinterpret_cast<Gaussian*>(gaussians_buf->map(device));
				parallel_for<size_t>(0, pos.size(), [&](size_t i) {
					data[i] = gaussians[i];
				});
				gaussians_buf->unmap(device);
			}
			{
				auto data = reinterpret_cast<PackedVertexColor*>(gaussians_color_buf->map(device));
				parallel_for<size_t>(0, pos.size(), [&](size_t i) {
					data[i] = gaussians_color[i];
				});
				gaussians_color_buf->unmap(device);
			}
			{
				auto data = reinterpret_cast<PackedVertexSH*>(gaussians_sh_buf->map(device));
				parallel_for<size_t>(0, pos.size(), [&](size_t i) {
					data[i] = gaussians_sh[i];
				});
				gaussians_sh_buf->unmap(device);
			}
			{
				auto states_data = reinterpret_cast<u32*>(gaussian_state_buf->map(device));
				parallel_for<size_t>(0, pos.size(), [&](size_t i) {
					uint state = states_data[i];
					state = setOpState(state, splat_state[i]);
					state = setTransformIndex(state, splat_transform_index[i]);
					states_data[i] = state;
				});
				gaussian_state_buf->unmap(device);
			}
		}
		else
		{
			int scaleFactor = std::ceil(static_cast<double>(gaussians.size()) / static_cast<double>(max_splats));
			auto num_gaussians = compact ? gaussians.size() : scaleFactor * max_splats;
			gaussians_buf = device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(num_gaussians * sizeof(Gaussian), rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::VERTEX_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "gaussian_buf", nullptr);
			gaussians_color_buf = device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(num_gaussians * sizeof(PackedVertexColor), rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::VERTEX_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "gaussian_color_buf", nullptr);
			gaussians_sh_buf = device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(num_gaussians * sizeof(PackedVertexSH), rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::VERTEX_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "gaussian_sh_buf", nullptr);
			points_key_buf = device->create_buffer(rhi::GpuBufferDesc::new_gpu_only(num_gaussians * sizeof(u32), rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "points_key_buf", nullptr);
			points_value_buf = device->create_buffer(rhi::GpuBufferDesc::new_gpu_only(num_gaussians * sizeof(u32), rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "points_value_buf", nullptr);
			gaussian_state_buf = device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(num_gaussians * sizeof(u32), rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::VERTEX_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "gaussian_state_buf", nullptr);

			{
				auto data = reinterpret_cast<Gaussian*>(gaussians_buf->map(device));
				parallel_for<size_t>(0, pos.size(), [&](size_t i) {
					data[i] = gaussians[i];
				});
				gaussians_buf->unmap(device);
			}
			{
				auto data = reinterpret_cast<PackedVertexColor*>(gaussians_color_buf->map(device));
				parallel_for<size_t>(0, pos.size(), [&](size_t i) {
					data[i] = gaussians_color[i];
				});
				gaussians_color_buf->unmap(device);
			}
			{
				auto data = reinterpret_cast<PackedVertexSH*>(gaussians_sh_buf->map(device));
				parallel_for<size_t>(0, pos.size(), [&](size_t i) {
					data[i] = gaussians_sh[i];
				});
				gaussians_sh_buf->unmap(device);
			}
			{
				auto states_data = reinterpret_cast<u32*>(gaussian_state_buf->map(device));
				parallel_for<size_t>(0, pos.size(), [&](size_t i) {
					states_data[i] = 0;
				});
				gaussian_state_buf->unmap(device);
			}
		}
		set_flag(AssetFlag::UploadedGpu);
	}

	void GaussianModel::update_data()
	{
		glm::vec3 minn(FLT_MAX, FLT_MAX, FLT_MAX);
		glm::vec3 maxx = -minn;
		for (int i = 0; i < pos.size(); i++)
		{
			maxx = glm::max(maxx, pos[i]);
			minn = glm::min(minn, pos[i]);
		}
		local_bounding_box = maths::BoundingBox(minn,maxx);
		set_flag(AssetFlag::Loaded);
		create_gpu_buffer();
		update_state();
	}

	void GaussianModel::update_state()
	{
		num_hidden = 0;
		num_select = 0;
		num_delete = 0;
		for (int i = 0; i < splat_state.size(); i++)
		{
			auto state = splat_state[i];
			if (state & DELETE_STATE)
				num_delete++;
			else if(state & SELECT_STATE)
				num_select++;
			else if(state & HIDE_STATE)
				num_hidden++;
		}
		if (gaussians_buf )
		{
			auto device = get_global_device();
			auto states_data = reinterpret_cast<u32*>(gaussian_state_buf->map(device));
			parallel_for<size_t>(0, pos.size(), [&](size_t i) {
				uint state = states_data[i];
				state = setOpState(state, splat_state[i]);
				state = setTransformIndex(state, splat_transform_index[i]);
				states_data[i]	= state;
			});
			gaussian_state_buf->unmap(device);
		}

		make_selection_bound_dirty();
	}

	void GaussianModel::update_feature_dc_data(const std::vector<u32>& indices)
	{
		auto device = get_global_device();
	
		auto data = reinterpret_cast<PackedVertexColor*>(gaussians_color_buf->map(device));
		parallel_for<size_t>(0, indices.size(), [&](size_t idx) {
			auto i = indices[idx];
			glm::vec4 harmonics;
			harmonics.x = shs[i][0];
			harmonics.y = shs[i][1];
			harmonics.z = shs[i][2];
			harmonics.w = 0;
			auto hom0 = u32_to_f32(glm::packHalf2x16(harmonics.xy));
			auto hom1 = u32_to_f32(glm::packHalf2x16(harmonics.zw));
			data[i].xy = glm::vec2(hom0, hom1);
		});
		gaussians_color_buf->unmap(device);
	}

	void GaussianModel::update_transform_index()
	{
		if (gaussians_buf)
		{
			auto device = get_global_device();
			auto states_data = reinterpret_cast<u32*>(gaussian_state_buf->map(device));
			parallel_for<size_t>(0, pos.size(), [&](size_t i) {
				uint state = states_data[i];
				state = setTransformIndex(state, splat_transform_index[i]);
				states_data[i] = state;
			});
			gaussian_state_buf->unmap(device);
		}
	}

	void GaussianModel::save_to_file(const std::string& filepath, bool apply_transfom)
	{
		std::vector<glm::vec3>	new_pos;
		std::vector<std::array<float, 48>>        new_shs;
		std::vector<float>      	new_opacities;
		std::vector<glm::vec3>      new_scales;
		std::vector<glm::vec4>      new_rot;
		std::vector<u8> 			new_degrees;
		for (int k = 0; k < pos.size(); k++)
		{
			auto state = splat_state[k];
			if (state & DELETE_STATE)
				continue;
			
			auto transform_index = splat_transform_index[k];
			auto transform = splat_transforms.get_transform(transform_index);
			auto q = glm::toQuat(transform);
			if(apply_transfom)
			{ 
				new_pos.push_back(transform * glm::vec4(pos[k],1.0f));
				auto r = rot[k];
				auto new_r = glm::quat(r.x, r.y, r.z, r.w) * q;
				new_rot.push_back(glm::vec4(new_r.w,new_r.x, new_r.y, new_r.z));
			}
			else
			{
				new_pos.push_back(pos[k]);
				new_rot.push_back(rot[k]);
			}
			new_scales.push_back(scales[k]);
			new_opacities.push_back(opacities[k]);
			new_shs.push_back(shs[k]);
			if (apply_transfom)
			{ 
				glm::mat3 tmpMat3 = glm::toMat3(q);
				SHRotation shRot(tmpMat3);
				for (auto c = 0; c < 3; c++)
				{
					std::vector<f32> tmpSHData(15);
					for (auto j = 0; j < 15; j++)
						tmpSHData[j] = shs[k][c * 15 + j + 3];
					shRot.apply(tmpSHData, {});
					for (auto j = 0; j < 15; j++)
						new_shs.back()[c * 15 + j + 3] = tmpSHData[j];
				}
			}
		}
		if( new_pos.size() == 0 ) 
		{
			DS_LOG_ERROR("this gaussian model is an empty model");
			return;
		}
		auto ext = std::filesystem::path(filepath).extension().string();
		std::string saved_path = filepath;
		bool ret = false;
		if( ext  == ".ply")
		{ 
			if (saved_path.find(".compressed") != std::string::npos)
			{
				ret = tinygsplat::save_compress_ply(saved_path, new_pos, new_scales, new_shs, new_rot, new_opacities,mip_antialiased);
			}
			else if(saved_path.find(".reduced") != std::string::npos)
			{
				auto [featureDc,featureRest] = get_feature_dc_rest(new_shs);
				ret = tinygsplat::save_reduced_ply(saved_path, new_pos, new_scales, featureDc, featureRest,new_rot, new_opacities, new_degrees);
			}
			else
				ret = tinygsplat::save_ply(filepath, new_pos, new_scales, new_shs, new_rot, new_opacities,mip_antialiased);
		}
		else if (ext == ".splat")
		{
			ret = tinygsplat::save_splat(saved_path, new_pos, new_scales, new_shs, new_rot, new_opacities);
		}
		else if (ext == ".dvsplat")
		{
			auto [featureDc, featureRest] = get_feature_dc_rest(new_shs);
			ret = tinygsplat::save_dvs_splat(saved_path, new_pos, new_scales, featureDc, featureRest, new_rot, new_opacities, new_degrees);
		}
		else if (ext == ".spz")
		{
			ret = tinygsplat::save_spz_splats(filepath, new_pos, new_scales, new_shs, new_rot, new_opacities,mip_antialiased);
		}
		if (!ret)
		{
			DS_LOG_ERROR("write gaussian file {} failed", saved_path);
		}
		else
			DS_LOG_INFO("write gaussian to file {} success", saved_path);
	}

	auto GaussianModel::load_model(const std::string& filePath)->void
	{
		bool load_ret = false;
		std::vector<tinygsplat::RichPoint> points;
		auto ext = std::filesystem::path(filePath).extension().string();
		try{
			if (ext == ".ply")
			{
				//compressed
				if (filePath.find(".compressed") != std::string::npos)
				{
					load_ret = tinygsplat::load_compress_ply(filePath, points,mip_antialiased);
				}
				else if(filePath.find(".reduced") != std::string::npos){
					load_ret = tinygsplat::load_reduced_ply(filePath, points);
				}
				else
				{
					load_ret = tinygsplat::load_ply(filePath, points,mip_antialiased);
				}
			}
			else if (ext == ".splat")
			{
				load_ret = tinygsplat::load_splat(filePath, points);
			}
			else if (ext == ".dvsplat")
			{
				load_ret = tinygsplat::load_dvs_splat(filePath, points);
			}
			else if (ext == ".spz")
			{
				load_ret = tinygsplat::load_spz_splats(filePath, points,mip_antialiased);
			}
			if (!load_ret)
			{
				DS_LOG_ERROR("loading gaussian model file {} failed!", filePath);
				set_flag(AssetFlag::Invalid);
				return;
			}
		}
		catch (...)
		{
			DS_LOG_ERROR("loading gaussian model file {} failed!", filePath);
			set_flag(AssetFlag::Invalid);
			return;
		}
		auto numSplats = points.size();
		// Resize our SoA data
		pos.resize(numSplats);
		shs.resize(numSplats);
		scales.resize(numSplats);
		rot.resize(numSplats);
		opacities.resize(numSplats);
		// Gaussians are done training, they won't move anymore. Arrange
		// them according to 3D Morton order. This means better cache
		// behavior for reading Gaussians that end up in the same tile 
		// (close in 3D --> close in 2D).
		glm::vec3 minn(FLT_MAX, FLT_MAX, FLT_MAX);
		glm::vec3 maxx = -minn;
		for (int i = 0; i < numSplats; i++)
		{
			maxx = glm::max(maxx, points[i].pos);
			minn = glm::min(minn, points[i].pos);
		}
		local_bounding_box = maths::BoundingBox(minn, maxx);
		std::vector<std::pair<uint64_t, int>> mapp(numSplats);
		// Compute Morton codes
		parallel_for<size_t>(0, numSplats, [&](size_t i) {
			glm::vec3 rel = (points[i].pos - minn) / (maxx - minn);
			glm::vec3 scaled = ((float((1 << 21) - 1)) * rel);
			glm::ivec3 xyz = scaled;

			uint64_t code = 0;
			for (int i = 0; i < 21; i++) {
				code |= ((uint64_t(xyz.x & (1 << i))) << (2 * i + 0));
				code |= ((uint64_t(xyz.y & (1 << i))) << (2 * i + 1));
				code |= ((uint64_t(xyz.z & (1 << i))) << (2 * i + 2));
			}

			mapp[i].first = code;
			mapp[i].second = i;
		});
		
		auto sorter = [](const std::pair < uint64_t, int>& a, const std::pair < uint64_t, int>& b) {
			return a.first < b.first;
			};
		std::sort(mapp.begin(), mapp.end(), sorter);
		

		// Move data from AoS to SoA
		parallel_for<size_t>(0, numSplats, [&](size_t k) {
			int i = mapp[k].second;
			pos[k] = points[i].pos;
			for (int j = 0; j < 4; j++)
				rot[k][j] = points[i].rot[j];

			// Exponentiate scale
			for (int j = 0; j < 3; j++)
				scales[k][j] = (points[i].scale[j]);

			// Activate alpha
			opacities[k] = (points[i].opacity);
			
			shs[k][0] = points[i].shs[0];
			shs[k][1] = points[i].shs[1];
			shs[k][2] = points[i].shs[2];
			for (int j = 1; j < 16; j++)
			{
				shs[k][j * 3 + 0] = points[i].shs[(j - 1) + 3];
				shs[k][j * 3 + 1] = points[i].shs[(j - 1) + 18];
				shs[k][j * 3 + 2] = points[i].shs[(j - 1) + 33];
			}
			
		});
		set_flag(AssetFlag::Loaded);
		create_gpu_buffer(true);
	}

	void GaussianModel::export_to_cpu()
	{
		auto device = get_global_device();

		std::vector<glm::vec3>	new_pos;
		std::vector<std::array<float, 48>>        new_shs;
		std::vector<float>      	new_opacities;
		std::vector<glm::vec3>      new_scales;
		std::vector<glm::vec4>      new_rot;
		std::vector<u16>			new_transform_idx;
		std::vector<u8>				new_state;
		std::vector<u8>				new_flag;
		for (int k = 0; k < pos.size(); k++)
		{
			auto state = splat_state[k];
			if (state & DELETE_STATE)
				continue;
			new_pos.push_back(pos[k]);
			new_rot.push_back(rot[k]);
			new_scales.push_back(scales[k]);
			new_opacities.push_back(opacities[k]);
			new_state.push_back(state);
			new_flag.push_back(splat_select_flag[k]);
			new_transform_idx.push_back(splat_transform_index[k]);
			new_shs.push_back(shs[k]);
		}
		pos = new_pos;
		shs = new_shs;
		opacities = new_opacities;
		scales = new_scales;
		rot = new_rot;
		splat_state = new_state;
		splat_select_flag = new_flag;
		splat_transform_index = new_transform_idx;
		update_data();
	}

	void GaussianModel::download_state_buffer()
	{
		auto device = get_global_device();
		std::vector<u32> states_data(pos.size());
		gaussian_state_buf->copy_to(device, (u8*)states_data.data(), states_data.size() * sizeof(u32), 0);
		parallel_for<size_t>(0, pos.size(), [&](size_t i) {
            auto state = states_data[i];
			splat_state[i] = getOpState(state);
            splat_select_flag[i] = getOpFlag(state);
			splat_transform_index[i] = getTransformIndex(state);
		});
	}

	u64 GaussianModel::get_num_gaussians() const
	{
		if( gaussians_buf )
			return pos.size();
		return 0;
	}

	auto GaussianModel::merge(GaussianModel* model, bool apply_transform)->void
	{
		const auto old_size = pos.size();
		if( model )
		{
			const auto molde_num_splats = model->position().size();
			const auto num_size = molde_num_splats + old_size;
			pos.resize(num_size);
			scales.resize(num_size);
			rot.resize(num_size);
			splat_state.resize(num_size);
			splat_select_flag.resize(num_size);
			splat_transform_index.resize(num_size);
			shs.resize(num_size);
			opacities.resize(num_size);
			parallel_for<size_t>(0, molde_num_splats, [&](size_t i) {
				auto idx = i + old_size;
				pos[idx] = model->position()[i];
				rot[idx] = model->rotation()[i];
				shs[idx] = model->sh()[i];
				scales[idx] = model->scale()[i];
				opacities[idx] = model->opacity()[i];
				splat_state[idx] = model->splat_state[i];
				splat_select_flag[idx] = model->splat_select_flag[i];
				splat_transform_index[idx] = model->splat_transform_index[i];
				if(apply_transform)
				{
					auto transform_index = splat_transform_index[idx];
					glm::mat4 transform = glm::transpose(splat_transforms[transform_index]);
					pos[idx] = transform * glm::vec4(pos[idx],1.0f);
					auto q = glm::toQuat(transform);
					auto r = rot[idx];auto new_r = glm::quat(r.x, r.y, r.z, r.w) * q;
					rot[idx] = glm::vec4(new_r.w, new_r.x, new_r.y, new_r.z);

					glm::mat3 tmpMat3 = glm::toMat3(q);
					SHRotation shRot(tmpMat3);
					for(auto c = 0;c<3;c++)
					{
						std::vector<f32> tmpSHData(15);
						for (auto j = 0; j < 15; j++)
							tmpSHData[j] = shs[idx][c * 15 + j + 3];
						shRot.apply(tmpSHData, {});
						for (auto j = 0; j < 15; j++)
							shs[idx][c* 15 + j + 3] = tmpSHData[j];
					}
				}
			});
			mip_antialiased = model->mip_antialiased;
			update_data();
		}
	}

	auto GaussianModel::merge(GaussianModel* model,const std::vector<u32>& indices, bool apply_transform)->std::vector<u32>
	{
		std::vector<u32> add_indices;
		if(indices.empty()) return add_indices;
		const auto old_size = pos.size();
		if (model)
		{
			const auto num_splats = indices.size();
			const auto num_size = num_splats + old_size;
			pos.resize(num_size);
			scales.resize(num_size);
			rot.resize(num_size);
			splat_state.resize(num_size);
			splat_select_flag.resize(num_size);
			splat_transform_index.resize(num_size);
			shs.resize(num_size);
			opacities.resize(num_size);
			add_indices.resize(indices.size());
			parallel_for<size_t>(0, num_splats, [&](size_t i) {
				auto idx = i + old_size;
				auto model_splat_id = indices[i];
				pos[idx] = model->position()[model_splat_id];
				scales[idx] = model->scale()[model_splat_id];
				rot[idx] = model->rotation()[model_splat_id];
				shs[idx] = model->sh()[model_splat_id];
				opacities[idx] = model->opacity()[model_splat_id];
				splat_state[idx] = model->splat_state[model_splat_id];
				splat_select_flag[idx] = model->splat_select_flag[model_splat_id];
				splat_transform_index[idx] = model->splat_transform_index[model_splat_id];
				if (apply_transform)
				{
					auto transform_index = splat_transform_index[idx];
					glm::mat4 transform = glm::transpose(splat_transforms[transform_index]);
					pos[idx] = transform * glm::vec4(pos[idx], 1.0f);
					auto q = glm::toQuat(transform);
					auto r = rot[idx]; auto new_r = glm::quat(r.x, r.y, r.z, r.w) * q;
					rot[idx] = glm::vec4(new_r.w, new_r.x, new_r.y, new_r.z);
					glm::mat3 tmpMat3 = glm::toMat3(q);
					SHRotation shRot(tmpMat3);
					for (auto c = 0; c < 3; c++)
					{
						std::vector<f32> tmpSHData(15);
						for (auto j = 0; j < 15; j++)
							tmpSHData[j] = shs[idx][c * 15 + j + 3];
						shRot.apply(tmpSHData, {});
						for (auto j = 0; j < 15; j++)
							shs[idx][c * 15 + j + 3] = tmpSHData[j];
					}
				}
				add_indices[i] = idx;
			});
			mip_antialiased = model->mip_antialiased;
			update_data();
		}
		return add_indices;
	}

	auto GaussianModel::remove(const std::vector<u32>& indices)->void
	{
		if(indices.empty()) return;

		const auto old_size = pos.size();
		const auto new_size = pos.size()- indices.size();
		std::vector<glm::vec3>	new_pos;
		std::vector<std::array<float, 48>>        new_shs;
		std::vector<float>      	new_opacities;
		std::vector<glm::vec3>      new_scales;
		std::vector<glm::vec4>      new_rot;
		std::vector<u8> 			new_splat_state;
		std::vector<u16> 			new_splat_transform_index;
		std::vector<u8> 			new_splat_select_flag;
		for(auto i= 0;i<pos.size();i++)
		{
			if(std::find(indices.begin(),indices.end(),i) == indices.end())
			{
				new_pos.push_back(pos[i]);
				new_scales.push_back(scales[i]);
				new_rot.push_back(rot[i]);
				new_opacities.push_back(opacities[i]);
				new_splat_state.push_back(splat_state[i]);
				new_splat_transform_index.push_back(splat_transform_index[i]);
				new_splat_select_flag.push_back(splat_select_flag[i]);
				new_shs.push_back(shs[i]);
			}
		}
		pos = new_pos;
		scales = new_scales;
		rot = new_rot;
		shs = new_shs;
		opacities = new_opacities;
		splat_state = new_splat_state;
		splat_transform_index = new_splat_transform_index;
		splat_select_flag = new_splat_select_flag;
		update_data();
	}

	auto GaussianModel::get_compressed_data(bool apply_transform)->std::vector<u8>
	{
		std::vector<glm::vec3>	new_pos;
		std::vector<std::array<float, 48>>        new_shs;
		std::vector<float>      	new_opacities;
		std::vector<glm::vec3>      new_scales;
		std::vector<glm::vec4>      new_rot;

		for (int k = 0; k < pos.size(); k++)
		{
			auto state = splat_state[k];
			if (state & DELETE_STATE)
				continue;
			new_pos.push_back(pos[k]);
			new_scales.push_back(scales[k]);
			new_rot.push_back(rot[k]);
			new_opacities.push_back(opacities[k]);
			new_shs.push_back(shs[k]);	
			if (apply_transform)
			{
				auto transform_index = splat_transform_index[k];
				glm::mat4 transform = glm::transpose(splat_transforms[transform_index]);
				new_pos.back() = transform * glm::vec4(pos[k], 1.0f);
				auto q = glm::toQuat(transform);
				auto r = rot[k]; auto new_r = glm::quat(r.x, r.y, r.z, r.w) * q;
				new_rot.back() = glm::vec4(new_r.w, new_r.x, new_r.y, new_r.z);

				glm::mat3 tmpMat3 = glm::toMat3(q);
				SHRotation shRot(tmpMat3);
				for (auto c = 0; c < 3; c++)
				{
					std::vector<f32> tmpSHData(15);
					for (auto j = 0; j < 15; j++)
						tmpSHData[j] = shs[k][c * 15 + j + 3];
					shRot.apply(tmpSHData, {});
					for (auto j = 0; j < 15; j++)
						new_shs.back()[c * 15 + j + 3] = tmpSHData[j];
				}
			}
		}
		if (new_pos.size() == 0)
		{
			DS_LOG_ERROR("this gaussian model is an empty model");
			return {};
		}
		auto numSplats = new_pos.size();
		u64 numChunks = (numSplats + 255)/ 256;
		std::vector<u64> indices(numSplats);
		for (auto i = 0; i < numSplats; i++)
			indices[i] = i;
		auto [pmin, pmax] = tinygsplat::calcMinMax(new_pos, indices, 0, indices.size());
		std::vector<std::pair<uint64_t, int>> mapp(numSplats);
		parallel_for<size_t>(0, numSplats, [&](size_t i) {
			glm::vec3 rel = (new_pos[i] - pmin) / (pmax - pmin);
			glm::vec3 scaled = ((float((1 << 21) - 1)) * rel);
			glm::ivec3 xyz = scaled;

			uint64_t code = 0;
			for (int i = 0; i < 21; i++) {
				code |= ((uint64_t(xyz.x & (1 << i))) << (2 * i + 0));
				code |= ((uint64_t(xyz.y & (1 << i))) << (2 * i + 1));
				code |= ((uint64_t(xyz.z & (1 << i))) << (2 * i + 2));
			}

			mapp[i].first = code;
			mapp[i].second = i;
		});
		auto sorter = [](const std::pair < uint64_t, int>& a, const std::pair < uint64_t, int>& b) {
			return a.first < b.first;
		};
		std::sort(mapp.begin(), mapp.end(), sorter);
		for (auto i = 0; i < numSplats; i++)
			indices[i] = mapp[i].second;
		const std::string chunkProps[12] = { "min_x", "min_y", "min_z", "max_x", "max_y", "max_z", "min_scale_x", "min_scale_y", "min_scale_z", "max_scale_x", "max_scale_y", "max_scale_z" };
		const std::string vertexProps[4] = { "packed_position", "packed_rotation", "packed_scale", "packed_color" };

		std::string header_text = std::format("ply\nformat binary_little_endian 1.0\ncomment generated by splatx\nelement chunk {}\n",numChunks);
		if (mip_antialiased) {
			header_text += "comment splatx.anti_aliasing=1\n";
		}
		for(auto i=0;i<12;i++){
			header_text += std::format("property float {}\n",chunkProps[i]);
		}
		header_text += std::format("element vertex {}\n",numSplats);
		for(auto i=0;i<4;i++){
			header_text += std::format("property uint {}\n",vertexProps[i]);
		}
		header_text += "end_header\n";
		size_t header_size = header_text.length();
		std::vector<u8> serilizeData(header_size + numChunks * 4 * 12 + numSplats * 4 * 4);
		memcpy(serilizeData.data(),header_text.c_str(),header_size);
		tinygsplat::DataView dataView(numChunks * 4 * 12 + numSplats * 4 * 4);

		auto vertexOffset = numChunks * 12 * 4;
		parallel_for<size_t>(0, numChunks, [&](size_t i) {
			tinygsplat::SplatChunk chunk(indices, i * 256, (i + 1) * 256);

			auto [pmin, pmax, smin, smax] = chunk.pack(new_pos, new_scales, new_rot, new_shs, new_opacities);

			dataView.setFloat32(i * 12 * 4 + 0, pmin.x);
			dataView.setFloat32(i * 12 * 4 + 4, pmin.y);
			dataView.setFloat32(i * 12 * 4 + 8, pmin.z);
			dataView.setFloat32(i * 12 * 4 + 12, pmax.x);
			dataView.setFloat32(i * 12 * 4 + 16, pmax.y);
			dataView.setFloat32(i * 12 * 4 + 20, pmax.z);

			dataView.setFloat32(i * 12 * 4 + 24, smin.x);
			dataView.setFloat32(i * 12 * 4 + 28, smin.y);
			dataView.setFloat32(i * 12 * 4 + 32, smin.z);
			dataView.setFloat32(i * 12 * 4 + 36, smax.x);
			dataView.setFloat32(i * 12 * 4 + 40, smax.y);
			dataView.setFloat32(i * 12 * 4 + 44, smax.z);

			// write splat data
			auto offset = vertexOffset + i * 256 * 4 * 4;
			const auto chunkSplats = std::min<u64>(numSplats, (i + 1) * 256) - i * 256;
			for (auto j = 0; j < chunkSplats; ++j) {
				dataView.setUint32(offset + j * 4 * 4 + 0, chunk.position[j]);
				dataView.setUint32(offset + j * 4 * 4 + 4, chunk.rotation[j]);
				dataView.setUint32(offset + j * 4 * 4 + 8, chunk.scale[j]);
				dataView.setUint32(offset + j * 4 * 4 + 12, chunk.color[j]);
			}
		});
		auto splatData = dataView.get_buffer_vec();
		memcpy(serilizeData.data() + header_size, splatData.data(),splatData.size());
		return serilizeData;
	}

	auto GaussianModel::get_world_bounding_box(const glm::mat4& t)-> maths::BoundingBox
	{
		if(!world_bound_dirty)
		{
			return world_bounding_box.transformed(t);
		}
		if(splat_transform_index.empty())
			return maths::BoundingBox(glm::vec3(-1),glm::vec3(1)).transformed(t);
		glm::vec3 minn(FLT_MAX, FLT_MAX, FLT_MAX);
		glm::vec3 maxx = -minn;
		for (int i = 0; i < pos.size(); i++)
		{
			auto state = splat_state[i];
			if ((state & DELETE_STATE) || (state & HIDE_STATE))
				continue;
			auto transform_index = splat_transform_index[i];
			glm::mat4 transform = splat_transforms[transform_index];
			auto p = glm::vec3(glm::transpose(transform) * glm::vec4(pos[i], 1.0));
			maxx = glm::max(maxx, p);
			minn = glm::min(minn, p);
		}
		world_bounding_box = maths::BoundingBox(minn, maxx);
		world_bound_dirty = false;
		return world_bounding_box.transformed(t);
	}

	auto GaussianModel::get_selection_bounding_box(const glm::mat4& t) -> maths::BoundingBox
	{
		if (!selection_bound_dirty)
		{
			return selection_bounding_box.transformed(t);
		}
		glm::vec3 minn(FLT_MAX, FLT_MAX, FLT_MAX);
		glm::vec3 maxx = -minn;
		for (int i = 0; i < splat_state.size(); i++)
		{
			auto state = splat_state[i];
			if(state == SELECT_STATE)
			{
				auto transform_index = splat_transform_index[i];
				auto transform = glm::transpose(splat_transforms[transform_index]);
				auto p = glm::vec3(transform * glm::vec4(pos[i], 1.0));
				maxx = glm::max(maxx, p);
				minn = glm::min(minn, p);
			}
		}
		selection_bound_dirty = false;
		selection_bounding_box = maths::BoundingBox(minn, maxx);
		return selection_bounding_box.transformed(t);
	}

	auto GaussianModel::make_selection_bound_dirty()->void
	{
		selection_bound_dirty = true;
		make_world_bound_dirty();
	}
}
