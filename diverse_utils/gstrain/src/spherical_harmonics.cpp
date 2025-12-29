#include "spherical_harmonics.hpp"

int degFromSh(int numBases){
    switch(numBases){
        case 1:
            return 0;
        case 4:
            return 1;
        case 9:
            return 2;
        case 16:
            return 3;
        default:
            return 4;
    }
}

const double C0 = 0.28209479177387814;

torch::Tensor rgb2sh(const torch::Tensor &rgb){
    // Converts from RGB values [0,1] to the 0th spherical harmonic coefficient
    return (rgb - 0.5) / C0;
}

torch::Tensor sh2rgb(const torch::Tensor &sh){
    // Converts from 0th spherical harmonic coefficients to RGB values [0,1]
    return torch::clamp((sh * C0) + 0.5, 0.0f, 1.0f);
}

#if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)

torch::Tensor SphericalHarmonics::forward(AutogradContext *ctx, 
            int degreesToUse,
            torch::Tensor viewDirs, 
            torch::Tensor sh0_coeffs,
            torch::Tensor shN_coeffs,
            torch::Tensor masks){
    int degree = shN_coeffs.size(-2) + 1;//degFromSh(coeffs.size(-2));

    ctx->saved_data["degreesToUse"] = degreesToUse;
    ctx->saved_data["degree"] = degree; 

    ctx->save_for_backward({ viewDirs, sh0_coeffs, shN_coeffs, masks });

    return gsplat::spherical_harmonics_fwd(degreesToUse, viewDirs, sh0_coeffs, shN_coeffs, masks);
}

tensor_list SphericalHarmonics::backward(AutogradContext *ctx, tensor_list grad_outputs){
    torch::Tensor v_colors = grad_outputs[0].contiguous();
    int degreesToUse = ctx->saved_data["degreesToUse"].toInt();
    int degree = ctx->saved_data["degree"].toInt();
    variable_list saved = ctx->get_saved_variables();

    torch::Tensor viewDirs = saved[0];
    torch::Tensor sh0_coeffs = saved[1];
    torch::Tensor shN_coeffs = saved[2];
    torch::Tensor masks = saved[3];
    auto compute_v_dirs = ctx->needs_input_grad(1);
    torch::Tensor none;
    auto  [v_coeffs0,v_coeffsN, v_dirs] = gsplat::spherical_harmonics_bwd(degree, degreesToUse, viewDirs, shN_coeffs, masks, v_colors, compute_v_dirs);
    if (!compute_v_dirs)
        v_dirs = none;

    return {
        none,
        v_dirs,
        v_coeffs0,
        v_coeffsN,
        none
    };
}

#endif
