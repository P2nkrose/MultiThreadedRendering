//--------------------------------------------------------------------------------------
// File: ConstantBuffers.h
//
// HLSL 상수 버퍼와 1:1로 대응하는 CPU 측 레이아웃 구조체와 바인딩 슬롯.
//--------------------------------------------------------------------------------------
#pragma once

#include <DirectXMath.h>
#include "RenderConstants.h"

struct CB_VS_PER_OBJECT
{
    DirectX::XMFLOAT4X4 World;
};
constexpr UINT CBVSPerObjectBind = 0;

struct CB_VS_PER_SCENE
{
    DirectX::XMFLOAT4X4 ViewProj;
};
constexpr UINT CBVSPerSceneBind = 1;

struct CB_PS_PER_OBJECT
{
    DirectX::XMFLOAT4 ObjectColor;
};
constexpr UINT CBPSPerObjectBind = 0;

struct CB_PS_PER_LIGHT
{
    struct LightDataStruct
    {
        DirectX::XMFLOAT4X4 LightViewProj;
        DirectX::XMFLOAT4   LightPos;
        DirectX::XMFLOAT4   LightDir;
        DirectX::XMFLOAT4   LightColor;
        DirectX::XMFLOAT4   Falloffs;    // x = dist end, y = dist range, z = cos angle end, w = cos range
    } LightData[NUM_LIGHTS];
};
constexpr UINT CBPSPerLightBind = 1;

struct CB_PS_PER_SCENE
{
    DirectX::XMFLOAT4 MirrorPlane;
    DirectX::XMFLOAT4 AmbientColor;
    DirectX::XMFLOAT4 TintColor;
};
constexpr UINT CBPSPerSceneBind = 2;
