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

PS_Input_Particle VS(uint vid : SV_VertexID)
{
    uint segIdx    = vid / 6;
    uint cornerIdx = vid % 6;
    uint2 Corner   = BeamCorner[cornerIdx];
    uint pointIdx  = segIdx + Corner.x;
    uint side      = Corner.y;

    float PointCountF = max((float)BeamPointCount - 1.0f, 1.0f);
    float T = (float)pointIdx / PointCountF;

    float3 BeamDelta = BeamTarget - BeamSource;
    float  BeamLen   = length(BeamDelta);
    float3 BeamDir   = (BeamLen > 1e-6f) ? BeamDelta / BeamLen : float3(1, 0, 0);

    float3 Center    = BeamSource + BeamDelta * T;
    float  Taper     = ApplyBeamTaper(BeamTaperMethod, BeamTaperFactor, BeamTaperScale, T);
    float  HalfWidth = max(0.0f, BeamWidth * Taper) * 0.5f;
    float  SideSign  = (side == 0) ? -1.0f : 1.0f;

    float3 ToCamera  = SafeNormalizeBeam(CameraWorldPos - Center, float3(0, 0, 1));
    float3 SideAxis  = SafeNormalizeBeam(cross(ToCamera, BeamDir), float3(1, 0, 0));
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
