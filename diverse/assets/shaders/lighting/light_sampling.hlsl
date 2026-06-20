#ifndef LIGHTING_LIGHT_SAMPLING_HLSL
#define LIGHTING_LIGHT_SAMPLING_HLSL

#include "light_types.hlsl"

bool light_type_sampleable_by_bsdf(PolymorphicLightType light_type)
{
    return light_type == PolymorphicLightType::kTriangle
        || light_type == PolymorphicLightType::kSphere
        || light_type == PolymorphicLightType::kCylinder
        || light_type == PolymorphicLightType::kDisk
        || light_type == PolymorphicLightType::kRect;
}

bool light_type_handled_by_scene_nee(PolymorphicLightType light_type)
{
    return light_type != PolymorphicLightType::kDirectional
        && light_type != PolymorphicLightType::kEnvironment;
}

LightSample sample_scene_light(uint light_index, float3 position, float2 urand, float selection_pdf)
{
    LightSample ret = LightSample::invalid();
    PolymorphicLightInfo light_info = scene_lights_dyn[light_index];
    PolymorphicLightType light_type = getLightType(light_info);

    if (!light_type_handled_by_scene_nee(light_type))
        return ret;

    PolymorphicLightSample polymorphic_sample = PolymorphicLight::calcSample(light_info, urand, position);
    if (polymorphic_sample.solidAnglePdf <= 0.0 || !any(polymorphic_sample.radiance > 0.0))
        return ret;

    float3 to_light = polymorphic_sample.position - position;
    float distance_to_light = length(to_light);
    if (distance_to_light <= 1e-4)
        return ret;

    ret.Li = polymorphic_sample.radiance;
    ret.direction = to_light / distance_to_light;
    ret.distance = distance_to_light;
    ret.light_index = light_index;
    ret.selection_pdf = selection_pdf;
    ret.solid_angle_pdf = polymorphic_sample.solidAnglePdf;
    ret.light_sampleable_by_bsdf = light_type_sampleable_by_bsdf(light_type);
    return ret;
}

LightSample sample_environment_light_candidate(float3 normal, float2 urand, float selection_pdf)
{
    LightSample ret = LightSample::invalid();
    EnvironmentLightSample environment_sample = sample_environment_light_nee(
        normal,
        urand,
        environment_nee_sample_mip_level());
    if (!environment_sample.valid())
        return ret;

    ret.Li = environment_sample.Li;
    ret.direction = environment_sample.direction;
    ret.distance = FLT_MAX;
    ret.light_index = 0xffffffff;
    ret.selection_pdf = selection_pdf;
    ret.solid_angle_pdf = environment_sample.pdf;
    ret.light_sampleable_by_bsdf = true;
    return ret;
}

#endif // LIGHTING_LIGHT_SAMPLING_HLSL
