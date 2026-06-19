#ifndef MATERIALS_BSDF_TYPES_HLSL
#define MATERIALS_BSDF_TYPES_HLSL

#include "../inc/math.hlsl"

/// BSDF lobe types for multi-lobe material system
/// Compatible with RTXPT's LobeType system
enum LobeType : uint
{
    LOBE_NONE = 0,
    LOBE_DIFFUSE_REFLECTION = 1 << 0,
    LOBE_SPECULAR_REFLECTION = 1 << 1,
    LOBE_SPECULAR_TRANSMISSION = 1 << 2,
    LOBE_DIFFUSE_TRANSMISSION = 1 << 3,
    LOBE_FUZZ = 1 << 4,
    LOBE_SHEEN = 1 << 5,
    LOBE_COAT = 1 << 6,
    LOBE_SUBSURFACE = 1 << 7,
};

/// BSDF evaluation context
struct BsdfEvalData
{
    float3 wo;              // Outgoing direction (point to viewer)
    float3 wi;             // Incoming direction (point to light)
    float3 shading_normal; // Shading normal at surface
    float3 geometry_normal;// Geometric normal at surface
    float3 tangent;         // Tangent vector for anisotropy
    float3 bitangent;       // Bitangent vector for anisotropy

    // Precomputed values
    float ndotv;           // cos(wo, normal)
    float ndotl;           // cos(wi, normal)
    float3 half_vector;     // Half vector (wo + wi) normalized

    static BsdfEvalData create(
        float3 wo_,
        float3 wi_,
        float3 sn,
        float3 gn,
        float3 tan = float3(1, 0, 0),
        float3 bitan = float3(0, 0, 1))
    {
        BsdfEvalData data;
        data.wo = wo_;
        data.wi = wi_;
        data.shading_normal = sn;
        data.geometry_normal = gn;
        data.tangent = tan;
        data.bitangent = bitan;

        data.ndotv = saturate(dot(wo_, sn));
        data.ndotl = saturate(dot(wi_, sn));
        data.half_vector = normalize(wo_ + wi_);

        return data;
    }
};

/// Single BSDF lobe evaluation result
struct BsdfLobeResult
{
    float3 value;     // BRDF value
    float pdf;        // PDF with respect to solid angle
    float weight;     // Lobe weight for MIS

    static BsdfLobeResult invalid()
    {
        BsdfLobeResult r;
        r.value = 0.0;
        r.pdf = 0.0;
        r.weight = 0.0;
        return r;
    }
};

/// Complete BSDF evaluation result (all lobes)
struct BsdfEvalResult
{
    float3 value;           // Combined BRDF value
    float pdf;              // Combined PDF
    float3 diffuse_value;   // Diffuse lobe contribution
    float3 specular_value;  // Specular lobe contribution
    float3 transmission_value; // Transmission lobe contribution
    float3 fuzz_value;      // Fuzz/sheen contribution

    static BsdfEvalResult invalid()
    {
        BsdfEvalResult r;
        r.value = 0.0;
        r.pdf = 0.0;
        r.diffuse_value = 0.0;
        r.specular_value = 0.0;
        r.transmission_value = 0.0;
        r.fuzz_value = 0.0;
        return r;
    }

    /// Add contribution from a single lobe
    void add_lobe(BsdfLobeResult lobe, float lobe_weight, float lobe_select_pdf)
    {
        float w = lobe_weight * lobe_select_pdf;
        value += lobe.value * w;
        pdf += lobe.pdf * lobe_select_pdf;
    }
};

/// BSDF sampling result
struct BsdfSampleResult
{
    float3 wi;             // Sampled incoming direction
    float3 value;          // BRDF value / PDF (for throughput)
    float pdf;             // PDF with respect to solid angle
    float3 lobe_value;     // Raw BRDF value (before division by PDF)
    float lobe_pdf;        // PDF of selected lobe
    float approx_roughness; // Approximate roughness for filtering
    LobeType selected_lobe; // Which lobe was sampled

    static BsdfSampleResult invalid()
    {
        BsdfSampleResult r;
        r.wi = float3(0, 0, -1);
        r.value = 0.0;
        r.pdf = 0.0;
        r.lobe_value = 0.0;
        r.lobe_pdf = 0.0;
        r.approx_roughness = 0.0;
        r.selected_lobe = LOBE_NONE;
        return r;
    }

    bool is_valid()
    {
        return pdf > 0.0 && selected_lobe != LOBE_NONE;
    }
};

#endif // MATERIALS_BSDF_TYPES_HLSL
