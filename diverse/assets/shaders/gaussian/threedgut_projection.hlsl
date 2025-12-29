#ifndef THREEDGUT_PROJECTION_HLSL
#define THREEDGUT_PROJECTION_HLSL

// 3DGUT - Gaussian Unscented Transform for Fisheye Camera Projection
// Based on NVIDIA's vk_gaussian_splatting implementation

// GUT parameters
#define GUT_D 3  // Dimensions
#define GUT_ALPHA 1.0
#define GUT_BETA 2.0
#define GUT_KAPPA 0.0
#define GUT_LAMBDA (GUT_ALPHA * GUT_ALPHA * (GUT_D + GUT_KAPPA) - GUT_D)
#define GUT_DELTA sqrt(GUT_D + GUT_LAMBDA)
#define GUT_COVARIANCE_DILATION 0.3
// Alpha threshold for culling 
#define GUT_ALPHA_THRESHOLD 0.01
// Margin tolerance for in-image check 
// This is a multiplier of resolution, not a scale factor
#define GUT_IN_IMAGE_MARGIN_FACTOR 0.1
#define GUT_REQUIRE_ALL_SIGMA_POINTS_VALID false
#define GUT_TIGHT_OPACITY_BOUNDING true
#define GUT_RECT_BOUNDING true

// Camera model types
#define CAMERA_PINHOLE 0
#define CAMERA_FISHEYE 1

// Sensor model for different camera types
struct SensorModel
{
    int cameraType;      // CAMERA_PINHOLE or CAMERA_FISHEYE
    float2 nearFar;      // Near and far planes
    float2 viewport;     // Viewport size in pixels
    float2 focal;        // Focal lengths
    float2 principalPoint; // Principal point (usually center of viewport)
    float maxAngle;      // Max angle for fisheye (computed from FOV)
    bool isOrtho;        // True for orthographic projection
};

// Compute max radius from principal point to image boundaries
float computeMaxRadius(float2 imageSize, float2 principalPoint)
{
    float2 maxDiag = float2(
        max(principalPoint.x, imageSize.x - principalPoint.x),
        max(principalPoint.y, imageSize.y - principalPoint.y)
    );
    return length(maxDiag);
}

// Compute max angle for fisheye camera (same as vk_gaussian_splatting)
float computeMaxAngle(float2 resolution, float2 principalPoint, float2 focalLength)
{
    float maxRadiusPixels = computeMaxRadius(resolution, principalPoint);
    float fovAngleX = 2.0 * maxRadiusPixels / focalLength.x;
    float fovAngleY = 2.0 * maxRadiusPixels / focalLength.y;
    float maxAngle = max(fovAngleX, fovAngleY) / 2.0;
    return maxAngle;
}

// Initialize a pinhole camera model (perspective or orthographic)
SensorModel initPinholeCamera(float2 nearFar, float2 viewport, float2 focal, bool isOrtho)
{
    SensorModel model;
    model.cameraType = CAMERA_PINHOLE;
    model.nearFar = nearFar;
    model.viewport = viewport;
    model.focal = focal;
    model.principalPoint = viewport * 0.5;
    model.maxAngle = 0.0;
    model.isOrtho = isOrtho;
    return model;
}

// Initialize a fisheye camera model with specified FOV in radians
// fisheyeFovRad: total field of view in radians (e.g., PI for 180 degrees)
// Uses separate focal lengths for X and Y to create elliptical fisheye effect
SensorModel initFisheyeCamera(float2 viewport, float fisheyeFovRad)
{
    SensorModel model;
    model.cameraType = CAMERA_FISHEYE;
    model.nearFar = float2(0.01, 1000.0);
    model.viewport = viewport;
    model.principalPoint = viewport * 0.5;
    
    // For equidistant fisheye: r = f * theta
    // At image edge, theta = fisheyeFov / 2
    // For elliptical fisheye, calculate separate focal lengths for X and Y
    // This creates an elliptical projection that fills the screen
    float maxAngle = fisheyeFovRad / 2.0;
    
    // focal_x = halfWidth / maxAngle, focal_y = halfHeight / maxAngle
    // This ensures the fisheye fills the entire screen as an ellipse
    float focal_x = model.principalPoint.x / maxAngle;
    float focal_y = model.principalPoint.y / maxAngle;
    model.focal = float2(focal_x, focal_y);
    model.maxAngle = maxAngle;
    model.isOrtho = false;  // Fisheye is always perspective-like
    
    return model;
}

// 4x4 Matrix inverse function
float4x4 inverse4x4(float4x4 m)
{
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0 / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}

// Transform point from world to camera/view space using frame_constants
// Note: frame_constants is already available via frame_constants.hlsl include
float3 worldToCamera(float3 worldPos)
{
    float4 viewPos = mul(float4(worldPos, 1.0), frame_constants.view_constants.world_to_view);
    return viewPos.xyz;
}

// Stable 2D norm (same as vk_gaussian_splatting)
float stableNorm2(float2 v)
{
    return sqrt(v.x * v.x + v.y * v.y);
}

// Check if point is within resolution bounds with tolerance
bool withinResolution(float2 resolution, float tolerance, float2 projected)
{
    float2 tolMargin = resolution * tolerance;
    return projected.x >= -tolMargin.x && projected.x < resolution.x + tolMargin.x &&
           projected.y >= -tolMargin.y && projected.y < resolution.y + tolMargin.y;
}

// Project point using pinhole camera model (returns pixel coordinates)
// Note: This is used for GUT covariance calculation
// camPos is in RUF coordinate system (+Z is forward)
// Supports both perspective and orthographic projection
bool projectPinhole(float3 camPos, float2 resolution, SensorModel sensorModel, float tolerance, out float2 projected)
{
    projected = float2(0, 0);
    
    // Check if behind camera (+Z is forward in RUF)
    if (camPos.z <= 0.0)
        return false;
    
    if (sensorModel.isOrtho)
    {
        // Orthographic projection: no perspective division
        // In ortho mode, focal length represents pixels per world unit
        projected.x = camPos.x * sensorModel.focal.x + sensorModel.principalPoint.x;
        projected.y = sensorModel.principalPoint.y - camPos.y * sensorModel.focal.y;
    }
    else
    {
        // Perspective projection to pixel coordinates
        float2 uvNormalized = camPos.xy / camPos.z;
        
        // Project to pixel coordinates with Y flip to match fragment shader
        // (pixel Y=0 at top, camera Y up in RUF)
        projected.x = uvNormalized.x * sensorModel.focal.x + sensorModel.principalPoint.x;
        projected.y = sensorModel.principalPoint.y - uvNormalized.y * sensorModel.focal.y;
    }
    
    // Check bounds with tolerance margin
    return withinResolution(resolution, tolerance, projected);
}

// Project point using fisheye camera model (OpenCV fisheye, equidistant projection)
// camPos is in RUF coordinate system (+Z is forward)
bool projectFisheye(float3 camPos, float2 resolution, SensorModel sensorModel, float tolerance, out float2 projected)
{
    projected = float2(0, 0);
    
    // Check if behind camera (+Z is forward in RUF)
    if (camPos.z <= 0.0)
        return false;
    
    const float eps = 1e-7;
    const float rho = max(stableNorm2(camPos.xy), eps);
    const float thetaFull = atan2(rho, camPos.z);
    
    // Limit angles to max_angle to prevent projected points outside valid cone
    const float theta = min(thetaFull, sensorModel.maxAngle);
    
    // Equidistant fisheye projection: r = f * theta
    const float delta = theta / rho;
    
    // Project to pixel coordinates with Y flip to match fragment shader
    // (pixel Y=0 at top, camera Y up in RUF)
    projected.x = sensorModel.focal.x * camPos.x * delta + sensorModel.principalPoint.x;
    projected.y = sensorModel.principalPoint.y - sensorModel.focal.y * camPos.y * delta;
    
    // Check if angle is within FOV and point is within resolution
    return (theta < sensorModel.maxAngle) && withinResolution(resolution, tolerance, projected);
}

// Project point with sensor model (uses frame_constants for view matrix)
// Converts from Vulkan view space (-Z forward) to RUF space (+Z forward) for projection
bool projectPoint(float3 worldPos, float2 resolution, SensorModel sensorModel, float tolerance, out float2 projected)
{
    float3 camPos = worldToCamera(worldPos);
    
    // Convert from Vulkan view space (-Z forward) to RUF space (+Z forward)
    // This is consistent with vk_gaussian_splatting's coordinate system
    float3 camPosRUF = float3(camPos.x, camPos.y, -camPos.z);
    
    if (sensorModel.cameraType == CAMERA_FISHEYE)
        return projectFisheye(camPosRUF, resolution, sensorModel, tolerance, projected);
    else
        return projectPinhole(camPosRUF, resolution, sensorModel, tolerance, projected);
}

// Convert quaternion to rotation matrix
float3x3 quatToMat3(float4 q)
{
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;
    
    return float3x3(
        1.0 - (yy + zz), xy - wz, xz + wy,
        xy + wz, 1.0 - (xx + zz), yz - wx,
        xz - wy, yz + wx, 1.0 - (xx + yy)
    );
}

// GUT particle projection using Unscented Transform
// Note: Uses frame_constants.view_constants.world_to_view internally
bool gutParticleProjection(
    float2 resolution,
    SensorModel sensorModel,
    float4x4 toWorldMatrix,
    float3 particlePosition,
    float3 particleScale,
    float3x3 particleRotation,
    out float2 particleProjCenter,
    out float3 particleProjCovariance)
{
    particleProjCenter = float2(0, 0);
    particleProjCovariance = float3(0, 0, 0);
    
    float3 particleMean = particlePosition;
    
    int numValidPoints = 0;
    float2 projectedSigmaPoints[2 * GUT_D + 1];
    
    const float Lambda = GUT_LAMBDA;
    
    // Project center point
    float3 worldPos = mul(float4(particleMean, 1.0), toWorldMatrix).xyz;
    if (projectPoint(worldPos, resolution, sensorModel, GUT_IN_IMAGE_MARGIN_FACTOR, projectedSigmaPoints[0]))
    {
        numValidPoints++;
    }
    particleProjCenter = projectedSigmaPoints[0] * (GUT_LAMBDA / (GUT_D + GUT_LAMBDA));
    
    const float weightI = 1.0 / (2.0 * (GUT_D + GUT_LAMBDA));
    
    // Project sigma points along each dimension
    [unroll]
    for (int i = 0; i < GUT_D; ++i)
    {
        // Get the i-th column of the rotation matrix (axis direction in world space)
        // In HLSL, matrix[i] returns the i-th row, so we need to extract column manually
        float3 rotationColumn = float3(particleRotation[0][i], particleRotation[1][i], particleRotation[2][i]);
        float3 delta = GUT_DELTA * particleScale[i] * rotationColumn;
        
        // Positive sigma point
        worldPos = mul(float4(particleMean + delta, 1.0), toWorldMatrix).xyz;
        if (projectPoint(worldPos, resolution, sensorModel, GUT_IN_IMAGE_MARGIN_FACTOR, projectedSigmaPoints[i + 1]))
        {
            numValidPoints++;
        }
        particleProjCenter += weightI * projectedSigmaPoints[i + 1];
        
        // Negative sigma point
        worldPos = mul(float4(particleMean - delta, 1.0), toWorldMatrix).xyz;
        if (projectPoint(worldPos, resolution, sensorModel, GUT_IN_IMAGE_MARGIN_FACTOR, projectedSigmaPoints[i + 1 + GUT_D]))
        {
            numValidPoints++;
        }
        particleProjCenter += weightI * projectedSigmaPoints[i + 1 + GUT_D];
    }
    
    // Check if enough points are valid
    if (GUT_REQUIRE_ALL_SIGMA_POINTS_VALID)
    {
        if (numValidPoints < (2 * GUT_D + 1))
            return false;
    }
    else if (numValidPoints == 0)
    {
        return false;
    }
    
    // Compute projected covariance
    {
        float2 centeredPoint = projectedSigmaPoints[0] - particleProjCenter;
        const float weight0 = GUT_LAMBDA / (GUT_D + GUT_LAMBDA) + (1.0 - GUT_ALPHA * GUT_ALPHA + GUT_BETA);
        particleProjCovariance = weight0 * float3(
            centeredPoint.x * centeredPoint.x,
            centeredPoint.x * centeredPoint.y,
            centeredPoint.y * centeredPoint.y
        );
    }
    
    [unroll]
    for (int j = 0; j < 2 * GUT_D; ++j)
    {
        float2 centeredPoint = projectedSigmaPoints[j + 1] - particleProjCenter;
        particleProjCovariance += weightI * float3(
            centeredPoint.x * centeredPoint.x,
            centeredPoint.x * centeredPoint.y,
            centeredPoint.y * centeredPoint.y
        );
    }
    
    return true;
}

// Compute extent and conic opacity from projected covariance
bool gutProjectedExtentConicOpacity(
    float3 covariance,
    float opacity,
    out float2 extent,
    out float4 conicOpacity,
    out float maxConicOpacityPower)
{
    extent = float2(0, 0);
    conicOpacity = float4(0, 0, 0, 0);
    maxConicOpacityPower = 0.0;
    
    float3 dilatedCovariance = float3(
        covariance.x + GUT_COVARIANCE_DILATION,
        covariance.y,
        covariance.z + GUT_COVARIANCE_DILATION
    );
    
    float dilatedCovDet = dilatedCovariance.x * dilatedCovariance.z - dilatedCovariance.y * dilatedCovariance.y;
    if (dilatedCovDet == 0.0)
        return false;
    
    conicOpacity.xyz = float3(dilatedCovariance.z, -dilatedCovariance.y, dilatedCovariance.x) / dilatedCovDet;
    
#if GSPLAT_AA
    float covDet = covariance.x * covariance.z - covariance.y * covariance.y;
    float convolutionFactor = sqrt(max(0.000025, covDet / dilatedCovDet));
    conicOpacity.w = opacity * convolutionFactor;
#else
    conicOpacity.w = opacity;
#endif
    
    if (conicOpacity.w < GUT_ALPHA_THRESHOLD)
        return false;
    
    maxConicOpacityPower = log(conicOpacity.w / GUT_ALPHA_THRESHOLD);
    float extentFactor = GUT_TIGHT_OPACITY_BOUNDING ? min(3.33, sqrt(2.0 * maxConicOpacityPower)) : 3.33;
    float minLambda = 0.01;
    float mid = 0.5 * (dilatedCovariance.x + dilatedCovariance.z);
    float lambda = mid + sqrt(max(minLambda, mid * mid - dilatedCovDet));
    float radius = extentFactor * sqrt(lambda);
    
    if (GUT_RECT_BOUNDING)
    {
        extent = min(extentFactor * sqrt(float2(dilatedCovariance.x, dilatedCovariance.z)), float2(radius, radius));
    }
    else
    {
        extent = float2(radius, radius);
    }
    
    return radius > 0.0;
}

// Compute extent basis vectors from projected covariance
bool gutProjectedExtentBasis(
    float3 covariance,
    float stdDev,
    float splatScale,
    inout float opacity,
    out float2 basisVector1,
    out float2 basisVector2)
{
    basisVector1 = float2(0, 0);
    basisVector2 = float2(0, 0);
    
#if GSPLAT_AA
    const float detOrig = covariance.x * covariance.z - covariance.y * covariance.y;
#endif
    
    covariance.x += GUT_COVARIANCE_DILATION;
    covariance.z += GUT_COVARIANCE_DILATION;
    
#if GSPLAT_AA
    const float detBlur = covariance.x * covariance.z - covariance.y * covariance.y;
    opacity *= sqrt(max(detOrig / detBlur, 0.0));
#endif
    
    const float a = covariance.x;
    const float d = covariance.z;
    const float b = covariance.y;
    const float D = a * d - b * b;
    const float trace = a + d;
    const float traceOver2 = 0.5 * trace;
    const float term2 = sqrt(max(0.1, traceOver2 * traceOver2 - D));
    float eigenValue1 = traceOver2 + term2;
    float eigenValue2 = traceOver2 - term2;
    
    if (eigenValue2 <= 0.0)
        return false;
    
    const float2 eigenVector1 = normalize(float2(b, eigenValue1 - a));
    const float2 eigenVector2 = float2(eigenVector1.y, -eigenVector1.x);
    
    basisVector1 = eigenVector1 * splatScale * min(stdDev * sqrt(eigenValue1), 2048.0);
    basisVector2 = eigenVector2 * splatScale * min(stdDev * sqrt(eigenValue2), 2048.0);
    
    return true;
}

#endif // THREEDGUT_PROJECTION_HLSL
