#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "ParticleCommon.hlsli"

Texture2D ParticleAtlas : register(t0);

//cbuffer ParticleParamBuffer : register(b2)
//{
//    float SubUVCols;
//    float SubUVRows;
//    float ScreenAlignment;
//    float _Pad;
//}

// Rotate a 2D corner offset (radians, CCW).
float2 RotateParticleCorner(float2 Corner, float Rotation)
{
    float S, C;
    sincos(Rotation, S, C);
    return float2(Corner.x * C - Corner.y * S,
                  Corner.x * S + Corner.y * C);
}

PS_Input_Particle VS(VS_Input_ParticleSprite Input)
{
    PS_Input_Particle Out;

    float2 Corner = Input.uv * 2.0f - 1.0f;
    Corner = RotateParticleCorner(Corner, Input.rotation);
    Corner *= Input.size.xy;

    // Particle positions are world-space (proxy uses identity Model).
    // World -> View, add corner on view-space XY, then View -> Clip.
    float4 ViewPos = mul(float4(Input.position, 1.0f), View);
    ViewPos.xy += Corner;
    Out.position = mul(ViewPos, Projection);
    Out.texcoord = float2(Input.uv.x, 1.0f - Input.uv.y);
    Out.color    = Input.color;
    return Out;
}

float4 PS(PS_Input_Particle Input) : SV_Target
{
    float4 Col = ParticleAtlas.Sample(LinearClampSampler, Input.texcoord);
    clip(Col.a * Input.color.a - 0.01f);

    return float4(ApplyWireframe(Col.rgb) * Input.color.rgb,
                  bIsWireframe ? 1.0f : (Col.a * Input.color.a));
}
