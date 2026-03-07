#include <ATen/Dispatch.h> // AT_DISPATCH_XXX
#include <ATen/core/Tensor.h>
#include <c10/cuda/CUDAStream.h> // at::cuda::getCurrentCUDAStream
#include <cooperative_groups.h>

#include "null.h"

namespace gsplat {

namespace cg = cooperative_groups;

template <typename scalar_t>
__global__ void adam_kernel(
    const uint32_t N,
    const uint32_t D,
    scalar_t *__restrict__ param,
    const scalar_t *__restrict__ param_grad,
    scalar_t *__restrict__ exp_avg,
    scalar_t *__restrict__ exp_avg_sq,
    const bool *valid,
    const scalar_t* __restrict__ step_per_gaussian,
    const float lr,
    const float b1,
    const float b2,
    const float eps
) {
    auto p_idx = cg::this_grid().thread_rank();
    const uint32_t g_idx = p_idx / D;

    if (g_idx >= N)
        return;
    if (valid != nullptr && !valid[g_idx])
        return;

    float register_param_grad = param_grad[p_idx];
    float register_exp_avg = exp_avg[p_idx];
    float register_exp_avg_sq = exp_avg_sq[p_idx];
    register_exp_avg =
        b1 * register_exp_avg + (1.0f - b1) * register_param_grad;
    register_exp_avg_sq = b2 * register_exp_avg_sq + (1.0f - b2) *
                                                         register_param_grad *
                                                         register_param_grad;
    // float step = -lr * register_exp_avg / (sqrt(register_exp_avg_sq) + eps);
    // Bias correction
    float bias_correction1 = 1.0f - powf(b1, step_per_gaussian[g_idx]);
    float bias_correction2 = 1.0f - powf(b2, step_per_gaussian[g_idx]);

    float m_hat = register_exp_avg / bias_correction1;
    float v_hat = register_exp_avg_sq / bias_correction2;

    // Parameter update
    float step = -lr * m_hat / (sqrtf(v_hat) + eps);

    param[p_idx] += step;
    exp_avg[p_idx] = register_exp_avg;
    exp_avg_sq[p_idx] = register_exp_avg_sq;
}

template <typename scalar_t>
__global__ void increment_step_kernel(
    scalar_t *__restrict__ step_per_gaussian,
    const bool* valid,
    int N
) {
    int g_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (g_idx >= N) return;
    if (valid == nullptr || valid[g_idx]) {
        step_per_gaussian[g_idx]++;
    }
}

void launch_adam_kernel(
    at::Tensor &param,                    // [N, ...]
    const at::Tensor &param_grad,         // [N, ...]
    at::Tensor &exp_avg,                  // [N, ...]
    at::Tensor &exp_avg_sq,               // [N, ...]
    const at::optional<at::Tensor> valid, // [N]
    at::Tensor &step_per_gaussian,       // [N]
    const float lr,
    const float b1,
    const float b2,
    const float eps
) {
    const uint32_t N = param.size(0);
    const uint32_t D = param.numel() / N;
    if( N == 0 || D == 0){
        return;
    }
    int64_t shmem_size = 0;
    // Step 1: increment step counter
    dim3 threads_step(256);
    dim3 grid_step((N + threads_step.x - 1) / threads_step.x);
    AT_DISPATCH_FLOATING_TYPES(param.scalar_type(), "increment_step_kernel", [&]() {
        increment_step_kernel<scalar_t>
            <<<grid_step, threads_step, shmem_size, at::cuda::getCurrentCUDAStream()>>>(
            step_per_gaussian.data_ptr<scalar_t>(),
            valid.value().data_ptr<bool>(),
            N
        );
    });
    // parallel over [N, ...]
    int64_t n_elements = N * D;
    dim3 threads(256);
    dim3 grid((n_elements + threads.x - 1) / threads.x);
    AT_DISPATCH_FLOATING_TYPES(param.scalar_type(), "adam_kernel", [&]() {
        adam_kernel<scalar_t>
            <<<grid, threads, shmem_size, at::cuda::getCurrentCUDAStream()>>>(
                N,
                D,
                param.data_ptr<scalar_t>(),
                param_grad.data_ptr<scalar_t>(),
                exp_avg.data_ptr<scalar_t>(),
                exp_avg_sq.data_ptr<scalar_t>(),
                valid.has_value() ? valid.value().data_ptr<bool>() : nullptr,
                step_per_gaussian.data_ptr<scalar_t>(),
                lr,
                b1,
                b2,
                eps
            );
    });
}

    __global__ void adam_step_vectorized_kernel(
        float* param,
        float* exp_avg,
        float* exp_avg_sq,
        const float* param_grad,
        const int n_elements,
        const float lr,
        const float beta1,
        const float beta2,
        const float eps,
        const float bias_correction1_rcp,
        const float bias_correction2_sqrt_rcp) {

        const int idx = blockIdx.x * blockDim.x + threadIdx.x;
        if (idx * 4 >= n_elements)
            return;

        const float beta1_comp = 1.0f - beta1;
        const float beta2_comp = 1.0f - beta2;
        const float step_size = lr * bias_correction1_rcp;

        const int base_idx = idx * 4;
        const int remaining = n_elements - base_idx;

        if (remaining >= 4) {
            float4 grad4 = *reinterpret_cast<const float4*>(param_grad + base_idx);
            float4 m1_4 = *reinterpret_cast<float4*>(exp_avg + base_idx);
            float4 m2_4 = *reinterpret_cast<float4*>(exp_avg_sq + base_idx);
            float4 p4 = *reinterpret_cast<float4*>(param + base_idx);

#pragma unroll
            for (int i = 0; i < 4; i++) {
                float g = reinterpret_cast<float*>(&grad4)[i];
                float m1 = reinterpret_cast<float*>(&m1_4)[i];
                float m2 = reinterpret_cast<float*>(&m2_4)[i];
                float p = reinterpret_cast<float*>(&p4)[i];

                m1 = beta1 * m1 + beta1_comp * g;
                m2 = beta2 * m2 + beta2_comp * g * g;
                p -= step_size * m1 / (sqrtf(m2) * bias_correction2_sqrt_rcp + eps);

                reinterpret_cast<float*>(&m1_4)[i] = m1;
                reinterpret_cast<float*>(&m2_4)[i] = m2;
                reinterpret_cast<float*>(&p4)[i] = p;
            }

            *reinterpret_cast<float4*>(exp_avg + base_idx) = m1_4;
            *reinterpret_cast<float4*>(exp_avg_sq + base_idx) = m2_4;
            *reinterpret_cast<float4*>(param + base_idx) = p4;
        } else {
#pragma unroll
            for (int i = 0; i < remaining; i++) {
                const int elem_idx = base_idx + i;
                const float g = param_grad[elem_idx];
                const float m1 = beta1 * exp_avg[elem_idx] + beta1_comp * g;
                const float m2 = beta2 * exp_avg_sq[elem_idx] + beta2_comp * g * g;
                param[elem_idx] -= step_size * m1 / (sqrtf(m2) * bias_correction2_sqrt_rcp + eps);
                exp_avg[elem_idx] = m1;
                exp_avg_sq[elem_idx] = m2;
            }
        }
    }

void launch_adam_step_kernel(
    at::Tensor& param,
    at::Tensor& exp_avg,
    at::Tensor& exp_avg_sq,
    const at::Tensor& param_grad,
    const float lr,
    const float beta1,
    const float beta2,
    const float eps,
    const float bias_correction1_rcp,
    const float bias_correction2_sqrt_rcp
){
    const int n_elements = param.numel();
    if (n_elements == 0) {
        return;
    }
    constexpr int threads_per_block = 256;
    const int n_vec_elements = (n_elements + 3) / 4;
    dim3 threads(threads_per_block);
    dim3 grid((n_vec_elements + threads_per_block - 1) / threads_per_block);
    adam_step_vectorized_kernel
        <<<grid, threads, 0, at::cuda::getCurrentCUDAStream()>>>(
            param.data_ptr<float>(),
            exp_avg.data_ptr<float>(),
            exp_avg_sq.data_ptr<float>(),
            param_grad.data_ptr<float>(),
            n_elements,
            lr,
            beta1,
            beta2,
            eps,
            bias_correction1_rcp,
            bias_correction2_sqrt_rcp
     );
}
} // namespace gsplat
