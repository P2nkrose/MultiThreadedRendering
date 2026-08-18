//--------------------------------------------------------------------------------------
// File: DepthStencilState.h
//--------------------------------------------------------------------------------------
#pragma once

#include <d3d11.h>

class DepthStencilState
{
public:
    bool Create(ID3D11Device* device, const D3D11_DEPTH_STENCIL_DESC& desc, const char* debugName = nullptr);
    void Destroy();

    operator ID3D11DepthStencilState* () const { return state; }

    DepthStencilState() : state(nullptr) {}
    ~DepthStencilState() { Destroy(); }

private:
    DepthStencilState(const DepthStencilState&) = delete;
    const DepthStencilState& operator = (const DepthStencilState&) = delete;

    ID3D11DepthStencilState* state;
};
