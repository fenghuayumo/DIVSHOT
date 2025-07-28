#ifndef RTXDI_CHECKERBOARD_HLSLI
#define RTXDI_CHECKERBOARD_HLSLI

bool RTXDI_IsActiveCheckerboardPixel(
    uint2 pixelPosition,
    bool previousFrame,
    uint activeCheckerboardField)
{
    if (activeCheckerboardField == 0)
        return true;

    return ((pixelPosition.x + pixelPosition.y + int(previousFrame)) & 1) == (activeCheckerboardField & 1);
}

void RTXDI_ActivateCheckerboardPixel(inout uint2 pixelPosition, bool previousFrame, uint activeCheckerboardField)
{
    if (RTXDI_IsActiveCheckerboardPixel(pixelPosition, previousFrame, activeCheckerboardField))
        return;
    
    if (previousFrame)
        pixelPosition.x += int(activeCheckerboardField) * 2 - 3;
    else
        pixelPosition.x += (pixelPosition.y & 1) != 0 ? 1 : -1;
}

void RTXDI_ActivateCheckerboardPixel(inout int2 pixelPosition, bool previousFrame, uint activeCheckerboardField)
{
    uint2 uPixelPosition = uint2(pixelPosition);
    RTXDI_ActivateCheckerboardPixel(uPixelPosition, previousFrame, activeCheckerboardField);
    pixelPosition = int2(uPixelPosition);
}

#endif // RTXDI_CHECKERBOARD_HLSLI
