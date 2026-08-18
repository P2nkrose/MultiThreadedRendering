//--------------------------------------------------------------------------------------
// File: ShadowMap.h
//
// 그림자 맵 하나 = 깊이 텍스처 + DSV + SRV + 뷰포트. RAII.
//--------------------------------------------------------------------------------------
#pragma once

#include <d3d11.h>

class ShadowMap
{
public:
    bool Create(ID3D11Device* device, float resolutionX, float resolutionY);
    void Destroy();

    ID3D11ShaderResourceView* GetSRV() const { return srv; }
    ID3D11DepthStencilView*   GetDSV() const { return dsv; }
    D3D11_VIEWPORT*           GetViewport() { return &viewport; }

    ShadowMap() : texture(nullptr), srv(nullptr), dsv(nullptr), viewport{} {}
    ~ShadowMap() { Destroy(); }

private:
    ShadowMap(const ShadowMap&) = delete;
    const ShadowMap& operator = (const ShadowMap&) = delete;

    ID3D11Texture2D*          texture;
    ID3D11ShaderResourceView* srv;
    ID3D11DepthStencilView*   dsv;
    D3D11_VIEWPORT            viewport;
};
