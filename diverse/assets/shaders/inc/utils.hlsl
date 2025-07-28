
#ifndef MIS_HLSL
#define MIS_HLSL

/**
 * Balanced heuristic used in MIS weight calculation.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @return The calculated weight.
 */
float balanceHeuristic(float fPDF, float gPDF)
{
    return fPDF / (fPDF + gPDF);
}

/**
 * Balanced heuristic used in MIS weight calculation over 3 input functions.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @param hPDF The PDF of the second MIS weighting value.
 * @return The calculated weight.
 */
float balanceHeuristic(float fPDF, float gPDF, float hPDF)
{
    return fPDF / (fPDF + gPDF + hPDF);
}

/**
 * Power heuristic used in MIS weight calculation.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @return The calculated weight.
 */
float powerHeuristic(float fPDF, float gPDF)
{
    return (fPDF * fPDF) / (fPDF * fPDF + gPDF * gPDF);
}

/**
 * Power heuristic used in MIS weight calculation over 3 input functions.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @return The calculated weight.
 */
float powerHeuristic(float fPDF, float gPDF, float hPDF)
{
    return (fPDF * fPDF) / (fPDF * fPDF + gPDF * gPDF + hPDF * hPDF);
}

/**
 * Heuristic used in MIS weight calculation.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @return The calculated weight.
 */
float heuristicMIS(float fPDF, float gPDF)
{
    return balanceHeuristic(fPDF, gPDF);
    //return powerHeuristic(fPDF, gPDF);
}

/**
 * Heuristic used in MIS weight calculation over 3 input functions.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @return The calculated weight.
 */
float heuristicMIS(float fPDF, float gPDF, float hPDF)
{
    return balanceHeuristic(fPDF, gPDF, hPDF);
    //return powerHeuristic(fPDF, gPDF, hPDF);
}

#endif // MIS_HLSL
