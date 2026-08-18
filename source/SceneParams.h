//--------------------------------------------------------------------------------------
// File: SceneParams.h
//
// 하나의 씬(메인/그림자/거울)을 그리는 데 필요한 파라미터.
//--------------------------------------------------------------------------------------
#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

// 어떤 대상(그림자/거울/메인)을 그리느냐에 따라 달라지지만, 한 프레임 동안은 바뀌지 않는
// 정적 셋업 파라미터. per-chunk 워커 스레드에는 참조(reference)로 전달되므로 절대 수정 금지.
struct SceneParamsStatic
{
    ID3D11DepthStencilState*    DepthStencilState;   // 깊이/스텐실 상태
    UINT8                       StencilRef;          // 스텐실 참조 값

    ID3D11RasterizerState*      RasterizerState;     // 래스터라이저 상태(컬링/채움 모드 등)

    DirectX::XMFLOAT4           TintColor;           // 반사 등에 적용할 틴트(RGBA)
    DirectX::XMFLOAT4           MirrorPlane;         // 거울 평면 방정식(Ax+By+Cz+D=0)

    // DepthStencilView 가 non-null 이면 그림자 맵 렌더링, null 이면 DXUT 기본값 사용.
    ID3D11DepthStencilView*     DepthStencilView;    // 그림자 맵 렌더 시의 DSV
    D3D11_VIEWPORT*             Viewport;            // 그림자/거울 렌더 시 별도 뷰포트
};

// 씬마다 매번 바뀌는 동적 파라미터. per-chunk 워커 스레드에는 값(value)으로 복사되어 전달된다.
struct SceneParamsDynamic
{
    DirectX::XMFLOAT4X4 ViewProj;    // 해당 씬 카메라의 View * Projection 행렬
};
