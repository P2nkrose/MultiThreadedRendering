//--------------------------------------------------------------------------------------
// File: Mirror.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "Mirror.h"

using namespace DirectX;

void MirrorSet::Initialize()
{
    // 이 값들은 sdkmesh 콘텐츠 + 수동 보정에 기반한 하드코딩 값이다.
    static const XMVECTORF32 MirrorCenter0 = { -35.1688f, 89.279683f, -0.7488765f, 0.f };
    static const XMVECTORF32 MirrorCenter1 = { 41.2174f, 89.279683f, -0.7488745f, 0.f };
    static const XMVECTORF32 MirrorCenter2 = { 3.024275f, 89.279683f, -54.344299f, 0.f };
    static const XMVECTORF32 MirrorCenter3 = { 3.02427475f, 89.279683f, 52.8466f, 0.f };

    center[0] = MirrorCenter0;
    center[1] = MirrorCenter1;
    center[2] = MirrorCenter2;
    center[3] = MirrorCenter3;

    width [0] = 104.190895f;   height[0] = 92.19922656f;
    width [1] = 104.190899f;   height[1] = 92.19923178f;
    width [2] = 76.3862f;      height[2] = 92.3427325f;
    width [3] = 76.386196f;    height[3] = 92.34274043f;

    static const XMVECTORF32 MirrorNormal0 = { -0.998638464f, -0.052165297f, 0.0f, 0.f };
    static const XMVECTORF32 MirrorNormal1 = { 0.998638407f, -0.052166381f, 3.15017E-08f, 0.f };
    static const XMVECTORF32 MirrorNormal2 = { 0.0f, -0.076278878f, -0.997086522f, 0.f };
    static const XMVECTORF32 MirrorNormal3 = { -5.22129E-08f, -0.076279957f, 0.99708644f, 0.f };

    normal[0] = MirrorNormal0;
    normal[1] = MirrorNormal1;
    normal[2] = MirrorNormal2;
    normal[3] = MirrorNormal3;

    for (int i = 0; i < NUM_MIRRORS; ++i)
    {
        resolutionX[i] = 320.0f;
        resolutionY[i] = (resolutionX[i] * height[i] / width[i]);
    }

    corner[0] = XMFLOAT3(-1.0f, -1.0f, 0.0f);
    corner[1] = XMFLOAT3(1.0f, -1.0f, 0.0f);
    corner[2] = XMFLOAT3(-1.0f,  1.0f, 0.0f);
    corner[3] = XMFLOAT3(1.0f,  1.0f, 0.0f);

    for (int iMirror = 0; iMirror < NUM_MIRRORS; ++iMirror)
    {
        plane[iMirror] = XMPlaneFromPointNormal(center[iMirror], normal[iMirror]);

        // 로컬 공간 거울 사각형 정점
        for (UINT iCorner = 0; iCorner < 4; ++iCorner)
        {
            rect[iMirror][iCorner].Position.x = 0.5f * width[iMirror]  * corner[iCorner].x;
            rect[iMirror][iCorner].Position.y = 0.5f * height[iMirror] * corner[iCorner].y;
            rect[iMirror][iCorner].Position.z =                          corner[iCorner].z;

            rect[iMirror][iCorner].Normal   = XMFLOAT3(0.0f, 0.0f, 0.0f);
            rect[iMirror][iCorner].Texcoord = XMFLOAT2(0.0f, 0.0f);
            rect[iMirror][iCorner].Tangent  = XMFLOAT3(0.0f, 0.0f, 0.0f);
        }
    }
}
