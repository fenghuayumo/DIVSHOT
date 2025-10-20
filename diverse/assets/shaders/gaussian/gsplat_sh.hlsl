// float3 unpack111011s(uint bits) {
//     return float3((uint3(bits,bits,bits) >> uint3(21u, 11u, 0u)) & uint3(0x7ffu, 0x3ffu, 0x7ffu)) / float3(2047.0, 1023.0, 2047.0) * 2.0 - 1.0;
// }

// fetch quantized spherical harmonic coefficients
void fetch_sh_scale(in uint4 t, out float scale, out float3 a, out float3 b, out float3 c) {
    scale = asfloat(t.x);
    a = unpack_normal_11_10_11_uint_no_normalize(t.y);
    b = unpack_normal_11_10_11_uint_no_normalize(t.z);
    c = unpack_normal_11_10_11_uint_no_normalize(t.w); 
}

// fetch quantized spherical harmonic coefficients
void fetch_sh(in uint4 t, out float3 a, out float3 b, out float3 c, out float3 d) {
    a = unpack_normal_11_10_11_uint_no_normalize(t.x);
    b = unpack_normal_11_10_11_uint_no_normalize(t.y);
    c = unpack_normal_11_10_11_uint_no_normalize(t.z);
    d = unpack_normal_11_10_11_uint_no_normalize(t.w);
}

void fetch_sh_0(in uint t, out float3 a) {
    a = unpack_normal_11_10_11_uint_no_normalize(t);
}

float3 read_splat_color(uint buf_id, uint index) {
    uint2 sh0 = bindless_gaussians_color(buf_id).Load<uint2>(index * sizeof(uint2));
    float4 color = unpack_uint2(sh0);
    return color.xyz;
}

#if SH_DEGREE == 1
#define SH_COEFFS 3
#elif SH_DEGREE == 2
#define SH_COEFFS 8
#elif SH_DEGREE == 3
#define SH_COEFFS 15
#else
#define SH_COEFFS 0
#endif

#if SH_DEGREE > 0
static const float SH_C1 = 0.4886025119029199f;
#endif

#if SH_DEGREE > 1
static const float SH_C2_0 = 1.0925484305920792f;
static const float SH_C2_1 = -1.0925484305920792f;
static const float SH_C2_2 = 0.31539156525252005f;
static const float SH_C2_3 = -1.0925484305920792f;
static const float SH_C2_4 = 0.5462742152960396f;
#endif

#if SH_DEGREE > 2
static const float SH_C3_0 = -0.5900435899266435f;
static const float SH_C3_1 = 2.890611442640554f;
static const float SH_C3_2 = -0.4570457994644658f;
static const float SH_C3_3 = 0.3731763325901154f;
static const float SH_C3_4 = -0.4570457994644658f;
static const float SH_C3_5 = 1.445305721320277f;
static const float SH_C3_6 = -0.5900435899266435f;
#endif

#if SH_DEGREE > 0
// see https://github.com/graphdeco-inria/gaussian-splatting/blob/main/utils/sh_utils.py
float3 evalSH(in float3 sh[15], in float3 dir) {
    float x = dir.x;
    float y = dir.y;
    float z = dir.z;

    // 1st degree
    float3 result = SH_C1 * (-sh[0] * y + sh[1] * z - sh[2] * x);

#if SH_DEGREE > 1
    // 2nd degree
    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float yz = y * z;
    float xz = x * z;

    result +=
        sh[3] * (SH_C2_0 * xy) +
        sh[4] * (SH_C2_1 * yz) +
        sh[5] * (SH_C2_2 * (2.0 * zz - xx - yy)) +
        sh[6] * (SH_C2_3 * xz) +
        sh[7] * (SH_C2_4 * (xx - yy));
#endif

#if SH_DEGREE > 2
    // 3rd degree
    result +=
        sh[8] * (SH_C3_0 * y * (3.0 * xx - yy)) +
        sh[9] * (SH_C3_1 * xy * z) +
        sh[10] * (SH_C3_2 * y * (4.0 * zz - xx - yy)) +
        sh[11] * (SH_C3_3 * z * (2.0 * zz - 3.0 * xx - 3.0 * yy)) +
        sh[12] * (SH_C3_4 * x * (4.0 * zz - xx - yy)) +
        sh[13] * (SH_C3_5 * z * (xx - yy)) +
        sh[14] * (SH_C3_6 * x * (xx - 3.0 * yy));
#endif

    return result;
}
#endif

float3 read_splat_sh_color(uint buf_id, uint index, float3 direction) {
    float scale = 1;
    float3 color = 0;
#if SH_DEGREE > 0
    float3 sh[15];
    PackedVertexSH sh_data = bindless_splat_sh_data(buf_id).Load<PackedVertexSH>(index * sizeof(PackedVertexSH));
    fetch_sh_scale(sh_data.sh1to3, scale, sh[0], sh[1], sh[2]);
#endif
#if SH_DEGREE > 1
    fetch_sh(sh_data.sh4to7, sh[3], sh[4], sh[5], sh[6]);
    fetch_sh(sh_data.sh8to11, sh[7], sh[8], sh[9], sh[10]);
#endif
#if SH_DEGREE > 2
    fetch_sh(sh_data.sh12to15, sh[11], sh[12], sh[13], sh[14]);
#endif
#if SH_DEGREE > 0
    color = evalSH(sh, direction);
#endif
    color = max(color, 0);
    return color * scale;
}