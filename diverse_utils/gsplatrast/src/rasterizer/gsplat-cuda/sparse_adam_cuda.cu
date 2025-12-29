#include <ATen/Dispatch.h>
#include <c10/cuda/CUDAStream.h>
#include <cooperative_groups.h>

#include "sparse_adam.h"

namespace cg = cooperative_groups;

namespace gsplat {

// CUDA kernel for sparse Adam update
// Each thread handles one element of the gradient values
template <typename scalar_t>
__global__ void sparse_adam_kernel(
    scalar_t* __restrict__ param,
    scalar_t* __restrict__ exp_avg,
    scalar_t* __restrict__ exp_avg_sq,
    const int64_t* __restrict__ grad_indices,
    const scalar_t* __restrict__ grad_values,
    const int nnz,
    const int feature_dim,
    const float lr,
    const float beta1,
    const float beta2,
    const float eps,
    const float bias_correction1_rcp,
    const float bias_correction2_sqrt_rcp
) {
    // Each thread processes one element
    const int idx = cg::this_grid().thread_rank();
    
    if (idx >= nnz * feature_dim) {
        return;
    }
    
    // Determine which sparse element and which feature dimension
    const int sparse_idx = idx / feature_dim;  // which non-zero element
    const int feat_idx = idx % feature_dim;    // which feature dimension
    
    // Get the actual parameter index from sparse indices
    const int64_t param_idx = grad_indices[sparse_idx];
    const int64_t global_idx = param_idx * feature_dim + feat_idx;
    
    // Load gradient value for this position
    const float grad = static_cast<float>(grad_values[idx]);
    
    // Load current momentum buffers
    float m = static_cast<float>(exp_avg[global_idx]);
    float v = static_cast<float>(exp_avg_sq[global_idx]);
    
    // Update first moment: m = beta1 * m + (1 - beta1) * grad
    m = beta1 * m + (1.0f - beta1) * grad;
    
    // Update second moment: v = beta2 * v + (1 - beta2) * grad^2
    v = beta2 * v + (1.0f - beta2) * grad * grad;
    
    // Compute bias-corrected moments
    const float m_hat = m * bias_correction1_rcp;
    const float v_hat = v * bias_correction2_sqrt_rcp * bias_correction2_sqrt_rcp;
    
    // Compute step
    const float denom = sqrtf(v_hat) + eps;
    const float step = -lr * m_hat / denom;
    
    // Update parameter
    param[global_idx] += static_cast<scalar_t>(step);
    
    // Store updated momentum buffers
    exp_avg[global_idx] = static_cast<scalar_t>(m);
    exp_avg_sq[global_idx] = static_cast<scalar_t>(v);
}

void launch_sparse_adam_kernel(
    at::Tensor& param,
    at::Tensor& exp_avg,
    at::Tensor& exp_avg_sq,
    const at::Tensor& grad_indices,
    const at::Tensor& grad_values,
    const float lr,
    const float beta1,
    const float beta2,
    const float eps,
    const float bias_correction1_rcp,
    const float bias_correction2_sqrt_rcp
) {
    // grad_indices: [nnz] - indices of non-zero gradients
    // grad_values: [nnz, D] or [nnz] - values of non-zero gradients
    // param: [N, D] or [N] - parameters to update
    
    const int nnz = grad_values.size(0);  // number of non-zero elements
    // Handle both 1D (e.g., opacities [N]) and 2D (e.g., means [N, 3]) parameters
    const int feature_dim = grad_values.dim() > 1 ? grad_values.size(1) : 1;
    const int total_elements = nnz * feature_dim;
    
    if (total_elements == 0) {
        return;
    }
    
    // Launch kernel
    const int threads = 256;
    const int blocks = (total_elements + threads - 1) / threads;
    
    AT_DISPATCH_FLOATING_TYPES(param.scalar_type(), "sparse_adam_kernel", [&]() {
        sparse_adam_kernel<scalar_t>
            <<<blocks, threads, 0, at::cuda::getCurrentCUDAStream()>>>(
                param.data_ptr<scalar_t>(),
                exp_avg.data_ptr<scalar_t>(),
                exp_avg_sq.data_ptr<scalar_t>(),
                grad_indices.data_ptr<int64_t>(),
                grad_values.data_ptr<scalar_t>(),
                nnz,
                feature_dim,
                lr,
                beta1,
                beta2,
                eps,
                bias_correction1_rcp,
                bias_correction2_sqrt_rcp
            );
    });
}

} // namespace gsplat

