//--------------------------------------------------------------------------------------
// File: SquidRenderer.h
//
// SquidRoom 씬의 모든 렌더링 상태와 로직을 소유하는 렌더러.
// 5가지 렌더 경로(즉시 / 씬·청크 단위 × 단일·멀티 스레드 디퍼드 컨텍스트)를 구현한다.
//--------------------------------------------------------------------------------------
#pragma once

#include <windows.h>
#include <d3d11.h>
#include <DirectXMath.h>

#include "RenderConstants.h"
#include "SceneParams.h"
#include "WorkQueue.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "ConstantBuffer.h"
#include "VertexBuffer.h"
#include "SamplerState.h"
#include "RasterizerState.h"
#include "DepthStencilState.h"
#include "Light.h"
#include "ShadowMap.h"
#include "Mirror.h"
#include "MultiDeviceContextDXUTMesh.h"

class CModelViewerCamera;

class SquidRenderer
{
public:
    SquidRenderer();
    ~SquidRenderer();

    HRESULT Create(ID3D11Device* device, ID3D11DeviceContext* immediateContext);
    void Destroy();

    // 한 프레임의 씬 렌더링(그림자 → 거울 → 메인 → 커맨드 리스트 실행). HUD 는 호출부에서.
    void Render(ID3D11DeviceContext* immediateContext, const CModelViewerCamera& camera);

    // 프레임 시작 시 상태 초기화(디버그 상태 누수 방지). 설정 대화상자보다 먼저 호출.
    void BeginFrame(ID3D11DeviceContext* immediateContext);

    // --- UI 연동 ---
    void SetDeviceContextType(DEVICECONTEXT_TYPE type) { deviceContextType = type; }
    void ToggleWireFrame()                               { wireFrame = !wireFrame; }
    void ToggleRenderSceneLightPOV()                     { renderSceneLightPOV = !renderSceneLightPOV; }
    void AnimateLights(float totalTime)                { lights.Animate(totalTime); }

private:
    SquidRenderer(const SquidRenderer&) = delete;
    const SquidRenderer& operator = (const SquidRenderer&) = delete;

    // --- 현재 렌더 경로 판별 ---
    bool IsRenderDeferredPerScene() const
    {
        return DEVICECONTEXT_ST_DEFERRED_PER_SCENE == deviceContextType
            || DEVICECONTEXT_MT_DEFERRED_PER_SCENE == deviceContextType;
    }
    bool IsRenderMultithreadedPerScene() const { return DEVICECONTEXT_MT_DEFERRED_PER_SCENE == deviceContextType; }
    bool IsRenderDeferredPerChunk() const
    {
        return DEVICECONTEXT_ST_DEFERRED_PER_CHUNK == deviceContextType
            || DEVICECONTEXT_MT_DEFERRED_PER_CHUNK == deviceContextType;
    }
    bool IsRenderMultithreadedPerChunk() const { return DEVICECONTEXT_MT_DEFERRED_PER_CHUNK == deviceContextType; }
    bool IsRenderDeferred() const { return IsRenderDeferredPerScene() || IsRenderDeferredPerChunk(); }

    // --- 초기화 ---
    HRESULT InitializeShadows(ID3D11Device* device);
    HRESULT InitializeMirrors(ID3D11Device* device);
    HRESULT InitializeWorkerThreads(ID3D11Device* device);

    // --- 렌더 ---
    void RenderMeshDirect(ID3D11DeviceContext* context, UINT iMesh);
    void OnRenderMeshCallback(UINT iMesh, ID3D11DeviceContext* context);

    HRESULT RenderSceneSetup(ID3D11DeviceContext* context, const SceneParamsStatic* staticParams, const SceneParamsDynamic* dynamicParams);
    HRESULT RenderScene(ID3D11DeviceContext* context, const SceneParamsStatic* staticParams, const SceneParamsDynamic* dynamicParams);

    void RenderShadow(int iShadow, ID3D11DeviceContext* context);
    void RenderMirror(int iMirror, ID3D11DeviceContext* context);
    void RenderSceneDirect(ID3D11DeviceContext* context);

    // --- 워커 스레드 ---
    [[noreturn]] void PerSceneThreadProc(int instance);
    [[noreturn]] void PerChunkThreadProc(int instance);

    static unsigned int WINAPI PerSceneThreadEntry(LPVOID param);
    static unsigned int WINAPI PerChunkThreadEntry(LPVOID param);

    // 메시가 청크를 그릴 때 호출하는 C 콜백 (정적) → 인스턴스로 위임
    static void RenderMeshCallback(CMultiDeviceContextDXUTMesh* mesh, UINT iMesh, bool adjacent,
                                    ID3D11DeviceContext* context, UINT diffuseSlot, UINT normalSlot, UINT specularSlot);

    static SquidRenderer* Instance;   // RenderMeshCallback 위임 대상

    static constexpr int NumPerSceneRenderThreads = NUM_SHADOWS + NUM_MIRRORS + 1;   // 씬마다 스레드 하나
    static constexpr int MaxPerChunkRenderThreads = 32;
    static constexpr int MaxPendingQueueEntries   = 1024;

    struct ThreadParam { SquidRenderer* renderer; int instance; };

    // --- 씬 데이터 ---
    CMultiDeviceContextDXUTMesh mesh;

    VertexShader        vertexShader;
    PixelShader         pixelShader;
    ID3D11InputLayout*  vertexLayout;
    ID3D11InputLayout*  mirrorVertexLayout;

    SamplerState        samPointClamp;
    SamplerState        samLinearWrap;

    RasterizerState     rsNoCull;
    RasterizerState     rsBackfaceCull;
    RasterizerState     rsFrontfaceCull;
    RasterizerState     rsNoCullWireFrame;

    DepthStencilState   dssNoStencil;
    DepthStencilState   dssMirrorDepthTestStencilOverwrite;
    DepthStencilState   dssMirrorDepthOverwriteStencilTest;
    DepthStencilState   dssMirrorDepthWriteStencilTest;
    DepthStencilState   dssMirrorDepthOverwriteStencilClear;

    ConstantBuffer      cbVSPerObject;
    ConstantBuffer      cbVSPerScene;
    ConstantBuffer      cbPSPerObject;
    ConstantBuffer      cbPSPerLight;
    ConstantBuffer      cbPSPerScene;

    LightSet            lights;
    ShadowMap           shadows[NUM_SHADOWS];
    MirrorSet           mirrors;
    VertexBuffer        mirrorVertexBuffer;

    SceneParamsStatic   staticParamsDirect;
    SceneParamsStatic   staticParamsShadow[NUM_SHADOWS];
    SceneParamsStatic   staticParamsMirror[NUM_MIRRORS];

    DEVICECONTEXT_TYPE  deviceContextType;
    bool                wireFrame;
    bool                renderSceneLightPOV;

    // 디버그: 상태 누수 검증용 플래그
    bool                clearStateUponBeginCommandList;
    bool                clearStateUponFinishCommandList;
    bool                clearStateUponExecuteCommandList;

    const CModelViewerCamera* pCamera;   // 현재 프레임의 카메라
    int                       nextAvailableChunkQueue;

    // --- per-scene 워커 스레드 ---
    HANDLE               perSceneThread[NumPerSceneRenderThreads];
    HANDLE               perSceneBeginEvent[NumPerSceneRenderThreads];
    HANDLE               perSceneEndEvent[NumPerSceneRenderThreads];
    ID3D11DeviceContext* perSceneDeferredContext[NumPerSceneRenderThreads];
    ID3D11CommandList*   perSceneCommandList[NumPerSceneRenderThreads];
    ThreadParam          perSceneThreadParam[NumPerSceneRenderThreads];

    // --- per-chunk 워커 스레드 ---
    int                  numPerChunkRenderThreads;
    HANDLE               perChunkThread[MaxPerChunkRenderThreads];
    HANDLE               perChunkBeginSemaphore[MaxPerChunkRenderThreads];
    HANDLE               perChunkEndEvent[MaxPerChunkRenderThreads];
    ID3D11DeviceContext* perChunkDeferredContext[MaxPerChunkRenderThreads];
    ID3D11CommandList*   perChunkCommandList[MaxPerChunkRenderThreads];
    ThreadParam          perChunkThreadParam[MaxPerChunkRenderThreads];
    ChunkQueue           chunkQueue[MaxPerChunkRenderThreads];
    int                  chunkQueueOffset[MaxPerChunkRenderThreads];
};
