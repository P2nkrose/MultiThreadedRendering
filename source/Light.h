//--------------------------------------------------------------------------------------
// File: Light.h
//
// 씬의 광원 집합. 위치/방향/색/감쇠 파라미터를 보관하고, 광원 시점의 ViewProj 행렬을 계산한다.
//--------------------------------------------------------------------------------------
#pragma once

#include <DirectXMath.h>
#include "RenderConstants.h"

class LightSet
{
public:
    // 광원 파라미터 (인덱스 0 = 하늘광, 1~3 = 천장 램프)
    DirectX::XMFLOAT4  color[NUM_LIGHTS];
    DirectX::XMVECTOR  pos[NUM_LIGHTS];
    DirectX::XMVECTOR  dir[NUM_LIGHTS];
    float              falloffDistEnd[NUM_LIGHTS];
    float              falloffDistRange[NUM_LIGHTS];
    float              falloffCosAngleEnd[NUM_LIGHTS];
    float              falloffCosAngleRange[NUM_LIGHTS];
    float              fov[NUM_LIGHTS];
    float              aspect[NUM_LIGHTS];
    float              nearPlane[NUM_LIGHTS];
    float              farPlane[NUM_LIGHTS];

    // 광원 초기값을 손으로 튜닝한 값으로 설정한다.
    void Initialize(DirectX::FXMVECTOR sceneCenter, float sceneRadius);

    // 천장 램프(1~3)를 시간에 따라 살짝 흔든다.
    void Animate(float totalTime);

    // 광원 iLight 시점의 View * Projection 행렬.
    DirectX::XMMATRIX CalcLightViewProj(int iLight) const;

private:
    float sceneRadius = 0.0f;
};
