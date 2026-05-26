#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Particle/ParticleCommon.hlsli"

Texture2D DiffuseTexture : register(t0);

// Triangle-list corner layout per segment (matches the previous CPU packer):
//   0: P0 left   1: P1 left   2: P0 right
//   3: P1 left   4: P1 right  5: P0 right
static const uint2 BeamCorner[6] = {
    uint2(0, 0), uint2(1, 0), uint2(0, 1),
    uint2(1, 0), uint2(1, 1), uint2(0, 1),
};

float3 SafeNormalizeBeam(float3 V, float3 Fallback)
{
    float LenSq = dot(V, V);
    return (LenSq > 1e-6f) ? V * rsqrt(LenSq) : Fallback;
}

float3 RotateAroundAxis(float3 V, float3 UnitAxis, float Radians)
{
    float S, C;
    sincos(Radians, S, C);
    return V * C + cross(UnitAxis, V) * S + UnitAxis * dot(UnitAxis, V) * (1.0f - C);
}

float3 EvaluateBeamCenter(float T)
{
    if (BeamUseTangents == 0u)
    {
        return lerp(BeamSource, BeamTarget, T);
    }

    float T2 = T * T;
    float T3 = T2 * T;
    float H00 = 2.0f * T3 - 3.0f * T2 + 1.0f;
    float H10 = T3 - 2.0f * T2 + T;
    float H01 = -2.0f * T3 + 3.0f * T2;
    float H11 = T3 - T2;
    return H00 * BeamSource + H10 * BeamSourceTangent + H01 * BeamTarget + H11 * BeamTargetTangent;
}

float3 EvaluateBeamDirection(float T, float PointCountF)
{
    float Step = 1.0f / max(PointCountF, 1.0f);
    float T0 = saturate(T - Step);
    float T1 = saturate(T + Step);
    return SafeNormalizeBeam(EvaluateBeamCenter(T1) - EvaluateBeamCenter(T0), SafeNormalizeBeam(BeamTarget - BeamSource, float3(1, 0, 0)));
}

float Noise01(float Seed)
{
    return frac(sin(Seed) * 43758.5453123f);
}

float3 BeamNoiseSample(float SampleIndex)
{
    float Seed = BeamNoiseSeed + SampleIndex * 17.137f;
    return lerp(
        BeamNoiseRangeMin,
        BeamNoiseRangeMax,
        float3(Noise01(Seed + 11.0f), Noise01(Seed + 29.0f), Noise01(Seed + 47.0f)));
}

float3 ApplyBeamNoise(float3 Center, float3 BeamDir, float T)
{
    if (BeamNoiseFrequency <= 0.0f)
    {
        return Center;
    }

    float3 AxisA = SafeNormalizeBeam(cross(BeamDir, float3(0, 0, 1)), float3(0, 1, 0));
    float3 AxisB = SafeNormalizeBeam(cross(BeamDir, AxisA), float3(0, 0, 1));
    float EndpointFade = sin(saturate(T) * 3.14159265359f);
    float Phase = BeamNoisePhase + BeamNoiseSeed * 6.28318530718f;
    float WaveA = sin((T * BeamNoiseFrequency) * 6.28318530718f + Phase);
    float WaveB = cos((T * BeamNoiseFrequency * 1.37f) * 6.28318530718f - Phase);
    float NoiseCoord = saturate(T) * BeamNoiseFrequency;
    float NoiseIndex = floor(NoiseCoord);
    float NoiseAlpha = frac(NoiseCoord);
    float3 UniformRange = lerp(BeamNoiseSample(NoiseIndex), BeamNoiseSample(NoiseIndex + 1.0f), NoiseAlpha);
    float3 AxisNoise = (AxisA * WaveA + AxisB * WaveB) * BeamNoiseAmplitude;
    return Center + (UniformRange + AxisNoise) * EndpointFade;
}

PS_Input_Particle VS(uint vid : SV_VertexID)
{
    uint segmentCount     = max(BeamPointCount, 2u) - 1u;
    uint verticesPerSheet = segmentCount * 6u;
    uint sheetIdx         = vid / verticesPerSheet;
    uint localVid         = vid - sheetIdx * verticesPerSheet;
    uint segIdx           = localVid / 6u;
    uint cornerIdx        = localVid % 6u;
    uint2 Corner   = BeamCorner[cornerIdx];
    uint pointIdx  = segIdx + Corner.x;
    uint side      = Corner.y;

    float PointCountF = max((float)BeamPointCount - 1.0f, 1.0f);
    float T = (float)pointIdx / PointCountF;

    float3 BeamDelta = BeamTarget - BeamSource;
    float  BeamLen   = length(BeamDelta);
    float3 BeamDir   = EvaluateBeamDirection(T, PointCountF);

    float3 Center    = ApplyBeamNoise(EvaluateBeamCenter(T), BeamDir, T);
    float  Taper     = ApplyBeamTaper(BeamTaperMethod, BeamTaperFactor, BeamTaperScale, T);
    float  HalfWidth = max(0.0f, BeamWidth * Taper) * 0.5f;
    float  SideSign  = (side == 0) ? -1.0f : 1.0f;

    float3 ToCamera  = SafeNormalizeBeam(CameraWorldPos - Center, float3(0, 0, 1));
    float3 SideAxis  = SafeNormalizeBeam(cross(ToCamera, BeamDir), float3(1, 0, 0));
    uint SheetCount  = max(BeamSheetCount, 1u);
    if (sheetIdx > 0u)
    {
        float SheetAngle = 3.14159265359f * (float)sheetIdx / (float)SheetCount;
        SideAxis = SafeNormalizeBeam(RotateAroundAxis(SideAxis, BeamDir, SheetAngle), SideAxis);
    }
    float3 WorldPos  = Center + SideAxis * (HalfWidth * SideSign);

    float U = (BeamTextureTileDistance > 0.0f)
        ? (BeamLen * T) / BeamTextureTileDistance
        : T * (float)max(BeamTextureTile, 1u);

    PS_Input_Particle Out;
    Out.position = mul(mul(float4(WorldPos, 1.0f), View), Projection);
    Out.texcoord = float2(U, (float)side);
    Out.color    = float4(BeamColor, BeamAlpha);
    return Out;
}

float4 PS(PS_Input_Particle Input) : SV_Target
{
    float4 Col = DiffuseTexture.Sample(LinearClampSampler, Input.texcoord);
    clip(Col.a * Input.color.a - 0.01f);

    return float4(ApplyWireframe(Col.rgb) * Input.color.rgb,
                  bIsWireframe ? 1.0f : (Col.a * Input.color.a));
}
