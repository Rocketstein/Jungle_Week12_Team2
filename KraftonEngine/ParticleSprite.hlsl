#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D ParticleAtlas : register(t0);

//cbuffer SubUVRegionBuffer : register(b2)
//{
//    float4 UVRegion; // xy = offset, zw = size
//}

PS_Input_Tex VS(VS_Input_ParticleSprite Input)
{
    PS_Input Out;
	
    return Out;
}

float4 PS(PS_Input_Tex Input) : SV_Target
{
    return float4(1, 1, 1, 1);
}