#ifndef THREEDGS_COVARIANCE_HLSL
#define THREEDGS_COVARIANCE_HLSL

// sqrt(8) = 2.828... used for extent calculation
static const float sqrt8 = 2.82842712475;

// Computes the 3D covariance matrix from scale and rotation quaternion
// Returns the 6 unique elements of the symmetric 3x3 matrix as float3(xx, xy, xz) and float3(yy, yz, zz)
#if VISUALIZE_NORMAL
void threedgs_compute_covariance_3d(float3 scale, float4 rotation, out float3 covA, out float3 covB, out float3 normal)
#else
void threedgs_compute_covariance_3d(float3 scale, float4 rotation, out float3 covA, out float3 covB)
#endif
{
    // Build rotation matrix from quaternion
    float r = rotation.x;
    float x = rotation.y;
    float y = rotation.z;
    float z = rotation.w;
    
    float3x3 R = float3x3(
        1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - r * z), 2.0 * (x * z + r * y),
        2.0 * (x * y + r * z), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - r * x),
        2.0 * (x * z - r * y), 2.0 * (y * z + r * x), 1.0 - 2.0 * (x * x + y * y)
    );
    
    // Build scale matrix
    float3x3 S = float3x3(
        scale.x, 0.0, 0.0,
        0.0, scale.y, 0.0,
        0.0, 0.0, scale.z
    );
#if VISUALIZE_NORMAL
    float3 scale_axis = float3(0, 1, 0);
    if (scale.x < scale.y && scale.x < scale.z) {
        scale_axis = float3(1, 0, 0);
    } else if (scale.x > scale.y && scale.y < scale.z) {
        scale_axis = float3(0, 1, 0);
    } else {
        scale_axis = float3(0, 0, 1);
    }
    normal = normalize(scale_axis);
#endif   
    // M = R * S (rotation followed by scale)
    float3x3 M = mul(R, S);
    
    // Covariance is M * M^T
    // Since we only need upper triangle (symmetric matrix):
    covA = float3(dot(M[0], M[0]), dot(M[0], M[1]), dot(M[0], M[2]));
    covB = float3(dot(M[1], M[1]), dot(M[1], M[2]), dot(M[2], M[2]));
}

// Projects 3D covariance to 2D screen space
// Based on EWA splatting (Zwicker et al. 2002)
float3 threedgs_covariance_projection(float3x3 cov3D, float4 viewCenter, float2 focal, float4x4 modelViewMatrix, bool is_ortho)
{
    const float3 v = is_ortho ? float3(0,0,1) : viewCenter.xyz;
    const float s = 1.0 / (v.z * v.z);
    // Clamp viewCenter position to avoid numerical issues
    const float3x3 J = float3x3(
        focal.x / v.z, 0.0, -(focal.x * v.x) * s,
        0.0, focal.y / v.z, -(focal.y * v.y) * s,
        0.0, 0.0, 0.0
    );
    
    // Extract rotation part of model-view matrix (upper-left 3x3)
    float3x3 W = float3x3(
        modelViewMatrix[0][0], modelViewMatrix[1][0], modelViewMatrix[2][0],
        modelViewMatrix[0][1], modelViewMatrix[1][1], modelViewMatrix[2][1],
        modelViewMatrix[0][2], modelViewMatrix[1][2], modelViewMatrix[2][2]
    );
    
    // Transform matrix: T = J * W
    const float3x3 T = mul(J, W);
    
    // Project covariance: cov2D = T * cov3D * T^T
    // We compute this efficiently by only calculating the upper triangle
    const float3x3 cov2D = mul(mul(T, cov3D), transpose(T));

    
    // Return the 3 unique elements: (xx, yy, xy)
    return float3(float(cov2D[0][0]), float(cov2D[0][1]), float(cov2D[1][1]));
}

// This function ingests the projected 2D covariance and outputs the basis vectors of its 2D extent
// opacity is updated if MipSplatting antialiasing is applied.
bool threedgs_projected_extent_basis(float3 cov2Dv, float stdDev, float splatScale, inout float opacity, out float2 basisVector1, out float2 basisVector2)
{

#if GSPLAT_AA
  // This mode is used when model is reconstructed using MipSplatting
  // https://niujinshuchong.github.io/mip-splatting/
  const float detOrig = cov2Dv[0] * cov2Dv[2] - cov2Dv[1] * cov2Dv[1];
#endif

  cov2Dv[0] += 0.3;
  cov2Dv[2] += 0.3;

#if GSPLAT_AA
  const float detBlur = cov2Dv[0] * cov2Dv[2] - cov2Dv[1] * cov2Dv[1];
  // apply the alpha compensation
  opacity *= sqrt(max(detOrig / detBlur, 0.0));
#endif

  // We now need to solve for the eigen-values and eigen vectors of the 2D covariance matrix
  // so that we can determine the 2D basis for the splat. This is done using the method described
  // here: https://people.math.harvard.edu/~knill/teaching/math21b2004/exhibits/2dmatrices/index.html
  // After calculating the eigen-values and eigen-vectors, we calculate the basis for rendering the splat
  // by normalizing the eigen-vectors and then multiplying them by (stdDev * eigen-value), which is
  // equal to scaling them by stdDev standard deviations.
  //
  // This is a different approach than in the original work at INRIA. In that work they compute the
  // max extents of the projected splat in screen space to form a screen-space aligned bounding rectangle
  // which forms the geometry that is actually rasterized. The dimensions of that bounding box are 3.0
  // times the maximum eigen-value, or 3 standard deviations. They then use the inverse 2D covariance
  // matrix (called 'conic') in the CUDA rendering thread to determine fragment opacity by calculating the
  // full gaussian: exp(-0.5 * (X - mean) * conic * (X - mean)) * splat opacity
  const float a           = cov2Dv.x;
  const float d           = cov2Dv.z;
  const float b           = cov2Dv.y;
  const float D           = a * d - b * b;
  const float trace       = a + d;
  const float traceOver2  = 0.5 * trace;
  const float term2       = sqrt(max(0.1f, traceOver2 * traceOver2 - D));
  float       eigenValue1 = traceOver2 + term2;
  float       eigenValue2 = traceOver2 - term2;

  if(eigenValue2 <= 0.0)
    return false;

  const float2 eigenVector1 = normalize(float2(b, eigenValue1 - a));
  // since the eigen vectors are orthogonal, we derive the second one from the first
  const float2 eigenVector2 = float2(eigenVector1.y, -eigenVector1.x);

  basisVector1 = eigenVector1 * splatScale * min(stdDev * sqrt(eigenValue1), 2048.0);
  basisVector2 = eigenVector2 * splatScale * min(stdDev * sqrt(eigenValue2), 2048.0);

  return true;
}

#endif // THREEDGS_COVARIANCE_HLSL

