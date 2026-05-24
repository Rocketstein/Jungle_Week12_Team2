#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Particle/ParticleCommon.hlsli"

Texture2D ParticleAtlas : register(t0);

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
    uint Alignment = GetParticleScreenAlignment();
    float4 ViewPos = mul(float4(Input.position, 1.0f), View);

    if (Alignment == PARTICLE_SCREEN_ALIGNMENT_SQUARE)
    {
        float UniformSize = Input.size.x;
        float2 ViewCorner = RotateParticleCorner(Corner, Input.rotation) * UniformSize;
        ViewPos.xy += ViewCorner;
        Out.position = mul(ViewPos, Projection);
    }
    else if (Alignment == PARTICLE_SCREEN_ALIGNMENT_RECTANGLE)
    {
        float2 ViewCorner = RotateParticleCorner(Corner, Input.rotation) * Input.size.xy;
        ViewPos.xy += ViewCorner;
        Out.position = mul(ViewPos, Projection);
    }
    else if (Alignment == PARTICLE_SCREEN_ALIGNMENT_VELOCITY)
    {
        float2 ViewVelocity = mul(float4(Input.velocity, 0.0f), View).xy;
        float SpeedSq = dot(ViewVelocity, ViewVelocity);
        float2 AxisX = (SpeedSq > 1e-6f) ? ViewVelocity * rsqrt(SpeedSq) : float2(1.0f, 0.0f);
        float2 AxisY = float2(-AxisX.y, AxisX.x);
        ViewPos.xy += AxisX * (Corner.x * Input.size.x) + AxisY * (Corner.y * Input.size.y);
        Out.position = mul(ViewPos, Projection);
    }
    else if (Alignment == PARTICLE_SCREEN_ALIGNMENT_AWAY_FROM_CENTER)
    {
        float2 ViewCenter = ViewPos.xy;
        float LenSq = dot(ViewCenter, ViewCenter);
        float2 AxisX = (LenSq > 1e-6f) ? ViewCenter * rsqrt(LenSq) : float2(1.0f, 0.0f);
        float2 AxisY = float2(-AxisX.y, AxisX.x);
        ViewPos.xy += AxisX * (Corner.x * Input.size.x) + AxisY * (Corner.y * Input.size.y);
        Out.position = mul(ViewPos, Projection);
    }
    else if (Alignment == PARTICLE_SCREEN_ALIGNMENT_TYPE_SPECIFIC)
    {
        float3 WorldCorner = float3(Corner * Input.size.xy, 0.0f);
        float4 WorldPos = float4(Input.position + WorldCorner, 1.0f);
        Out.position = mul(mul(WorldPos, View), Projection);
    }
    else if (Alignment == PARTICLE_SCREEN_ALIGNMENT_FACING_CAMERA_POSITION)
    {
        // Camera-facing billboard: spin the quad in view space with the per-particle rotation.
        float2 ViewCorner = RotateParticleCorner(Corner, Input.rotation) * Input.size.xy;
        ViewPos.xy += ViewCorner;
        Out.position = mul(ViewPos, Projection);
    }
    else
    {
        // Non-billboard fallback until type-specific sprite bases are provided.
        float3 WorldCorner = float3(Corner * Input.size.xy, 0.0f);
        float4 WorldPos = float4(Input.position + WorldCorner, 1.0f);
        Out.position = mul(mul(WorldPos, View), Projection);
    }

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
