//--------------------------------------------------------------------------------------
// File: ShadowMap.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "ShadowMap.h"

bool ShadowMap::Create(ID3D11Device* device, float resolutionX, float resolutionY)
{
    Destroy();

    D3D11_TEXTURE2D_DESC texDesc = {
        UINT(resolutionX),                    // Width
        UINT(resolutionY),                    // Height
        1,                                      // MipLevels
        1,                                      // ArraySize
        DXGI_FORMAT_R32_TYPELESS,               // Format
        { 1, 0, },                              // SampleDesc
        D3D11_USAGE_DEFAULT,                    // Usage
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL,
        0,                                      // CPUAccessFlags
        0,                                      // MiscFlags
    };
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {
        DXGI_FORMAT_D32_FLOAT,
        D3D11_DSV_DIMENSION_TEXTURE2D,
        0,
        { 0, },
    };
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
        DXGI_FORMAT_R32_FLOAT,
        D3D11_SRV_DIMENSION_TEXTURE2D,
        { 0, 1, },
    };

    if (FAILED(device->CreateTexture2D(&texDesc, nullptr, &texture)))
        return false;
    DXUT_SetDebugName(texture, "Shadow");

    if (FAILED(device->CreateDepthStencilView(texture, &dsvDesc, &dsv)))
        return false;
    DXUT_SetDebugName(dsv, "Shadow DSV");

    if (FAILED(device->CreateShaderResourceView(texture, &srvDesc, &srv)))
        return false;
    DXUT_SetDebugName(srv, "Shadow RSV");

    viewport.Width    = resolutionX;
    viewport.Height   = resolutionY;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;

    return true;
}

void ShadowMap::Destroy()
{
    SAFE_RELEASE(texture);
    SAFE_RELEASE(srv);
    SAFE_RELEASE(dsv);
}
