#ifndef LIGHT_UNIFORM_SAMPLER_HLSL
#define LIGHT_UNIFORM_SAMPLER_HLSL
#include "../random.hlsl"

struct LightSamplerUniform
{
    Random randomNG;


    float2 rand2()
    {
        // Generate a random float2 using the random number generator
        return randomNG.rand2();
    }
    /**
     * Get a sample light.
     * @param position Current position on surface.
     * @param normal   Shading normal vector at current position.
     * @param lightPDF (Out) The PDF for the calculated sample (is equal to zero if no valid samples could be found).
     * @return The index of the new light sample
     */
    uint sampleLights(float3 position, float3 normal, out float lightPDF)
    {
        uint totalLights = frame_constants.scene_lights_count;

        // Return invalid sample if there are no lights
        if (totalLights == 0)
        {
            lightPDF = 0.0f;
            return 0;
        }

        // Choose a light to sample from
        lightPDF = 1.0f / totalLights;
        uint lightIndex = randomNG.randInt(totalLights);
        return lightIndex;
    }

    /**
     * Calculate the PDF of sampling a given light.
     * @param lightID  The index of the given light.
     * @param position The position on the surface currently being shaded.
     * @param normal   Shading normal vector at current position.
     * @return The calculated PDF with respect to the light.
     */
    float sampleLightPDF(uint lightID, float3 position, float3 normal)
    {
        return 1.0f / frame_constants.scene_lights_count;
    }
};

LightSamplerUniform make_light_sampler(Random random)
{
    LightSamplerUniform ret;
    ret.randomNG = random;
    return ret;
}

typedef LightSamplerUniform LightSampler;
#endif