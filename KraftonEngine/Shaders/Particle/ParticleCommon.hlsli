#ifndef PARTICLE_COMMON_HLSLI
#define PARTICLE_COMMON_HLSLI

// b2 (PerShader0): common per-emitter particle parameters.
cbuffer ParticleParamBuffer : register(b2)
{
    uint SubUVCols;
    uint SubUVRows;
    uint ScreenAlignment;
    float _Pad;
    float3 EmitterOrigin;
    float _Pad1;
    uint AlphaSource;
    float AlphaThreshold;
    float AlphaPower;
    float ColorIntensity;
}

static const uint PARTICLE_SCREEN_ALIGNMENT_SQUARE = 0;
static const uint PARTICLE_SCREEN_ALIGNMENT_RECTANGLE = 1;
static const uint PARTICLE_SCREEN_ALIGNMENT_VELOCITY = 2;
static const uint PARTICLE_SCREEN_ALIGNMENT_AWAY_FROM_CENTER = 3;
static const uint PARTICLE_SCREEN_ALIGNMENT_TYPE_SPECIFIC = 4;
static const uint PARTICLE_SCREEN_ALIGNMENT_FACING_CAMERA_POSITION = 5;

uint GetParticleScreenAlignment()
{
    return ScreenAlignment;
}

// b3 (PerShader1): per-beam parameters. Matches FBeamParamConstants in RenderConstants.h.
cbuffer BeamParamBuffer : register(b3)
{
    float3 BeamSource;
    float  BeamWidth;
    float3 BeamTarget;
    float  BeamAlpha;
    float3 BeamSourceTangent;
    uint   BeamUseTangents;
    float3 BeamTargetTangent;
    float  BeamNoiseSeed;
    float3 BeamColor;
    float  BeamTaperFactor;
    float  BeamTaperScale;
    uint   BeamTaperMethod;
    uint   BeamPointCount;
    uint   BeamTextureTile;
    float  BeamTextureTileDistance;
    uint   BeamSheetCount;
    float  BeamNoiseAmplitude;
    float  BeamNoiseFrequency;
    float  BeamNoisePhase;
    float  _BeamPad0;
    float3 BeamNoiseRangeMin;
    float  _BeamPad1;
    float3 BeamNoiseRangeMax;
    float  _BeamPad2;
    float2 _BeamPad;
}

static const uint BEAM_TAPER_NONE    = 0;
static const uint BEAM_TAPER_FULL    = 1;
static const uint BEAM_TAPER_PARTIAL = 2;

float ApplyBeamTaper(uint TaperMethod, float TaperFactor, float TaperScale, float Alpha)
{
    Alpha = saturate(Alpha);
    TaperScale = max(0.0f, TaperScale);

    if (TaperMethod == BEAM_TAPER_FULL)
    {
        return (1.0f - Alpha * (1.0f - TaperFactor)) * TaperScale;
    }
    if (TaperMethod == BEAM_TAPER_PARTIAL)
    {
        if (Alpha <= TaperFactor)
        {
            return TaperScale;
        }
        float Denom = max(1.0f - TaperFactor, 1e-6f);
        return (1.0f - (Alpha - TaperFactor) / Denom) * TaperScale;
    }
    return 1.0f;
}

#endif
