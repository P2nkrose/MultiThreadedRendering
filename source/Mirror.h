//--------------------------------------------------------------------------------------
// File: Mirror.h
//
// 거울 집합의 기하 데이터. 거울 평면/사각형 정점을 보관하고 초기화한다.
// (거울용 깊이/스텐실 상태와 정점 버퍼는 렌더러가 소유한다.)
//--------------------------------------------------------------------------------------
#pragma once

#include <DirectXMath.h>
#include "RenderConstants.h"

// 거울 사각형 정점. Position 만 실제로 쓰이고, 나머지는 정점 셰이더 입력 레이아웃 호환용.
struct MirrorVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 Texcoord;
    DirectX::XMFLOAT3 Tangent;
};
typedef MirrorVertex MirrorRect[4];

// 거울 스텐실 마스크/참조 값
constexpr UINT8 MIRROR_STENCIL_MASK = 0x01;
constexpr UINT8 MIRROR_STENCIL_REF  = 0x01;

class MirrorSet
{
public:
    DirectX::XMVECTOR center[NUM_MIRRORS];
    DirectX::XMVECTOR normal[NUM_MIRRORS];
    DirectX::XMVECTOR plane[NUM_MIRRORS];
    float             width[NUM_MIRRORS];
    float             height[NUM_MIRRORS];
    float             resolutionX[NUM_MIRRORS];
    float             resolutionY[NUM_MIRRORS];
    DirectX::XMFLOAT3 corner[4];
    MirrorRect        rect[NUM_MIRRORS];

    // sdkmesh 콘텐츠에 맞춰 손으로 튜닝한 거울 위치/법선/크기와, 평면·사각형 정점을 계산한다.
    void Initialize();
};
