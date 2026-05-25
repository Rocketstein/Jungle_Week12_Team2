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
    float  Progress  = saturate(BeamProgress);
    float  VisibleLen = BeamLen * Progress;
    float3 VisibleDelta = BeamDelta * Progress;
    float3 BeamDir   = (BeamLen > 1e-6f) ? BeamDelta / BeamLen : float3(1, 0, 0);

    float3 Center    = BeamSource + VisibleDelta * T;
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
        ? (VisibleLen * T) / BeamTextureTileDistance
        : T * (float)max(BeamTextureTile, 1u);

    PS_Input_Particle Out;
    Out.position = mul(mul(float4(WorldPos, 1.0f), View), Projection);
    Out.texcoord = float2(U, (float)side);
    Out.color    = float4(BeamColor, BeamAlpha);
    return Out;
}

float4 PS(PS_Input_Particle Input) : SV_Target
{
    float4 Col = DiffuseTexture.Sample(LinearWrapSampler, Input.texcoord);
    clip(Col.a * Input.color.a - 0.01f);

    return float4(ApplyWireframe(Col.rgb) * Input.color.rgb,
                  bIsWireframe ? 1.0f : (Col.a * Input.color.a));
}
