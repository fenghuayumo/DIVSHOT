#ifndef RT_LIGHT_COMMON_HLSL
#define RT_LIGHT_COMMON_HLSL

struct LightHit {
	float3 radiance;
	float pdf;
	float hitT;

	bool is_miss() { return hitT <= 0; }
	bool is_hit() { return hitT > 0; }
};

struct LightSample {
	float3 radiance;
	float pdf; //solidAnglePdf
	float3 position;
    float3 normal;
};

LightHit null_light_hit() {
    return (LightHit)0;
}

LightHit create_light_hit(float3 radiance, float pdf, float hitT) {
    LightHit result;
    result.radiance = radiance;
    result.pdf = pdf;
    result.hitT = hitT;
    return result;
}

LightSample null_light_sample() {
    return (LightSample)0;
}

LightSample create_light_sample(float3 radiance, float pdf, float3 position, float3 normal) {
    LightSample sample;
    sample.radiance = radiance;
    sample.pdf = pdf;
    sample.position = position;
    sample.normal = normal;
    return sample;
}


#endif