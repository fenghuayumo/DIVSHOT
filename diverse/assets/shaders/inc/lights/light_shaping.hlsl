
#ifndef LIGHT_SHAPING_HLSLI
#define LIGHT_SHAPING_HLSLI
// #include "../inc/math.hlsl"

static const uint kSecondaryGBuffer_IsSpecularRay = 1;
static const uint kSecondaryGBuffer_IsDeltaSurface = 2;
static const uint kSecondaryGBuffer_IsEnvironmentMap = 4;

static const uint kPolymorphicLightTypeShift = 24;
static const uint kPolymorphicLightTypeMask = 0xf;
static const uint kPolymorphicLightShapingEnableBit = 1 << 28;
static const uint kPolymorphicLightIesProfileEnableBit = 1 << 29;
static const float kPolymorphicLightMinLog2Radiance = -8.f;
static const float kPolymorphicLightMaxLog2Radiance = 40.f;

enum PolymorphicLightType
{
    kSphere = 0,
    kCylinder,
    kDisk,
    kRect,
    kTriangle,
    kDirectional,
    kEnvironment,
    kPoint
};

// Stores shared light information (type) and specific light information
// See PolymorphicLight.hlsli for encoding format
struct PolymorphicLightInfo
{
    // uint4[0]
    float3 center;
    uint colorTypeAndFlags; // RGB8 + uint8 (see the kPolymorphicLight... constants above)

    // uint4[1]
    uint direction1;  // oct-encoded
    uint direction2;  // oct-encoded
    uint scalars;     // 2x float16
    uint logRadiance; // uint16

    // uint4[2] -- optional, contains only shaping data
    uint iesProfileIndex;
    uint primaryAxis;             // oct-encoded
    uint cosConeAngleAndSoftness; // 2x float16
    uint padding;
};

struct LightShaping
{
    float cosConeAngle;
    float3 primaryAxis;
    float cosConeSoftness;
    uint isSpot;
    int iesProfileIndex;
};

typedef PolymorphicLightInfo Light;

#endif // LIGHT_SHAPING_HLSLI