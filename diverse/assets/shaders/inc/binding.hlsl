#ifndef DS_BINDING_HLSL
#define DS_BINDING_HLSL

#define DS_DESCRIPTOR_SET_BINDLESS 1
#define DS_DESCRIPTOR_SET_FRAME 2
#define DS_DESCRIPTOR_SET_ACCELERATION 3

#if defined(DS_SHADER_BACKEND_DXR) || defined(DS_SHADER_BACKEND_METAL)
#define DS_BINDING(slot)
#define DS_BINDING_SET(slot, descriptor_set)
#define DS_LOCATION(loc)
#else
#define DS_BINDING(slot) [[vk::binding(slot)]]
#define DS_BINDING_SET(slot, descriptor_set) [[vk::binding(slot, descriptor_set)]]
#define DS_LOCATION(loc) [[vk::location(loc)]]
#endif

#define DS_RESOURCE(slot) DS_BINDING(slot)
#define DS_RESOURCE_SET(slot, descriptor_set) DS_BINDING_SET(slot, descriptor_set)
#define DS_CBUFFER(slot) DS_BINDING(slot)
#define DS_CBUFFER_SET(slot, descriptor_set) DS_BINDING_SET(slot, descriptor_set)
#define DS_BINDLESS_RESOURCE(slot) DS_BINDING_SET(slot, DS_DESCRIPTOR_SET_BINDLESS)
#define DS_FRAME_RESOURCE(slot) DS_BINDING_SET(slot, DS_DESCRIPTOR_SET_FRAME)
#define DS_RT_ACCELERATION(slot, descriptor_set) DS_BINDING_SET(slot, descriptor_set)

#endif
