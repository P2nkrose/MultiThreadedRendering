//--------------------------------------------------------------------------------------
// File: RasterizerState.h
//--------------------------------------------------------------------------------------
#pragma once

#include <d3d11.h>

class RasterizerState
{
public:
    bool Create(ID3D11Device* device, const D3D11_RASTERIZER_DESC& desc, const char* debugName = nullptr);
    void Destroy();

    operator ID3D11RasterizerState* () const { return state; }

    RasterizerState() : state(nullptr) {}
    ~RasterizerState() { Destroy(); }

private:
    RasterizerState(const RasterizerState&) = delete;
    const RasterizerState& operator = (const RasterizerState&) = delete;

    ID3D11RasterizerState* state;
};
