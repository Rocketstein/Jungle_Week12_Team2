#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D ParticleAtlas : register(t0);

//cbuffer SubUVRegionBuffer : register(b2)
//{
//    float4 UVRegion; // xy = offset, zw = size
//}

float2 RotateParticleCorner(float2 Corner, float Rotation)
{
    float S;
    float C;
    sincos(Rotation, S, C);
    return float2(Corner.x * C - Corner.y * S, Corner.x * S + Corner.y * C);
}

PS_Input_Tex VS(VS_Input_ParticleSprite Input)
{
    PS_Input_Tex Out;
    float2 Corner = Input.uv * 2.0f - 1.0f;
    Corner = RotateParticleCorner(Corner, Input.rotation);
    Corner *= Input.size.xy;
    float4 viewPos = mul(float4(position, 1), View);
    viewPos.xy += corner;
    output.position = mul(viewPos, Projection);
    
    Out.color = Input.color;
    return Out;
}

float4 PS(PS_Input_Tex Input) : SV_Target
{
    return float4(1, 1, 1, 1);
}