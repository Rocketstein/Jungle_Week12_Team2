#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

// Mesh-particle base color. Same convention as ParticleSprite.hlsl —
// bound from the material's "DiffuseTexture" slot (EMaterialTextureSlot::Diffuse → t0).
Texture2D ParticleAtlas : register(t0);

//cbuffer ParticleParamBuffer : register(b2)
//{
//    float SubUVCols;
//    float SubUVRows;
//    float ScreenAlignment;
//    float _Pad;
//}

// VS:
PS_Input_Particle VS(VS_Input_MeshParticle Input)
{
    float4x4 InstanceModel = float4x4(
        Input.instanceTransform0,
        Input.instanceTransform1,
        Input.instanceTransform2,
        Input.instanceTransform3);

    PS_Input_Particle Out;
    float4 WorldPos = mul(float4(Input.position, 1.0f), InstanceModel);
    float4 ViewPos  = mul(WorldPos, View);
    Out.position    = mul(ViewPos, Projection);
    Out.color    = Input.color * Input.instanceColor;
    Out.texcoord = Input.texcoord;
    return Out;
}

float4 PS(PS_Input_Particle Input) : SV_Target
{
    float4 Col = ParticleAtlas.Sample(LinearClampSampler, Input.texcoord);

    // Match ParticleSprite — discard fully-transparent texels so the alpha mask
    // works regardless of blend mode (Additive ignores alpha; AlphaBlend still
    // benefits from the early-out and avoids depth writes for invisible pixels
    // if depth-write is ever turned back on).
    clip(Col.a * Input.color.a - 0.01f);

    return float4(ApplyWireframe(Col.rgb) * Input.color.rgb,
                  bIsWireframe ? 1.0f : (Col.a * Input.color.a));
}
