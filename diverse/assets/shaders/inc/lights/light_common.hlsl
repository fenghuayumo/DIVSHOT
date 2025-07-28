
#ifndef POLYMORPHIC_LIGHT_HLSLI
#define POLYMORPHIC_LIGHT_HLSLI


#include "../math.hlsl"
#include "../pack_unpack.hlsl"
#include "../color/srgb.hlsl"
#include "../monte_carlo.hlsl"
#include "light_shaping.hlsl"

#define LIGHT_SAMPING_EPSILON 1e-10
#define DISTANT_LIGHT_DISTANCE 10000.0

LightShaping unpackLightShaping(PolymorphicLightInfo lightInfo)
{
    LightShaping shaping;
    shaping.isSpot = (lightInfo.colorTypeAndFlags & kPolymorphicLightShapingEnableBit) != 0;
    shaping.primaryAxis = octToNdirUnorm32(lightInfo.primaryAxis);
    shaping.cosConeAngle = f16tof32(lightInfo.cosConeAngleAndSoftness);
    shaping.cosConeSoftness = f16tof32(lightInfo.cosConeAngleAndSoftness >> 16);
    shaping.iesProfileIndex = (lightInfo.colorTypeAndFlags & kPolymorphicLightIesProfileEnableBit) ? lightInfo.iesProfileIndex : -1;
    return shaping;
}

float evaluateIesProfile(int profileIndex, float3 emissionDirection_, float3 lightPrimaryAxis)
{
    if (profileIndex < 0)
        return 1.0;

    float3 xAxis;
    float3 yAxis;
    branchlessONB(lightPrimaryAxis, xAxis, yAxis);

    float3 emissionDirection;
    emissionDirection.x = dot(emissionDirection_, xAxis);
    emissionDirection.y = dot(emissionDirection_, yAxis);
    emissionDirection.z = dot(emissionDirection_, lightPrimaryAxis);
    emissionDirection = normalize(emissionDirection);

    const float angle = acos(emissionDirection.z);
    const float normAngle = angle / M_PI;

    const float tangentAngle = atan2(emissionDirection.y, emissionDirection.x);
    const float normTangentAngle = tangentAngle * .5f / M_PI + .5f;

    Texture2D<float4> iesProfileTexture = bindless_textures[NonUniformResourceIndex(profileIndex)];

    float iesMultiplier = iesProfileTexture.SampleLevel(IES_SAMPLER, float2(normAngle, normTangentAngle), 0).x;

    return iesMultiplier;
}

float3 evaluateLightShaping(LightShaping shaping, float3 surfacePosition, float3 lightSamplePosition)
{
    if (!shaping.isSpot)
        return 1.0;

    const float3 lightToSurface = normalize(surfacePosition - lightSamplePosition);

    const float cosTheta = dot(shaping.primaryAxis, lightToSurface);

    const float softSpotlight = smoothstep(shaping.cosConeAngle,
                                           shaping.cosConeAngle + shaping.cosConeSoftness, cosTheta);

    if (softSpotlight <= 0)
        return 0.0;

    const float iesMultiplier = evaluateIesProfile(shaping.iesProfileIndex,
                                                   lightToSurface, shaping.primaryAxis);

    return softSpotlight * iesMultiplier;
}

// Computes the conservative cone of influence for a spherical light source that has a shaping angle.
// The cone angle and axis are the same as the shaping angle and axis, and the cone vertex is the
// light center offset against the axis by the distance that is necessary to inscribe the sphere into the cone.
// Assumes nonzero cone angle.
float3 getConeVertexForSphericalSource(float3 sphereCenter, float sphereRadius, float3 coneAxis, float coneHalfAngle)
{
    // Compute the sine of the clamped half angle. When the angle is more than 90 degrees (half a pi),
    // the offset should be exactly one sphere radius.
    float sinHalfAngle = sin(min(coneHalfAngle, M_PI * 0.5));

    // Offset is the hypotenuse of a right triangle whose vertices are: the light center; the cone vertex;
    // and any point on the circle where the cone touches the sphere.
    float offset = sphereRadius / sinHalfAngle;

    // Compute the cone vertex assuming that the aforementioned hypotenuse is collinear with the cone axis.
    float3 coneVertex = sphereCenter - coneAxis * offset;

    return coneVertex;
}

// Tests whether a sphere intersects with a cone. Returns true if they do intersect.
bool testSphereConeIntersection(float3 coneVertex, float3 coneAxis, float coneHalfAngle, float3 sphereCenter, float sphereRadius)
{
    // The intersection is determined by comparing three angles in the plane that goes through the cone axis
    // and the sphere center. The geometry and solution should be clear from the variable names.

    float3 coneVertexToSphereCenter = sphereCenter - coneVertex;
    float distanceFromConeVertexToSphereCenter = length(coneVertexToSphereCenter);

    if (distanceFromConeVertexToSphereCenter <= sphereRadius)
        return true;

    float invDistanceFromConeVertexToSphereCenter = rcp(distanceFromConeVertexToSphereCenter);

    float angleBetweenConeAxisAndVertexToSphere = acos(dot(coneVertexToSphereCenter, coneAxis) * invDistanceFromConeVertexToSphereCenter);

    float halfAngleSubtendedBySphereAtVertex = asin(sphereRadius * invDistanceFromConeVertexToSphereCenter);

    bool intersection = angleBetweenConeAxisAndVertexToSphere <= halfAngleSubtendedBySphereAtVertex + coneHalfAngle;

    return intersection;
}

// Tests whether a sphere intersects with the cone of influence of a shaped spherical light.
// Returns true if they do intersect. Also returns true if the light is not shaped.
// The test is a bit conservative, i.e. some volume behind the light will be considered intersecting.
// That volume is coming from the conservative cone of influence, and it gets larger for wide lights
// with a small shaping angle because the cone goes further back to include the light sphere.
bool testSphereIntersectionForShapedLight(float3 lightCenter, float lightRadius, LightShaping shaping, float3 sphereCenter, float sphereRadius)
{
    if (!shaping.isSpot)
        return true;

    // Recover the angle from the cosine because the further algorithms operate with raw angles or sines.
    float coneHalfAngle = acos(shaping.cosConeAngle);

    // Compute the conservative cone of influence.
    float3 coneVertex = getConeVertexForSphericalSource(lightCenter, lightRadius, shaping.primaryAxis, coneHalfAngle);

    // Test the intersection of the given sphere with the cone of influence.
    return testSphereConeIntersection(coneVertex, shaping.primaryAxis, coneHalfAngle, sphereCenter, sphereRadius);
}

// Returns the approximate ratio of the flux of a shaped sphere light and onmidirectional sphere light.
float getShapingFluxFactor(LightShaping shaping)
{
    if (!shaping.isSpot)
        return 1.0;

    float solidAngleOverTwoPi = (1.0 - shaping.cosConeAngle);

    // TODO: this is pulled out of thin air, need a more grounded expression
    solidAngleOverTwoPi *= lerp(1.0, 0.5, shaping.cosConeSoftness);

    // TODO: account for IES profiles

    return solidAngleOverTwoPi * 0.5; // (solidAngle / (4 * M_PI))
}

struct PolymorphicLightSample
{
    float3 position;
    float3 normal;
    float3 radiance;
    float solidAnglePdf;
};

PolymorphicLightType getLightType(PolymorphicLightInfo lightInfo)
{
    uint typeCode = (lightInfo.colorTypeAndFlags >> kPolymorphicLightTypeShift) 
        & kPolymorphicLightTypeMask;

    return (PolymorphicLightType)typeCode;
}

float unpackLightRadiance(uint logRadiance)
{
    return (logRadiance == 0) ? 0 : exp2((float(logRadiance - 1) / 65534.0) * (kPolymorphicLightMaxLog2Radiance - kPolymorphicLightMinLog2Radiance) + kPolymorphicLightMinLog2Radiance);
}

float3 unpackLightColor(PolymorphicLightInfo lightInfo)
{
    float3 color = unpack_color_888(lightInfo.colorTypeAndFlags);
    float radiance = unpackLightRadiance(lightInfo.logRadiance & 0xffff);
    return color * radiance.xxx;
}

void packLightColor(float3 radiance, inout PolymorphicLightInfo lightInfo)
{   
    float intensity = max(radiance.r, max(radiance.g, radiance.b));

    if (intensity > 0.0)
    {
        float logRadiance = saturate((log2(intensity) - kPolymorphicLightMinLog2Radiance) 
            / (kPolymorphicLightMaxLog2Radiance - kPolymorphicLightMinLog2Radiance));
        uint packedRadiance = min(uint32_t(ceil(logRadiance * 65534.0)) + 1, 0xffffu);
        float unpackedRadiance = unpackLightRadiance(packedRadiance);

        float3 normalizedRadiance = saturate(radiance.rgb / unpackedRadiance.xxx);

        lightInfo.logRadiance |= packedRadiance;
        lightInfo.colorTypeAndFlags |= pack_color_888(normalizedRadiance);
    }
}

bool packCompactLightInfo(PolymorphicLightInfo lightInfo, out uint4 res1, out uint4 res2)
{
    if (unpackLightShaping(lightInfo).isSpot)
    {
        res1 = 0;
        res2 = 0;
        return false;
    }

    res1.xyz = asuint(lightInfo.center.xyz);
    res1.w = lightInfo.colorTypeAndFlags;

    res2.x = lightInfo.direction1;
    res2.y = lightInfo.direction2;
    res2.z = lightInfo.scalars;
    res2.w = lightInfo.logRadiance;
    return true;
}

PolymorphicLightInfo unpackCompactLightInfo(const uint4 data1, const uint4 data2)
{
    PolymorphicLightInfo lightInfo = (PolymorphicLightInfo)0;
    lightInfo.center.xyz = asfloat(data1.xyz);
    lightInfo.colorTypeAndFlags = data1.w;
    lightInfo.direction1 = data2.x;
    lightInfo.direction2 = data2.y;
    lightInfo.scalars = data2.z;
    lightInfo.logRadiance = data2.w;
    return lightInfo;
}

// Computes estimated distance between a given point in space and a random point inside
// a spherical volume. Since the geometry of this solution is spherically symmetric,
// only the distance from the volume center to the point and the volume radius matter here.
float getAverageDistanceToVolume(float distanceToCenter, float volumeRadius)
{
    // The expression and factor are fitted to a Monte Carlo estimated curve.
    // At distanceToCenter == 0, this function returns (0.75 * volumeRadius) which is analytically accurate.
    // At infinity, the result asymptotically approaches distanceToCenter.

    const float nonlinearFactor = 1.1547;

    return distanceToCenter + volumeRadius * square(volumeRadius) 
        / square(distanceToCenter + volumeRadius * nonlinearFactor);
}


// Note: Sphere lights always assume an interaction point is not going to be inside of the sphere, so special logic handling this case
// can be avoided in sampling logic (for PDF/radiance calculation), as well as individual PDF calculation and radiance evaluation.
struct SphereLight
{
    float3 position;
    float radius; // Note: Assumed to always be >0 to avoid point light special cases
    float3 radiance;
    LightShaping shaping;

    // Interface methods

    PolymorphicLightSample calcSample(in const float2 random, in const float3 viewerPosition)
    {
        const float3 lightVector = position - viewerPosition;
        const float lightDistance2 = dot(lightVector, lightVector);
        const float lightDistance = sqrt(lightDistance2);
        const float radius2 = square(radius);

        // Note: Sampling based on PBRT's solid angle sphere sampling, resulting in fewer rays occluded by the light itself,
        // ignoring special case for when viewing inside the light (which should just use normal spherical area sampling)
        // for performance. Similarly condition ignored in PDF calculation as well.

        // Compute theta and phi for cone sampling

        const float2 u = random;
        const float sinThetaMax2 = radius2 / lightDistance2;
        const float cosThetaMax = sqrt(max(0.0f, 1.0f - sinThetaMax2));
        const float phi = 2.0f * M_PI * u.x;
        const float cosTheta = lerp(cosThetaMax, 1.0f, u.y);
        const float sinTheta = sqrt(max(0.0f, 1.0f - square(cosTheta)));
        const float sinTheta2 = sinTheta * sinTheta;

        // Calculate the alpha value representing the spherical coordinates of the sample point

        const float dc = lightDistance;
        const float dc2 = lightDistance2;
        const float ds = dc * cosTheta - sqrt(max(LIGHT_SAMPING_EPSILON, radius2 - dc2 * sinTheta2));
        const float cosAlpha = (dc2 + radius2 - square(ds)) / (2.0f * dc * radius);
        const float sinAlpha = sqrt(max(0.0f, 1.0f - square(cosAlpha)));

        // Construct a coordinate frame to sample in around the direction of the light vector

        const float3 sampleSpaceNormal = normalize(lightVector);
        float3 sampleSpaceTangent;
        float3 sampleSpaceBitangent;
        branchlessONB(sampleSpaceNormal, sampleSpaceTangent, sampleSpaceBitangent);

        // Calculate sample position and normal on the sphere

        float sinPhi;
        float cosPhi;
        sincos(phi, sinPhi, cosPhi);

        const float3 radiusVector = spherical_direction(
            sinAlpha, cosAlpha, sinPhi, cosPhi, -sampleSpaceTangent, -sampleSpaceBitangent, -sampleSpaceNormal);
        const float3 spherePositionSample = position + radius * radiusVector;
        const float3 sphereNormalSample = normalize(radiusVector);
        // Note: Reprojection for position to minimize error here skipped for performance

        // Calculate the pdf

        // Note: The cone already represents a solid angle effectively so its pdf is already a solid angle pdf
        const float solidAnglePdf = 1.0f / (2.0f * M_PI * (1.0f - cosThetaMax));

        // Create the light sample

        PolymorphicLightSample lightSample;

        lightSample.position = spherePositionSample;
        lightSample.normal = sphereNormalSample;
        lightSample.radiance = radiance;
        lightSample.solidAnglePdf = solidAnglePdf;

        return lightSample;
    }

    float getSurfaceArea()
    {
        return 4 * M_PI * square(radius);
    }

    float getPower()
    {
        return getSurfaceArea() * M_PI * sRGB_to_luminance(radiance) * getShapingFluxFactor(shaping);
    }

    float getWeightForVolume(in const float3 volumeCenter, in const float volumeRadius)
    {
        if (!testSphereIntersectionForShapedLight(position, radius, shaping, volumeCenter, volumeRadius))
            return 0.0;

        float distance = length(volumeCenter - position);
        distance = getAverageDistanceToVolume(distance, volumeRadius);

        float sinHalfAngle = radius / distance;
        float solidAngle = 2 * M_PI * (1.0 - sqrt(1.0 - square(sinHalfAngle)));

        return solidAngle * sRGB_to_luminance(radiance);
    }

    static SphereLight Create(in const PolymorphicLightInfo lightInfo)
    {
        SphereLight sphereLight;

        sphereLight.position = lightInfo.center;
        sphereLight.radius = f16tof32(lightInfo.scalars);
        sphereLight.radiance = unpackLightColor(lightInfo);
        sphereLight.shaping = unpackLightShaping(lightInfo);

        return sphereLight;
    }
};

// Point light is a sphere light with zero radius.
// On the host side, they are both created from LightType_Point, depending on the radius.
// The values returned from all interface methods of PointLight are the same as SphereLight
// would produce in the limit when radius approaches zero, with some exceptions in calcSample.
struct PointLight
{
    float3 position;
    float3 flux;
    LightShaping shaping;

    // Interface methods

    PolymorphicLightSample calcSample(in const float3 viewerPosition)
    {
        const float3 lightVector = position - viewerPosition;
        
        PolymorphicLightSample lightSample;

        // We cannot compute finite values for radiance and solidAnglePdf for a point light,
        // so return the limit of (radiance / solidAnglePdf) with radius --> 0 as radiance.
        lightSample.position = position;
        lightSample.normal = normalize(-lightVector);
        lightSample.radiance = flux / dot(lightVector, lightVector);
        lightSample.solidAnglePdf = 1.0;

        return lightSample;
    }

    float getPower()
    {
        return 4.0 * M_PI * sRGB_to_luminance(flux) * getShapingFluxFactor(shaping);
    }

    float getWeightForVolume(in const float3 volumeCenter, in const float volumeRadius)
    {
        if (!testSphereIntersectionForShapedLight(position, 0, shaping, volumeCenter, volumeRadius))
            return 0.0;

        float distance = length(volumeCenter - position);
        distance = getAverageDistanceToVolume(distance, volumeRadius);

        return sRGB_to_luminance(flux) / square(distance);
    }

    static PointLight Create(in const PolymorphicLightInfo lightInfo)
    {
        PointLight pointLight;

        pointLight.position = lightInfo.center;
        pointLight.flux = unpackLightColor(lightInfo);
        pointLight.shaping = unpackLightShaping(lightInfo);

        return pointLight;
    }
};

struct CylinderLight
{
    float3 position;
    float radius; // Note: Assumed to always be >0 to avoid line light special cases
    float3 radiance;
    float axisLength; // Note: Assumed to always be >0 to avoid ring light special cases
    float3 tangent;

    // Interface methods

    PolymorphicLightSample calcSample(in const float2 random, in const float3 viewerPosition)
    {
        // Construct a coordinate frame around the tangent vector

        float3 normal;
        float3 bitangent;
        branchlessONB(tangent, normal, bitangent);

        // Compute phi and z

        const float2 u = random;
        const float phi = 2.0f * M_PI * u.x;

        float sinPhi;
        float cosPhi;
        sincos(phi, sinPhi, cosPhi);

        const float z = (u.y - 0.5f) * axisLength;

        // Calculate sample position and normal on the cylinder

        const float3 radiusVector = sinPhi * bitangent + cosPhi * normal;
        const float3 cylinderPositionSample = position + z * tangent + radius * radiusVector;
        const float3 cylinderNormalSample = normalize(radiusVector);
        // Note: Reprojection for position to minimize error here skipped for performance

        // Calculate pdf

        const float areaPdf = 1.0f / getSurfaceArea();
        const float3 sampleVector = cylinderPositionSample - viewerPosition;
        const float sampleDistance = length(sampleVector);
        const float sampleCosTheta = dot(normalize(sampleVector), -cylinderNormalSample);
        const float solidAnglePdf = pdfAtoW(areaPdf, sampleDistance, abs(sampleCosTheta));

        // Create the light sample

        PolymorphicLightSample lightSample;

        lightSample.position = cylinderPositionSample;
        lightSample.normal = cylinderNormalSample;

        if (sampleCosTheta <= 0.0f)
        {
            lightSample.radiance = float3(0.0f, 0.0f, 0.0f);
            lightSample.solidAnglePdf = 0.0f;
        }
        else
        {
            lightSample.radiance = radiance;
            lightSample.solidAnglePdf = solidAnglePdf;
        }

        return lightSample;
    }

    float getSurfaceArea()
    {
        return 2.0f * M_PI * radius * axisLength;
    }

    float getPower()
    {
        return getSurfaceArea() * M_PI * sRGB_to_luminance(radiance);
    }

    float getWeightForVolume(in const float3 volumeCenter, in const float volumeRadius)
    {
        float distance = length(volumeCenter - position);
        distance = getAverageDistanceToVolume(distance, volumeRadius);

        // Assume illumination by a quad that represents the cylinder when viewed from afar.
        float quadArea = 2.0 * radius * axisLength;
        float approximateSolidAngle = quadArea / square(distance);
        approximateSolidAngle = min(approximateSolidAngle, 2 * M_PI);

        return approximateSolidAngle * sRGB_to_luminance(radiance);
    }

    static CylinderLight Create(in const PolymorphicLightInfo lightInfo)
    {
        CylinderLight cylinderLight;

        cylinderLight.position = lightInfo.center;
        cylinderLight.radius = f16tof32(lightInfo.scalars);
        cylinderLight.radiance = unpackLightColor(lightInfo);
        cylinderLight.axisLength = f16tof32(lightInfo.scalars >> 16);
        cylinderLight.tangent = octToNdirUnorm32(lightInfo.direction1);

        return cylinderLight;
    }
};

struct DiskLight
{
    float3 position;
    float radius; // Note: Assumed to always be >0 to avoid point light special cases
    float3 radiance;
    float3 normal;

    // Interface methods

    PolymorphicLightSample calcSample(in const float2 random, in const float3 viewerPosition)
    {
        float3 tangent;
        float3 bitangent;
        branchlessONB(normal, tangent, bitangent);

        // Compute a raw disk sample

        const float2 rawDiskSample = uniform_sample_disk(random) * radius;

        // Calculate sample position and normal on the disk

        const float3 diskPositionSample = position + tangent * rawDiskSample.x + bitangent * rawDiskSample.y;
        const float3 diskNormalSample = normal;

        // Calculate pdf

        const float areaPdf = 1.0f / getSurfaceArea();
        const float3 sampleVector = diskPositionSample - viewerPosition;
        const float sampleDistance = length(sampleVector);
        const float sampleCosTheta = dot(normalize(sampleVector), -diskNormalSample);
        const float solidAnglePdf = pdfAtoW(areaPdf, sampleDistance, abs(sampleCosTheta));

        // Create the light sample

        PolymorphicLightSample lightSample;

        lightSample.position = diskPositionSample;
        lightSample.normal = diskNormalSample;

        if (sampleCosTheta <= 0.0f)
        {
            lightSample.radiance = float3(0.0f, 0.0f, 0.0f);
            lightSample.solidAnglePdf = 0.0f;
        }
        else
        {
            lightSample.radiance = radiance;
            lightSample.solidAnglePdf = solidAnglePdf;
        }

        return lightSample;
    }

    float getSurfaceArea()
    {
        return M_PI * square(radius);
    }

    float getPower()
    {
        return getSurfaceArea() * M_PI * sRGB_to_luminance(radiance);// * getShapingFluxFactor(shaping);
    }

    float getWeightForVolume(in const float3 volumeCenter, in const float volumeRadius)
    {
        float distanceToPlane = dot(volumeCenter - position, normal);
        if (distanceToPlane < -volumeRadius)
            return 0; // Cull - the entire volume is below the light's horizon

        float distance = length(volumeCenter - position);
        distance = getAverageDistanceToVolume(distance, volumeRadius);

        float approximateSolidAngle = getSurfaceArea() / square(distance);
        approximateSolidAngle = min(approximateSolidAngle, 2 * M_PI);

        return approximateSolidAngle * sRGB_to_luminance(radiance);
    }

    static DiskLight Create(in const PolymorphicLightInfo lightInfo)
    {
        DiskLight diskLight;

        diskLight.position = lightInfo.center;
        diskLight.radius = f16tof32(lightInfo.scalars);
        diskLight.normal = octToNdirUnorm32(lightInfo.direction1);
        diskLight.radiance = unpackLightColor(lightInfo);

        return diskLight;
    }
};

struct RectLight
{
    float3 position;
    float2 dimensions; // Note: Assumed to always be >0 to avoid point light special cases
    float3 dirx;
    float3 diry;
    float3 radiance;

    float3 normal;

    // Interface methods

    PolymorphicLightSample calcSample(in const float2 random, in const float3 viewerPosition)
    {
        // Compute x and y

        const float2 u = random;
        const float2 rawRectangleSample = float2((u.x - 0.5f) * dimensions.x, (u.y - 0.5f) * dimensions.y);

        // Calculate sample position on the rectangle

        const float3 rectanglePositionSample = position + dirx * rawRectangleSample.x + diry * rawRectangleSample.y;
        const float3 rectangleNormalSample = normal;

        // Calculate pdf

        const float areaPdf = 1.0f / getSurfaceArea();
        const float3 sampleVector = rectanglePositionSample - viewerPosition;
        const float sampleDistance = length(sampleVector);
        const float sampleCosTheta = dot(normalize(sampleVector), -rectangleNormalSample);
        const float solidAnglePdf = pdfAtoW(areaPdf, sampleDistance, abs(sampleCosTheta));

        // Create the light sample

        PolymorphicLightSample lightSample;

        lightSample.position = rectanglePositionSample;
        lightSample.normal = rectangleNormalSample;

        if (sampleCosTheta <= 0.0f)
        {
            lightSample.radiance = float3(0.0f, 0.0f, 0.0f);
            lightSample.solidAnglePdf = 0.0f;
        }
        else
        {
            lightSample.radiance = radiance;
            lightSample.solidAnglePdf = solidAnglePdf;
        }

        return lightSample;
    }

    float getSurfaceArea()
    {
        return dimensions.x * dimensions.y;
    }

    float getPower()
    {
        return getSurfaceArea() * M_PI * sRGB_to_luminance(radiance);
    }

    float getWeightForVolume(in const float3 volumeCenter, in const float volumeRadius)
    {
        float distanceToPlane = dot(volumeCenter - position, normal);
        if (distanceToPlane < -volumeRadius)
            return 0; // Cull - the entire volume is below the light's horizon

        float distance = length(volumeCenter - position);
        distance = getAverageDistanceToVolume(distance, volumeRadius);

        float approximateSolidAngle = getSurfaceArea() / square(distance);
        approximateSolidAngle = min(approximateSolidAngle, 2 * M_PI);

        return approximateSolidAngle * sRGB_to_luminance(radiance);
    }

    static RectLight Create(in const PolymorphicLightInfo lightInfo)
    {
        RectLight rectLight;

        rectLight.position = lightInfo.center;
        rectLight.dimensions.x = f16tof32(lightInfo.scalars);
        rectLight.dimensions.y = f16tof32(lightInfo.scalars >> 16);
        rectLight.dirx = octToNdirUnorm32(lightInfo.direction1);
        rectLight.diry = octToNdirUnorm32(lightInfo.direction2);
        rectLight.radiance = unpackLightColor(lightInfo);

        // Note: Precomputed to avoid recomputation when evaluating multiple quantities on the same light
        rectLight.normal = cross(rectLight.dirx, rectLight.diry);


        return rectLight;
    }
};

struct DirectionalLight
{
    float3 direction;
    float cosHalfAngle; // Note: Assumed to be != 1 to avoid delta light special case
    float sinHalfAngle;
    float solidAngle;
    float3 radiance;

    // Interface methods

    PolymorphicLightSample calcSample(in const float2 random, in const float3 viewerPosition)
    {
        const float2 diskSample = uniform_sample_disk(random);

        float3 tangent, bitangent;
        branchlessONB(direction, tangent, bitangent);

        const float3 distantDirectionSample = direction 
            + tangent * diskSample.x * sinHalfAngle
            + bitangent * diskSample.y * sinHalfAngle;

        // Calculate sample position on the distant light
        // Since there is no physical distant light to hit (as it is at infinity), this simply uses a large
        // number far enough away from anything in the world.

        const float3 distantPositionSample = viewerPosition - distantDirectionSample * DISTANT_LIGHT_DISTANCE;
        const float3 distantNormalSample = direction;

        // Create the light sample

        PolymorphicLightSample lightSample;

        lightSample.position = distantPositionSample;
        lightSample.normal = distantNormalSample;
        lightSample.radiance = radiance;
        lightSample.solidAnglePdf = 1.0 / solidAngle;
        
        return lightSample;
    }

    // Helper methods

    static DirectionalLight Create(in const PolymorphicLightInfo lightInfo)
    {
        DirectionalLight directionalLight;

        directionalLight.direction = octToNdirUnorm32(lightInfo.direction1);

        float halfAngle = f16tof32(lightInfo.scalars);
        sincos(halfAngle, directionalLight.sinHalfAngle, directionalLight.cosHalfAngle);
        directionalLight.solidAngle = f16tof32(lightInfo.scalars >> 16);
        directionalLight.radiance = unpackLightColor(lightInfo);

        return directionalLight;
    }
};

struct TriangleLight
{
    float3 base;
    float3 edge1;
    float3 edge2;
    float3 radiance;
    float3 normal;
    float surfaceArea;

    // Interface methods

    PolymorphicLightSample calcSample(in const float2 random, in const float3 viewerPosition)
    {
        PolymorphicLightSample result;

        float3 bary = uniform_sample_triangle(random);
        result.position = base + edge1 * bary.y + edge2 * bary.z;
        result.normal = normal;

        result.solidAnglePdf = calcSolidAnglePdf(viewerPosition, result.position, result.normal);

        result.radiance = radiance;

        return result;   
    }

    float calcSolidAnglePdf(in const float3 viewerPosition,
                            in const float3 lightSamplePosition,
                            in const float3 lightSampleNormal)
    {
        float3 L = lightSamplePosition - viewerPosition;
        float Ldist = length(L);
        L /= Ldist;

        const float areaPdf = 1.0 / surfaceArea;
        const float sampleCosTheta = saturate(dot(L, -lightSampleNormal));

        return pdfAtoW(areaPdf, Ldist, sampleCosTheta);
    }

    float getPower()
    {
        return surfaceArea * M_PI * sRGB_to_luminance(radiance);
    }

    float getWeightForVolume(in const float3 volumeCenter, in const float volumeRadius)
    {
        float distanceToPlane = dot(volumeCenter - base, normal);
        if (distanceToPlane < -volumeRadius)
            return 0; // Cull - the entire volume is below the light's horizon

        float3 barycenter = base + (edge1 + edge2) / 3.0;
        float distance = length(barycenter - volumeCenter);
        distance = getAverageDistanceToVolume(distance, volumeRadius);

        float approximateSolidAngle = surfaceArea / square(distance);
        approximateSolidAngle = min(approximateSolidAngle, 2 * M_PI);

        return approximateSolidAngle * sRGB_to_luminance(radiance);
    }

    // Helper methods

    static TriangleLight Create(in const PolymorphicLightInfo lightInfo)
    {
        TriangleLight triLight;

        triLight.edge1 = octToNdirUnorm32(lightInfo.direction1) * f16tof32(lightInfo.scalars);
        triLight.edge2 = octToNdirUnorm32(lightInfo.direction2) * f16tof32(lightInfo.scalars >> 16);
        triLight.base = lightInfo.center - (triLight.edge1 + triLight.edge2) / 3.0;
        triLight.radiance = unpackLightColor(lightInfo);

        float3 lightNormal = cross(triLight.edge1, triLight.edge2);
        float lightNormalLength = length(lightNormal);

        if(lightNormalLength > 0.0)
        {
            triLight.surfaceArea = 0.5 * lightNormalLength;
            triLight.normal = lightNormal / lightNormalLength;
        }
        else
        {
           triLight.surfaceArea = 0.0;
           triLight.normal = 0.0; 
        }

        return triLight;
    }

    PolymorphicLightInfo Store()
    {
        PolymorphicLightInfo lightInfo = (PolymorphicLightInfo)0;

        packLightColor(radiance, lightInfo);
        lightInfo.center = base + (edge1 + edge2) / 3.0;
        lightInfo.direction1 = ndirToOctUnorm32(normalize(edge1));
        lightInfo.direction2 = ndirToOctUnorm32(normalize(edge2));
        lightInfo.scalars = f32tof16(length(edge1)) | (f32tof16(length(edge2)) << 16);
        lightInfo.colorTypeAndFlags |= uint(PolymorphicLightType::kTriangle) << kPolymorphicLightTypeShift;
        
        return lightInfo;
    }
};

#ifndef ENVIRONMENT_SAMPLER
#define ENVIRONMENT_SAMPLER sampler_llc
#endif

struct EnvironmentLight
{
    int textureIndex;
    bool importanceSampled;
    float3 radianceScale;
    float rotation;
    uint2 textureSize;

    // Interface methods

    PolymorphicLightSample calcSample(in const float2 random, in const float3 viewerPosition)
    {
        PolymorphicLightSample lightSample;

        float2 textureUV;
        float3 sampleDirection;
        if (importanceSampled)
        {
            float2 directionUV = random;
            directionUV.x += rotation;

            float cosElevation;
            sampleDirection = equirect_uv_2_dir(directionUV, cosElevation);

            // Inverse of the solid angle of one texel of the environment map using the equirectangular projection.
            lightSample.solidAnglePdf = (textureSize.x * textureSize.y) / (2 * M_PI * M_PI * cosElevation);
            textureUV = random;
        }
        else
        {
            sampleDirection = sample_sphere(random, lightSample.solidAnglePdf);
            textureUV = dir_2_equirect_uv(sampleDirection);
            textureUV.x -= rotation;
        }

        float3 sampleRadiance = radianceScale;
        if (textureIndex >= 0)
        {
            Texture2D texture = bindless_textures[textureIndex];
            sampleRadiance *= texture.SampleLevel(ENVIRONMENT_SAMPLER, textureUV, 0).xyz;
        }

        // Inf / NaN guard.
        // Sometimes EXR files might contain those values (e.g. when saved by Photoshop).
        float radianceSum = sampleRadiance.r + sampleRadiance.g + sampleRadiance.b;
        if (isinf(radianceSum) || isnan(radianceSum))
            sampleRadiance = 0;

        lightSample.position = viewerPosition + sampleDirection * DISTANT_LIGHT_DISTANCE;
        lightSample.normal = -sampleDirection;
        lightSample.radiance = sampleRadiance;

        return lightSample;
    }

    // Helper methods

    static EnvironmentLight Create(in const PolymorphicLightInfo lightInfo)
    {
        EnvironmentLight envLight;

        envLight.textureIndex = int(lightInfo.direction1);
        envLight.rotation = f16tof32(lightInfo.scalars);
        envLight.importanceSampled = ((lightInfo.scalars >> 16) != 0);
        envLight.radianceScale = unpackLightColor(lightInfo);
        envLight.textureSize.x = lightInfo.direction2 & 0xffff;
        envLight.textureSize.y = lightInfo.direction2 >> 16;

        return envLight;
    }
};

struct PolymorphicLight
{
    static PolymorphicLightSample calcSample(
        in const PolymorphicLightInfo lightInfo, 
        in const float2 random, 
        in const float3 viewerPosition)
    {
        PolymorphicLightSample lightSample = (PolymorphicLightSample)0;

        switch (getLightType(lightInfo))
        {
        case PolymorphicLightType::kSphere:      lightSample = SphereLight::Create(lightInfo).calcSample(random, viewerPosition); break;
        case PolymorphicLightType::kPoint:       lightSample = PointLight::Create(lightInfo).calcSample(viewerPosition); break;
        case PolymorphicLightType::kCylinder:    lightSample = CylinderLight::Create(lightInfo).calcSample(random, viewerPosition); break;
        case PolymorphicLightType::kDisk:        lightSample = DiskLight::Create(lightInfo).calcSample(random, viewerPosition); break;
        case PolymorphicLightType::kRect:        lightSample = RectLight::Create(lightInfo).calcSample(random, viewerPosition); break;
        case PolymorphicLightType::kTriangle:    lightSample = TriangleLight::Create(lightInfo).calcSample(random, viewerPosition); break;
        case PolymorphicLightType::kDirectional: lightSample = DirectionalLight::Create(lightInfo).calcSample(random, viewerPosition); break;
        case PolymorphicLightType::kEnvironment: lightSample = EnvironmentLight::Create(lightInfo).calcSample(random, viewerPosition); break;
        }

        if (lightSample.solidAnglePdf > 0)
        {
            lightSample.radiance *= evaluateLightShaping(unpackLightShaping(lightInfo),
                viewerPosition, lightSample.position);
        }

        return lightSample;
    }

    static float getPower(
        in const PolymorphicLightInfo lightInfo)
    {
        switch (getLightType(lightInfo))
        {
        case PolymorphicLightType::kSphere:      return SphereLight::Create(lightInfo).getPower();
        case PolymorphicLightType::kPoint:       return PointLight::Create(lightInfo).getPower();
        case PolymorphicLightType::kCylinder:    return CylinderLight::Create(lightInfo).getPower();
        case PolymorphicLightType::kDisk:        return DiskLight::Create(lightInfo).getPower();
        case PolymorphicLightType::kRect:        return RectLight::Create(lightInfo).getPower();
        case PolymorphicLightType::kTriangle:    return TriangleLight::Create(lightInfo).getPower();
        case PolymorphicLightType::kDirectional: return 0; // infinite lights don't go into the local light PDF map
        case PolymorphicLightType::kEnvironment: return 0;
        default: return 0;
        }
    }

    static float getWeightForVolume(
        in const PolymorphicLightInfo lightInfo, 
        in const float3 volumeCenter,
        in const float volumeRadius)
    {
        switch (getLightType(lightInfo))
        {
        case PolymorphicLightType::kSphere:      return SphereLight::Create(lightInfo).getWeightForVolume(volumeCenter, volumeRadius);
        case PolymorphicLightType::kPoint:       return PointLight::Create(lightInfo).getWeightForVolume(volumeCenter, volumeRadius);
        case PolymorphicLightType::kCylinder:    return CylinderLight::Create(lightInfo).getWeightForVolume(volumeCenter, volumeRadius);
        case PolymorphicLightType::kDisk:        return DiskLight::Create(lightInfo).getWeightForVolume(volumeCenter, volumeRadius);
        case PolymorphicLightType::kRect:        return RectLight::Create(lightInfo).getWeightForVolume(volumeCenter, volumeRadius);
        case PolymorphicLightType::kTriangle:    return TriangleLight::Create(lightInfo).getWeightForVolume(volumeCenter, volumeRadius);
        case PolymorphicLightType::kDirectional: return 0; // infinite lights do not affect volume sampling
        case PolymorphicLightType::kEnvironment: return 0;
        default: return 0;
        }
    }
};

bool hasLightPosition(in Light light)
{
    PolymorphicLightType lightype = getLightType(light);
    bool infinite_light = lightype == PolymorphicLightType::kDirectional || PolymorphicLightType::kEnvironment == lightype;
    return !infinite_light;
}

bool isDeltaLight(in Light light)
{
    PolymorphicLightType lightype = getLightType(light);
    return lightype == PolymorphicLightType::kDirectional || lightype == PolymorphicLightType::kPoint 
        || lightype == PolymorphicLightType::kEnvironment;
}
#endif // POLYMORPHIC_LIGHT_HLSLI