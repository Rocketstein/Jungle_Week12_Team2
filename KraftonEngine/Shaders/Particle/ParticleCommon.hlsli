#ifndef PARTICLE_COMMON_HLSLI
#define PARTICLE_COMMON_HLSLI

// b2 (PerShader0): common per-emitter particle parameters.
cbuffer ParticleParamBuffer : register(b2)
{
    float SubUVCols;
    float SubUVRows;
    float ScreenAlignment;
    float _Pad;
}

#endif
