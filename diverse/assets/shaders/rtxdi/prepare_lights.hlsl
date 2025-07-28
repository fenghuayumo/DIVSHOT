#include "../inc/math.hlsl"
#include "../inc/samplers.hlsl"
#include "../inc/mesh.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/bindless.hlsl"
#include "../inc/lights/light_shaping.hlsl"

struct PrepareLightsTask
{
    uint instanceAndGeometryIndex; // low 12 bits are geometryIndex, mid 19 bits are instanceIndex, high bit is TASK_PRIMITIVE_LIGHT_BIT
    uint triangleCount;
    uint lightBufferOffset;
    int previousLightBufferOffset; // -1 means no previous data
};
struct PrepareLightsConstants {
    uint numTasks;
    uint currentFrameLightOffset;
    uint previousFrameLightOffset;
    uint pad;
};

[[vk::binding(0)]] StructuredBuffer<PrepareLightsTask> t_TaskBuffer;
[[vk::binding(1)]] RWStructuredBuffer<PolymorphicLightInfo> u_LightDataBuffer;
[[vk::binding(2)]] RWBuffer<uint> u_LightIndexMappingBuffer;
[[vk::binding(3)]] RWTexture2D<float> u_LocalLightPdfTexture;
[[vk::binding(4)]] ConstantBuffer<PrepareLightsConstants> g_Const;

#define ENVIRONMENT_SAMPLER sampler_llc // doesn't matter in this pass
#define IES_SAMPLER sampler_llc
#include "../inc/lights/light_common.hlsl"

#define TASK_PRIMITIVE_LIGHT_BIT 0x80000000u

bool FindTask(uint dispatchThreadId, out PrepareLightsTask task)
{
    // Use binary search to find the task that contains the current thread's output index:
    //   task.lightBufferOffset <= dispatchThreadId < (task.lightBufferOffset + task.triangleCount)

    int left = 0;
    int right = int(g_Const.numTasks) - 1;

    while (right >= left)
    {
        int middle = (left + right) / 2;
        task = t_TaskBuffer[middle];

        int tri = int(dispatchThreadId) - int(task.lightBufferOffset); // signed

        if (tri < 0)
        {
            // Go left
            right = middle - 1;
        }
        else if (tri < task.triangleCount)
        {
            // Found it!
            return true;
        }
        else
        {
            // Go right
            left = middle + 1;
        }
    }

    return false;
}

[numthreads(256, 1, 1)]
void main(uint dispatchThreadId: SV_DispatchThreadID, uint groupThreadId: SV_GroupThreadID)
{
    PrepareLightsTask task = (PrepareLightsTask)0;

    if (!FindTask(dispatchThreadId, task))
        return;

    uint triangleIdx = dispatchThreadId - task.lightBufferOffset;
    bool isPrimitiveLight = (task.instanceAndGeometryIndex & TASK_PRIMITIVE_LIGHT_BIT) != 0;

    PolymorphicLightInfo lightInfo = (PolymorphicLightInfo)0;

    if (!isPrimitiveLight)
    {
        uint instance_id = task.instanceAndGeometryIndex >> 12;
        uint geometry_id = task.instanceAndGeometryIndex & 0xFFF;
        uint geometry_offset = instance_dynamic_parameters_dyn[instance_id].geometry_offset;
        Mesh mesh = meshes_buffer[geometry_offset + geometry_id];

        // Indices of the triangle
        uint primitive_index = triangleIdx * 3;
        uint3 ind = uint3(
            bindless_ib[mesh.index_buf_id].Load((primitive_index + 0) * sizeof(uint)),
            bindless_ib[mesh.index_buf_id].Load((primitive_index + 1) * sizeof(uint)),
            bindless_ib[mesh.index_buf_id].Load((primitive_index + 2) * sizeof(uint))
        );

        Vertex v0 = unpack_vertex(VertexPacked(asfloat(bindless_vb[mesh.vertex_buf_id].Load4(ind.x * sizeof(float4) + mesh.vertex_pos_nor_offset))));
        Vertex v1 = unpack_vertex(VertexPacked(asfloat(bindless_vb[mesh.vertex_buf_id].Load4(ind.y * sizeof(float4) + mesh.vertex_pos_nor_offset))));
        Vertex v2 = unpack_vertex(VertexPacked(asfloat(bindless_vb[mesh.vertex_buf_id].Load4(ind.z * sizeof(float4) + mesh.vertex_pos_nor_offset))));

        float3 positions[3];
        positions[0] = v0.position;
        positions[1] = v1.position;
        positions[2] = v2.position;

        uint material_id = mesh.material_id;
        MeshMaterial material = materials_buffer[material_id];
        
        float3 radiance = material.emissive;

        if (material.emissive_map >= 0 )
        {
            Texture2D emissiveTexture = bindless_textures[NonUniformResourceIndex(material.emissive_map)];

            // Load the vertex UVs
            float2 uvs[3];
            uvs[0] = asfloat(bindless_vb[mesh.vertex_buf_id].Load2(ind.x * sizeof(float2) + mesh.vertex_uv_offset));
            uvs[1] = asfloat(bindless_vb[mesh.vertex_buf_id].Load2(ind.y * sizeof(float2) + mesh.vertex_uv_offset));
            uvs[2] = asfloat(bindless_vb[mesh.vertex_buf_id].Load2(ind.z * sizeof(float2) + mesh.vertex_uv_offset));

            // Calculate the triangle edges and edge lengths in UV space
            float2 edges[3];
            edges[0] = uvs[1] - uvs[0];
            edges[1] = uvs[2] - uvs[1];
            edges[2] = uvs[0] - uvs[2];

            float3 edgeLengths;
            edgeLengths[0] = length(edges[0]);
            edgeLengths[1] = length(edges[1]);
            edgeLengths[2] = length(edges[2]);

            // Find the shortest edge and the other two (longer) edges
            float2 shortEdge;
            float2 longEdge1;
            float2 longEdge2;

            if (edgeLengths[0] < edgeLengths[1] && edgeLengths[0] < edgeLengths[2])
            {
                shortEdge = edges[0];
                longEdge1 = edges[1];
                longEdge2 = edges[2];
            }
            else if (edgeLengths[1] < edgeLengths[2])
            {
                shortEdge = edges[1];
                longEdge1 = edges[2];
                longEdge2 = edges[0];
            }
            else
            {
                shortEdge = edges[2];
                longEdge1 = edges[0];
                longEdge2 = edges[1];
            }

            // Use anisotropic sampling with the sample ellipse axes parallel to the short edge
            // and the median from the opposite vertex to the short edge.
            // This ellipse is roughly inscribed into the triangle and approximates long or skinny
            // triangles with highly anisotropic sampling, and is mostly round for usual triangles.
            float2 shortGradient = shortEdge * (2.0 / 3.0);
            float2 longGradient = (longEdge1 + longEdge2) / 3.0;

            // Sample
            float2 centerUV = (uvs[0] + uvs[1] + uvs[2]) / 3.0;
            float3 emissiveMask = emissiveTexture.SampleGrad(sampler_llc, centerUV, shortGradient, longGradient).rgb;

            radiance *= emissiveMask;
        }

        radiance.rgb = max(0, radiance.rgb);

        TriangleLight triLight;
        triLight.base = positions[0];
        triLight.edge1 = positions[1] - positions[0];
        triLight.edge2 = positions[2] - positions[0];
        triLight.radiance = radiance;

        lightInfo = triLight.Store();
    }
    else
    {
        uint primitiveLightIndex = task.instanceAndGeometryIndex & ~TASK_PRIMITIVE_LIGHT_BIT;
        lightInfo = scene_lights_dyn[primitiveLightIndex];
    }

    uint lightBufferPtr = task.lightBufferOffset + triangleIdx;
    u_LightDataBuffer[g_Const.currentFrameLightOffset + lightBufferPtr] = lightInfo;

    // If this light has existed on the previous frame, write the index mapping information
    // so that temporal resampling can be applied to the light correctly when it changes
    // the index inside the light buffer.
    if (task.previousLightBufferOffset >= 0)
    {
        uint prevBufferPtr = task.previousLightBufferOffset + triangleIdx;

        // Mapping buffer for the previous frame points at the current frame.
        // Add one to indicate that this is a valid mapping, zero is invalid.
        u_LightIndexMappingBuffer[g_Const.previousFrameLightOffset + prevBufferPtr] =
            g_Const.currentFrameLightOffset + lightBufferPtr + 1;

        // Mapping buffer for the current frame points at the previous frame.
        // Add one to indicate that this is a valid mapping, zero is invalid.
        u_LightIndexMappingBuffer[g_Const.currentFrameLightOffset + lightBufferPtr] =
            g_Const.previousFrameLightOffset + prevBufferPtr + 1;
    }

    // Calculate the total flux
    float emissiveFlux = PolymorphicLight::getPower(lightInfo);

    // Write the flux into the PDF texture
    uint2 pdfTexturePosition = linear_index_to_z_curve(lightBufferPtr);
    u_LocalLightPdfTexture[pdfTexturePosition] = emissiveFlux;
}