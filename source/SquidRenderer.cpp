//--------------------------------------------------------------------------------------
// File: SquidRenderer.cpp
//
// SquidRoom 씬 렌더러 구현. 원본 MultithreadedRendering.cpp 의 렌더링/스레딩 로직을
// 하나의 클래스로 옮긴 것으로, 동작(5가지 렌더 경로)은 원본과 동일하다.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "SDKMesh.h"

#include <process.h>
#include <algorithm>
#include <cassert>

#include "SquidRenderer.h"
#include "ConstantBuffers.h"

using namespace DirectX;

//--------------------------------------------------------------------------------------
// 씬 상수 (조명/기하 튜닝 값)
//--------------------------------------------------------------------------------------
static const XMVECTORF32 AmbientColor = { 0.04f * 0.760f, 0.04f * 0.793f, 0.04f * 0.822f, 1.000f };
static const XMVECTORF32 MirrorTint   = { 0.3f, 0.5f, 1.0f, 1.0f };
static const XMVECTORF32 SceneCenter  = { 0.0f, 350.0f, 0.0f, 0.f };
static const float       SceneRadius  = 600.0f;

SquidRenderer* SquidRenderer::Instance = nullptr;

//--------------------------------------------------------------------------------------
// 물리 코어 수를 세는 헬퍼 
//--------------------------------------------------------------------------------------
typedef BOOL (WINAPI *LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

static DWORD CountBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = ULONG_PTR(1) << LSHIFT;
    for (DWORD i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest) ? 1 : 0);
        bitTest /= 2;
    }
    return bitSetCount;
}

static int GetPhysicalProcessorCount()
{
    DWORD procCoreCount = 0;

    HMODULE hMod = GetModuleHandle(L"kernel32");
    assert(hMod);
    _Analysis_assume_(hMod);

    auto Glpi = reinterpret_cast<LPFN_GLPI>(GetProcAddress(hMod, "GetLogicalProcessorInformation"));
    if (!Glpi)
        return procCoreCount;

    bool done = false;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = nullptr;
    DWORD returnLength = 0;

    while (!done)
    {
        BOOL rc = Glpi(buffer, &returnLength);
        if (FALSE == rc)
        {
            if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
            {
                if (buffer)
                    free(buffer);
                buffer = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(malloc(returnLength));
                if (!buffer)
                    return procCoreCount;
            }
            else
                return procCoreCount;
        }
        else done = true;
    }

    assert(buffer);
    _Analysis_assume_(buffer);

    DWORD byteOffset = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = buffer;
    while (byteOffset < returnLength)
    {
        if (RelationProcessorCore == ptr->Relationship)
        {
            if (ptr->ProcessorCore.Flags)
                procCoreCount += 1;                             // 하이퍼스레딩: 논리 프로세서가 같은 코어
            else
                procCoreCount += CountBits(ptr->ProcessorMask);
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }

    free(buffer);
    return procCoreCount;
}

//--------------------------------------------------------------------------------------
SquidRenderer::SquidRenderer()
    : vertexLayout(nullptr)
    , mirrorVertexLayout(nullptr)
    , deviceContextType(DEVICECONTEXT_IMMEDIATE)
    , wireFrame(false)
    , renderSceneLightPOV(false)
    , clearStateUponBeginCommandList(false)
    , clearStateUponFinishCommandList(false)
    , clearStateUponExecuteCommandList(false)
    , pCamera(nullptr)
    , nextAvailableChunkQueue(0)
    , numPerChunkRenderThreads(0)
{
    ZeroMemory(&staticParamsDirect, sizeof(staticParamsDirect));
    ZeroMemory(staticParamsShadow, sizeof(staticParamsShadow));
    ZeroMemory(staticParamsMirror, sizeof(staticParamsMirror));

    for (int i = 0; i < NumPerSceneRenderThreads; ++i)
    {
        perSceneThread[i] = nullptr;
        perSceneBeginEvent[i] = nullptr;
        perSceneEndEvent[i] = nullptr;
        perSceneDeferredContext[i] = nullptr;
        perSceneCommandList[i] = nullptr;
    }
    for (int i = 0; i < MaxPerChunkRenderThreads; ++i)
    {
        perChunkThread[i] = nullptr;
        perChunkBeginSemaphore[i] = nullptr;
        perChunkEndEvent[i] = nullptr;
        perChunkDeferredContext[i] = nullptr;
        perChunkCommandList[i] = nullptr;
        chunkQueueOffset[i] = 0;
    }
}

SquidRenderer::~SquidRenderer()
{
    Destroy();
}

//--------------------------------------------------------------------------------------
// 백 버퍼에 의존하지 않는 D3D11 리소스 생성
//--------------------------------------------------------------------------------------
HRESULT SquidRenderer::Create(ID3D11Device* pd3dDevice, ID3D11DeviceContext* /*immediateContext*/)
{
    HRESULT hr;

    Instance = this;

    // 셰이더 컴파일 및 생성
    if (!vertexShader.Create(pd3dDevice, L"shader\\MultithreadedRendering_VS.hlsl", "VSMain", "vs_4_0"))
        return E_FAIL;
    if (!pixelShader.Create(pd3dDevice, L"shader\\MultithreadedRendering_PS.hlsl", "PSMain", "ps_4_0"))
        return E_FAIL;

    // 정점 입력 레이아웃.
    // 콘텐츠 익스포터는 노멀/탄젠트를 압축/비압축 두 형식으로 지원한다. 메시는 압축 형식을,
    // 거울 사각형은 비압축 형식을 사용한다.
    const D3D11_INPUT_ELEMENT_DESC uncompressedLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    const D3D11_INPUT_ELEMENT_DESC compressedLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R10G10B10A2_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT,      0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R10G10B10A2_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (!vertexShader.CreateInputLayout(pd3dDevice, compressedLayout, ARRAYSIZE(compressedLayout), &vertexLayout))
        return E_FAIL;
    DXUT_SetDebugName(vertexLayout, "Compressed");

    if (!vertexShader.CreateInputLayout(pd3dDevice, uncompressedLayout, ARRAYSIZE(uncompressedLayout), &mirrorVertexLayout))
        return E_FAIL;
    DXUT_SetDebugName(mirrorVertexLayout, "Mirror");

    // 표준 깊이/스텐실 상태
    D3D11_DEPTH_STENCIL_DESC depthStencilDescNoStencil = {
        TRUE,                           // DepthEnable
        D3D11_DEPTH_WRITE_MASK_ALL,     // DepthWriteMask
        D3D11_COMPARISON_LESS_EQUAL,    // DepthFunc
        FALSE,                          // StencilEnable
        0, 0,                           // Stencil read/write mask
        { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_NEVER },
        { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_NEVER },
    };
    if (!dssNoStencil.Create(pd3dDevice, depthStencilDescNoStencil, "No Stencil"))
        return E_FAIL;

    // 메시가 청크를 그릴 때 우리 콜백을 거치게 하여, 서로 다른 청크를 서로 다른 컨텍스트로 분배한다.
    MDC_SDKMESH_CALLBACKS11 meshCallbacks = {};
    meshCallbacks.pRenderMesh = RenderMeshCallback;
    V_RETURN(mesh.Create(pd3dDevice, L"SquidRoom\\SquidRoom.sdkmesh", &meshCallbacks));

    // 샘플러: 그림자맵용 point/clamp, 그 외 linear/wrap
    D3D11_SAMPLER_DESC samDesc;
    samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samDesc.MipLODBias = 0.0f;
    samDesc.MaxAnisotropy = 1;
    samDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samDesc.BorderColor[0] = samDesc.BorderColor[1] = samDesc.BorderColor[2] = samDesc.BorderColor[3] = 0;
    samDesc.MinLOD = 0;
    samDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (!samPointClamp.Create(pd3dDevice, samDesc, "PointClamp"))
        return E_FAIL;

    samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    if (!samLinearWrap.Create(pd3dDevice, samDesc, "LinearWrap"))
        return E_FAIL;

    // 상수 버퍼
    if (!cbVSPerScene.Create(pd3dDevice, sizeof(CB_VS_PER_SCENE), "CB_VS_PER_SCENE"))   return E_FAIL;
    if (!cbVSPerObject.Create(pd3dDevice, sizeof(CB_VS_PER_OBJECT), "CB_VS_PER_OBJECT")) return E_FAIL;
    if (!cbPSPerScene.Create(pd3dDevice, sizeof(CB_PS_PER_SCENE), "CB_PS_PER_SCENE"))   return E_FAIL;
    if (!cbPSPerObject.Create(pd3dDevice, sizeof(CB_PS_PER_OBJECT), "CB_PS_PER_OBJECT")) return E_FAIL;
    if (!cbPSPerLight.Create(pd3dDevice, sizeof(CB_PS_PER_LIGHT), "CB_PS_PER_LIGHT"))   return E_FAIL;

    // 백페이스 컬링 상태들
    D3D11_RASTERIZER_DESC rasterizerDescNoCull = {
        D3D11_FILL_SOLID, D3D11_CULL_NONE, TRUE, 0, 0, 0, FALSE, FALSE, TRUE, FALSE,
    };
    if (!rsNoCull.Create(pd3dDevice, rasterizerDescNoCull, "NoCull"))
        return E_FAIL;

    rasterizerDescNoCull.FillMode = D3D11_FILL_WIREFRAME;
    if (!rsNoCullWireFrame.Create(pd3dDevice, rasterizerDescNoCull, "Wireframe"))
        return E_FAIL;

    D3D11_RASTERIZER_DESC rasterizerDescBackfaceCull = {
        D3D11_FILL_SOLID, D3D11_CULL_BACK, TRUE, 0, 0, 0, FALSE, FALSE, TRUE, FALSE,
    };
    if (!rsBackfaceCull.Create(pd3dDevice, rasterizerDescBackfaceCull, "BackfaceCull"))
        return E_FAIL;

    D3D11_RASTERIZER_DESC rasterizerDescFrontfaceCull = {
        D3D11_FILL_SOLID, D3D11_CULL_FRONT, TRUE, 0, 0, 0, FALSE, FALSE, TRUE, FALSE,
    };
    if (!rsFrontfaceCull.Create(pd3dDevice, rasterizerDescFrontfaceCull, "FrontfaceCull"))
        return E_FAIL;

    // 메인 씬용 정적 파라미터
    staticParamsDirect.DepthStencilState = dssNoStencil;
    staticParamsDirect.StencilRef = 0;
    staticParamsDirect.RasterizerState = rsFrontfaceCull;
    XMStoreFloat4(&staticParamsDirect.MirrorPlane, g_XMZero);
    XMStoreFloat4(&staticParamsDirect.TintColor, Colors::White);
    staticParamsDirect.DepthStencilView = nullptr;
    staticParamsDirect.Viewport = nullptr;

#ifdef DEBUG
    // 컨텍스트 간 상태 누수를 잡기 위한 플래그(성능 저하가 있지만 오류를 노출).
    clearStateUponBeginCommandList = true;
    clearStateUponFinishCommandList = true;
    clearStateUponExecuteCommandList = true;
#endif

    lights.Initialize(SceneCenter, SceneRadius);

    V_RETURN(InitializeShadows(pd3dDevice));
    V_RETURN(InitializeMirrors(pd3dDevice));
    V_RETURN(InitializeWorkerThreads(pd3dDevice));

    return S_OK;
}

//--------------------------------------------------------------------------------------
// 그림자용 리소스
//--------------------------------------------------------------------------------------
HRESULT SquidRenderer::InitializeShadows(ID3D11Device* pd3dDevice)
{
    for (int iShadow = 0; iShadow < NUM_SHADOWS; ++iShadow)
    {
        if (!shadows[iShadow].Create(pd3dDevice, 2048.0f, 2048.0f))
            return E_FAIL;

        staticParamsShadow[iShadow].DepthStencilState = dssNoStencil;
        staticParamsShadow[iShadow].StencilRef = 0;
        staticParamsShadow[iShadow].RasterizerState = rsFrontfaceCull;
        XMStoreFloat4(&staticParamsShadow[iShadow].MirrorPlane, g_XMZero);
        XMStoreFloat4(&staticParamsShadow[iShadow].TintColor, Colors::White);
        staticParamsShadow[iShadow].DepthStencilView = shadows[iShadow].GetDSV();
        staticParamsShadow[iShadow].Viewport = shadows[iShadow].GetViewport();
    }
    return S_OK;
}

//--------------------------------------------------------------------------------------
// 거울용 리소스 (스텐실 기법을 위한 여러 깊이/스텐실 상태 + 정점 버퍼)
//--------------------------------------------------------------------------------------
HRESULT SquidRenderer::InitializeMirrors(ID3D11Device* pd3dDevice)
{
    // 깊이 테스트 통과 시 스텐실 기록
    D3D11_DEPTH_STENCIL_DESC descDepthTestStencilOverwrite = {
        TRUE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_LESS_EQUAL,
        TRUE, 0, MIRROR_STENCIL_MASK,
        { D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_REPLACE, D3D11_COMPARISON_ALWAYS },
        { D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_REPLACE, D3D11_COMPARISON_ALWAYS },
    };
    if (!dssMirrorDepthTestStencilOverwrite.Create(pd3dDevice, descDepthTestStencilOverwrite, "Mirror SO"))
        return E_FAIL;

    // 스텐실 테스트 통과 시 깊이 덮어쓰기
    D3D11_DEPTH_STENCIL_DESC descDepthOverwriteStencilTest = {
        TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_ALWAYS,
        TRUE, MIRROR_STENCIL_MASK, 0,
        { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL },
        { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL },
    };
    if (!dssMirrorDepthOverwriteStencilTest.Create(pd3dDevice, descDepthOverwriteStencilTest, "Mirror DO"))
        return E_FAIL;

    // 스텐실 테스트 통과 시 일반 깊이 테스트/기록
    D3D11_DEPTH_STENCIL_DESC descDepthWriteStencilTest = {
        TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS_EQUAL,
        TRUE, MIRROR_STENCIL_MASK, 0,
        { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL },
        { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL },
    };
    if (!dssMirrorDepthWriteStencilTest.Create(pd3dDevice, descDepthWriteStencilTest, "Mirror Normal"))
        return E_FAIL;

    // 스텐실 테스트 통과 시 깊이 덮어쓰고 스텐실 초기화
    D3D11_DEPTH_STENCIL_DESC descDepthOverwriteStencilClear = {
        TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_ALWAYS,
        TRUE, MIRROR_STENCIL_MASK, MIRROR_STENCIL_MASK,
        { D3D11_STENCIL_OP_ZERO, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_ZERO, D3D11_COMPARISON_EQUAL },
        { D3D11_STENCIL_OP_ZERO, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_ZERO, D3D11_COMPARISON_EQUAL },
    };
    if (!dssMirrorDepthOverwriteStencilClear.Create(pd3dDevice, descDepthOverwriteStencilClear, "Mirror Clear"))
        return E_FAIL;

    // 거울 기하 계산
    mirrors.Initialize();

    // 거울 사각형용 동적 정점 버퍼
    if (!mirrorVertexBuffer.CreateDynamic(pd3dDevice, sizeof(MirrorRect), "Mirror VB"))
        return E_FAIL;

    for (int iMirror = 0; iMirror < NUM_MIRRORS; ++iMirror)
    {
        staticParamsMirror[iMirror].DepthStencilState = dssMirrorDepthWriteStencilTest;
        staticParamsMirror[iMirror].StencilRef = MIRROR_STENCIL_REF;
        staticParamsMirror[iMirror].RasterizerState = rsBackfaceCull;
        XMStoreFloat4(&staticParamsMirror[iMirror].MirrorPlane, mirrors.plane[iMirror]);
        XMStoreFloat4(&staticParamsMirror[iMirror].TintColor, MirrorTint);
        staticParamsMirror[iMirror].DepthStencilView = nullptr;
        staticParamsMirror[iMirror].Viewport = nullptr;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// 워커 스레드별 리소스 생성
//--------------------------------------------------------------------------------------
HRESULT SquidRenderer::InitializeWorkerThreads(ID3D11Device* pd3dDevice)
{
    HRESULT hr;

    // per-scene 초기화
    for (int iInstance = 0; iInstance < NumPerSceneRenderThreads; ++iInstance)
    {
        perSceneThreadParam[iInstance].renderer = this;
        perSceneThreadParam[iInstance].instance = iInstance;

        perSceneBeginEvent[iInstance] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        perSceneEndEvent[iInstance] = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        V_RETURN(pd3dDevice->CreateDeferredContext(0, &perSceneDeferredContext[iInstance]));

        perSceneThread[iInstance] = reinterpret_cast<HANDLE>(_beginthreadex(
            nullptr, 0, PerSceneThreadEntry, &perSceneThreadParam[iInstance], CREATE_SUSPENDED, nullptr));

#if defined(PROFILE) || defined(DEBUG)
        char threadid[16];
        sprintf_s(threadid, sizeof(threadid), "PS %d", iInstance);
        DXUT_SetDebugName(perSceneDeferredContext[iInstance], threadid);
#endif

        ResumeThread(perSceneThread[iInstance]);
    }

    // per-chunk 초기화: 메인 스레드용으로 코어 하나를 남긴다.
    numPerChunkRenderThreads = GetPhysicalProcessorCount() - 1;
    numPerChunkRenderThreads = std::min(numPerChunkRenderThreads, MaxPerChunkRenderThreads);
    numPerChunkRenderThreads = std::max(numPerChunkRenderThreads, 1);

    for (int iInstance = 0; iInstance < numPerChunkRenderThreads; ++iInstance)
    {
        perChunkThreadParam[iInstance].renderer = this;
        perChunkThreadParam[iInstance].instance = iInstance;

        perChunkBeginSemaphore[iInstance] = CreateSemaphore(nullptr, 0, MaxPendingQueueEntries, nullptr);
        perChunkEndEvent[iInstance] = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        V_RETURN(pd3dDevice->CreateDeferredContext(0, &perChunkDeferredContext[iInstance]));

        perChunkThread[iInstance] = reinterpret_cast<HANDLE>(_beginthreadex(
            nullptr, 0, PerChunkThreadEntry, &perChunkThreadParam[iInstance], CREATE_SUSPENDED, nullptr));

#if defined(PROFILE) || defined(DEBUG)
        char threadid[16];
        sprintf_s(threadid, sizeof(threadid), "PC %d", iInstance);
        DXUT_SetDebugName(perChunkDeferredContext[iInstance], threadid);
#endif

        ResumeThread(perChunkThread[iInstance]);
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// 항상 일반 DXUT 경로로 그리는 RenderMesh. per-object 상수 버퍼를 설정한다.
//--------------------------------------------------------------------------------------
void SquidRenderer::RenderMeshDirect(ID3D11DeviceContext* pd3dContext, UINT iMesh)
{
    HRESULT hr = S_OK;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    XMMATRIX id = XMMatrixIdentity();

    V(pd3dContext->Map(cbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
    auto pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>(mappedResource.pData);
    XMStoreFloat4x4(&pVSPerObject->World, id);
    pd3dContext->Unmap(cbVSPerObject, 0);

    pd3dContext->VSSetConstantBuffers(CBVSPerObjectBind, 1, cbVSPerObject.GetAddressOf());

    V(pd3dContext->Map(cbPSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
    auto pPSPerObject = reinterpret_cast<CB_PS_PER_OBJECT*>(mappedResource.pData);
    XMStoreFloat4(&pPSPerObject->ObjectColor, Colors::White);
    pd3dContext->Unmap(cbPSPerObject, 0);

    pd3dContext->PSSetConstantBuffers(CBPSPerObjectBind, 1, cbPSPerObject.GetAddressOf());

    mesh.RenderMesh(iMesh, false, pd3dContext, 0, 1, INVALID_SAMPLER_SLOT);
}

//--------------------------------------------------------------------------------------
// 메시가 청크를 그릴 때 호출되는 실제 처리. 렌더 경로에 따라 다른 컨텍스트/스레드로 분배한다.
//--------------------------------------------------------------------------------------
void SquidRenderer::OnRenderMeshCallback(UINT iMesh, ID3D11DeviceContext* pd3dDeviceContext)
{
    if (IsRenderMultithreadedPerChunk())
    {
        // 워커 큐 항목 생성·제출
        ChunkQueue& workerQueue = chunkQueue[nextAvailableChunkQueue];
        int iQueueOffset = chunkQueueOffset[nextAvailableChunkQueue];
        HANDLE hSemaphore = perChunkBeginSemaphore[nextAvailableChunkQueue];

        chunkQueueOffset[nextAvailableChunkQueue] += sizeof(WorkQueueEntryChunk);
        assert(chunkQueueOffset[nextAvailableChunkQueue] < SceneQueueSizeInBytes);

        auto pEntry = reinterpret_cast<WorkQueueEntryChunk*>(&workerQueue[iQueueOffset]);
        pEntry->Type = WORK_QUEUE_ENTRY_TYPE_CHUNK;
        pEntry->Mesh = iMesh;

        ReleaseSemaphore(hSemaphore, 1, nullptr);
    }
    else if (IsRenderDeferredPerChunk())
    {
        ID3D11DeviceContext* pd3dDeferredContext = perChunkDeferredContext[nextAvailableChunkQueue];
        RenderMeshDirect(pd3dDeferredContext, iMesh);
    }
    else
    {
        RenderMeshDirect(pd3dDeviceContext, iMesh);
    }

    nextAvailableChunkQueue = ++nextAvailableChunkQueue % numPerChunkRenderThreads;
}

//--------------------------------------------------------------------------------------
// per-scene d3d 컨텍스트 셋업. 완전히 새 컨텍스트에서 시작해 RenderMesh 를 호출할 수 있을 만큼 설정.
//--------------------------------------------------------------------------------------
HRESULT SquidRenderer::RenderSceneSetup(ID3D11DeviceContext* pd3dContext, const SceneParamsStatic* pStaticParams,
                                         const SceneParamsDynamic* pDynamicParams)
{
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    bool bShadow = (nullptr != pStaticParams->DepthStencilView);

    if (bShadow)
    {
        // 그림자 맵을 텍스처가 아닌 depth-stencil 로 사용
        ID3D11ShaderResourceView* ppNullResources[NUM_SHADOWS] = { nullptr };
        pd3dContext->PSSetShaderResources(2, NUM_SHADOWS, ppNullResources);

        pd3dContext->RSSetViewports(1, pStaticParams->Viewport);
        pd3dContext->OMSetRenderTargets(0, nullptr, pStaticParams->DepthStencilView);
    }
    else
    {
        V(DXUTSetupD3D11Views(pd3dContext));

        // 모든 그림자 맵을 텍스처로 바인딩
        ID3D11ShaderResourceView* ppShadowResources[NUM_SHADOWS];
        for (int i = 0; i < NUM_SHADOWS; ++i)
            ppShadowResources[i] = shadows[i].GetSRV();
        pd3dContext->PSSetShaderResources(2, NUM_SHADOWS, ppShadowResources);
    }

    pd3dContext->OMSetDepthStencilState(pStaticParams->DepthStencilState, pStaticParams->StencilRef);
    pd3dContext->RSSetState(wireFrame ? (ID3D11RasterizerState*)rsNoCullWireFrame : pStaticParams->RasterizerState);

    pd3dContext->VSSetShader(vertexShader, nullptr, 0);
    pd3dContext->IASetInputLayout(vertexLayout);

    // VS per-scene 상수 데이터
    V(pd3dContext->Map(cbVSPerScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
    auto pVSPerScene = reinterpret_cast<CB_VS_PER_SCENE*>(mappedResource.pData);
    XMMATRIX mvp = XMLoadFloat4x4(&pDynamicParams->ViewProj);
    XMStoreFloat4x4(&pVSPerScene->ViewProj, XMMatrixTranspose(mvp));
    pd3dContext->Unmap(cbVSPerScene, 0);

    pd3dContext->VSSetConstantBuffers(CBVSPerSceneBind, 1, cbVSPerScene.GetAddressOf());

    if (bShadow)
    {
        pd3dContext->PSSetShader(nullptr, nullptr, 0);
    }
    else
    {
        pd3dContext->PSSetShader(pixelShader, nullptr, 0);

        ID3D11SamplerState* ppSamplerStates[2] = { samPointClamp, samLinearWrap };
        pd3dContext->PSSetSamplers(0, 2, ppSamplerStates);

        // PS per-scene 상수 데이터 (유저 클립 평면으로 거울 뒤쪽 물체를 클립)
        V(pd3dContext->Map(cbPSPerScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
        auto pPSPerScene = reinterpret_cast<CB_PS_PER_SCENE*>(mappedResource.pData);
        pPSPerScene->MirrorPlane = pStaticParams->MirrorPlane;
        XMStoreFloat4(&pPSPerScene->AmbientColor, AmbientColor);
        pPSPerScene->TintColor = pStaticParams->TintColor;
        pd3dContext->Unmap(cbPSPerScene, 0);

        pd3dContext->PSSetConstantBuffers(CBPSPerSceneBind, 1, cbPSPerScene.GetAddressOf());

        // PS per-light 상수 데이터
        V(pd3dContext->Map(cbPSPerLight, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
        auto pPSPerLight = reinterpret_cast<CB_PS_PER_LIGHT*>(mappedResource.pData);
        for (int iLight = 0; iLight < NUM_LIGHTS; ++iLight)
        {
            XMVECTOR vLightPos = XMVectorSetW(lights.pos[iLight], 1.0f);
            XMVECTOR vLightDir = XMVectorSetW(lights.dir[iLight], 0.0f);
            XMMATRIX mLightViewProj = lights.CalcLightViewProj(iLight);

            pPSPerLight->LightData[iLight].LightColor = lights.color[iLight];
            XMStoreFloat4(&pPSPerLight->LightData[iLight].LightPos, vLightPos);
            XMStoreFloat4(&pPSPerLight->LightData[iLight].LightDir, vLightDir);
            XMStoreFloat4x4(&pPSPerLight->LightData[iLight].LightViewProj, XMMatrixTranspose(mLightViewProj));
            pPSPerLight->LightData[iLight].Falloffs = XMFLOAT4(
                lights.falloffDistEnd[iLight],
                lights.falloffDistRange[iLight],
                lights.falloffCosAngleEnd[iLight],
                lights.falloffCosAngleRange[iLight]);
        }
        pd3dContext->Unmap(cbPSPerLight, 0);

        pd3dContext->PSSetConstantBuffers(CBPSPerLightBind, 1, cbPSPerLight.GetAddressOf());
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// 즉시/디퍼드(메인 또는 워커 스레드) 컨텍스트에서 씬 하나를 렌더링한다.
//--------------------------------------------------------------------------------------
HRESULT SquidRenderer::RenderScene(ID3D11DeviceContext* pd3dContext, const SceneParamsStatic* pStaticParams,
                                    const SceneParamsDynamic* pDynamicParams)
{
    HRESULT hr = S_OK;

    if (clearStateUponBeginCommandList)
        pd3dContext->ClearState();

    // 그림자 버퍼 클리어
    if (pStaticParams->DepthStencilView)
    {
        pd3dContext->ClearDepthStencilView(pStaticParams->DepthStencilView,
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0, 0);
    }

    // 사용할 모든 d3d 컨텍스트에 씬 셋업 수행
    if (IsRenderMultithreadedPerChunk())
    {
        for (int iInstance = 0; iInstance < numPerChunkRenderThreads; ++iInstance)
        {
            chunkQueueOffset[iInstance] = 0;

            ChunkQueue& workerQueue = chunkQueue[iInstance];
            int iQueueOffset = chunkQueueOffset[iInstance];
            HANDLE hSemaphore = perChunkBeginSemaphore[iInstance];

            chunkQueueOffset[iInstance] += sizeof(WorkQueueEntrySetup);
            assert(chunkQueueOffset[iInstance] < SceneQueueSizeInBytes);

            auto pEntry = reinterpret_cast<WorkQueueEntrySetup*>(&workerQueue[iQueueOffset]);
            pEntry->Type = WORK_QUEUE_ENTRY_TYPE_SETUP;
            pEntry->SceneParamsStatic = pStaticParams;    // 얕은 복사
            pEntry->SceneParamsDynamic = *pDynamicParams;  // 깊은 복사

            ReleaseSemaphore(hSemaphore, 1, nullptr);
        }
    }
    else if (IsRenderDeferredPerChunk())
    {
        for (int iInstance = 0; iInstance < numPerChunkRenderThreads; ++iInstance)
        {
            ID3D11DeviceContext* pd3dDeferredContext = perChunkDeferredContext[iInstance];
            V(RenderSceneSetup(pd3dDeferredContext, pStaticParams, pDynamicParams));
        }
    }
    else
    {
        V(RenderSceneSetup(pd3dContext, pStaticParams, pDynamicParams));
    }

    // 렌더
    mesh.Render(pd3dContext, 0, 1);

    // per-chunk 경로면 지금 커맨드 리스트를 생성·실행한다.
    if (IsRenderDeferredPerChunk())
    {
        if (IsRenderMultithreadedPerChunk())
        {
            // 모든 워커 스레드에게 커맨드 리스트 마무리를 지시
            for (int iInstance = 0; iInstance < numPerChunkRenderThreads; ++iInstance)
            {
                ChunkQueue& workerQueue = chunkQueue[iInstance];
                int iQueueOffset = chunkQueueOffset[iInstance];
                HANDLE hSemaphore = perChunkBeginSemaphore[iInstance];

                chunkQueueOffset[iInstance] += sizeof(WorkQueueEntryFinalize);
                assert(chunkQueueOffset[iInstance] < SceneQueueSizeInBytes);

                auto pEntry = reinterpret_cast<WorkQueueEntryFinalize*>(&workerQueue[iQueueOffset]);
                pEntry->Type = WORK_QUEUE_ENTRY_TYPE_FINALIZE;

                ReleaseSemaphore(hSemaphore, 1, nullptr);
            }

            WaitForMultipleObjects(numPerChunkRenderThreads, perChunkEndEvent, TRUE, INFINITE);
        }
        else
        {
            for (int iInstance = 0; iInstance < numPerChunkRenderThreads; ++iInstance)
            {
                V(perChunkDeferredContext[iInstance]->FinishCommandList(
                    !clearStateUponFinishCommandList, &perChunkCommandList[iInstance]));
            }
        }

        // 모든 커맨드 리스트 실행 (렌더 순서는 흩어진다)
        for (int iInstance = 0; iInstance < numPerChunkRenderThreads; ++iInstance)
        {
            pd3dContext->ExecuteCommandList(perChunkCommandList[iInstance], !clearStateUponExecuteCommandList);
            SAFE_RELEASE(perChunkCommandList[iInstance]);
        }
    }
    else
    {
        if (clearStateUponFinishCommandList || clearStateUponExecuteCommandList)
            pd3dContext->ClearState();
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// 그림자 맵 렌더
//--------------------------------------------------------------------------------------
void SquidRenderer::RenderShadow(int iShadow, ID3D11DeviceContext* pd3dContext)
{
    HRESULT hr;

    XMMATRIX m = lights.CalcLightViewProj(iShadow);

    SceneParamsDynamic dynamicParams;
    XMStoreFloat4x4(&dynamicParams.ViewProj, m);

    V(RenderScene(pd3dContext, &staticParamsShadow[iShadow], &dynamicParams));
}

//--------------------------------------------------------------------------------------
// 거울 사각형을 스텐실 버퍼에 그린 뒤, 반사 투영으로 스텐실 영역에 월드를 그린다.
//--------------------------------------------------------------------------------------
void SquidRenderer::RenderMirror(int iMirror, ID3D11DeviceContext* pd3dContext)
{
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    XMVECTOR vEyePoint;
    XMMATRIX mViewProj;

    if (renderSceneLightPOV)
    {
        vEyePoint = lights.pos[0];
        mViewProj = lights.CalcLightViewProj(0);
    }
    else
    {
        vEyePoint = pCamera->GetEyePt();
        mViewProj = pCamera->GetViewMatrix() * pCamera->GetProjMatrix();
    }

    // 뒷면 거울이면(현재 시점 기준) 건너뛴다
    if (XMVectorGetX(XMPlaneDotCoord(mirrors.plane[iMirror], vEyePoint)) < 0.0f)
        return;

    XMMATRIX mReflect = XMMatrixReflect(mirrors.plane[iMirror]);

    // 거울 로컬→월드 행렬
    XMVECTOR vMirrorPointAt = XMVectorAdd(mirrors.normal[iMirror], mirrors.center[iMirror]);
    XMMATRIX mMirrorWorld = XMMatrixLookAtLH(vMirrorPointAt, mirrors.center[iMirror], g_XMIdentityR1);
    mMirrorWorld = XMMatrixTranspose(mMirrorWorld);
    mMirrorWorld.r[0] = XMVectorSetW(mMirrorWorld.r[0], 0.f);
    mMirrorWorld.r[1] = XMVectorSetW(mMirrorWorld.r[1], 0.f);
    mMirrorWorld.r[2] = XMVectorSetW(mMirrorWorld.r[2], 0.f);
    mMirrorWorld.r[3] = XMVectorSetW(mirrors.center[iMirror], 1.f);

    if (clearStateUponBeginCommandList)
        pd3dContext->ClearState();

    DXUTSetupD3D11Views(pd3dContext);

    //--- 거울 사각형을 스텐실 버퍼에 그리며 스텐실 참조값 설정 ---
    pd3dContext->OMSetDepthStencilState(dssMirrorDepthTestStencilOverwrite, MIRROR_STENCIL_REF);
    pd3dContext->RSSetState(rsBackfaceCull);

    pd3dContext->IASetInputLayout(mirrorVertexLayout);
    pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11Buffer* pVB[1] = { mirrorVertexBuffer };
    UINT pStride[1] = { sizeof(MirrorVertex) };
    UINT pOffset[1] = { 0 };
    pd3dContext->IASetVertexBuffers(0, 1, pVB, pStride, pOffset);

    pd3dContext->VSSetShader(vertexShader, nullptr, 0);
    pd3dContext->PSSetShader(nullptr, nullptr, 0);

    V(pd3dContext->Map(mirrorVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
    memcpy(mappedResource.pData, mirrors.rect[iMirror], sizeof(mirrors.rect[iMirror]));
    pd3dContext->Unmap(mirrorVertexBuffer, 0);

    V(pd3dContext->Map(cbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
    auto pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>(mappedResource.pData);
    XMStoreFloat4x4(&pVSPerObject->World, XMMatrixTranspose(mMirrorWorld));
    pd3dContext->Unmap(cbVSPerObject, 0);

    pd3dContext->VSSetConstantBuffers(CBVSPerObjectBind, 1, cbVSPerObject.GetAddressOf());

    V(pd3dContext->Map(cbVSPerScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
    auto pVSPerScene = reinterpret_cast<CB_VS_PER_SCENE*>(mappedResource.pData);
    XMStoreFloat4x4(&pVSPerScene->ViewProj, XMMatrixTranspose(mViewProj));
    pd3dContext->Unmap(cbVSPerScene, 0);

    pd3dContext->VSSetConstantBuffers(CBVSPerSceneBind, 1, cbVSPerScene.GetAddressOf());

    pd3dContext->Draw(4, 0);

    //--- 스텐실 영역 내에서만 깊이 클리어 ---
    pd3dContext->OMSetDepthStencilState(dssMirrorDepthOverwriteStencilTest, MIRROR_STENCIL_REF);

    XMFLOAT4X4 mvp4x4;
    XMStoreFloat4x4(&mvp4x4, mViewProj);

    V(pd3dContext->Map(cbVSPerScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
    pVSPerScene = reinterpret_cast<CB_VS_PER_SCENE*>(mappedResource.pData);
    XMStoreFloat4x4(&pVSPerScene->ViewProj, XMMatrixTranspose(mViewProj));
    pVSPerScene->ViewProj._31 = mvp4x4._14;
    pVSPerScene->ViewProj._32 = mvp4x4._24;
    pVSPerScene->ViewProj._33 = mvp4x4._34;
    pVSPerScene->ViewProj._34 = mvp4x4._44;
    pd3dContext->Unmap(cbVSPerScene, 0);

    pd3dContext->Draw(4, 0);

    //--- 스텐실 영역에 반사된 월드를 그린다 ---
    XMMATRIX mvp = mReflect * mViewProj;
    SceneParamsDynamic dynamicParams;
    XMStoreFloat4x4(&dynamicParams.ViewProj, mvp);

    V(RenderScene(pd3dContext, &staticParamsMirror[iMirror], &dynamicParams));

    //--- 거울 사각형 위 스텐실 비트를 0으로 지우고, 동시에 깊이를 거울 깊이로 설정 ---
    V(DXUTSetupD3D11Views(pd3dContext));

    pd3dContext->OMSetDepthStencilState(dssMirrorDepthOverwriteStencilClear, MIRROR_STENCIL_REF);
    pd3dContext->RSSetState(rsBackfaceCull);

    pd3dContext->IASetInputLayout(mirrorVertexLayout);
    pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    pd3dContext->IASetVertexBuffers(0, 1, pVB, pStride, pOffset);

    pd3dContext->VSSetShader(vertexShader, nullptr, 0);
    pd3dContext->PSSetShader(nullptr, nullptr, 0);

    V(pd3dContext->Map(cbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
    pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>(mappedResource.pData);
    XMStoreFloat4x4(&pVSPerObject->World, XMMatrixTranspose(mMirrorWorld));
    pd3dContext->Unmap(cbVSPerObject, 0);

    pd3dContext->VSSetConstantBuffers(CBVSPerObjectBind, 1, cbVSPerObject.GetAddressOf());

    V(pd3dContext->Map(cbVSPerScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
    pVSPerScene = reinterpret_cast<CB_VS_PER_SCENE*>(mappedResource.pData);
    XMStoreFloat4x4(&pVSPerScene->ViewProj, XMMatrixTranspose(mViewProj));
    pd3dContext->Unmap(cbVSPerScene, 0);

    pd3dContext->VSSetConstantBuffers(CBVSPerSceneBind, 1, cbVSPerScene.GetAddressOf());

    pd3dContext->Draw(4, 0);
}

//--------------------------------------------------------------------------------------
// 거울/그림자가 아닌 메인 월드로 씬을 렌더
//--------------------------------------------------------------------------------------
void SquidRenderer::RenderSceneDirect(ID3D11DeviceContext* pd3dContext)
{
    HRESULT hr;

    XMMATRIX mvp;
    if (renderSceneLightPOV)
        mvp = lights.CalcLightViewProj(0);
    else
        mvp = pCamera->GetViewMatrix() * pCamera->GetProjMatrix();

    SceneParamsDynamic dynamicParams;
    XMStoreFloat4x4(&dynamicParams.ViewProj, mvp);

    V(RenderScene(pd3dContext, &staticParamsDirect, &dynamicParams));
}

//--------------------------------------------------------------------------------------
// per-scene 워커 스레드 루프
//--------------------------------------------------------------------------------------
void SquidRenderer::PerSceneThreadProc(int iInstance)
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dDeferredContext = perSceneDeferredContext[iInstance];
    ID3D11CommandList*& pd3dCommandList = perSceneCommandList[iInstance];

    for (;;)
    {
        WaitForSingleObject(perSceneBeginEvent[iInstance], INFINITE);

        if (clearStateUponBeginCommandList)
            pd3dDeferredContext->ClearState();

        if (iInstance < NUM_SHADOWS)
            RenderShadow(iInstance, pd3dDeferredContext);
        else if (iInstance < NUM_SHADOWS + NUM_MIRRORS)
            RenderMirror(iInstance - NUM_SHADOWS, pd3dDeferredContext);
        else
            RenderSceneDirect(pd3dDeferredContext);

        V(pd3dDeferredContext->FinishCommandList(!clearStateUponFinishCommandList, &pd3dCommandList));

        SetEvent(perSceneEndEvent[iInstance]);
    }
}

//--------------------------------------------------------------------------------------
// per-chunk 워커 스레드 루프
//--------------------------------------------------------------------------------------
void SquidRenderer::PerChunkThreadProc(int iInstance)
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dDeferredContext = perChunkDeferredContext[iInstance];
    ID3D11CommandList*& pd3dCommandList = perChunkCommandList[iInstance];
    const ChunkQueue& localQueue = chunkQueue[iInstance];

    int iQueueOffset = 0;

    for (;;)
    {
        WaitForSingleObject(perChunkBeginSemaphore[iInstance], INFINITE);

        assert(iQueueOffset < SceneQueueSizeInBytes);
        auto pEntry = reinterpret_cast<const WorkQueueEntryBase*>(&localQueue[iQueueOffset]);

        switch (pEntry->Type)
        {
        case WORK_QUEUE_ENTRY_TYPE_SETUP:
            {
                auto pSetupEntry = reinterpret_cast<const WorkQueueEntrySetup*>(pEntry);

                if (clearStateUponBeginCommandList)
                    pd3dDeferredContext->ClearState();

                V(RenderSceneSetup(pd3dDeferredContext, pSetupEntry->SceneParamsStatic,
                    &pSetupEntry->SceneParamsDynamic));

                iQueueOffset += sizeof(WorkQueueEntrySetup);
                break;
            }

        case WORK_QUEUE_ENTRY_TYPE_CHUNK:
            {
                auto pChunkEntry = reinterpret_cast<const WorkQueueEntryChunk*>(pEntry);

                RenderMeshDirect(pd3dDeferredContext, pChunkEntry->Mesh);

                iQueueOffset += sizeof(WorkQueueEntryChunk);
                break;
            }

        case WORK_QUEUE_ENTRY_TYPE_FINALIZE:
            {
                V(pd3dDeferredContext->FinishCommandList(!clearStateUponFinishCommandList, &pd3dCommandList));

                SetEvent(perChunkEndEvent[iInstance]);

                iQueueOffset += sizeof(WorkQueueEntryFinalize);
                iQueueOffset = 0;   // 큐 리셋
                break;
            }

        default:
            assert(false);
            break;
        }
    }
}

//--------------------------------------------------------------------------------------
// 정적 진입점 / 콜백
//--------------------------------------------------------------------------------------
unsigned int WINAPI SquidRenderer::PerSceneThreadEntry(LPVOID param)
{
    auto p = reinterpret_cast<ThreadParam*>(param);
    p->renderer->PerSceneThreadProc(p->instance);
}

unsigned int WINAPI SquidRenderer::PerChunkThreadEntry(LPVOID param)
{
    auto p = reinterpret_cast<ThreadParam*>(param);
    p->renderer->PerChunkThreadProc(p->instance);
}

void SquidRenderer::RenderMeshCallback(CMultiDeviceContextDXUTMesh* /*pMesh*/, UINT iMesh, bool /*bAdjacent*/,
                                        ID3D11DeviceContext* pd3dDeviceContext, UINT /*iDiffuseSlot*/,
                                        UINT /*iNormalSlot*/, UINT /*iSpecularSlot*/)
{
    Instance->OnRenderMeshCallback(iMesh, pd3dDeviceContext);
}

//--------------------------------------------------------------------------------------
// 프레임 시작 시 상태 초기화 (설정 대화상자 렌더보다 먼저)
//--------------------------------------------------------------------------------------
void SquidRenderer::BeginFrame(ID3D11DeviceContext* pd3dImmediateContext)
{
    HRESULT hr;
    if (clearStateUponBeginCommandList)
    {
        pd3dImmediateContext->ClearState();
        V(DXUTSetupD3D11Views(pd3dImmediateContext));
    }
}

//--------------------------------------------------------------------------------------
// 한 프레임 씬 렌더링
//--------------------------------------------------------------------------------------
void SquidRenderer::Render(ID3D11DeviceContext* pd3dImmediateContext, const CModelViewerCamera& camera)
{
    HRESULT hr;

    pCamera = &camera;

    pd3dImmediateContext->ClearRenderTargetView(DXUTGetD3D11RenderTargetView(), Colors::MidnightBlue);
    pd3dImmediateContext->ClearDepthStencilView(DXUTGetD3D11DepthStencilView(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0, 0);

    if (IsRenderMultithreadedPerScene())
    {
        // 워커 스레드 전부 신호 후 완료 대기
        for (int iInstance = 0; iInstance < NumPerSceneRenderThreads; ++iInstance)
            SetEvent(perSceneBeginEvent[iInstance]);

        WaitForMultipleObjects(NumPerSceneRenderThreads, perSceneEndEvent, TRUE, INFINITE);
    }
    else if (IsRenderDeferredPerScene())
    {
        // 동일 작업을 메인 스레드에서 직렬화하되 디퍼드 컨텍스트 사용
        for (int iShadow = 0; iShadow < NUM_SHADOWS; ++iShadow)
        {
            RenderShadow(iShadow, perSceneDeferredContext[iShadow]);
            V(perSceneDeferredContext[iShadow]->FinishCommandList(
                !clearStateUponFinishCommandList, &perSceneCommandList[iShadow]));
        }

        for (int iMirror = 0; iMirror < NUM_MIRRORS; ++iMirror)
        {
            RenderMirror(iMirror, perSceneDeferredContext[iMirror]);
            V(perSceneDeferredContext[iMirror]->FinishCommandList(
                !clearStateUponFinishCommandList, &perSceneCommandList[NUM_SHADOWS + iMirror]));
        }

        RenderSceneDirect(perSceneDeferredContext[NUM_MIRRORS]);
        V(perSceneDeferredContext[NUM_MIRRORS]->FinishCommandList(
            !clearStateUponFinishCommandList, &perSceneCommandList[NUM_SHADOWS + NUM_MIRRORS]));
    }
    else
    {
        // 즉시 컨텍스트로 직렬 처리
        for (int iShadow = 0; iShadow < NUM_SHADOWS; ++iShadow)
            RenderShadow(iShadow, pd3dImmediateContext);

        for (int iMirror = 0; iMirror < NUM_MIRRORS; ++iMirror)
            RenderMirror(iMirror, pd3dImmediateContext);

        RenderSceneDirect(pd3dImmediateContext);
    }

    // per-scene 경로면 생성된 커맨드 리스트를 실행
    if (IsRenderDeferredPerScene())
    {
        for (int iInstance = 0; iInstance < NumPerSceneRenderThreads; ++iInstance)
        {
            pd3dImmediateContext->ExecuteCommandList(perSceneCommandList[iInstance], !clearStateUponExecuteCommandList);
            SAFE_RELEASE(perSceneCommandList[iInstance]);
        }
    }
    else
    {
        if (clearStateUponFinishCommandList || clearStateUponExecuteCommandList)
            pd3dImmediateContext->ClearState();
    }

    // 이후 HUD 렌더를 위해 뷰를 다시 설정
    V(DXUTSetupD3D11Views(pd3dImmediateContext));
}

//--------------------------------------------------------------------------------------
// 리소스 해제
//--------------------------------------------------------------------------------------
void SquidRenderer::Destroy()
{
    for (int iInstance = 0; iInstance < NumPerSceneRenderThreads; ++iInstance)
    {
        if (perSceneThread[iInstance])     { CloseHandle(perSceneThread[iInstance]);     perSceneThread[iInstance] = nullptr; }
        if (perSceneEndEvent[iInstance])   { CloseHandle(perSceneEndEvent[iInstance]);   perSceneEndEvent[iInstance] = nullptr; }
        if (perSceneBeginEvent[iInstance]) { CloseHandle(perSceneBeginEvent[iInstance]); perSceneBeginEvent[iInstance] = nullptr; }
        SAFE_RELEASE(perSceneDeferredContext[iInstance]);
    }

    for (int iInstance = 0; iInstance < numPerChunkRenderThreads; ++iInstance)
    {
        if (perChunkThread[iInstance])         { CloseHandle(perChunkThread[iInstance]);         perChunkThread[iInstance] = nullptr; }
        if (perChunkEndEvent[iInstance])       { CloseHandle(perChunkEndEvent[iInstance]);       perChunkEndEvent[iInstance] = nullptr; }
        if (perChunkBeginSemaphore[iInstance]) { CloseHandle(perChunkBeginSemaphore[iInstance]); perChunkBeginSemaphore[iInstance] = nullptr; }
        SAFE_RELEASE(perChunkDeferredContext[iInstance]);
    }
    numPerChunkRenderThreads = 0;

    mesh.Destroy();

    SAFE_RELEASE(vertexLayout);
    SAFE_RELEASE(mirrorVertexLayout);

    vertexShader.Destroy();
    pixelShader.Destroy();

    samPointClamp.Destroy();
    samLinearWrap.Destroy();

    rsNoCull.Destroy();
    rsBackfaceCull.Destroy();
    rsFrontfaceCull.Destroy();
    rsNoCullWireFrame.Destroy();

    dssNoStencil.Destroy();
    dssMirrorDepthTestStencilOverwrite.Destroy();
    dssMirrorDepthOverwriteStencilTest.Destroy();
    dssMirrorDepthWriteStencilTest.Destroy();
    dssMirrorDepthOverwriteStencilClear.Destroy();

    cbVSPerObject.Destroy();
    cbVSPerScene.Destroy();
    cbPSPerObject.Destroy();
    cbPSPerLight.Destroy();
    cbPSPerScene.Destroy();

    for (int iShadow = 0; iShadow < NUM_SHADOWS; ++iShadow)
        shadows[iShadow].Destroy();

    mirrorVertexBuffer.Destroy();
}
