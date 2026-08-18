//--------------------------------------------------------------------------------------
// File: SamplerState.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SamplerState.h"

bool SamplerState::Create(ID3D11Device* device, const D3D11_SAMPLER_DESC& desc, const char* debugName)
{
    Destroy();
    if (FAILED(device->CreateSamplerState(&desc, &state)))
        return false;
    if (debugName)
    {
        DXUT_SetDebugName(state, debugName);
    }
    return true;
}

void SamplerState::Destroy()
{
    SAFE_RELEASE(state);
}
