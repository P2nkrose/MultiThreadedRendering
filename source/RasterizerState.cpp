//--------------------------------------------------------------------------------------
// File: RasterizerState.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "RasterizerState.h"

bool RasterizerState::Create(ID3D11Device* device, const D3D11_RASTERIZER_DESC& desc, const char* debugName)
{
    Destroy();
    if (FAILED(device->CreateRasterizerState(&desc, &state)))
        return false;
    if (debugName)
    {
        DXUT_SetDebugName(state, debugName);
    }
    return true;
}

void RasterizerState::Destroy()
{
    SAFE_RELEASE(state);
}
