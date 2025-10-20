 #include "tiny_gsplat.hpp"
#include <load-spz.h>

// #include <tinyply.h>
 namespace tinygsplat
 {
	 constexpr float colorScale = 0.15f;
	 uint8_t toUint8(float x) { return static_cast<uint8_t>(std::clamp(std::round(x), 0.0f, 255.0f)); }

	 // Quantizes to 8 bits, the round to nearest bucket center. 0 always maps to a bucket center.
	 uint8_t quantizeSH(float x, int bucketSize) {
		 int q = static_cast<int>(std::round(x * 128.0f) + 128.0f);
		 q = (q + bucketSize / 2) / bucketSize * bucketSize;
		 return static_cast<uint8_t>(std::clamp(q, 0, 255));
	 }

	 float unquantizeSH(uint8_t x) { return (static_cast<float>(x) - 128.0f) / 128.0f; }

	 float sigmoid(float x) { return 1 / (1 + std::exp(-x)); }

	 float invSigmoid(float x) { return std::log(x / (1.0f - x)); }

	 template <typename T>
	 size_t countBytes(std::vector<T> vec) {
		 return vec.size() * sizeof(vec[0]);
	 }

	 float distancePoint(const float x1, const float x2)
	 {
		 return sqrt((x2 - x1) * (x2 - x1));
	 }

	 void updateCentersCpu(
		 const float* values,
		 const int* ids,
		 float* centers,
		 int* center_sizes,
		 const int n_values,
		 const int n_centers)
	 {
		 const auto num_threads = 256;
		 std::vector<std::thread> threads;
		 const int block = (n_values + num_threads - 1) / num_threads;
		 std::mutex cmutex;
		 parallel_for(0, block, [&](int i) {
			 int start_idx = i * num_threads;
			 int end_idx = (i == block - 1) ? n_values : start_idx + num_threads;

			 std::vector<float> block_center_sums(n_centers, 0.0f);
			 std::vector<int> block_center_sizes(n_centers, 0);

			 for (int idx = start_idx; idx < end_idx; ++idx) {
				 int clust_id = ids[idx];
				 block_center_sums[clust_id] += values[idx];
				 block_center_sizes[clust_id] += 1;
			 }

			 for (int j = 0; j < n_centers; ++j) {
				 std::lock_guard<std::mutex> mutex_guard(cmutex);
				 centers[j] += block_center_sums[j];
				 center_sizes[j] += block_center_sizes[j];
			 }
			 });
	 }

	 void updateIdsCpu(
		 const float* values,
		 int* ids,
		 const float* centers,
		 const int n_values,
		 const int n_centers)
	 {
		 const auto num_threads = 256;
		 std::vector<std::thread> threads;
		 const int block = (n_values + num_threads - 1) / num_threads;
		 parallel_for(0, block, [&](int i) {
			 int start_idx = i * num_threads;
			 int end_idx = (i == block - 1) ? n_values : start_idx + num_threads;
			 for (int idx = start_idx; idx < end_idx; ++idx) {
				 float min_dist = std::numeric_limits<float>::infinity();
				 int closest_centroid = 0;

				 for (int j = 0; j < n_centers; ++j) {
					 float dist = distancePoint(values[idx], centers[j]);
					 if (dist < min_dist) {
						 min_dist = dist;
						 closest_centroid = j;
					 }
				 }

				 ids[idx] = closest_centroid;
			 }
			 });
	 }

	 std::tuple<std::vector<int>, std::vector<float>>  kmeans_cluster1(
		 const std::vector<float>& values,
		 const std::vector<float>& centers,
		 const float tol,
		 const int max_iterations)
	 {
		 const int n_values = values.size();
		 const int n_centers = centers.size();
		 std::vector<int> ids(n_values);
		 std::vector<float> new_centers(n_centers, 0.0f);
		 std::vector<float> old_centers(n_centers, 0.0f);
		 std::vector<int> center_sizes(n_centers, 0);
		 new_centers = centers;
		 for (int i = 0; i < max_iterations; ++i)
		 {
			 updateIdsCpu(
				 values.data(),
				 ids.data(),
				 new_centers.data(),
				 n_values,
				 n_centers);
			 old_centers = new_centers;
			 parallel_for<size_t>(0, new_centers.size(), [&](size_t p) {
				 new_centers[p] = 0;
				 center_sizes[p] = 0;
				 });
			 updateCentersCpu(
				 values.data(),
				 ids.data(),
				 new_centers.data(),
				 center_sizes.data(),
				 n_values,
				 n_centers);

			 float center_shift = 0;
			 for (auto j = 0; j < new_centers.size(); j++)
			 {
				 new_centers[j] /= center_sizes[j];
				 if (std::isnan(new_centers[j]))
					 new_centers[j] = 0.0f;
				 center_shift += std::abs(old_centers[j] - new_centers[j]);
			 }
			 if (center_shift < tol)
				 break;
		 }
		 updateIdsCpu(
			 values.data(),
			 ids.data(),
			 new_centers.data(),
			 n_values,
			 n_centers);

		 return std::make_tuple(ids, new_centers);
	 }

	 auto generateCodeBook(const std::vector<float> values, std::function<std::vector<float>(const std::vector<float>& x)>&& inverseActiveFn, int numClusters, float tol = 0.0001f) -> CodeBook
	 {
		 std::random_device rd;
		 std::mt19937 gen(rd());
		 std::uniform_int_distribution<> distrib(0, values.size()-1);

		 std::vector<float> centers(numClusters);
		 for (size_t i = 0; i < numClusters; ++i)
			 centers[i] = values[distrib(gen)];
		 auto [ids, newCenters] = kmeans_cluster1(values, centers, tol, 500);
		 std::vector<uint8_t> u8ids(ids.size());
#pragma omp parallel for
		 for (auto i = 0; i < ids.size(); i++) u8ids[i] = ids[i];
		 auto invCenters = inverseActiveFn(newCenters);
		 return CodeBook{ u8ids, invCenters };
	 }

	 bool	save_ply(const std::string& file_path,
		 const std::vector<glm::vec3>& pos,
		 const std::vector<glm::vec3>& scales,
		 const std::vector<std::array<f32, 48>>& shs,
		 const std::vector<glm::vec4>& rot,
		 const std::vector<f32>& opacities,
		 bool antialiased)
	 {
		 std::ofstream outfile(file_path, std::ios_base::binary);

		 if (!outfile.good())
		 {
			 std::cout << std::format("Unable to find model's PLY file, attempted:\n {} ", file_path);
			 outfile.close();
			 return false;
		 }
		 std::string buff = "ply\n";
		 outfile << buff;
		 buff = "format binary_little_endian 1.0\n";
		 outfile << buff;
		 outfile << "comment generated by spaltX\n";
		 if(antialiased)
		 	outfile << "comment splatx.anti_aliasing=1\n";
		 std::string line = "element vertex " + std::to_string(pos.size()) + "\n";
		 outfile << line;

		 outfile << "property float x\n";
		 outfile << "property float y\n";
		 outfile << "property float z\n";

		 outfile << "property float f_dc_0\n";
		 outfile << "property float f_dc_1\n";
		 outfile << "property float f_dc_2\n";

		 for (int i = 0; i < 15; i++)
		 {
			 outfile << "property float f_rest_" + std::to_string(i * 3 + 0) + "\n";
			 outfile << "property float f_rest_" + std::to_string(i * 3 + 1) + "\n";
			 outfile << "property float f_rest_" + std::to_string(i * 3 + 2) + "\n";
		 }
		 outfile << "property float opacity\n";
		 outfile << "property float scale_0\n";
		 outfile << "property float scale_1\n";
		 outfile << "property float scale_2\n";

		 outfile << "property float rot_0\n";
		 outfile << "property float rot_1\n";
		 outfile << "property float rot_2\n";
		 outfile << "property float rot_3\n";

		 outfile << "end_header\n";

		 std::vector<RichPoint>	points(pos.size());

		 parallel_for<size_t>(0, pos.size(), [&](size_t i) {
			 points[i].pos = pos[i];
			 points[i].opacity = opacities[i];
			 points[i].rot = rot[i];
			 points[i].scale = scales[i];

			 points[i].shs[0] = shs[i][0];
			 points[i].shs[1] = shs[i][1];
			 points[i].shs[2] = shs[i][2];
			 for (int j = 1; j < 16; j++)
			 {
				 points[i].shs[(j - 1) + 3] = shs[i][j * 3 + 0];
				 points[i].shs[(j - 1) + 18] = shs[i][j * 3 + 1];
				 points[i].shs[(j - 1) + 33] = shs[i][j * 3 + 2];
			 }
		 });
		 outfile.write(reinterpret_cast<char*>(points.data()), sizeof(RichPoint) * points.size());
		 outfile.close();
		 return true;
	 }

	 bool	save_splat(const std::string& file_path,
		 const std::vector<glm::vec3>& pos,
		 const std::vector<glm::vec3>& scales,
		 const std::vector<std::array<f32, 48>>& shs,
		 const std::vector<glm::vec4>& rot,
		 const std::vector<f32>& opacities)
	 {
		 DataView dataView(pos.size() * 32);
		 parallel_for<size_t>(0, pos.size(), [&](size_t i) {
			 auto v = pos[i];
			 const auto off = i * 32;
			 dataView.setFloat32(off + 0, v.x);
			 dataView.setFloat32(off + 4, v.y);
			 dataView.setFloat32(off + 8, v.z);

			 auto scale = glm::exp(scales[i]);
			 dataView.setFloat32(off + 12, scale.x);
			 dataView.setFloat32(off + 16, scale.y);
			 dataView.setFloat32(off + 20, scale.z);

			 // dataView.setUint32(off + 24, packColor(shs[i][0], shs[i][1], shs[i][2], opacities[i]));
			 auto f_dc_0 = shs[i][0];
			 auto f_dc_1 = shs[i][1];
			 auto f_dc_2 = shs[i][2];
			 const auto SH_C0 = 0.28209479177387814;
			 dataView.setUint8(off + 24, (u8)glm::clamp<f32>((0.5 + SH_C0 * f_dc_0) * 255, 0, 255));
			 dataView.setUint8(off + 25, (u8)glm::clamp<f32>((0.5 + SH_C0 * f_dc_1) * 255, 0, 255));
			 dataView.setUint8(off + 26, (u8)glm::clamp<f32>((0.5 + SH_C0 * f_dc_2) * 255, 0, 255));
			 dataView.setUint8(off + 27, (u8)glm::clamp<f32>((1 / (1 + std::exp(-opacities[i]))) * 255, 0, 255));

			 auto q = glm::normalize(rot[i]);
			 dataView.setUint8(off + 28, std::clamp<f32>(q.x * 128 + 128, 0, 255));
			 dataView.setUint8(off + 29, std::clamp<f32>(q.y * 128 + 128, 0, 255));
			 dataView.setUint8(off + 30, std::clamp<f32>(q.z * 128 + 128, 0, 255));
			 dataView.setUint8(off + 31, std::clamp<f32>(q.w * 128 + 128, 0, 255));
			 });

		 std::ofstream outfile(file_path, std::ios_base::binary);

		 if (!outfile.good())
		 {
			 std::cout << std::format("Unable to find model's splat file, attempted:\n {} ", file_path);
			 outfile.close();
			 return false;
		 }
		 outfile.write(reinterpret_cast<char*>(dataView.data()), dataView.size());
		 outfile.close();
		 return true;
	 }

	 bool	save_compress_ply(const std::string& file_path,
		 const std::vector<glm::vec3>& pos,
		 const std::vector<glm::vec3>& scales,
		 const std::vector<std::array<f32, 48>>& shs,
		 const std::vector<glm::vec4>& rot,
		 const std::vector<f32>& opacities,
		 bool antialiased)
	 {
		 auto numSplats = pos.size();
		 u64 numChunks = (numSplats + 255) / 256;
		 std::vector<u64> indices(numSplats);
		 for (auto i = 0; i < numSplats; i++)
			 indices[i] = i;
		 auto [pmin, pmax] = calcMinMax(pos, indices, 0, indices.size());
		 std::vector<std::pair<uint64_t, int>> mapp(numSplats);
		 parallel_for<size_t>(0, numSplats, [&](size_t i) {
			 glm::vec3 rel = (pos[i] - pmin) / (pmax - pmin);
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
		 DataView dataView(numChunks * 4 * 12 + numSplats * 4 * 4);

		 auto vertexOffset = numChunks * 12 * 4;
		 parallel_for<size_t>(0, numChunks, [&](size_t i) {
			 SplatChunk chunk(indices, i * 256, (i + 1) * 256);

			 auto [pmin, pmax, smin, smax] = chunk.pack(pos, scales, rot, shs, opacities);

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

		 std::ofstream outfile(file_path, std::ios_base::binary);
		 if (!outfile.good())
		 {
			 std::cout << std::format("Unable to find model's PLY file, attempted:\n {} ", file_path);
			 outfile.close();
			 return false;
		 }
		 std::string buff = "ply\n";
		 outfile << buff;
		 buff = "format binary_little_endian 1.0\n";
		 outfile << buff;
		 outfile << "comment generated by diverseshot\n";
		 if(antialiased)
		 	outfile << "comment splatx.anti_aliasing=1\n";
		 std::string line = "element chunk " + std::to_string(numChunks) + "\n";
		 outfile << line;

		 for (auto i = 0; i < 12; i++)
			 outfile << "property float " + chunkProps[i] + "\n";

		 line = "element vertex " + std::to_string(numSplats) + "\n";
		 outfile << line;

		 for (int i = 0; i < 4; i++)
		 {
			 outfile << "property uint " + vertexProps[i] + "\n";
		 }
		 outfile << "end_header\n";

		 outfile.write(reinterpret_cast<char*>(dataView.data()), dataView.size());
		 outfile.close();
		 return true;
	 }

	bool save_reduced_ply(
		const std::string& file_path,
		const std::vector<glm::vec3>& pos,
		const std::vector<glm::vec3>& scales,
		const std::vector<glm::vec3>& featureDc,
		const std::vector<std::array<glm::vec3, 15>>& featureRest,
		const std::vector<glm::vec4>& rot,
		const std::vector<f32>& opacities,
		const std::vector<uint8_t>& degrees,
		std::optional<std::unordered_map<std::string, CodeBook>> codeBookDict,
		bool quantised,
		bool halfFloat)
	{
		std::array<std::vector<int>, 4> deg2Id;
		for (auto shDegree = 0; shDegree < 4; shDegree++)
		{
			auto coeffsNum = (shDegree + 1) * (shDegree + 1) - 1;
			for (auto i = 0; i < pos.size(); i++) {
				if (degrees[i] == shDegree) {
					deg2Id[shDegree].push_back(i);
				}
			}
		}
		size_t numSize = 0;
		auto xyzSize = (halfFloat ? sizeof(glm::u16vec3) : sizeof(glm::vec3));
		for (auto shDegree = 0; shDegree < 4; shDegree++)
		{
			auto coeffsNum = (shDegree + 1) * (shDegree + 1) - 1;
			if (quantised)
				numSize += deg2Id[shDegree].size() * (xyzSize + sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(char) + (1 + coeffsNum) * sizeof(glm::u8vec3));
			else
				numSize += deg2Id[shDegree].size() * (xyzSize + sizeof(glm::vec3) + sizeof(glm::vec4) + sizeof(float) + (1 + coeffsNum) * sizeof(glm::vec3));
		}
		if (quantised) numSize += codeBookDict->at("opacity").centers.size() * sizeof(float) * 20;
		DataView dataView(numSize);
		if (quantised)
		{
			if (!codeBookDict.has_value())
			{
				std::cout << "Clustering codebook missing. Returning without saving\n";
				return false;
			}
			auto opacityIds = codeBookDict->at("opacity").ids;
			auto scalingIds = codeBookDict->at("scaling").ids;
			auto featureDcIds = codeBookDict->at("feature_dc").ids;
			std::array<std::vector<uint8_t>, 15>	featureRestIds;
			for (auto i = 0; i < 15; i++)
				featureRestIds[i] = codeBookDict->at(std::format("feature_rest_{}", i)).ids;
			auto rot = codeBookDict->at("rotation_re").ids;
			auto rot1 = codeBookDict->at("rotation_im").ids;
			std::vector<glm::u8vec4>	rotIds(rot.size());

			parallel_for<size_t>(0, opacityIds.size(), [&](size_t i) {
				rotIds[i] = glm::u8vec4(rot[i], rot1[i * 3], rot1[i * 3 + 1], rot1[i * 3 + 2]);
				});

			auto numEntries = codeBookDict->at("opacity").centers.size();
			auto centerDatastride = sizeof(float) * 20;
			auto centerOffset = deg2Id[0].size() * (sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(glm::u8vec3) + sizeof(char) + xyzSize) +
				deg2Id[1].size() * (sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(glm::u8vec3) + sizeof(char) + xyzSize + 3 * sizeof(glm::u8vec3)) +
				deg2Id[2].size() * (sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(glm::u8vec3) + sizeof(char) + xyzSize + 8 * sizeof(glm::u8vec3)) +
				deg2Id[3].size() * (sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(glm::u8vec3) + sizeof(char) + xyzSize + 15 * sizeof(glm::u8vec3));

			parallel_for<size_t>(0, numEntries, [&](size_t entryId) {
				dataView.setFloat32(centerOffset + entryId * centerDatastride, codeBookDict->at("feature_dc").centers[entryId]);
				for (auto sh = 0; sh < 15; sh++)
					dataView.setFloat32(centerOffset + entryId * centerDatastride + (sh + 1) * sizeof(float), codeBookDict->at(std::format("feature_rest_{}", sh)).centers[entryId]);
				dataView.setFloat32(centerOffset + entryId * centerDatastride + 16 * sizeof(float), codeBookDict->at("opacity").centers[entryId]);
				dataView.setFloat32(centerOffset + entryId * centerDatastride + 17 * sizeof(float), codeBookDict->at("scaling").centers[entryId]);
				dataView.setFloat32(centerOffset + entryId * centerDatastride + 18 * sizeof(float), codeBookDict->at("rotation_re").centers[entryId]);
				dataView.setFloat32(centerOffset + entryId * centerDatastride + 19 * sizeof(float), codeBookDict->at("rotation_im").centers[entryId]);
				});
			size_t offset = 0;
			for (auto shDegree = 0; shDegree < 4; shDegree++)
			{
				auto coeffsNum = (shDegree + 1) * (shDegree + 1) - 1;
				auto stride = xyzSize + sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(u8) + (1 + coeffsNum) * sizeof(glm::u8vec3);
				auto scaleOff = xyzSize + (1 + coeffsNum) * sizeof(glm::u8vec3);
				parallel_for<size_t>(0, deg2Id[shDegree].size(), [&](size_t splatId) {
					auto pointId = deg2Id[shDegree][splatId];
					if (halfFloat)
					{
						auto halfxyz = glm::u16vec3(glm::detail::toFloat16(pos[pointId].x), glm::detail::toFloat16(pos[pointId].y), glm::detail::toFloat16(pos[pointId].z));
						dataView.setData(offset + splatId * stride, (u8*)&halfxyz, sizeof(glm::u16vec3));
					}
					else
					{
						dataView.setData(offset + splatId * stride, (u8*)&pos[pointId], sizeof(glm::vec3));
					}

					dataView.setData(offset + splatId * stride + xyzSize, (u8*)&featureDcIds[pointId * 3 + 0], sizeof(glm::u8vec3));
					for (auto j = 0; j < coeffsNum; j++)
						dataView.setData(offset + splatId * stride + xyzSize + (j + 1) * sizeof(glm::u8vec3), (u8*)&featureRestIds[j][pointId * 3 + 0], sizeof(glm::u8vec3));
					dataView.setUint8(offset + splatId * stride + scaleOff, opacityIds[pointId]);
					dataView.setUint8(offset + splatId * stride + scaleOff + 1, scalingIds[3 * pointId]);
					dataView.setUint8(offset + splatId * stride + scaleOff + 2, scalingIds[3 * pointId + 1]);
					dataView.setUint8(offset + splatId * stride + scaleOff + 3, scalingIds[3 * pointId + 2]);

					dataView.setUint8(offset + splatId * stride + scaleOff + 4, rotIds[pointId].x);
					dataView.setUint8(offset + splatId * stride + scaleOff + 5, rotIds[pointId].y);
					dataView.setUint8(offset + splatId * stride + scaleOff + 6, rotIds[pointId].z);
					dataView.setUint8(offset + splatId * stride + scaleOff + 7, rotIds[pointId].w);
					});
				offset += deg2Id[shDegree].size() * stride;
			}
		}
		else
		{
			size_t offset = 0;
			for (auto shDegree = 0; shDegree < 4; shDegree++)
			{
				auto coeffsNum = (shDegree + 1) * (shDegree + 1) - 1;
				auto stride = xyzSize + sizeof(glm::vec3) + sizeof(glm::vec4) + sizeof(float) + (1 + coeffsNum) * sizeof(glm::vec3);
				auto scaleOff = xyzSize + (1 + coeffsNum) * sizeof(glm::vec3);
				parallel_for<size_t>(0, deg2Id[shDegree].size(), [&](size_t splatId) {
					auto pointId = deg2Id[shDegree][splatId];
					if (halfFloat) {
						auto halfxyz = glm::u16vec3(glm::detail::toFloat16(pos[pointId].x), glm::detail::toFloat16(pos[pointId].y), glm::detail::toFloat16(pos[pointId].z));
						dataView.setData(offset + splatId * stride, (u8*)&halfxyz, sizeof(glm::u16vec3));
					}
					else {
						dataView.setData(offset + splatId * stride, (u8*)&pos[pointId], sizeof(glm::vec3));
					}
					dataView.setData(offset + splatId * stride + xyzSize, (u8*)&featureDc[pointId], sizeof(glm::vec3));
					for (auto j = 0; j < coeffsNum; j++)
						dataView.setData(offset + splatId * stride + xyzSize + (j + 1) * sizeof(glm::vec3), (u8*)&featureRest[pointId][j], sizeof(glm::vec3));
					dataView.setFloat32(offset + splatId * stride + scaleOff + 0 * sizeof(float), opacities[pointId]);
					dataView.setFloat32(offset + splatId * stride + scaleOff + 1 * sizeof(float), scales[pointId].x);
					dataView.setFloat32(offset + splatId * stride + scaleOff + 2 * sizeof(float), scales[pointId].y);
					dataView.setFloat32(offset + splatId * stride + scaleOff + 3 * sizeof(float), scales[pointId].z);

					dataView.setFloat32(offset + splatId * stride + scaleOff + 4 * sizeof(float), rot[pointId].x);
					dataView.setFloat32(offset + splatId * stride + scaleOff + 5 * sizeof(float), rot[pointId].y);
					dataView.setFloat32(offset + splatId * stride + scaleOff + 6 * sizeof(float), rot[pointId].z);
					dataView.setFloat32(offset + splatId * stride + scaleOff + 7 * sizeof(float), rot[pointId].w);
					});
				offset += deg2Id[shDegree].size() * stride;
			}
		}

		std::ofstream outfile(file_path, std::ios_base::binary);
		if (!outfile.good())
		{
			std::cout << std::format("Unable to find model's PLY file, attempted:\n {} ", file_path);
			outfile.close();
			return false;
		}
		std::string buff = "ply\n";
		outfile << buff;
		buff = "format binary_little_endian 1.0\n";
		outfile << buff;
		outfile << "comment generated by diverseshot\n";

		for (auto shDegree = 0; shDegree < 4; shDegree++)
		{
			std::string line = "element vertex " + std::to_string(deg2Id[shDegree].size()) + "\n";
			outfile << line;

			auto coeffsNum = (shDegree + 1) * (shDegree + 1) - 1;
			if (halfFloat)
			{
				outfile << "property f16 x\n";
				outfile << "property f16 y\n";
				outfile << "property f16 z\n";
			}
			else
			{
				outfile << "property float x\n";
				outfile << "property float y\n";
				outfile << "property float z\n";
			}
			if (quantised)
			{
				outfile << "property u8 f_dc_0\n";
				outfile << "property u8 f_dc_1\n";
				outfile << "property u8 f_dc_2\n";
				for (int i = 0; i < coeffsNum; i++)
				{
					outfile << "property u8 f_rest_" + std::to_string(i * 3 + 0) + "\n";
					outfile << "property u8 f_rest_" + std::to_string(i * 3 + 1) + "\n";
					outfile << "property u8 f_rest_" + std::to_string(i * 3 + 2) + "\n";
				}
				outfile << "property u8 opacity\n";
				outfile << "property u8 scale_0\n";
				outfile << "property u8 scale_1\n";
				outfile << "property u8 scale_2\n";

				outfile << "property u8 rot_0\n";
				outfile << "property u8 rot_1\n";
				outfile << "property u8 rot_2\n";
				outfile << "property u8 rot_3\n";
			}
			else
			{
				outfile << "property float f_dc_0\n";
				outfile << "property float f_dc_1\n";
				outfile << "property float f_dc_2\n";
				for (int i = 0; i < coeffsNum; i++)
				{
					outfile << "property float f_rest_" + std::to_string(i * 3 + 0) + "\n";
					outfile << "property float f_rest_" + std::to_string(i * 3 + 1) + "\n";
					outfile << "property float f_rest_" + std::to_string(i * 3 + 2) + "\n";
				}
				outfile << "property float opacity\n";
				outfile << "property float scale_0\n";
				outfile << "property float scale_1\n";
				outfile << "property float scale_2\n";

				outfile << "property float rot_0\n";
				outfile << "property float rot_1\n";
				outfile << "property float rot_2\n";
				outfile << "property float rot_3\n";
			}
		}
		if (quantised)
		{
			std::string line = "element cook_center " + std::to_string(codeBookDict->at("opacity").centers.size()) + "\n";
			outfile << line;
			outfile << "property float f_dc\n";
			for (int i = 0; i < 15; i++)
			{
				outfile << "property float f_rest_" + std::to_string(i) + "\n";
			}
			outfile << "property float opacity\n";
			outfile << "property float scale\n";
			outfile << "property float rot_re\n";
			outfile << "property float rot_im\n";
		}
		outfile << "end_header\n";
		outfile.write(reinterpret_cast<char*>(dataView.data()), dataView.size());
		outfile.close();
		return true;
	}

	bool load_ply(const std::string& file_path,
		std::vector<RichPoint>& points,
		bool& antialiased)
	{
		u64 numSplats = 0;
		std::ifstream infile(file_path, std::ios_base::binary);
		if (!infile.good())
		{
			std::cout << std::format("Unable to find model's PLY file, attempted:\n {} ", file_path);
			infile.close();
			return false;
		}
		// "Parse" header (it has to be a specific format anyway)
		std::string buff;
		std::string dummy;
		u32 vertex_stride = 0;
		bool is_vertex_block = false;
		bool is_2dgs = true;
		auto get_vertex_bytes = [](const std::string& types)->u32 {
			if (types == "float") {
				return 4;
			}
			else if (types == "uint") {
				return 4;
			}
			throw std::runtime_error(std::format("encounter unrecognized type {}", types));
			return 4;
		};
		std::unordered_map<std::string,u32> vertex_offset_map;
		while (std::getline(infile, buff)) {
			if (buff.compare("end_header") == 0)
				break;
			if(buff.find("anti_aliasing=1") != std::string::npos){
				antialiased = true;
			}
			if (buff.find("element vertex") != std::string::npos) {
				std::stringstream ss(buff);
				ss >> dummy >> dummy >> numSplats;
				is_vertex_block = true;
			}
			if (buff.find("property") != std::string::npos) {
				std::stringstream ss(buff);
				std::string types, name;
				ss >> dummy >> types >> name;
				vertex_offset_map[name] = vertex_stride;
				if (is_vertex_block) {
					vertex_stride += get_vertex_bytes(types);
				}
			}
			if (buff.find("scale_2") != std::string::npos) {
				is_2dgs = false;
			}
		}
		std::cout << std::format("Loading {}  Gaussian splats\n", numSplats);
		if (numSplats <= 0) return false;
		// Read all Gaussians at once (AoS)
		points.resize(numSplats);
		{
			std::vector<u8> datas(numSplats * vertex_stride);
			infile.read((char*)datas.data(), numSplats * vertex_stride);
			const auto udata = (char*)(datas.data());
			parallel_for<size_t>(0, numSplats, [&](size_t splat_id) {
				auto x = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["x"]);
				auto y = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["y"]);
				auto z = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["z"]);
				points[splat_id].pos = glm::vec3(x,y,z);
				if (is_2dgs)
					points[splat_id].scale.z = std::log(1e-6f);
				else
				{
					auto scale_0 = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["scale_0"]);
					auto scale_1 = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["scale_1"]);
					auto scale_2 = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["scale_2"]);
					points[splat_id].scale = glm::vec3(scale_0, scale_1, scale_2);
				}
				points[splat_id].shs[0] = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["f_dc_0"]);
				points[splat_id].shs[1] = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["f_dc_1"]);
				points[splat_id].shs[2] = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["f_dc_2"]);
				points[splat_id].opacity = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["opacity"]);
				points[splat_id].rot[0] = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["rot_0"]);
				points[splat_id].rot[1] = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["rot_1"]);
				points[splat_id].rot[2] = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["rot_2"]);
				points[splat_id].rot[3] = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["rot_3"]);
				for (int i = 0; i < 45; i++)
				{
					points[splat_id].shs[3 + i ] = *(float*)(udata + splat_id * vertex_stride + vertex_offset_map["f_rest_" + std::to_string(i)]);
				}
			});
		}
		infile.close();
	}

	bool load_splat(const std::string& file_path,
		std::vector<RichPoint>& points)
	{
		u64 numSplats = 0;
		std::ifstream file(file_path, std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			auto dataSize = (size_t)file.tellg();
			file.seekg(0, file.beg);
			DataView dataView(dataSize);
			auto udata = (char*)(dataView.data());
			file.read(udata, dataSize);
			file.close();

			numSplats = dataSize / 32;
			if (numSplats <= 0) return false;
			points.resize(numSplats);
			parallel_for<size_t>(0, numSplats, [&](size_t i) {
				const auto off = i * 32;
				points[i].pos = glm::vec3(dataView.getFloat32(off + 0), dataView.getFloat32(off + 4), dataView.getFloat32(off + 8));
				points[i].scale = glm::log(glm::vec3(dataView.getFloat32(off + 12), dataView.getFloat32(off + 16), dataView.getFloat32(off + 20)));

				const auto SH_C0 = 0.28209479177387814f;
				points[i].shs = { 0 };
				for (int j = 0; j < 3; j++)
				{
					points[i].shs[j] = (dataView.getUint8(off + 24 + j) / 255.0 - 0.5) / SH_C0;
				}
				const auto opacity = dataView.getUint8(off + 27) / 255.0f;
				points[i].opacity = std::log(opacity / (1 - opacity));

				for (int j = 0; j < 4; j++)
				{
					auto q = dataView.getUint8(off + 28 + j);
					points[i].rot[j] = (q - 128) / 128.0f;
				}
			});
			return true;
		}
		return false;
	}

	bool load_compress_ply(const std::string& file_path,
		std::vector<RichPoint>& points,
		bool& antialiased)
	{
		u64 numSplats;
		std::ifstream infile(file_path, std::ios_base::binary);
		if (!infile.good())
		{
			std::cout << std::format("Unable to find model's PLY file, attempted:\n {} ", file_path);
			infile.close();
			return false;
		}

		std::string buff;
		std::string dummy;
		u64 numChunks = 0;
		while (std::getline(infile, buff)) {
			if (buff.compare("end_header") == 0)
				break;
			if (buff.find("element vertex") != std::string::npos) {
				std::stringstream ss(buff);
				ss >> dummy >> dummy >> numSplats;
			}
			if (buff.find("element chunk") != std::string::npos) {
				std::stringstream ss(buff);
				ss >> dummy >> dummy >> numChunks;
			}
			if(buff.find("anti_aliasing=1") != std::string::npos){
				antialiased = true;
			}
		}
		std::cout << std::format("Loading {} Gaussian splats chunk , {} count\n", numChunks, numSplats);
		if (numSplats <= 0 || numChunks <= 0) return false;
		DataView dataView(numChunks * 4 * 12 + numSplats * 4 * 4);
		auto udata = (char*)(dataView.data());
		infile.read(udata, dataView.size());
		infile.close();

		points.resize(numSplats);
		auto vertexOffset = numChunks * 12 * 4;

		std::vector<u64> indices(numSplats);
		for (auto i = 0; i < numSplats; i++)
			indices[i] = i;
		parallel_for<size_t>(0, numChunks, [&](size_t i) {
			SplatChunk chunk(indices, i * 256, (i + 1) * 256);
			chunk.unpack(dataView, i, numChunks, points);
		});
		return true;
	}

	bool load_reduced_ply(const std::string& file_path,
		std::vector<RichPoint>& points)
	{
		std::ifstream infile(file_path, std::ios_base::binary);
		if (!infile.good())
		{
			std::cout << std::format("Unable to find model's PLY file, attempted:\n {} ", file_path);
			infile.close();
			return false;
		}
		auto get_vertex_bytes = [](const std::string& types)->u32 {
			if (types == "float") {
				return 4;
			}
			else if (types == "uint") {
				return 4;
			}
			else if (types == "u8") {
				return 1;
			}
			else if (types == "f16") {
				return 2;
			}
			throw std::runtime_error(std::format("encounter unrecognized type {}", types));
			return 4;
		};
		std::string buff;
		std::string dummy;
		u32 vertex_stride = 0;
		std::array<u64, 4> numSplats;
		int vetex_block_id = 0;
		bool halfFloat = false;
		bool quantised = false;
		u64 numEntries = 0;
		while (std::getline(infile, buff)) {
			if (buff.compare("end_header") == 0)
				break;
			if (buff.find("element vertex") != std::string::npos) {
				std::stringstream ss(buff);
				ss >> dummy >> dummy >> numSplats[vetex_block_id];
				vetex_block_id++;
				getline(infile, buff);
				if (buff.find("f16") != std::string::npos) {
					halfFloat = true;
				}
			}
			if (buff.find("property") != std::string::npos) {
				std::stringstream ss(buff);
				std::string types, name;
				ss >> dummy >> types >> name;
				if (vetex_block_id > 0) {
					auto vbytes = get_vertex_bytes(types);
					vertex_stride += vbytes;
					quantised |= vbytes == 1 ? true : false;
				}
			}
			if (buff.find("element cook_center") != std::string::npos) {
				std::stringstream ss(buff);
				ss >> dummy >> dummy >> numEntries;
			}
		}
		std::cout << std::format("Loading vertex block 0: {} splats\n \
			vertex block 1: {} splats\n vertex block 2: {} splats\n vertex block 3: {} splats\n", numSplats[0], numSplats[1], numSplats[2], numSplats[3]);
		if (numSplats[0] <= 0 && numSplats[1] <= 0 && numSplats[2] <= 0 && numSplats[3] <= 0) return false;
		points.resize(numSplats[0] + numSplats[1] + numSplats[2] + numSplats[3]);

		size_t numSize = 0;
		auto xyzSize = (halfFloat ? sizeof(glm::u16vec3) : sizeof(glm::vec3));
		for (auto shDegree = 0; shDegree < 4; shDegree++)
		{
			auto coeffsNum = (shDegree + 1) * (shDegree + 1) - 1;
			if (quantised)
				numSize += numSplats[shDegree] * (xyzSize + sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(char) + (1 + coeffsNum) * sizeof(glm::u8vec3));
			else
				numSize += numSplats[shDegree] * (xyzSize + sizeof(glm::vec3) + sizeof(glm::vec4) + sizeof(float) + (1 + coeffsNum) * sizeof(glm::vec3));
		}
		if (quantised) numSize += numEntries * sizeof(float) * 20;

		DataView dataView(numSize);
		auto udata = (char*)(dataView.data());
		infile.read(udata, dataView.size());
		infile.close();

		auto centerOffset = numSplats[0] * (sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(glm::u8vec3) + sizeof(char) + xyzSize) +
			numSplats[1] * (sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(glm::u8vec3) + sizeof(char) + xyzSize + 3 * sizeof(glm::u8vec3)) +
			numSplats[2] * (sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(glm::u8vec3) + sizeof(char) + xyzSize + 8 * sizeof(glm::u8vec3)) +
			numSplats[3] * (sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(glm::u8vec3) + sizeof(char) + xyzSize + 15 * sizeof(glm::u8vec3));
		auto centerDatastride = sizeof(float) * 20;
		size_t offset = 0;
		for (auto shDegree = 0; shDegree < 4; shDegree++)
		{
			auto coeffsNum = (shDegree + 1) * (shDegree + 1) - 1;
			u32 stride = 0;
			if (quantised)
				stride = (xyzSize + sizeof(glm::u8vec3) + sizeof(glm::u8vec4) + sizeof(u8) + (1 + coeffsNum) * sizeof(glm::u8vec3));
			else
				stride = (xyzSize + sizeof(glm::vec3) + sizeof(glm::vec4) + sizeof(float) + (1 + coeffsNum) * sizeof(glm::vec3));
			auto scaleOff = quantised ? (xyzSize + (1 + coeffsNum) * sizeof(glm::u8vec3)) : (xyzSize + (1 + coeffsNum) * sizeof(glm::vec3));
			u64 pointOffset = 0;
			for (auto i = shDegree; i > 0; i--)
				pointOffset += numSplats[i - 1];
			parallel_for<size_t>(0, numSplats[shDegree], [&](size_t splatId) {
				auto pointId = splatId + pointOffset;
				if (halfFloat)
				{
					points[pointId].pos = glm::vec3(glm::detail::toFloat32(dataView.getU16(offset + splatId * stride)),
						glm::detail::toFloat32(dataView.getU16(offset + splatId * stride + 1 * sizeof(glm::u16))),
						glm::detail::toFloat32(dataView.getU16(offset + splatId * stride + 2 * sizeof(glm::u16))));
				}
				else
					points[pointId].pos = glm::vec3(dataView.getFloat32(offset + splatId * stride),
						dataView.getFloat32(offset + splatId * stride + 1 * sizeof(float)),
						dataView.getFloat32(offset + splatId * stride + 2 * sizeof(float)));

				if (quantised)
				{
					auto featureDcId = glm::u8vec3(dataView.getUint8(offset + splatId * stride + xyzSize),
						dataView.getUint8(offset + splatId * stride + xyzSize + 1),
						dataView.getUint8(offset + splatId * stride + xyzSize + 2));

					for (auto sh = 0; sh < 3; sh++)
						points[pointId].shs[sh] = dataView.getFloat32(centerOffset + centerDatastride * featureDcId[sh]);
					for (auto j = 0; j < coeffsNum; j++)
					{
						auto featureRestId = glm::u8vec3(dataView.getUint8(offset + splatId * stride + xyzSize + (j + 1) * sizeof(glm::u8vec3)),
							dataView.getUint8(offset + splatId * stride + xyzSize + (j + 1) * sizeof(glm::u8vec3) + 1),
							dataView.getUint8(offset + splatId * stride + xyzSize + (j + 1) * sizeof(glm::u8vec3) + 2));
						points[pointId].shs[j * 3 + 3 + 0] = dataView.getFloat32(centerOffset + sizeof(float) * (j + 1) + featureRestId[0] * centerDatastride);
						points[pointId].shs[j * 3 + 3 + 1] = dataView.getFloat32(centerOffset + sizeof(float) * (j + 1) + featureRestId[1] * centerDatastride);
						points[pointId].shs[j * 3 + 3 + 2] = dataView.getFloat32(centerOffset + sizeof(float) * (j + 1) + featureRestId[2] * centerDatastride);
					}
					auto opcaityId = dataView.getUint8(offset + splatId * stride + scaleOff);
					auto scalingId = glm::u8vec3(dataView.getUint8(offset + splatId * stride + scaleOff + 1),
						dataView.getUint8(offset + splatId * stride + scaleOff + 2),
						dataView.getUint8(offset + splatId * stride + scaleOff + 3));

					auto rotId = glm::u8vec4(dataView.getUint8(offset + splatId * stride + scaleOff + 4),
						dataView.getUint8(offset + splatId * stride + scaleOff + 5),
						dataView.getUint8(offset + splatId * stride + scaleOff + 6),
						dataView.getUint8(offset + splatId * stride + scaleOff + 7));

					points[pointId].opacity = dataView.getFloat32(centerOffset + sizeof(float) * 16 + opcaityId * centerDatastride);
					points[pointId].scale = glm::vec3(dataView.getFloat32(centerOffset + sizeof(float) * 17 + scalingId[0] * centerDatastride),
						dataView.getFloat32(centerOffset + sizeof(float) * 17 + scalingId[1] * centerDatastride),
						dataView.getFloat32(centerOffset + sizeof(float) * 17 + scalingId[2] * centerDatastride));

					points[pointId].rot = glm::vec4(dataView.getFloat32(centerOffset + sizeof(float) * 18 + rotId[0] * centerDatastride),
						dataView.getFloat32(centerOffset + sizeof(float) * 19 + rotId[1] * centerDatastride),
						dataView.getFloat32(centerOffset + sizeof(float) * 19 + rotId[2] * centerDatastride),
						dataView.getFloat32(centerOffset + sizeof(float) * 19 + rotId[3] * centerDatastride));
				}
				else
				{
					for (auto sh = 0; sh < 3; sh++)
						points[pointId].shs[sh] = dataView.getFloat32(offset + splatId * stride + xyzSize + sh * sizeof(float));
					for (auto sh = 0; sh < coeffsNum; sh++)
					{
						points[pointId].shs[sh * 3 + 3 + 0] = dataView.getFloat32(offset + splatId * stride + xyzSize + (sh + 1) * sizeof(glm::vec3) + 0);
						points[pointId].shs[sh * 3 + 3 + 1] = dataView.getFloat32(offset + splatId * stride + xyzSize + (sh + 1) * sizeof(glm::vec3) + 4);
						points[pointId].shs[sh * 3 + 3 + 2] = dataView.getFloat32(offset + splatId * stride + xyzSize + (sh + 1) * sizeof(glm::vec3) + 8);
					}
					points[pointId].opacity = dataView.getFloat32(offset + splatId * stride + scaleOff);
					points[pointId].scale = glm::vec3(dataView.getFloat32(offset + splatId * stride + scaleOff + 1 * sizeof(float)),
						dataView.getFloat32(offset + splatId * stride + scaleOff + 2 * sizeof(float)),
						dataView.getFloat32(offset + splatId * stride + scaleOff + 3 * sizeof(float)));

					points[pointId].rot = glm::vec4(dataView.getFloat32(offset + splatId * stride + scaleOff + 4 * sizeof(float)),
						dataView.getFloat32(offset + splatId * stride + scaleOff + 5 * sizeof(float)),
						dataView.getFloat32(offset + splatId * stride + scaleOff + 6 * sizeof(float)),
						dataView.getFloat32(offset + splatId * stride + scaleOff + 7 * sizeof(float)));
				}
				});
			offset += numSplats[shDegree] * stride;
		}
		return true;
	}

	bool save_dvs_splat(
		const std::string& file_path,
		const std::vector<glm::vec3>& pos,
		const std::vector<glm::vec3>& scales,
		const std::vector<glm::vec3>& featureDc,
		const std::vector<std::array<glm::vec3, 15>>& featureRest,
		const std::vector<glm::vec4>& rot,
		const std::vector<f32>& opacities,
		const std::vector<uint8_t>& degrees)
	{
		auto numSplats = pos.size();
		u64 numChunks = (numSplats + 255) / 256;
		std::vector<u64> indices(numSplats);
		for (auto i = 0; i < numSplats; i++)
			indices[i] = i;
		auto [pmin, pmax] = calcMinMax(pos, indices, 0, indices.size());
		std::vector<std::pair<uint64_t, int>> mapp(numSplats);

		parallel_for<size_t>(0, numSplats, [&](size_t i) {
			glm::vec3 rel = (pos[i] - pmin) / (pmax - pmin);
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

		//numsplats,numchunks, 4 vertex elements, 1 quatized entry num
		const auto headerSize = sizeof(DvsSplatHeader);
		size_t excludePosDataSize = 0;
		std::array<std::vector<int>, 4> deg2Id;
		for (auto shDegree = 0; shDegree < 4; shDegree++)
		{
			auto coeffsNum = (shDegree + 1) * (shDegree + 1) - 1;
			for (auto i = 0; i < pos.size(); i++)
				if (degrees[i] == shDegree)
					deg2Id[shDegree].push_back(i);
			//scale, rot, opacity, shs
			excludePosDataSize += deg2Id[shDegree].size() * (sizeof(glm::u8vec3) + sizeof(glm::u8vec3) + sizeof(char) + (1 + coeffsNum) * sizeof(glm::u8vec3));
		}
		auto posDataSize = numChunks * 4 * 6 + numSplats * 4 * 1;
		DataView dataView(posDataSize + headerSize + excludePosDataSize);
		auto quatisizedDataOffset = posDataSize + headerSize;
		auto vertexOffset = numChunks * 6 * 4 + headerSize;
		dataView.setUint32(0, numSplats);
		dataView.setUint32(4, numChunks);
		for (auto i = 0; i < 4; i++)
			dataView.setUint32(8 + i * 4, deg2Id[i].size());
		//set flag
		dataView.setUint32(sizeof(DvsSplatHeader) - 4, 0);

		parallel_for<size_t>(0, numChunks, [&](size_t i) {
			SplatChunk chunk(indices, i * 256, (i + 1) * 256);

			auto [pmin, pmax] = chunk.pack_pos(pos);

			dataView.setFloat32(headerSize + i * 6 * 4 + 0, pmin.x);
			dataView.setFloat32(headerSize + i * 6 * 4 + 4, pmin.y);
			dataView.setFloat32(headerSize + i * 6 * 4 + 8, pmin.z);
			dataView.setFloat32(headerSize + i * 6 * 4 + 12, pmax.x);
			dataView.setFloat32(headerSize + i * 6 * 4 + 16, pmax.y);
			dataView.setFloat32(headerSize + i * 6 * 4 + 20, pmax.z);

			// write splat data
			auto offset = vertexOffset + i * 256 * 4 * 1;
			const auto chunkSplats = std::min<u64>(numSplats, (i + 1) * 256) - i * 256;
			for (auto j = 0; j < chunkSplats; ++j)
				dataView.setUint32(offset + j * 4 * 1 + 0, chunk.position[j]);
		});
		u64 offset = quatisizedDataOffset;
		for (auto shDegree = 0; shDegree < 4; shDegree++)
		{
			auto coeffsNum = (shDegree + 1) * (shDegree + 1) - 1;
			auto stride = sizeof(glm::u8vec3) + sizeof(glm::u8vec3) + sizeof(u8) + (1 + coeffsNum) * sizeof(glm::u8vec3);

			parallel_for<size_t>(0, deg2Id[shDegree].size(), [&](size_t splatId) {
				auto pointId = indices[deg2Id[shDegree][splatId]];
				glm::u8vec3 quantizeScale = glm::u8vec3(toUint8((scales[pointId].x + 10.0f) * 16.0f),
					toUint8((scales[pointId].y + 10.0f) * 16.0f),
					toUint8((scales[pointId].z + 10.0f) * 16.0f));
				dataView.setData(offset + splatId * stride, (u8*)&quantizeScale, sizeof(glm::u8vec3));
				u8 quatizedOpacity = toUint8(sigmoid(opacities[pointId]) * 255.0f);
				auto q = glm::normalize(glm::vec4(rot[pointId]));
				q = q * (q[0] < 0 ? -127.5f : 127.5f);
				q = q + glm::vec4(127.5f, 127.5f, 127.5f, 127.5f);
				glm::u8vec3 quatizedRot = glm::u8vec3(toUint8(q[1]), toUint8(q[2]), toUint8(q[3]));
				dataView.setData(offset + splatId * stride + 3, (u8*)&quatizedRot, sizeof(glm::u8vec3));
				dataView.setUint8(offset + splatId * stride + 6, quatizedOpacity);
				glm::u8vec3 quantizeColors = glm::u8vec3(toUint8(featureDc[pointId].x * (colorScale * 255.0f) + (0.5f * 255.0f)),
					toUint8(featureDc[pointId].y * (colorScale * 255.0f) + (0.5f * 255.0f)),
					toUint8(featureDc[pointId].z * (colorScale * 255.0f) + (0.5f * 255.0f)));
				dataView.setData(offset + splatId * stride + 7, (u8*)(&quantizeColors), sizeof(glm::u8vec3));
				constexpr int sh1Bits = 5;
				constexpr int shRestBits = 4;
				for (auto j = 0; j < coeffsNum * 3; j++)
				{
					u8 qsh;
					float* rest = (float*)featureRest[pointId].data();
					if (j < 9)
						qsh = quantizeSH(rest[j], 1 << (8 - sh1Bits));
					else
						qsh = quantizeSH(rest[j], 1 << (8 - shRestBits));
					dataView.setUint8(offset + splatId * stride + 10 + j, qsh);
				}
			});
			offset += deg2Id[shDegree].size() * stride;
		}
		std::ofstream outfile(file_path, std::ios::binary | std::ios::ate);
		if (!outfile.good())
		{
			std::cout << std::format("Unable to find model's splat file, attempted:\n {} ", file_path);
			outfile.close();
			return false;
		}
		outfile.write(reinterpret_cast<char*>(dataView.data()), dataView.size());
		outfile.close();
		return true;
	}

	bool load_dvs_splat(const std::string& file_path,
		std::vector<RichPoint>& points)
	{
		std::ifstream infile(file_path, std::ios::binary | std::ios::ate);
		if (!infile.good())
		{
			std::cout << std::format("Unable to find model's PLY file, attempted:\n {} ", file_path);
			infile.close();
			return false;
		}
		auto dataSize = (size_t)infile.tellg();
		infile.seekg(0, infile.beg);
		DataView dataView(dataSize);
		auto udata = (char*)(dataView.data());
		infile.read(udata, dataSize);
		infile.close();

		auto numSplats = dataView.getUint32(0);
		auto numChunks = dataView.getUint32(4);
		if (numSplats <= 0) return false;

		auto headerSize = sizeof(DvsSplatHeader);
		points.resize(numSplats);
		std::array<u32, 4> numSplatsArray;
		for (auto i = 0; i < 4; i++)
			numSplatsArray[i] = dataView.getUint32(8 + i * 4);
		std::vector<u64> indices(numSplats);
		for (auto i = 0; i < numSplats; i++)
			indices[i] = i;
		parallel_for<size_t>(0, numChunks, [&](size_t i) {
			SplatChunk chunk(indices, i * 256, (i + 1) * 256);
			chunk.unpack_pos(dataView, i, numChunks, headerSize, points);
		});
		auto posDataSize = numChunks * 4 * 6 + numSplats * 4 * 1;
		auto quatisizedDataOffset = posDataSize + headerSize;
		size_t offset = quatisizedDataOffset;

		for (auto shDegree = 0; shDegree < 4; shDegree++)
		{
			auto coeffsNum = (shDegree + 1) * (shDegree + 1) - 1;
			u32 stride = (sizeof(glm::u8vec3) + sizeof(glm::u8vec3) + sizeof(u8) + (1 + coeffsNum) * sizeof(glm::u8vec3));
			auto scaleOff = (1 + coeffsNum) * sizeof(glm::u8vec3);
			u64 pointOffset = 0;
			for (auto i = shDegree; i > 0; i--)
				pointOffset += numSplatsArray[i - 1];

			parallel_for<size_t>(0, numSplatsArray[shDegree], [&](size_t splatId) {
				auto pointId = splatId + pointOffset;
				points[pointId].scale = glm::u8vec3(dataView.getUint8(offset + splatId * stride),
					dataView.getUint8(offset + splatId * stride + 1),
					dataView.getUint8(offset + splatId * stride + 2));
				points[pointId].scale = points[pointId].scale / 16.0f - 10.0f;
				auto r = glm::u8vec3(dataView.getUint8(offset + splatId * stride + 3),
					dataView.getUint8(offset + splatId * stride + 4),
					dataView.getUint8(offset + splatId * stride + 5));

				glm::vec3 xyz = (glm::vec3{ static_cast<float>(r[0]), static_cast<float>(r[1]), static_cast<float>(r[2]) } *1.0f / 127.5f) + glm::vec3{ -1, -1, -1 };
				points[pointId].rot = glm::vec4(std::sqrt(std::max(0.0f, 1.0f - glm::dot(xyz, xyz))),
					xyz.x, xyz.y, xyz.z);
				points[pointId].opacity = invSigmoid(dataView.getUint8(offset + splatId * stride + 6) / 255.0f);
				for (size_t i = 0; i < 3; i++) {
					points[pointId].shs[i] = (dataView.getUint8(offset + splatId * stride + 7 + i) / 255.0f - 0.5f) / colorScale;
				}
				for (auto j = 0; j < coeffsNum * 3; j++)
				{
					points[pointId].shs[3 + j] = unquantizeSH(dataView.getUint8(offset + splatId * stride + 10 + j));
				}
			});
			offset += numSplatsArray[shDegree] * stride;
		}
		return true;
	}

	bool load_spz_splats(
		const std::string& file_path,
		std::vector<RichPoint>& points,
		bool& antialiased)
	{
		bool load_ret = false;
		try {
			spz::UnpackOptions opts;
			auto spz_pc = spz::loadSpz(file_path,opts);
			points.resize(spz_pc.numPoints);
			for (auto p = 0; p < spz_pc.numPoints; p++) {
				points[p].opacity = spz_pc.alphas[p];
				points[p].pos = glm::vec3(spz_pc.positions[p * 3], spz_pc.positions[p * 3 + 1], spz_pc.positions[p * 3 + 2]);
				points[p].rot = glm::vec4(spz_pc.rotations[p * 4 + 3], spz_pc.rotations[p * 4], spz_pc.rotations[p * 4 + 1], spz_pc.rotations[p * 4 + 2]);
				points[p].scale = glm::vec3(spz_pc.scales[p * 3], spz_pc.scales[p * 3 + 1], spz_pc.scales[p * 3 + 2]);
				points[p].shs[0] = spz_pc.colors[p * 3 + 0];
				points[p].shs[1] = spz_pc.colors[p * 3 + 1];
				points[p].shs[2] = spz_pc.colors[p * 3 + 2];
				for (int j = 0; j < 15; j++) {
					points[p].shs[j * 3 + 0 + 3] = spz_pc.sh[(p * 15 + j) * 3 + 0];
					points[p].shs[j * 3 + 1 + 3] = spz_pc.sh[(p * 15 + j) * 3 + 1];
					points[p].shs[j * 3 + 2 + 3] = spz_pc.sh[(p * 15 + j) * 3 + 2];
				}
			}
			load_ret = true;
			antialiased = spz_pc.antialiased;
		}
		catch (...) {
			load_ret = false;
		}
		return load_ret;
	}

	bool save_spz_splats(
		const std::string& file_path,
		const std::vector<glm::vec3>& pos,
		const std::vector<glm::vec3>& scales,
		const std::vector<std::array<f32, 48>>& shs,
		const std::vector<glm::vec4>& rot,
		const std::vector<f32>& opacities,
		bool antialiased
	) {
		spz::GaussianCloud spz_pc;
		spz_pc.alphas = opacities;
		spz_pc.numPoints = pos.size();
		spz_pc.positions.resize(pos.size() * 3);
		spz_pc.rotations.resize(rot.size() * 4);
		spz_pc.scales.resize(scales.size() * 3);
		spz_pc.colors.resize(scales.size() * 3);
		spz_pc.shDegree = 3;
		spz_pc.antialiased = antialiased;
		spz_pc.sh.resize(45 * pos.size());
		memcpy(spz_pc.positions.data(), pos.data(), pos.size() * 3 * sizeof(float));
		//memcpy(spz_pc.rotations.data(), new_rot.data(), new_rot.size() * 4 * sizeof(float));
		memcpy(spz_pc.scales.data(), scales.data(), scales.size() * 3 * sizeof(float));
		for (auto p = 0; p < spz_pc.numPoints; p++) {
			spz_pc.colors[p * 3 + 0] = shs[p][0];
			spz_pc.colors[p * 3 + 1] = shs[p][1];
			spz_pc.colors[p * 3 + 2] = shs[p][2];
			spz_pc.rotations[p * 4 + 0] = rot[p][1];
			spz_pc.rotations[p * 4 + 1] = rot[p][2];
			spz_pc.rotations[p * 4 + 2] = rot[p][3];
			spz_pc.rotations[p * 4 + 3] = rot[p][0];
		}
		for (auto p = 0; p < spz_pc.numPoints; p++) {
			for (auto j = 0; j < 15; j++) {
				spz_pc.sh[(p * 15 + j) + 0] = shs[p][3 + j * 3];
				spz_pc.sh[(p * 15 + j) + 1] = shs[p][3 + j * 3 + 1];
				spz_pc.sh[(p * 15 + j) + 2] = shs[p][3 + j * 3 + 2];
			}
		}
		spz::PackOptions opts;
		return spz::saveSpz(spz_pc, opts,file_path);
	}
 }