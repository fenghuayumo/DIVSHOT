#define NORMAL_STATE    0
#define SELECT_STATE    1
#define HIDE_STATE      2
#define DELETE_STATE    4

#define RESET_OP 0
#define DELETE_OP 1
#define UNDO_OP 2
#define REDO_OP 3

#define ALL_OP 1
#define INVERT_OP 2

// #define SA_GS 1
struct Gaussian
{
    float4 position;         // Gaussian position
    float4 rotation_scale;   // rotation, scale, and opacity
};

struct PackedVertexSH
{
    uint4 sh1to3;
    uint4 sh4to7;
    uint4 sh8to11;
    uint4 sh12to15;
};

struct GaussianAuxi
{
    int2 min_tile;           // minimum tile
    int2 max_tile;           // maximum tile
    float4 covariance_depth; // covariance and depth
    float4 position_color;   // position and color
    float2 lambda;
    float2 evector; // half float to store 2 evector
};

struct GaussianConstants
{
    int tiles_width;
    int tiles_height;
    int surface_width;
    int surface_height;
    uint num_gaussians;
    uint max_gaussians;
    uint num_tiles;
    uint padding;

    float4 gs_translation;
    float4 gs_scaling;
    float4 gs_rotation;
};

// #define GS_CONSTANT_BUF cbuffer _ { \
//     int tiles_width;                \
//     int tiles_height;               \
//     int surface_width;              \
//     int surface_height;             \
//     uint num_gaussians;             \
//     uint max_gaussians;             \
//     uint num_tiles;                 \
//     uint padding;                   \

//     float4 gs_translation;          \
//     float4 gs_scaling;              \
//     float4 gs_rotation;             \
// };                                  


float2 pack_half4(float4 v) {

    uint2 u = uint2(f32tof16(v.x) | (f32tof16(v.y) << 16), f32tof16(v.z) | (f32tof16(v.w) << 16));
    return asfloat(u);
}

float4 unpack_half4(float2 v) {
    uint2 u = asuint(v);
    float x = f16tof32(u.x);
    float y = f16tof32(u.x >> 16);
    float z = f16tof32(u.y);
    float w = f16tof32(u.y >> 16);
    return float4(x, y, z, w);
}

uint setOpState(uint value, uint op_state) {
    return (value & 0xFFFFFF00) | (op_state & 0x000000FF);
}
// 获取 op_state (8)
uint getOpState(uint value) {
    return value & 0x000000FF;
}

//get op_flag
uint getOpFlag(uint value) {
    return (value >> 8) & 0xFF;
}
//set op_flag 
uint setOpFlag(uint value, uint op_flag) {
    return (value & 0xFFFF00FF) | ((op_flag << 8) & 0x0000FF00);
}
//high 16 bit is tranform index
uint getTransformIndex(uint value) {
    return (value >> 16) & 0xFFFF;
}
//set transform index
uint setTransformIndex(uint value, uint index) {
    return (value & 0x0000FFFF) | ((index << 16) & 0xFFFF0000);
}

uint FloatToSortableUint(float f)
{
    uint fu = asuint(f);
    uint mask = -((int)(fu >> 31)) | 0x80000000;
    return fu ^ mask;
}

float relu(float x)
{
    return max(0.0, x);
}

float sigmoid(float x) {
    return 1.0 / (1.0 + exp(-x));
}

float3 sigmoid(float3 x) {
    return 1.0.xxx / (1.0.xxx + exp(-x));
}

#if SPLAT_EDIT
float4x4 get_transform(in ByteAddressBuffer splat_transform,uint transform_index)
{
    float4 transform0 = splat_transform.Load<float4>(transform_index * sizeof(float4) * 3);
    float4 transform1 = splat_transform.Load<float4>(transform_index * sizeof(float4) * 3 + sizeof(float4));
    float4 transform2 = splat_transform.Load<float4>(transform_index * sizeof(float4) * 3 + sizeof(float4) * 2);
    return float4x4(transform0.x, transform1.x,transform2.x, 0,
                    transform0.y, transform1.y,transform2.y, 0,
                    transform0.z, transform1.z,transform2.z, 0,
                    transform0.w, transform1.w,transform2.w, 1);
}
#endif

#define SPLAT_POS_INDEX 0
#define SPLAT_COLOR_INDEX 1
#define SPLAT_SH_DATA_INDEX 2
#define SPLAT_TRANSFORM_INDEX 3
// #define SPLAT_SH_4to7_INDEX 4
// #define SPLAT_SH_8to11_INDEX 5
// #define SPLAT_SH_12to15_INDEX 6

#define bindless_gaussians(buf_id) bindless_gaussians_buf[buf_id * 4 + SPLAT_POS_INDEX]
#define bindless_gaussians_color(buf_id) bindless_gaussians_buf[buf_id * 4 + SPLAT_COLOR_INDEX]
#define bindless_splat_sh_data(buf_id) bindless_gaussians_buf[buf_id * 4 + SPLAT_SH_DATA_INDEX]
#define bindless_splat_transform(buf_id) bindless_gaussians_buf[buf_id * 4 + SPLAT_TRANSFORM_INDEX]

#define GROUP_SIZE 64
#define GROUP_WIDTH 16
#define GROUP_HEIGHT 16


