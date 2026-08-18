//--------------------------------------------------------------------------------------
// File: Light.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "Light.h"

using namespace DirectX;

void LightSet::Initialize(FXMVECTOR sceneCenter, float radius)
{
    sceneRadius = radius;

    // 하늘광을 손으로 근사한 방향 (인덱스 0)
    static const XMVECTORF32 lightDir0 = { -0.67f, -0.71f, +0.21f, 0.f };

    color[0] = XMFLOAT4(3.0f * 0.160f, 3.0f * 0.341f, 3.0f * 1.000f, 1.000f);
    dir[0]   = XMVector3Normalize(lightDir0);
    pos[0]   = sceneCenter - sceneRadius * dir[0];
    fov[0]   = XM_PI / 4.0f;

    // 천장 램프 3개
    static const XMVECTORF32 lightPos1 = { 0.0f, 400.0f, -250.0f, 0.f };
    static const XMVECTORF32 lightPos2 = { 0.0f, 400.0f, 0.0f, 0.f };
    static const XMVECTORF32 lightPos3 = { 0.0f, 400.0f, 250.0f, 0.f };

    color[1] = XMFLOAT4(0.4f * 0.895f, 0.4f * 0.634f, 0.4f * 0.626f, 1.0f);
    pos[1]   = lightPos1;
    dir[1]   = g_XMNegIdentityR1;
    fov[1]   = 65.0f * (XM_PI / 180.0f);

    color[2] = XMFLOAT4(0.5f * 0.388f, 0.5f * 0.641f, 0.5f * 0.401f, 1.0f);
    pos[2]   = lightPos2;
    dir[2]   = g_XMNegIdentityR1;
    fov[2]   = 65.0f * (XM_PI / 180.0f);

    color[3] = XMFLOAT4(0.4f * 1.000f, 0.4f * 0.837f, 0.4f * 0.848f, 1.0f);
    pos[3]   = lightPos3;
    dir[3]   = g_XMNegIdentityR1;
    fov[3]   = 65.0f * (XM_PI / 180.0f);

    // 나머지 파라미터는 모든 광원에 동일 패턴 적용
    for (int iLight = 0; iLight < NUM_LIGHTS; ++iLight)
    {
        aspect[iLight]    = 1.0f;
        nearPlane[iLight] = 100.f;
        farPlane[iLight]  = 2.0f * sceneRadius;

        falloffDistEnd[iLight]   = farPlane[iLight];
        falloffDistRange[iLight] = 100.0f;

        falloffCosAngleEnd[iLight]   = cosf(fov[iLight] / 2.0f);
        falloffCosAngleRange[iLight] = 0.1f;
    }
}

void LightSet::Animate(float totalTime)
{
    XMVECTOR cycle1 = XMVectorSet(0.f, 0.f,
                                   0.20f * sinf(2.0f * (totalTime + 0.0f * XM_PI)), 0.f);
    dir[1] = XMVector3Normalize(g_XMNegIdentityR1 + cycle1);

    XMVECTOR cycle2 = XMVectorSet(0.10f * cosf(1.6f * (totalTime + 0.3f * XM_PI)), 0.f,
                                   0.10f * sinf(1.6f * (totalTime + 0.0f * XM_PI)), 0.f);
    dir[2] = XMVector3Normalize(g_XMNegIdentityR1 + cycle2);

    XMVECTOR cycle3 = XMVectorSet(0.30f * cosf(2.4f * (totalTime + 0.3f * XM_PI)), 0.f, 0.f, 0.f);
    dir[3] = XMVector3Normalize(g_XMNegIdentityR1 + cycle3);
}

XMMATRIX LightSet::CalcLightViewProj(int iLight) const
{
    XMVECTOR vLightDir = dir[iLight];
    XMVECTOR vLightPos = pos[iLight];

    XMVECTOR vLookAt = vLightPos + sceneRadius * vLightDir;

    XMMATRIX mLightView = XMMatrixLookAtLH(vLightPos, vLookAt, g_XMIdentityR1);
    XMMATRIX mLightProj = XMMatrixPerspectiveFovLH(fov[iLight], aspect[iLight], nearPlane[iLight], farPlane[iLight]);

    return mLightView * mLightProj;
}
