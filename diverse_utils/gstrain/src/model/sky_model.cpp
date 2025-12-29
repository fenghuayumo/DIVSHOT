#include "sky_model.hpp"

// Function to generate uniform points on a sphere using Fibonacci sampling
torch::Tensor get_uniform_points_on_sphere_fibonacci(int num_points) {
    // Golden angle in radians
    const float phi = M_PI * (3.0f - std::sqrt(5.0f));
    float N = (num_points - 1) / 2.0f;

    // Generate latitude and longitude
    auto i = torch::linspace(-N, N, num_points, torch::kFloat32);
    auto lat = torch::asin(2.0f * i / (2.0f * N + 1.0f));
    auto lon = phi * i;

    // Convert spherical coordinates to Cartesian coordinates
    auto x = torch::cos(lon) * torch::cos(lat);
    auto y = torch::sin(lon) * torch::cos(lat);
    auto z = torch::sin(lat);

    return torch::stack({x, y, z}, -1);
}

// Function to compute sky points
std::tuple<torch::Tensor,torch::Tensor ,float> get_sky_points(int num_points, const torch::Tensor& points3D) {
    // Generate uniform points on a sphere
    torch::Tensor points = get_uniform_points_on_sphere_fibonacci(num_points);
    points = points.to(points3D.device());

    // Compute the mean of points3D
    torch::Tensor mean = points3D.mean(0).unsqueeze(0);

    // Compute the sky distance
    torch::Tensor distances = (points3D - mean).norm(2, -1);
    float sky_distance = torch::quantile(distances, 0.97).item<float>() * 10.0f;

    // Scale and translate the points
    points = points * sky_distance;
    points = points + mean;

    // Uncomment and implement the masking logic if needed
    /*
    torch::Tensor gmask = torch::zeros({points.size(0)}, torch::TensorOptions().dtype(torch::kBool).device(points.device()));
    for (const auto& cam : cameras) {
        torch::Tensor uv = camera_project(cam, points.masked_select(!gmask));
        torch::Tensor mask = !torch::isnan(uv).any(-1);
        torch::Tensor image_size = torch::tensor({cam.width, cam.height}, torch::TensorOptions().dtype(torch::kFloat32));
        mask = mask & (uv.index({Slice(), -1}) < (2.0 / 3.0) * image_size[1]);
        gmask.masked_scatter_(!gmask, gmask | mask);
    }
    */

    return {points,mean, sky_distance / 2.0f};
}

SkyGaussianModel::SkyGaussianModel(int num_sky_points,torch::Tensor points3D)
{
    auto [sky_pts,means,sky_distance] = get_sky_points(num_sky_points, points3D);
    points = sky_pts;
    sky_center = means;
    sky_dist = sky_distance;
    rgbs = torch::tensor({237, 247, 252}, torch::TensorOptions().dtype(torch::kU8)).repeat({points.size(0), 1}) / 255.0;
}

SkyGaussianModel::~SkyGaussianModel()
{
}