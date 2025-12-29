#include <ATen/TensorUtils.h>
#include <ATen/ATen.h>
#include <ATen/core/Tensor.h>
#include <c10/cuda/CUDAGuard.h> // for DEVICE_GUARD
#include <tuple>

#include <ATen/Functions.h>
#include <ATen/NativeFunctions.h>

#include "common.h"     // where all the macros are defined
#include "ops.h"        // a collection of all gsplat operators
#include "reduce_kmeans.h"

namespace gsplat {
    // Works with 256 centers 1 dimensional data only
    std::tuple<at::Tensor, at::Tensor>
    reduced_kmeans(
        const at::Tensor &values,
        const at::Tensor &centers,
        const float tol,
        const int max_iterations){
            
        const int n_values = values.size(0);
        const int n_centers = centers.size(0);
        at::Tensor ids = at::zeros({n_values, 1}, values.options().dtype(at::kInt));
        at::Tensor new_centers = at::zeros({n_centers}, values.options().dtype(at::kFloat));
        at::Tensor old_centers = at::zeros({n_centers}, values.options().dtype(at::kFloat));
        new_centers = centers.clone();
        at::Tensor center_sizes = at::zeros({n_centers}, values.options().dtype(at::kInt));
    
        for (int i = 0; i < max_iterations; ++i)
        {
            updateIds(
                values.contiguous().data<float>(),
                ids.contiguous().data<int>(),
                new_centers.contiguous().data<float>(),
                n_values,
                n_centers);
    
            old_centers = new_centers.clone();
            new_centers.zero_();
            center_sizes.zero_();
    
            updateCenters(
                values.contiguous().data<float>(),
                ids.contiguous().data<int>(),
                new_centers.contiguous().data<float>(),
                center_sizes.contiguous().data<int>(),
                n_values,
                n_centers);
    
            new_centers = new_centers / center_sizes;
            new_centers.index_put_({new_centers.isnan()}, 0.f);
            float center_shift = (old_centers - new_centers).abs().sum().item<float>();
            if (center_shift < tol)
                break;
        }
    
        updateIds(
            values.contiguous().data<float>(),
            ids.contiguous().data<int>(),
            new_centers.contiguous().data<float>(),
            n_values,
            n_centers);
    
        return std::make_tuple(ids, new_centers);
    }
    
}