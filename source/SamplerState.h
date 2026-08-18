//--------------------------------------------------------------------------------------
// File: SamplerState.h
//--------------------------------------------------------------------------------------
#pragma once

#include <d3d11.h>

class SamplerState
{
public:
    bool Create(ID3D11Device* device, const D3D11_SAMPLER_DESC& desc, const char* debugName = nullptr);
    void Destroy();

    operator ID3D11SamplerState* () const { return state; }

    SamplerState() : state(nullptr) {}
    ~SamplerState() { Destroy(); }

private:
    SamplerState(const SamplerState&) = delete;
    const SamplerState& operator = (const SamplerState&) = delete;

    ID3D11SamplerState* state;
};
