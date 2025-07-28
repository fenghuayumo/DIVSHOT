#pragma once

#include <vector>
#include <cstring>
#include <string>
#include <memory>
#include <array>
#include <algorithm>
#include <glm/glm.hpp>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <thread>
#include <random>
#include <future>
#include <sstream>
#include <deque>

#ifdef _WIN32
#pragma warning(disable : 4251)
#ifdef GS_DYNAMIC
#ifdef GS_ENGINE
#define GS_EXPORT __declspec(dllexport)
#else
#define GS_EXPORT __declspec(dllimport)
#endif
#else
#define GS_EXPORT
#endif
#define GS_HIDDEN
#else
#define GS_EXPORT __attribute__((visibility("default")))
#define GS_HIDDEN __attribute__((visibility("hidden")))
#endif

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using u16 = unsigned short;

namespace tinygsplat 
{
	template <typename T>
	void wait_all(T&& futures) {
		for (auto& f : futures) {
			f.get();
		}
	}

	template <typename T>
	std::vector<T> wait_and_get_all(std::vector<std::future<T>>&& futures) {
		std::vector<T> res;
		for (auto& f : futures) {
			f.wait();
			res.push_back(f.get());
		}
		return res;
	}


	class ThreadPool {
	public:
		ThreadPool()
			: ThreadPool{ std::thread::hardware_concurrency() } {}
		ThreadPool(size_t max_num_threads, bool force = false)
		{
			if (!force) {
				max_num_threads = std::min((size_t)std::thread::hardware_concurrency(), max_num_threads);
			}
			start_threads(max_num_threads);
		}
		virtual ~ThreadPool()
		{
			wait_until_queue_completed();
			shutdown_threads(m_threads.size());
		}

		template <class F>
		auto enqueue_task(F&& f, bool high_priority = false) -> std::future<std::invoke_result_t<F>> {
			using return_type = std::invoke_result_t<F>;

			auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));

			auto res = task->get_future();

			{
				std::lock_guard<std::mutex> lock{ m_task_queue_mutex };

				if (high_priority) {
					m_task_queue.emplace_front([task]() { (*task)(); });
				}
				else {
					m_task_queue.emplace_back([task]() { (*task)(); });
				}
			}

			m_worker_condition.notify_one();
			return res;
		}

		void start_threads(size_t num)
		{
			m_num_threads += num;
			for (size_t i = m_threads.size(); i < m_num_threads; ++i) {
				m_threads.emplace_back([this, i] {
					while (true) {
						std::unique_lock<std::mutex> lock{ m_task_queue_mutex };

						// look for a work item
						while (i < m_num_threads && m_task_queue.empty()) {
							// if there are none, signal that the queue is completed
							// and wait for notification of new work items.
							m_task_queue_completed_condition.notify_all();
							m_worker_condition.wait(lock);
						}

						if (i >= m_num_threads) {
							break;
						}

						std::function<void()> task{ move(m_task_queue.front()) };
						m_task_queue.pop_front();

						// Unlock the lock, so we can process the task without blocking other threads
						lock.unlock();

						task();
					}
					});
			}
		}
		void shutdown_threads(size_t num)
		{
			auto num_to_close = std::min(num, m_num_threads);

			{
				std::lock_guard<std::mutex> lock{ m_task_queue_mutex };
				m_num_threads -= num_to_close;
			}

			// Wake up all the threads to have them quit
			m_worker_condition.notify_all();
			for (auto i = 0u; i < num_to_close; ++i) {
				m_threads.back().join();
				m_threads.pop_back();
			}
		}
		void set_n_threads(size_t num)
		{
			if (m_num_threads > num) {
				shutdown_threads(m_num_threads - num);
			}
			else if (m_num_threads < num) {
				start_threads(num - m_num_threads);
			}
		}

		void wait_until_queue_completed()
		{
			std::unique_lock<std::mutex> lock{ m_task_queue_mutex };
			m_task_queue_completed_condition.wait(lock, [this]() { return m_task_queue.empty(); });
		}
		void flush_queue()
		{
			std::lock_guard<std::mutex> lock{ m_task_queue_mutex };
			m_task_queue.clear();
		}

		template <typename Int, typename F>
		void parallel_for_async(Int start, Int end, F body, std::vector<std::future<void>>& futures) {
			Int local_num_threads = (Int)m_num_threads;

			Int range = end - start;
			Int chunk = (range / local_num_threads) + 1;

			for (Int i = 0; i < local_num_threads; ++i) {
				futures.emplace_back(enqueue_task([i, chunk, start, end, body] {
					Int inner_start = start + i * chunk;
					Int inner_end = std::min(end, start + (i + 1) * chunk);
					for (Int j = inner_start; j < inner_end; ++j) {
						body(j);
					}
				}));
			}
		}

		template <typename Int, typename F>
		std::vector<std::future<void>> parallel_for_async(Int start, Int end, F body) {
			std::vector<std::future<void>> futures;
			parallel_for_async(start, end, body, futures);
			return futures;
		}

		template <typename Int, typename F>
		void parallel_for(Int start, Int end, F body) {
			wait_all(parallel_for_async(start, end, body));
		}

	private:
		size_t m_num_threads = 0;
		std::vector<std::thread> m_threads;

		std::deque<std::function<void()>> m_task_queue;
		std::mutex m_task_queue_mutex;
		std::condition_variable m_worker_condition;
		std::condition_variable m_task_queue_completed_condition;
	};
	template <typename Int, typename F>
	void parallel_for(Int start, Int end, F body) {
#ifdef USE_OPENMP
#pragma omp parallel for
		for(int i=start;i<end;i++){
			body(i);
		}
#else
		ThreadPool	pool;
		return pool.parallel_for(start, end, body);
#endif
	}
}

#ifndef TINYSPLAT_CODE_BOOK
#define TINYSPLAT_CODE_BOOK
namespace tinygsplat
{
	struct CodeBook
	{
		std::vector<uint8_t>    ids;
		std::vector<float>  centers;

		auto evaluate() const -> std::vector<float>
		{
			std::vector<float> r(ids.size());
#pragma omp parallel for
			for (auto i = 0; i < ids.size(); i++)
				r[i] = centers[ids[i]];
			return r;
		}
	};
}
#endif

namespace tinygsplat
{
	inline float logit(const float p)
	{
		return std::log(p / (1 - p));
	}

	std::tuple<std::vector<int>, std::vector<float>>  kmeans_cluster(
		const std::vector<float>& values,
		const std::vector<float>& centers,
		const float tol,
		const int max_iterations);
}

namespace tinygsplat
{
	struct RichPoint
	{
		glm::vec3 pos;
		std::array<f32,48> shs;
		float opacity;
		glm::vec3 scale;
		glm::vec4 rot;
	};

    struct DataView
    {
        DataView(u64 size)
        {
            buffer.resize(size);
        }
        DataView(u8* data, u64 size)
        {
            buffer.resize(size);
            memcpy(buffer.data(), data, size);    
        }
        // void append();
        inline void setFloat32(u64 offset, float v)
        {
            memcpy(buffer.data() + offset, &v, sizeof(float));
        }
        inline void setUint8(u64 offset,u8 v)
        {
            buffer[offset] = v;
        }
        inline void setUint32(u64 offset,u32 v)
        {
            memcpy(buffer.data()+offset, &v, sizeof(u32));  
        }
		inline void setData(u64 offset,u8* src_data, u64 size)
		{
			memcpy(buffer.data() + offset, src_data, size);
		}
        inline u8 getUint8(u64 offset)
        {
            return buffer[offset];
        }
        inline u32 getUint32(u64 offset)
        {
            u32 v = 0;
            memcpy(&v, buffer.data() + offset, sizeof(u32));
            return v;    
        }
        inline float getFloat32(u64 offset)
        {
            float v = 0;
            memcpy(&v, buffer.data() + offset, sizeof(float));
            return v;        
        }
		inline glm::u16 getU16(size_t offset)
		{
			glm::u16 v = 0;
			memcpy(&v, buffer.data() + offset, sizeof(glm::u16));
			return v;
		}

		inline void resize(size_t size){buffer.resize(size); }
        inline size_t  size() const {return buffer.size(); }
        inline u8*  data() {return buffer.data(); }
		inline std::vector<u8>& get_buffer_vec() {return buffer;}
    protected:
        std::vector<u8> buffer;
        u64 cur_offset = 0;
    };

	auto inline calcMinMax(const std::vector<glm::vec3>& p, const std::vector<u64>& indices, size_t start, size_t end) -> std::pair<glm::vec3, glm::vec3> {
		glm::vec3 pmin = p[start], pmax = p[start];
		for (auto i = start; i < std::min(end, indices.size()); i++)
		{
			auto j = indices[i];
			const auto v = p[j];
			pmin = glm::min(pmin, v);
			pmax = glm::max(pmax, v);
		}
		return { pmin,pmax };
	};
	auto inline packUnorm(f32 value, u32 bits) -> u32 {
		const auto t = (1 << bits) - 1;
		//return std::max<u32>(0, std::min<u32>(t, std::floor(value * t + 0.5)));
		return static_cast<u32>(glm::clamp<f32>(std::floor(value * t + 0.5), 0, t));
	};
	auto inline pack8888(f32 x, f32 y, f32 z, f32 w) -> u32 {
		return packUnorm(x, 8) << 24 |
			packUnorm(y, 8) << 16 |
			packUnorm(z, 8) << 8 |
			packUnorm(w, 8);
	};

	auto inline packColor(f32 r, f32 g, f32 b, f32 a) -> u32 {
		const auto SH_C0 = 0.28209479177387814f;
		return pack8888(
			r * SH_C0 + 0.5f,
			g * SH_C0 + 0.5f,
			b * SH_C0 + 0.5f,
			1 / (1 + std::exp(-a))
		);
	};

	auto inline unpackUnorm(u32 pckd, u32 bitCount) -> f32 {
		u32 maxVal = (1u << bitCount) - 1;
		return float(pckd & maxVal) / maxVal;
	};

	auto inline unpack8888(u32 pckd) -> glm::vec4 {
		return glm::vec4(
			unpackUnorm(pckd >> 24, 8),
			unpackUnorm(pckd >> 16, 8),
			unpackUnorm(pckd >> 8, 8),
			unpackUnorm(pckd, 8)
		);
	};

	auto inline unpackColor(u32 pckd) -> glm::vec4 {
		const auto SH_C0 = 0.28209479177387814f;
		return glm::vec4(
			(unpackUnorm(pckd >> 24, 8) - 0.5f) / SH_C0,
			(unpackUnorm(pckd >> 16, 8) - 0.5f) / SH_C0,
			(unpackUnorm(pckd >> 8, 8) - 0.5f) / SH_C0,
			-std::log(1 / unpackUnorm(pckd, 8) - 1)
		);
	};

	struct SplatChunk
	{
		SplatChunk(const std::vector<u64>& index, size_t start_off = 0, size_t end_off = 256)
			: indices(index), start_index(start_off), end_index(end_off)
		{
			auto size = (end_off - start_off);
			position.resize(size);
			rotation.resize(size);
			color.resize(size);
			scale.resize(size);
		}

		auto pack(const std::vector<glm::vec3>& gs_pos,
			const std::vector<glm::vec3>& gs_scale,
			const std::vector<glm::vec4>& gs_rot,
			const std::vector<std::array<float, 48>>& gs_color,
			const std::vector<float>& gs_opacity) -> std::tuple<glm::vec3, glm::vec3, glm::vec3, glm::vec3>
		{
			auto pack111011 = [=](f32 x, f32 y, f32 z)->u32 {
				return packUnorm(x, 11) << 21 |
					packUnorm(y, 10) << 11 |
					packUnorm(z, 11);
				};

			// pack quaternion into 2,10,10,10
			auto packRot = [=](f32 x, f32 y, f32 z, f32 w)->u32 {
				auto q = glm::normalize(glm::vec4(x, y, z, w));
				// Store the components in an array for easy manipulation
				std::array<float, 4> a = { q.x, q.y, q.z, q.w };
				// Find the index of the largest component
				int largest = std::distance(a.begin(), std::max_element(a.begin(), a.end(), [](float lhs, float rhs) {
					return std::abs(lhs) < std::abs(rhs);
					}));
				// Ensure the largest component is positive
				if (a[largest] < 0) {
					a[0] = -a[0];
					a[1] = -a[1];
					a[2] = -a[2];
					a[3] = -a[3];
				}
				// Calculate the norm factor
				const float norm = std::sqrt(2) * 0.5f;
				unsigned int result = largest;
				// Pack the other three components into the result
				for (int i = 0; i < 4; ++i) {
					if (i != largest) {
						result = (result << 10) | packUnorm(a[i] * norm + 0.5f, 10);
					}
				}

				return result;
				};

			auto normalize = [](f32 x, f32 min, f32 max) ->f32 {
				return (max - min < 0.00001) ? 0 : (x - min) / (max - min);
				};

			const auto [p_min, p_max] = calcMinMax(gs_pos, indices, start_index, end_index);
			const auto [s_min, s_max] = calcMinMax(gs_scale, indices, start_index, end_index);

			for (auto j = start_index; j < std::min(indices.size(), end_index); ++j) {
				auto i = indices[j];
				auto idx = j - start_index;
				position[idx] = pack111011(
					normalize(gs_pos[i].x, p_min.x, p_max.x),
					normalize(gs_pos[i].y, p_min.y, p_max.y),
					normalize(gs_pos[i].z, p_min.z, p_max.z)
				);

				rotation[idx] = packRot(gs_rot[i].x, gs_rot[i].y, gs_rot[i].z, gs_rot[i].w);

				scale[idx] = pack111011(
					normalize(gs_scale[i].x, s_min.x, s_max.x),
					normalize(gs_scale[i].y, s_min.y, s_max.y),
					normalize(gs_scale[i].z, s_min.z, s_max.z)
				);

				color[idx] = packColor(gs_color[i][0], gs_color[i][1], gs_color[i][2], gs_opacity[i]);
			}
			return { p_min,p_max, s_min,s_max };
		}

		auto unpack(DataView& dataView, u64 chunkIndex, u64 numChunks, std::vector<RichPoint>& points)
		{
			auto unpack111011 = [=](u32 pckd) ->glm::vec3 {
				return glm::vec3(
					unpackUnorm(pckd >> 21, 11),
					unpackUnorm(pckd >> 11, 10),
					unpackUnorm(pckd, 11)
				);
				};
			auto unpackRot = [=](u32 packed) ->glm::vec4 {
				const auto norm = 1.0 / (std::sqrt(2) * 0.5);
				const float a = (unpackUnorm(packed >> 20, 10) - 0.5) * norm;
				const float b = (unpackUnorm(packed >> 10, 10) - 0.5) * norm;
				const float c = (unpackUnorm(packed, 10) - 0.5) * norm;
				const float m = std::sqrt(1.0f - (a * a + b * b + c * c));

				glm::vec4 q;
				switch (packed >> 30) {
				case 0:  q = glm::vec4(m, a, b, c); break;
				case 1:  q = glm::vec4(a, m, b, c); break;
				case 2: q = glm::vec4(a, b, m, c); break;
				case 3: q = glm::vec4(a, b, c, m); break;
				}
				return q;
				};
			auto unnormalize = [](f32 x, f32 min, f32 max) ->f32 {
				return x * (max - min) + min;
				};
			auto vertexOffset = numChunks * 12 * 4;
			auto offset = vertexOffset + chunkIndex * 256 * 4 * 4;
			const auto chunkSplats = std::min<u64>(points.size(), (chunkIndex + 1) * 256) - chunkIndex * 256;
			for (auto j = 0; j < chunkSplats; ++j) {
				position[j] = dataView.getUint32(offset + j * 4 * 4 + 0);
				rotation[j] = dataView.getUint32(offset + j * 4 * 4 + 4);
				scale[j] = dataView.getUint32(offset + j * 4 * 4 + 8);
				color[j] = dataView.getUint32(offset + j * 4 * 4 + 12);
			}
			const auto pmin = glm::vec3(dataView.getFloat32(chunkIndex * 12 * 4 + 0), dataView.getFloat32(chunkIndex * 12 * 4 + 4), dataView.getFloat32(chunkIndex * 12 * 4 + 8));
			const auto pmax = glm::vec3(dataView.getFloat32(chunkIndex * 12 * 4 + 12), dataView.getFloat32(chunkIndex * 12 * 4 + 16), dataView.getFloat32(chunkIndex * 12 * 4 + 20));
			const auto smin = glm::vec3(dataView.getFloat32(chunkIndex * 12 * 4 + 24), dataView.getFloat32(chunkIndex * 12 * 4 + 28), dataView.getFloat32(chunkIndex * 12 * 4 + 32));
			const auto smax = glm::vec3(dataView.getFloat32(chunkIndex * 12 * 4 + 36), dataView.getFloat32(chunkIndex * 12 * 4 + 40), dataView.getFloat32(chunkIndex * 12 * 4 + 44));

			for (auto j = start_index; j < std::min(indices.size(), end_index); ++j) {
				auto i = indices[j];
				auto idx = j - start_index;
				const auto unpckPos = unpack111011(position[idx]);
				points[i].pos = glm::vec3(
					unnormalize(unpckPos.x, pmin.x, pmax.x),
					unnormalize(unpckPos.y, pmin.y, pmax.y),
					unnormalize(unpckPos.z, pmin.z, pmax.z)
				);
				points[i].rot = unpackRot(rotation[idx]);
				const auto unpackScale = unpack111011(scale[idx]);
				points[i].scale = glm::vec3(
					unnormalize(unpackScale.x, smin.x, smax.x),
					unnormalize(unpackScale.y, smin.y, smax.y),
					unnormalize(unpackScale.z, smin.z, smax.z)
				);
				auto color_opacity = unpackColor(color[idx]);
				points[i].shs[0] = color_opacity.x;
				points[i].shs[1] = color_opacity.y;
				points[i].shs[2] = color_opacity.z;
				points[i].opacity = color_opacity.w;
			}
		}

		auto pack_pos(const std::vector<glm::vec3>& gs_pos) -> std::tuple<glm::vec3, glm::vec3>
		{
			auto pack111011 = [=](f32 x, f32 y, f32 z)->u32 {
				return packUnorm(x, 11) << 21 |
					packUnorm(y, 10) << 11 |
					packUnorm(z, 11);
				};

			auto normalize = [](f32 x, f32 min, f32 max) ->f32 {
				return (max - min < 0.00001) ? 0 : (x - min) / (max - min);
				};

			const auto [p_min, p_max] = calcMinMax(gs_pos, indices, start_index, end_index);

			for (auto j = start_index; j < std::min(indices.size(), end_index); ++j) {
				auto i = indices[j];
				auto idx = j - start_index;
				position[idx] = pack111011(
					normalize(gs_pos[i].x, p_min.x, p_max.x),
					normalize(gs_pos[i].y, p_min.y, p_max.y),
					normalize(gs_pos[i].z, p_min.z, p_max.z)
				);
			}
			return { p_min,p_max };
		}

		auto unpack_pos(DataView& dataView, u64 chunkIndex, u64 numChunks, u64 headSize, std::vector<RichPoint>& points)
		{
			auto unpack111011 = [=](u32 pckd) ->glm::vec3 {
				return glm::vec3(
					unpackUnorm(pckd >> 21, 11),
					unpackUnorm(pckd >> 11, 10),
					unpackUnorm(pckd, 11)
				);
				};
			auto unnormalize = [](f32 x, f32 min, f32 max) ->f32 {
				return x * (max - min) + min;
				};
			auto offset = headSize + numChunks * 6 * 4 + chunkIndex * 256 * 4 * 1;
			const auto chunkSplats = std::min<u64>(points.size(), (chunkIndex + 1) * 256) - chunkIndex * 256;
			for (auto j = 0; j < chunkSplats; ++j) {
				position[j] = dataView.getUint32(offset + j * 4 * 1 + 0);
			}
			const auto pmin = glm::vec3(dataView.getFloat32(headSize + chunkIndex * 6 * 4 + 0), dataView.getFloat32(headSize + chunkIndex * 6 * 4 + 4), dataView.getFloat32(headSize + chunkIndex * 6 * 4 + 8));
			const auto pmax = glm::vec3(dataView.getFloat32(headSize + chunkIndex * 6 * 4 + 12), dataView.getFloat32(headSize + chunkIndex * 6 * 4 + 16), dataView.getFloat32(headSize + chunkIndex * 6 * 4 + 20));

			for (auto j = start_index; j < std::min(indices.size(), end_index); ++j) {
				auto i = indices[j];
				auto idx = j - start_index;
				const auto unpckPos = unpack111011(position[idx]);
				points[i].pos = glm::vec3(
					unnormalize(unpckPos.x, pmin.x, pmax.x),
					unnormalize(unpckPos.y, pmin.y, pmax.y),
					unnormalize(unpckPos.z, pmin.z, pmax.z)
				);
			}
		}
		const std::vector<u64>& indices;
		size_t start_index;
		size_t end_index;

		std::vector<u32> position;
		std::vector<u32> rotation;
		std::vector<u32> scale;
		std::vector<u32> color;
	};
	GS_EXPORT bool	save_ply(const std::string& file_path,
		const std::vector<glm::vec3>& pos,
		const std::vector<glm::vec3>& scales,
		const std::vector<std::array<f32, 48>>& shs,
		const std::vector<glm::vec4>& rot,
		const std::vector<f32>& opacities);

	GS_EXPORT bool	save_splat(const std::string& file_path,
		const std::vector<glm::vec3>& pos, 
		const std::vector<glm::vec3>& scales,
		const std::vector<std::array<f32,48>>& shs,
		const std::vector<glm::vec4>& rot,
		const std::vector<f32>& opacities);
	
	GS_EXPORT bool	save_compress_ply(const std::string& file_path,
		const std::vector<glm::vec3>& pos,
		const std::vector<glm::vec3>& scales,
		const std::vector<std::array<f32, 48>>& shs,
		const std::vector<glm::vec4>& rot,
		const std::vector<f32>& opacities);

	GS_EXPORT  bool	save_reduced_ply(
		const std::string& file_path,
		const std::vector<glm::vec3>& pos,
		const std::vector<glm::vec3>& scales,
		const std::vector<glm::vec3>& featureDc,
		const std::vector<std::array<glm::vec3, 15>>& featureRest,
		const std::vector<glm::vec4>& rot,
		const std::vector<f32>& opacities,
		const std::vector<uint8_t>& degrees,
		std::optional<std::unordered_map<std::string, CodeBook>> codeBookDict = {},
		bool quantised = false,
		bool halfFloat = false);

	//read
	GS_EXPORT bool load_ply(const std::string& file_path,
		std::vector<RichPoint>& points);

	GS_EXPORT bool load_splat(const std::string& file_path,
		std::vector<RichPoint>& points);

	GS_EXPORT bool load_compress_ply(const std::string& file_path,
		std::vector<RichPoint>& points);

	GS_EXPORT bool load_reduced_ply(const std::string& file_path,
		std::vector<RichPoint>& points);

	struct DvsSplatHeader
	{
		u32 numSplats;
		u32 numChunks;
		u32 numVertexs[4];
		u32 flag = 0;
	};
	GS_EXPORT bool save_dvs_splat(
		const std::string& file_path,
		const std::vector<glm::vec3>& pos,
		const std::vector<glm::vec3>& scales,
		const std::vector<glm::vec3>& featureDc,
		const std::vector<std::array<glm::vec3, 15>>& featureRest,
		const std::vector<glm::vec4>& rot,
		const std::vector<f32>& opacities,
		const std::vector<uint8_t>& degrees);

	GS_EXPORT bool load_dvs_splat(
		const std::string& file_path,
		std::vector<RichPoint>& points);

	GS_EXPORT	bool load_spz_splats(
		const std::string& file_path,
		std::vector<RichPoint>& points);

	GS_EXPORT bool save_spz_splats(
		const std::string& file_path,
		const std::vector<glm::vec3>& pos,
		const std::vector<glm::vec3>& scales,
		const std::vector<std::array<f32, 48>>& shs,
		const std::vector<glm::vec4>& rot,
		const std::vector<f32>& opacities
	);
}