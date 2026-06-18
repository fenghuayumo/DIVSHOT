#include "../inc/binding.hlsl"
// 3DGUT Mesh Shader for Gaussian Splatting with Fisheye Camera Support
// Based on NVIDIA's vk_gaussian_splatting implementation
// 
// Key differences from standard 3DGS:
// - For pinhole cameras: uses standard 3DGS covariance projection (efficient)
// - For fisheye cameras: uses GUT (Unscented Transform) projection (handles non-linear cameras)
// - Fragment shader uses ray-based evaluation for fisheye (in gsplat_gut.frag.hlsl)

#include "../inc/frame_constants.hlsl"
#include "../inc/bindless.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "gaussian_common.hlsl"
#include "gsplat_sh.hlsl"
#include "threedgs_covariance.hlsl"
#include "threedgut_projection.hlsl"

#define MESH_SHADER_WORKGROUP_SIZE 32
#define MAX_VERTICES   (4 * MESH_SHADER_WORKGROUP_SIZE)
#define MAX_PRIMITIVES (2 * MESH_SHADER_WORKGROUP_SIZE)

// Per-vertex output
struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(5)]] float2 fragPos : TEXCOORD0;
};

// Per-primitive output - contains flat data for all vertices of the primitive
struct PrimitiveOutput
{
    [[vk::location(0)]] nointerpolation uint splatId : SPLATID;
    [[vk::location(1)]] nointerpolation float4 splatCol : COLOR0;
    [[vk::location(2)]] nointerpolation float3 splatPosition : POSITION0;
    [[vk::location(3)]] nointerpolation float3 splatScale : SCALE0;
    [[vk::location(4)]] nointerpolation float4 splatRotation : ROTATION0;
};

// Convert quaternion (scalar-last: x,y,z,w) to 3x3 rotation matrix
float3x3 quatToRotationMatrix(float4 q)
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

// Push constants
DS_CBUFFER(0) cbuffer PushConstants {
    float4x4 model_matrix;
    uint buf_id;
    uint surface_width;
    uint surface_height;
    uint num_gaussians;
    float4 locked_color;
    float4 select_color;
    float4 tintColor;
    float4 color_offset;
};

DS_RESOURCE(1) StructuredBuffer<uint> point_list_value_buffer;
DS_RESOURCE(2) StructuredBuffer<uint> indirect_params;

// Get color with SH evaluation
float3 get_color(uint index, float3 direction) {
    float3 color = read_splat_color(buf_id, index);
#if SH_DEGREE > 0
    color += read_splat_sh_color(buf_id, index, direction);
#endif
    return color;   
}

// Emit degenerate quad for culled splats
// Same as NVIDIA's vk_gaussian_splatting - only set vertex position, Z=2.0 is beyond far plane
void emitDegeneratedQuad(uint localIndex, out vertices VertexOutput verts[MAX_VERTICES])
{
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        verts[localIndex * 4 + i].position = float4(0.0, 0.0, 2.0, 1.0);
        verts[localIndex * 4 + i].fragPos = float2(0.0, 0.0);
    }
}

[outputtopology("triangle")]
[numthreads(MESH_SHADER_WORKGROUP_SIZE, 1, 1)]
void main(
    uint3 groupThreadID : SV_GroupThreadID,
    uint3 groupID : SV_GroupID,
    uint3 dispatchThreadID : SV_DispatchThreadID,
    out vertices VertexOutput verts[MAX_VERTICES],
    out primitives PrimitiveOutput prims[MAX_PRIMITIVES],
    out indices uint3 triangles[MAX_PRIMITIVES])
{
    const uint localIndex = groupThreadID.x;
    const uint baseIndex = dispatchThreadID.x;
    
    // Get actual splat count from indirect buffer
    // prepare_gut_mesh_args.hlsl stores: [0]=groupCountX, [1]=groupCountY, [2]=groupCountZ, [3]=splatCount
    uint splatCount = indirect_params[3];
    if (splatCount == 0)
        splatCount = num_gaussians;
    
    // Compute output count with underflow protection
    const uint groupStartIndex = groupID.x * MESH_SHADER_WORKGROUP_SIZE;
    uint outputQuadCount = 0;
    if (groupStartIndex < splatCount)
    {
        outputQuadCount = min(MESH_SHADER_WORKGROUP_SIZE, splatCount - groupStartIndex);
    }
    
    // Set output counts - only first thread does this (like NVIDIA's implementation)
    if (localIndex == 0)
    {
        SetMeshOutputCounts(outputQuadCount * 4, outputQuadCount * 2);
    }
    
    // Early out if this group has no work
    if (outputQuadCount == 0)
        return;
    
    // Only process if this thread's splat is valid
    if (baseIndex < splatCount)
    {
    const uint splatIndex = point_list_value_buffer[baseIndex];
    
    // Emit triangle indices first (always needed for valid threads)
    triangles[localIndex * 2 + 0] = uint3(0, 2, 1) + localIndex * 4;
    triangles[localIndex * 2 + 1] = uint3(2, 0, 3) + localIndex * 4;
    
    // Load Gaussian data
    Gaussian gaussian = bindless_gaussians(buf_id).Load<Gaussian>(splatIndex * sizeof(Gaussian));
    uint state = bindless_splat_state[buf_id].Load<uint>(splatIndex * sizeof(uint));
    
    float4 gs_position = float4(gaussian.position.xyz, asfloat(state));
    uint gs_state = asuint(gs_position.w);
    uint op_state = getOpState(gs_state);
    
    // Check delete state
    if (op_state & DELETE_STATE)
    {
        emitDegeneratedQuad(localIndex, verts);
        return;
    }
    
    // Get transform
#if SPLAT_EDIT
    uint transform_index = getTransformIndex(gs_state);
    float4 transform0 = bindless_splat_transform(buf_id).Load<float4>(transform_index * sizeof(float4) * 3);
    float4 transform1 = bindless_splat_transform(buf_id).Load<float4>(transform_index * sizeof(float4) * 3 + sizeof(float4));
    float4 transform2 = bindless_splat_transform(buf_id).Load<float4>(transform_index * sizeof(float4) * 3 + sizeof(float4) * 2);
    float4x4 transform = float4x4(transform0.x, transform1.x, transform2.x, 0,
                                   transform0.y, transform1.y, transform2.y, 0,
                                   transform0.z, transform1.z, transform2.z, 0,
                                   transform0.w, transform1.w, transform2.w, 1);
    transform = mul(transform, model_matrix);
#else
    float4x4 transform = model_matrix;
#endif
    
    // Get rotation and scale
    uint4 rotation_scale = gaussian.rotation_scale;
    float4 scale_opacity = unpack_uint2(rotation_scale.zw);
    float opac = scale_opacity.w;
    
    // Alpha culling
    static const float alphaCullThreshold = 1.0 / 255.0;
    if (opac < alphaCullThreshold)
    {
        emitDegeneratedQuad(localIndex, verts);
        return;
    }
    
    float4 q = unpack_uint2(rotation_scale.xy);
    float quat_norm_sqr = dot(q, q);
    if (quat_norm_sqr < 1e-6)
    {
        emitDegeneratedQuad(localIndex, verts);
        return;
    }
    
    q = normalize(q);
    // Convert quaternion from scalar-first (w,x,y,z) to scalar-last (x,y,z,w) format
    // Current code stores as (w,x,y,z) but our functions expect (x,y,z,w)
    float4 q_scalar_last = float4(q.yzw, q.x);
    
    float3 s = scale_opacity.xyz * saturate(color_offset.w);
    
    // Set primitive data ASAP (same for both triangles)
    prims[localIndex * 2 + 0].splatId = splatIndex;
    prims[localIndex * 2 + 1].splatId = splatIndex;
    prims[localIndex * 2 + 0].splatPosition = gs_position.xyz;
    prims[localIndex * 2 + 1].splatPosition = gs_position.xyz;
    prims[localIndex * 2 + 0].splatScale = s;
    prims[localIndex * 2 + 1].splatScale = s;
    prims[localIndex * 2 + 0].splatRotation = q_scalar_last;
    prims[localIndex * 2 + 1].splatRotation = q_scalar_last;
    
    // Compute view direction for SH evaluation (same method as gsplat_vs.hlsl)
    // Use model_view matrix's 3x3 part to transform view-space position to model space direction
    float4x4 view = frame_constants.view_constants.world_to_view;
    float4x4 model_view = mul(transform, view);
    float4 world_pos_sh = mul(float4(gs_position.xyz, 1.0), transform);
    float4 p_view = mul(world_pos_sh, view);
    float3 center_view = p_view.xyz / p_view.w;
    
    float3x3 model_view_3x3 = float3x3(
        model_view[0][0], model_view[0][1], model_view[0][2],
        model_view[1][0], model_view[1][1], model_view[1][2],
        model_view[2][0], model_view[2][1], model_view[2][2]
    );
    float3 viewDir = normalize(mul(model_view_3x3, center_view));
    
    // Compute color with view-dependent effects
    float4 splatColor;
#if VISUALIZE_NORMAL
    splatColor = float4(0.5, 0.5, 1.0, opac);
#elif VISUALIZE_DEPTH
    // Use p_view already computed above (world_pos_sh is equivalent to world_pos)
    float normalized_depth = saturate(-p_view.z / 100.0);
    float r = smoothstep(0.5, 1.0, normalized_depth);
    float g = 1.0 - abs(normalized_depth - 0.5) * 2.0;
    float b = 1.0 - smoothstep(0.0, 0.5, normalized_depth);
    splatColor = float4(r, g, b, opac);
#else
    if (op_state & DELETE_STATE)
        splatColor = float4(0, 0, 0, 0);
    else if (op_state & HIDE_STATE)
        splatColor = float4(get_color(splatIndex, viewDir) * locked_color.xyz, opac);
    else if (op_state & SELECT_STATE)
        splatColor = float4(lerp(get_color(splatIndex, viewDir), select_color.xyz * 0.8, select_color.w), opac);
    else
        splatColor = float4(get_color(splatIndex, viewDir) * tintColor.xyz + color_offset.xyz, tintColor.w * opac);
#endif
    
    prims[localIndex * 2 + 0].splatCol = splatColor;
    prims[localIndex * 2 + 1].splatCol = splatColor;
    
    // Camera type: 0 = pinhole, 1 = fisheye
    const uint cameraType = frame_constants.camera_type;
    float2 viewport = float2(surface_width, surface_height);
    float4x4 proj = frame_constants.view_constants.view_to_clip;
    
    // Note: view, model_view, p_view already computed above for SH evaluation
    // world_pos_sh is the same as world_pos
    
    // Check if behind camera
    if (p_view.z >= 0.0)
    {
        emitDegeneratedQuad(localIndex, verts);
        return;
    }
    
    // Variables for projection results
    float2 ndcCenter2D;
    float ndcZ;
    float2 basisVector1, basisVector2;
    float adjustedOpacity = opac;
    
    if (cameraType == CAMERA_FISHEYE)
    {
        // ============ FISHEYE CAMERA: Use GUT (Unscented Transform) Projection ============
        // This handles the non-linear fisheye projection correctly
        
        // Initialize fisheye camera model using FOV from frame constants
        float fisheyeFovRad = frame_constants.fov_rad;
        if (fisheyeFovRad <= 0.0)
            fisheyeFovRad = 3.14159265359; // Default to 180 degrees
        
        SensorModel sensorModel = initFisheyeCamera(viewport, fisheyeFovRad);
        
        // Convert quaternion to rotation matrix for GUT projection
        // q_scalar_last is in (x,y,z,w) format
        float3x3 particleRotation = quatToRotationMatrix(q_scalar_last);
        
        // Perform GUT projection
        float2 particleProjCenter;
        float3 particleProjCovariance;
        
        if (!gutParticleProjection(viewport, sensorModel, transform, 
                                   gs_position.xyz, s, particleRotation,
                                   particleProjCenter, particleProjCovariance))
        {
            emitDegeneratedQuad(localIndex, verts);
            return;
        }
        
        // Compute basis vectors from GUT covariance using eigen decomposition
        // Use sqrt8 (same as pinhole) for consistent splat sizing
        if (!gutProjectedExtentBasis(particleProjCovariance, sqrt8, 1.0, adjustedOpacity, basisVector1, basisVector2))
        {
            emitDegeneratedQuad(localIndex, verts);
            return;
        }
        
        // Clamp basis vectors to prevent extremely large splats at fisheye edges
        // This helps reduce artifacts from numerical instability in edge regions
        const float maxBasisLength = 512.0;  // Maximum pixel extent
        float len1 = length(basisVector1);
        float len2 = length(basisVector2);
        if (len1 > maxBasisLength) basisVector1 *= maxBasisLength / len1;
        if (len2 > maxBasisLength) basisVector2 *= maxBasisLength / len2;
        
        // Convert projected center from pixel coordinates to NDC
        // Pixel coordinates: (0,0) at top-left, Y increases downward
        // Our projection matrix uses Vulkan convention where Y is flipped
        // So we need to flip Y: pixel Y=0 (top) -> NDC Y=+1, pixel Y=height (bottom) -> NDC Y=-1
        ndcCenter2D.x = (particleProjCenter.x / viewport.x) * 2.0 - 1.0;
        ndcCenter2D.y = 1.0 - (particleProjCenter.y / viewport.y) * 2.0;  // Flip Y
        
        // Also flip basisVector Y component to match the coordinate system
        basisVector1.y = -basisVector1.y;
        basisVector2.y = -basisVector2.y;
        
        // Compute depth in clip space (approximate for fisheye)
        // Note: projection matrix is not fully correct for fisheye, this is a reasonable approximation
        float4 clipPos = mul(p_view, proj);
        ndcZ = clipPos.z / clipPos.w;
    }
    else
    {
        // ============ PINHOLE CAMERA: Use Standard 3DGS Covariance Projection ============
        // This is the efficient standard method for linear camera models
        
        // Perspective projection using projection matrix (same as gsplat_vs.hlsl)
        float4 p_hom = mul(p_view, proj);
        float p_w = 1.0 / (p_hom.w + 0.0000001);
        float4 p_proj = p_hom * p_w;
        
        // Frustum culling
        const float frustumDilation = 0.15;
        const float clip = 1.0 + frustumDilation;
        if (abs(p_proj.x) > clip || abs(p_proj.y) > clip || 
            p_proj.z < (0.0 - frustumDilation) || p_proj.z > 1.0)
        {
            emitDegeneratedQuad(localIndex, verts);
            return;
        }
        
        // Compute 3D covariance matrix (same as gsplat_vs.hlsl)
        float3 covA, covB;
        // Use original quaternion format (scalar-first: w,x,y,z)
        threedgs_compute_covariance_3d(s, q, covA, covB);
        
        // Reconstruct full 3x3 covariance matrix
        float3x3 cov3D = float3x3(
            covA.x, covA.y, covA.z,
            covA.y, covB.x, covB.y,
            covA.z, covB.y, covB.z
        );
        
        // Calculate focal lengths
        const float tan_fovx = 1.0 / proj[0][0];
        const float tan_fovy = 1.0 / abs(proj[1][1]);
        const float focal_x = surface_width / (2.0 * tan_fovx);
        const float focal_y = surface_height / (2.0 * tan_fovy);
        float2 focal = float2(focal_x, focal_y);
        const bool is_ortho = proj[2][3] == 0.0;
        
        // Project 3D covariance to 2D (same as gsplat_vs.hlsl)
        float3 cov2Dv = threedgs_covariance_projection(cov3D, p_view, focal, model_view, is_ortho);
        
        // Compute basis vectors from projected covariance
        if (!threedgs_projected_extent_basis(cov2Dv, sqrt8, 1.0, adjustedOpacity, basisVector1, basisVector2))
        {
            emitDegeneratedQuad(localIndex, verts);
            return;
        }
        
        ndcCenter2D = p_proj.xy;
        ndcZ = p_proj.z;
    }
    
    // ============ Common Code: Generate Quad Vertices ============
    
    // Numerical stability check for basis vectors
    if (!isfinite(basisVector1.x) || !isfinite(basisVector1.y) ||
        !isfinite(basisVector2.x) || !isfinite(basisVector2.y))
    {
        emitDegeneratedQuad(localIndex, verts);
        return;
    }
    
    // Update opacity after antialiasing adjustment
    prims[localIndex * 2 + 0].splatCol.w = adjustedOpacity;
    prims[localIndex * 2 + 1].splatCol.w = adjustedOpacity;
    
    // Check for valid NDC coordinates
    if (!isfinite(ndcCenter2D.x) || !isfinite(ndcCenter2D.y) || !isfinite(ndcZ))
    {
        emitDegeneratedQuad(localIndex, verts);
        return;
    }
    
    float2 basisViewport = 1.0 / viewport;
    
    // Vertex positions for quad
    const float2 vertexPositions[4] = {
        float2(-1.0, -1.0),
        float2(1.0, -1.0),
        float2(1.0, 1.0),
        float2(-1.0, 1.0)
    };
    
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        float2 vertPos = vertexPositions[i];
        // basisVector is in pixel space, convert to NDC
        float2 ndcOffset = (vertPos.x * basisVector1 + vertPos.y * basisVector2) * basisViewport * 2.0;
        
        verts[localIndex * 4 + i].position = float4(ndcCenter2D + ndcOffset, ndcZ, 1.0);
        // fragPos is the position relative to splat center, scaled by sqrt(8) like gsplat_vs.hlsl
        verts[localIndex * 4 + i].fragPos = vertPos * sqrt8;
    }
    } // end if (baseIndex < splatCount)
}
