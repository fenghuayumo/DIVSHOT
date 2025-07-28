#ifdef DS_RENDER_API_METAL
#include "gpu_pipeline_metal.h"
#include "gpu_device_metal.h"

// This empty class lets us query for files relative to this file's bundle path using NSBundle bundleForClass hack
@interface DummyClassForPathHackBundle : NSObject
@end
@implementation DummyClassForPathHackBundle
@end


namespace diverse
{
    extern auto convert_spirv_metal_ir(const std::vector<u8>& spirv_ir,const std::string& entry,rhi::ShaderPipelineStage shader_stage)->std::vector<u8>;
    extern auto convert_dxil_metal_ir(const std::vector<u8>& dx_ir,const std::string& entry,rhi::ShaderPipelineStage shader_stage)->std::vector<u8>;
    extern auto convert_spirv_2_msl(const std::vector<u8>& spv,bool use_argument)->std::string;

    namespace rhi
    {
        auto get_metal_lib(id<MTLDevice> device,const CompiledShaderCode& shader_code,bool use_argument)->id<MTLLibrary>
        {
            NSError* __autoreleasing error = nil;
            
//            auto ir_data = convert_metal_ir(shader_code.codes, "main", rhi::ShaderPipelineStage::Compute);
//            dispatch_data_t data =
//                dispatch_data_create(ir_data.data(), ir_data.size(), dispatch_get_main_queue(), NULL);
//            id<MTLLibrary> lib = [device newLibraryWithData:data error:&error];
            
            NSMutableDictionary * __autoreleasing prep = [NSMutableDictionary dictionary];

            MTLCompileOptions* __autoreleasing options = [MTLCompileOptions new];
            options.preprocessorMacros = prep;
            auto msl = convert_spirv_2_msl(shader_code.codes,use_argument);
        
            //convert string to nstring
            NSString* msl_str = [NSString stringWithUTF8String:msl.c_str()];
            id<MTLLibrary> lib = [device newLibraryWithSource:msl_str options:options error:&error];
            if (error) {
                printf("%s: error: %s\n", __func__, [[error description] UTF8String]);
                return lib;
            }
            return lib;
        }
        
        GpuPipelineMetal::~GpuPipelineMetal()
        {
        }
        
        GpuPipelineMetal::GpuPipelineMetal(const StageDescriptorSetLayouts& descSets)
            :descriptor_layouts(descSets)
        {
        }

        ComputePipelineMetal::ComputePipelineMetal(id<MTLComputePipelineState> id_pipe,
                                                   const std::array<u32, 3>& group_size,
                                                   const StageDescriptorSetLayouts& descSets)
            : GpuPipelineMetal(descSets), pipeline(id_pipe)
        {
            this->group_size = group_size;
            this->ty = GpuPipeline::PieplineType::Compute;
        }
        RasterPipelineMetal::RasterPipelineMetal(id<MTLRenderPipelineState> id_pipe,const StageDescriptorSetLayouts& descSets)
            :  GpuPipelineMetal(descSets), pipeline(id_pipe)
        {
            this->ty = GpuPipeline::PieplineType::Raster;
        }
    
    
        auto cull_model_2_metal(CullMode mode)->MTLCullMode
        {
            switch (mode) {
                case CullMode::FRONT:
                    return MTLCullModeFront;
                case CullMode::BACK:
                    return MTLCullModeBack;
                    
                default:
                    break;
            }
            return MTLCullModeNone;
        }
        auto face_oder_2_metal(FrontFaceOrder order)->MTLWinding
        {
            switch (order) 
            {
                case FrontFaceOrder::CW:
                    return MTLWindingClockwise;
                case FrontFaceOrder::CCW:
                    return MTLWindingCounterClockwise;
                default:
                    break;
            }
            return MTLWindingClockwise;
        }
    
        auto create_descriptor_set_layouts(const GpuDeviceMetal& device, 
                StageDescriptorSetLayouts&& descriptor_sets, MetalShaderStageFlag stage_flags, 
                std::array<std::optional<std::pair<uint32, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> set_opts) -> StageDescriptorSetLayouts
        {
            uint32 max_set_id = 0;
            for (auto des_set : descriptor_sets)
                max_set_id = des_set.first > max_set_id ? des_set.first : max_set_id;

            auto set_count = max_set_id + 1;
            for (auto value : set_opts)
            {
                if (value)
                {
                    auto& [set_id, xx] = *value;
                    set_count = std::max(set_count, set_id + 1);
                }
            }

            for(auto set_index = 0; set_index < set_count; set_index++)
            {
                auto stage_flag = 0 == set_index ? stage_flags : MetalShaderStageFlag::All;
                // Find the descriptor set opts corresponding to the set index, and remove them from the opts list

                DescriptorSetLayoutOpts	_set_opts_default;
                auto& resolved_set_opts = _set_opts_default;
                for(auto& maybe_opt : set_opts)
                {
                    if (maybe_opt.has_value())
                    {
                        auto& opt = *maybe_opt;
                        if (opt.first == set_index)
                            resolved_set_opts = opt.second;
                    }
                }

                auto& setopts = resolved_set_opts;
                auto& set = setopts.replace.has_value() ? setopts.replace.value() : descriptor_sets[set_index];
                if( set.size() > 0)
                {
                }
                
            }
            
            return descriptor_sets;
        }
        
        auto get_descriptor_info(MTLArgument* arg, u32 set_idx)->std::unordered_map<u32,DescriptorInfo>
        {
            std::unordered_map<u32,DescriptorInfo> descriptor_dindings;
            NSLog(@"Found arg: %@\n", arg.name);
            if( arg.type == MTLArgumentTypeTexture)//not use argument buffer
            {
                
            }
            else if( arg.type == MTLArgumentTypeBuffer) //argument buffer
            {
                if (arg.bufferDataType == MTLDataTypeStruct)
                {
                    for( MTLStructMember* uniform in arg.bufferStructType.members )
                    {
                        NSLog(@"uniform: %@ type:%lu, location: %lu index: %lu", uniform.name, (unsigned long)uniform.dataType, (unsigned long)uniform.offset, (unsigned long)uniform.argumentIndex);
                        DescriptorInfo info;
                        info.name = [uniform.name UTF8String];
                        if( uniform.dataType == MTLDataTypeTexture)
                        {
                            if( uniform.textureReferenceType.access == MTLBindingAccessReadOnly)
                                info.ty = DescriptorType::SAMPLED_IMAGE;
                            else
                                info.ty = DescriptorType::STORAGE_IMAGE;
                            info.dimensionality.value = 1;
                            info.dimensionality.ty = DescriptorDimensionality::Single;
                        }
                        else if( uniform.dataType == MTLDataTypePointer)
                        {
                            if( uniform.pointerType.access == MTLBindingAccessReadOnly)
                                info.ty = DescriptorType::UNIFORM_BUFFER;
                            else
                                info.ty = DescriptorType::STORAGE_BUFFER;
                            info.dimensionality.value = 8000; ;//gpu_limits.maxPerStageDescriptorUpdateAfterBindStorageBuffers;
                            info.dimensionality.ty = DescriptorDimensionality::Single;
                        }
                        else if( uniform.dataType == MTLDataTypeArray)
                        {
                            info.dimensionality.value = uniform.arrayType.arrayLength;
                            info.dimensionality.ty = DescriptorDimensionality::Array;
                           
                            for(MTLStructMember* member in uniform.arrayType.elementStructType.members)
                            {
                                if( member.dataType == MTLDataTypePointer) //bindless
                                {
                                    info.dimensionality.ty = DescriptorDimensionality::RuntimeArray;
                                    if( member.pointerType.access == MTLBindingAccessReadOnly)
                                        info.ty = DescriptorType::UNIFORM_BUFFER;
                                    else
                                        info.ty = DescriptorType::STORAGE_BUFFER;
                                }
                                else if( member.dataType == MTLDataTypeTexture)
                                {
                                    if( member.textureReferenceType.access == MTLBindingAccessReadOnly)
                                        info.ty = DescriptorType::SAMPLED_IMAGE;
                                    else
                                        info.ty = DescriptorType::STORAGE_IMAGE;
                                }
                            }
                        }
                        else if( uniform.dataType == MTLDataTypeStruct)
                        {
                            info.dimensionality.ty = DescriptorDimensionality::Single;
                            for(MTLStructMember* member in uniform.structType.members)
                            {
                                if( member.dataType == MTLDataTypePointer)
                                {
                                    info.dimensionality.ty = DescriptorDimensionality::RuntimeArray;
                                    if( member.pointerType.access == MTLBindingAccessReadOnly)
                                        info.ty = DescriptorType::UNIFORM_BUFFER;
                                    else
                                        info.ty = DescriptorType::STORAGE_BUFFER;
                                }
                            }
                        }
                        descriptor_dindings[uniform.argumentIndex] = info;
                    }
                }
            }
            return descriptor_dindings;
        }

        auto get_descriptor_set_info(NSArray<MTLArgument *> *arguments)->StageDescriptorSetLayouts
        {
            StageDescriptorSetLayouts descriptor_sets;
            u32 set_id = 0;
            for (MTLArgument *arg in arguments)
            {
                descriptor_sets[set_id] = get_descriptor_info(arg, set_id);
                set_id++;
            }
            return descriptor_sets;
        }
    }
}
#endif