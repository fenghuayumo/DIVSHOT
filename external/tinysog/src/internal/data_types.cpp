/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "data_types.h"
#include "cuda_utils.cuh"
#include <cstdlib>
#include <cstring>

namespace tinysog {
    namespace internal {

        template <typename T>
        void Tensor<T>::allocate(size_t count) {
            if (is_cuda) {
                data = cuda_malloc<T>(count);
            } else {
                data = static_cast<T*>(std::malloc(count * sizeof(T)));
                if (!data) {
                    throw std::bad_alloc();
                }
            }
        }

        template <typename T>
        void Tensor<T>::free_memory() {
            if (data) {
                if (is_cuda) {
                    cuda_free(data);
                } else {
                    std::free(data);
                }
                data = nullptr;
            }
        }

        template <typename T>
        void Tensor<T>::to_cuda() {
            if (is_cuda)
                return;

            T* cuda_data = cuda_malloc<T>(total_size());
            cuda_copy_h2d(cuda_data, data, total_size());

            if (owns_data) {
                std::free(data);
            }

            data = cuda_data;
            is_cuda = true;
            owns_data = true;
        }

        template <typename T>
        void Tensor<T>::to_cpu() {
            if (!is_cuda)
                return;

            T* cpu_data = static_cast<T*>(std::malloc(byte_size()));
            if (!cpu_data) {
                throw std::bad_alloc();
            }

            cuda_copy_d2h(cpu_data, data, total_size());

            if (owns_data) {
                cuda_free(data);
            }

            data = cpu_data;
            is_cuda = false;
            owns_data = true;
        }

        // 显式实例化常用类型
        template struct Tensor<float>;
        template struct Tensor<double>;
        template struct Tensor<int32_t>;
        template struct Tensor<int64_t>;
        template struct Tensor<uint8_t>;

    } // namespace internal
} // namespace tinysog




