#include "tensor_math.hpp"

using namespace torch::indexing;

torch::Tensor randomQuatTensor(long long n){
    torch::Tensor u = torch::rand(n);
    torch::Tensor v = torch::rand(n);
    torch::Tensor w = torch::rand(n);
    return torch::stack({
        torch::sqrt(1 - u) * torch::sin(2 * PI * v),
        torch::sqrt(1 - u) * torch::cos(2 * PI * v),
        torch::sqrt(u) * torch::sin(2 * PI * w),
        torch::sqrt(u) * torch::cos(2 * PI * w)
    }, -1);
}

torch::Tensor quatToRotMat(const torch::Tensor &quat){
    auto u = torch::unbind(torch::nn::functional::normalize(quat, torch::nn::functional::NormalizeFuncOptions().dim(-1)), -1);
    torch::Tensor w = u[0];
    torch::Tensor x = u[1];
    torch::Tensor y = u[2];
    torch::Tensor z = u[3];
    return torch::stack({
        torch::stack({
            1.0 - 2.0 * (y.pow(2) + z.pow(2)),
            2.0 * (x * y - w * z),
            2.0 * (x * z + w * y)
        }, -1),
        torch::stack({
            2.0 * (x * y + w * z),
            1.0 - 2.0 * (x.pow(2) + z.pow(2)),
            2.0 * (y * z - w * x)
        }, -1),
        torch::stack({
            2.0 * (x * z - w * y),
            2.0 * (y * z + w * x),
            1.0 - 2.0 * (x.pow(2) + y.pow(2))
        }, -1)
    }, -2);
}

std::tuple<torch::Tensor, torch::Tensor, float> autoScaleAndCenterPoses(const torch::Tensor &poses){
    // Center at mean
    torch::Tensor origins = poses.index({"...", Slice(None, 3), 3});
    // torch::Tensor center = torch::mean(origins, 0);
    // origins -= center;

    // // Scale
    // float f = std::max(0.2f,1.0f / torch::max(torch::abs(origins)).item<float>());
    // origins *= f;
    float f = 1.0f;
    torch::Tensor center = torch::zeros_like(torch::mean(origins, 0));
    torch::Tensor transformedPoses = poses.clone();
    transformedPoses.index_put_({"...", Slice(None, 3), 3}, origins);

    return std::make_tuple(transformedPoses, center, f);
}


torch::Tensor rotationMatrix(const torch::Tensor &a, const torch::Tensor &b){
    // Rotation matrix that rotates vector a to vector b
    torch::Tensor a1 = a / a.norm();
    torch::Tensor b1 = b / b.norm();
    torch::Tensor v = torch::linalg_cross(a1, b1);
    torch::Tensor c = torch::dot(a1, b1);
    const float EPS = 1e-8;
    if (c.item<float>() < -1 + EPS){
        torch::Tensor eps = (torch::rand(3) - 0.5f) * 0.01f;
        return rotationMatrix(a1 + eps, b1);
    }
    torch::Tensor s = v.norm();
    torch::Tensor skew = torch::zeros({3, 3}, torch::kFloat32);
    skew[0][1] = -v[2];
    skew[0][2] = v[1];
    skew[1][0] = v[2];
    skew[1][2] = -v[0];
    skew[2][0] = -v[1];
    skew[2][1] = v[0];

    return torch::eye(3) + skew + torch::matmul(skew, skew * ((1 - c) / (s.pow(2) + EPS)));
}

torch::Tensor rodriguesToRotation(const torch::Tensor &rodrigues){
    float theta = torch::linalg_vector_norm(rodrigues, 2, { -1 }, true, torch::kFloat32).item<float>();
    if (theta < FLOAT_EPS){
        return torch::eye(3, torch::kFloat32);
    }
    torch::Tensor r = rodrigues / theta;
    torch::Tensor ident = torch::eye(3, torch::kFloat32);
    float a = r[0].item<float>();
    float b = r[1].item<float>();
    float c = r[2].item<float>();
    torch::Tensor rrT = torch::tensor({
        {a * a, a * b, a * c},
        {b * a, b * b, b * c},
        {c * a, c * b, c * c}
    }, torch::kFloat32);
    torch::Tensor rCross = torch::tensor({
        {0.0f, -c, b},
        {c, 0.0f, -a},
        {-b, a, 0.0f}
    }, torch::kFloat32);
    float cosTheta = std::cos(theta);

    return cosTheta * ident + (1 - cosTheta) * rrT + std::sin(theta) * rCross;
}

torch::Tensor depth_to_points(
    const torch::Tensor& depths,
    const torch::Tensor& camtoworlds,
    const torch::Tensor& Ks,
    bool z_depth
) {
    // Assertions for input shapes
    TORCH_CHECK(depths.size(-1) == 1, "Invalid depth shape: ", depths.sizes());
    TORCH_CHECK(camtoworlds.size(-2) == 4 && camtoworlds.size(-1) == 4,
        "Invalid camtoworlds shape: ", camtoworlds.sizes());
    TORCH_CHECK(Ks.size(-2) == 3 && Ks.size(-1) == 3, "Invalid Ks shape: ", Ks.sizes());

    // Get device and dimensions
    auto device = depths.device();
    int64_t height = depths.size(-3);
    int64_t width = depths.size(-2);

    // Create meshgrid for pixel coordinates
    auto grid_x = torch::arange(width, torch::kFloat) + 0.5;
    auto grid_y = torch::arange(height, torch::kFloat) + 0.5;
    auto grid = torch::meshgrid({ grid_x.to(device), grid_y.to(device)}, "xy");
    auto x = grid[0];
    auto y = grid[1];

    // Extract intrinsic parameters
    auto fx = Ks.index({ "...", 0, 0 });  // [...]
    auto fy = Ks.index({ "...", 1, 1 });  // [...]
    auto cx = Ks.index({ "...", 0, 2 });  // [...]
    auto cy = Ks.index({ "...", 1, 2 });  // [...]
    // Compute camera directions in camera coordinates
    auto camera_dirs = torch::stack({
        (x - cx.unsqueeze(-1).unsqueeze(-1) + 0.5) / fx.unsqueeze(-1).unsqueeze(-1),
        (y - cy.unsqueeze(-1).unsqueeze(-1) + 0.5) / fy.unsqueeze(-1).unsqueeze(-1)
        }, -1); // [..., H, W, 2]

    camera_dirs = torch::nn::functional::pad(camera_dirs,
        torch::nn::functional::PadFuncOptions({ 0, 1 }).value(1.0)); // [..., H, W, 3]
    // Compute ray directions in world coordinates
    auto directions = torch::einsum("...ij,...hwj->...hwi",
        { camtoworlds.index({"...", torch::indexing::Slice(0,3), torch::indexing::Slice(0, 3)}),
         camera_dirs }); // [..., H, W, 3]
    auto origins = camtoworlds.index({ "...", torch::indexing::Slice(0,3), 3 }); // [..., 3]

    // Normalize directions if not z_depth
    if (!z_depth) {
        directions = torch::nn::functional::normalize(directions,
            torch::nn::functional::NormalizeFuncOptions().dim(-1));
    }

    // Compute 3D points in world coordinates
    auto points = origins.unsqueeze(-2).unsqueeze(-2) + depths * directions; // [..., H, W, 3]

    return points;
}


torch::Tensor depth_to_normal(
    const torch::Tensor& depths,
    const torch::Tensor& camtoworlds,
    const torch::Tensor& Ks,
    bool z_depth
) {
    // Convert depth maps to 3D points
    auto points = depth_to_points(depths, camtoworlds, Ks, z_depth); // [..., H, W, 3]
    // Compute dx (difference along the x-axis)
    auto dx = points.index({
        torch::indexing::Slice(torch::indexing::None, torch::indexing::None),
        torch::indexing::Slice(2, torch::indexing::None),
        torch::indexing::Slice(1, -1),
        torch::indexing::Ellipsis
        }) - points.index({
            torch::indexing::Slice(torch::indexing::None, torch::indexing::None),
            torch::indexing::Slice(torch::indexing::None, -2),
            torch::indexing::Slice(1, -1),
            torch::indexing::Ellipsis
            });

    // Compute dy: points[..., 1:-1, 2:, :] - points[..., 1:-1, :-2, :]
    auto dy = points.index({
        torch::indexing::Slice(torch::indexing::None, torch::indexing::None),
        torch::indexing::Slice(1, -1),
        torch::indexing::Slice(2, torch::indexing::None),
        torch::indexing::Ellipsis
        }) - points.index({
            torch::indexing::Slice(torch::indexing::None, torch::indexing::None),
            torch::indexing::Slice(1, -1),
            torch::indexing::Slice(torch::indexing::None, -2),
            torch::indexing::Ellipsis
            });

    // Compute normals using cross product and normalize
    auto normals = torch::cross(dx, dy, -1);
    normals = torch::nn::functional::normalize(normals, torch::nn::functional::NormalizeFuncOptions().dim(-1));

    // Pad normals to match the original shape
    normals = torch::nn::functional::pad(
        normals, torch::nn::functional::PadFuncOptions({ 0, 0, 1, 1, 1, 1 }).value(0.0)
    ); // [..., H, W, 3]

    return normals;
}

std::pair<torch::Tensor, torch::Tensor> pointsBounds(const torch::Tensor& points){
    auto [min_points,_im] = torch::min(points, 0);
    auto [max_points,ia] = torch::max(points, 0);
    return std::make_pair(min_points, max_points);
}
float pointsBoundsExtent(const torch::Tensor& points){
    auto [bmin,bmax] = pointsBounds(points);
    return torch::linalg_vector_norm(bmax - bmin, 2, { -1 }, true, torch::kFloat32).item<float>();
}
