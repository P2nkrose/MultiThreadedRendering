//--------------------------------------------------------------------------------------
// File: DepthStencilState.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DepthStencilState.h"

bool DepthStencilState::Create(ID3D11Device* device, const D3D11_DEPTH_STENCIL_DESC& desc, const char* debugName)
{
    Destroy();
    if (FAILED(device->CreateDepthStencilState(&desc, &state)))
        return false;
    if (debugName)
    {
        DXUT_SetDebugName(state, debugName);
    }
    return true;
}

void DepthStencilState::Destroy()
{
    SAFE_RELEASE(state);
}
