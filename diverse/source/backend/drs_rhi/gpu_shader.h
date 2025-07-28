// #pragma once
// #include <core/config.hpp>
// #include "config.hpp"
// #include <string>
// namespace diverse
// {
//     namespace rhi
//     {
//         enum ShaderPipelineStage
//         {
//             Vertex,
//             Pixel,
//             RayGen,
//             RayMiss,
//             RayClosestHit,
//         };

//         struct PipelineShaderDesc
//         {
//             ShaderPipelineStage		stage;
//             // std::optional<std::vector<std::pair<u32, VkDescriptorSetLayoutCreateFlagBits>>>	descriptor_set_layout_flags;
//             uint32						push_constants_bytes;
//             std::string				entry;
//             std::string				path;
//         };


//     }
// }