#ifdef DS_RENDER_API_METAL
//#include <metal_ir_convert/Foundation/Foundation.hpp>
//#include <metal_ir_convert/Metal/Metal.hpp>

//#import <Foundation/Foundation.h>
//#import <Metal/Metal.h>
//#define IR_RUNTIME_METALCPP       // enable metal-cpp compatibility mode
//#define IR_PRIVATE_IMPLEMENTATION // define only once in an implementation file
//#include <metal_ir_convert/metal_irconverter_runtime.h>
#include "core/base_type.h"
#include <vector>
#include <metal_irconverter/metal_irconverter.h>
#include <spirv_msl.hpp>
#include "backend/drs_rhi/gpu_pipeline.h"
#include "core/ds_log.h"
namespace diverse
{
        auto convert_dxil_metal_ir(const std::vector<u8>& dx_ir,const std::string& entry,rhi::ShaderPipelineStage shader_stage)->std::vector<u8>
        {
            std::vector<u8> metallib;
            IRCompiler* pCompiler = IRCompilerCreate();
            IRCompilerSetEntryPointName(pCompiler, entry.c_str());

            IRObject* pDXIL = IRObjectCreateFromDXIL(dx_ir.data(), dx_ir.size(), IRBytecodeOwnershipNone);

           // Compile DXIL to Metal IR:
           IRError* pError = nullptr;
           IRObject* pOutIR = IRCompilerAllocCompileAndLink(pCompiler, NULL,  pDXIL, &pError);

           if (!pOutIR)
           {
             // Inspect pError to determine cause.
             IRErrorDestroy( pError );
               std::string error_str;
               switch (IRErrorGetCode(pError)) 
               {
                   case IRErrorCodeShaderRequiresRootSignature:
                       error_str = "IRErrorCodeShaderRequiresRootSignature";
                       break;
                   case IRErrorCodeUnrecognizedRootSignatureDescriptor:
                       error_str = "IRErrorCodeShaderRequiresRootSignature";
                       break;
                   case IRErrorCodeUnrecognizedParameterTypeInRootSignature:
                       error_str = "IRErrorCodeUnrecognizedParameterTypeInRootSignature";
                       break;
                   case IRErrorCodeResourceNotReferencedByRootSignature:
                       error_str = "IRErrorCodeResourceNotReferencedByRootSignature";
                       break;
                case IRErrorCodeShaderIncompatibleWithDualSourceBlending:
                    error_str = "IRErrorCodeShaderIncompatibleWithDualSourceBlending";
                    break;
                case IRErrorCodeUnsupportedWaveSize:
                    error_str = "IRErrorCodeUnsupportedWaveSize";
                    break;
                case IRErrorCodeUnsupportedInstruction:
                    error_str = "IRErrorCodeUnsupportedInstruction";
                    break;
                case IRErrorCodeCompilationError:
                    error_str = "IRErrorCodeCompilationError";
                    break;
                case IRErrorCodeFailedToSynthesizeStageInFunction:
                    error_str = "IRErrorCodeFailedToSynthesizeStageInFunction";
                    break;
                case IRErrorCodeFailedToSynthesizeStreamOutFunction:    
                    error_str = "IRErrorCodeFailedToSynthesizeStreamOutFunction";
                    break;
                case IRErrorCodeFailedToSynthesizeIndirectIntersectionFunction:
                    error_str = "IRErrorCodeFailedToSynthesizeIndirectIntersectionFunction";
                    break;
                case IRErrorCodeUnableToVerifyModule:
                    error_str = "IRErrorCodeUnableToVerifyModule";
                    break;
                case IRErrorCodeUnableToLinkModule:
                    error_str = "IRErrorCodeUnableToLinkModule";
                    break;
                case IRErrorCodeUnrecognizedDXILHeader:
                    error_str = "IRErrorCodeUnrecognizedDXILHeader";
                    break;
                case IRErrorCodeInvalidRaytracingAttribute:
                    error_str = "IRErrorCodeInvalidRaytracingAttribute";
                    break;
                case IRErrorCodeUnknown:
                    error_str = "IRErrorCodeUnknown";
                    break;
                default:
                    error_str = "IRErrorCodeUnknown";
                    break;
               }
             DS_LOG_ERROR("convert metal ir error code {}", error_str);
             
             return metallib;
           }
           IRMetalLibBinary* pMetallib = IRMetalLibBinaryCreate();
            
           IRShaderStage stage = IRShaderStage::IRShaderStageCompute;
            switch (shader_stage) {
                case rhi::ShaderPipelineStage::Vertex:
                    stage = IRShaderStage::IRShaderStageVertex;
                    break;
                case rhi::ShaderPipelineStage::Pixel:
                    stage = IRShaderStage::IRShaderStageFragment;
                    break;
                case rhi::ShaderPipelineStage::Compute:
                    stage = IRShaderStage::IRShaderStageCompute;
                    break;
                default:
                    break;
            }
           IRObjectGetMetalLibBinary(pOutIR, stage, pMetallib);
           size_t metallibSize = IRMetalLibGetBytecodeSize(pMetallib);
           metallib.resize(metallibSize);
           IRMetalLibGetBytecode(pMetallib, metallib.data());
          IRShaderReflection* pReflection = IRShaderReflectionCreate();
          IRObjectGetReflection( pOutIR, IRShaderStageVertex, pReflection );
          auto cnt = IRShaderReflectionGetResourceCount(pReflection);
          IRShaderReflectionDestroy( pReflection );
           // Store the metallib to custom format or disk, or use to create a MTLLibrary.
           IRMetalLibBinaryDestroy(pMetallib);
           IRObjectDestroy(pDXIL);
           IRObjectDestroy(pOutIR);
           IRCompilerDestroy(pCompiler);
            
            return metallib;
        }

        auto convert_spirv_metal_ir(const std::vector<u8>& spirv,const std::string& entry,rhi::ShaderPipelineStage shader_stage)->std::vector<u8>
        {
            std::vector<u8> metallib;
            //todo:
            return metallib;
        }

        auto convert_spirv_2_msl(const std::vector<u8>& spv,bool use_argument=false)->std::string
        {
            std::string msl_code;
//            // Derive the context under which conversion will occur
            using namespace SPIRV_CROSS_NAMESPACE;
            CompilerMSL::Options mslOptions;
#ifdef DS_PLATFORM_MACOS
            mslOptions.platform = spirv_cross::CompilerMSL::Options::Platform::macOS;
#endif
            mslOptions.set_msl_version(3, 0, 0);
//            mslOptions.options.shouldFlipVertexY = true;
            mslOptions.argument_buffers = use_argument;
//            mslOptions.force_active_argument_buffer_resources = true;
//            mslOptions.pad_argument_buffer_resources = false;
            mslOptions.enable_decoration_binding = true;
            mslOptions.argument_buffers_tier = CompilerMSL::Options::ArgumentBuffersTier::Tier2;
            
            CompilerMSL compiler((u32*)spv.data(), spv.size() / 4);
            CompilerGLSL::Options commonOpts;
            commonOpts.vertex.flip_vert_y = true;
            compiler.set_common_options(commonOpts);
            compiler.set_msl_options(mslOptions);
            compiler.set_argument_buffer_device_address_space(1, true);
            compiler.set_argument_buffer_device_address_space(2, true);
            msl_code = compiler.compile();
            DS_LOG_INFO("MSLBegin:\n {} \nMSLEND", msl_code);
            return msl_code;
        }

//        auto get_ir_reflection_info()
//        {
//            // Reflection the entry point's name:
//            IRShaderReflection* pReflection = IRShaderReflectionCreate();
////            IRObjectGetReflection( pOutIR, IRShaderStageVertex, pReflection );
//            const char* str = IRShaderReflectionGetEntryPointFunctionName( pReflection );
//            IRShaderReflectionGetResourceCount(pReflection);
//            IRShaderReflectionDestroy( pReflection );
//        }
}

#endif
