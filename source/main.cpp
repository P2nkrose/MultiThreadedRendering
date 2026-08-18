//--------------------------------------------------------------------------------------
// File: main.cpp
//
// Direct3D 11 디퍼드 컨텍스트를 이용한 멀티스레드 렌더링
// DXUT 진입점 / 콜백 / GUI 만 담당하고, 실제 렌더링은 SquidRenderer 에 위임한다.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "resource.h"
#include "SDKmisc.h"

#include "SquidRenderer.h"
#include "RenderConstants.h"

#pragma warning(disable : 4100)

using namespace DirectX;

//--------------------------------------------------------------------------------------
// UI 컨트롤 ID
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN                    1
#define IDC_TOGGLEWIRE                          5
#define IDC_DEVICECONTEXT_GROUP                 6
#define IDC_DEVICECONTEXT_IMMEDIATE             7
#define IDC_DEVICECONTEXT_ST_DEFERRED_PER_SCENE 8
#define IDC_DEVICECONTEXT_MT_DEFERRED_PER_SCENE 9
#define IDC_DEVICECONTEXT_ST_DEFERRED_PER_CHUNK 10
#define IDC_DEVICECONTEXT_MT_DEFERRED_PER_CHUNK 11

//--------------------------------------------------------------------------------------
// 전역 변수
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager  DialogResourceManager;     // 대화상자 공유 리소스 관리자
CD3DSettingsDlg             D3DSettingsDlg;            // 디바이스 설정 대화상자
CDXUTDialog                 HUD;                       // HUD
CDXUTDialog                 SampleUI;                  // 샘플 전용 컨트롤
CDXUTTextHelper*            TxtHelper = nullptr;

CModelViewerCamera          Camera;
SquidRenderer               Renderer;

// 기본 뷰 파라미터
static const XMVECTORF32    DefaultEye          = { 30.0f, 150.0f, -150.0f, 0.f };
static const XMVECTORF32    DefaultLookAt       = { 0.0f, 60.0f, 0.0f, 0.f };
static const FLOAT          NearPlane           = 2.0f;
static const FLOAT          FarPlane            = 4000.0f;
static const FLOAT          FOV                 = XM_PI / 4.0f;
static const FLOAT          DefaultCameraRadius = 600.0f;
static const FLOAT          MinCameraRadius     = 150.0f;
static const FLOAT          MaxCameraRadius     = 700.0f;

//--------------------------------------------------------------------------------------
// 전방 선언
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext);
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext);
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext);
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext);

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext);
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext);
void CALLBACK OnD3D11DestroyDevice(void* pUserContext);
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext);

void InitApp();
void RenderText();

//--------------------------------------------------------------------------------------
// 프로그램 진입점
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackFrameMove(OnFrameMove);

    DXUTSetCallbackD3D11DeviceAcceptable(IsD3D11DeviceAcceptable);
    DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
    DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
    DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
    DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
    DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);

    InitApp();
    DXUTInit(true, true, lpCmdLine);
    DXUTSetCursorSettings(true, true);
    DXUTCreateWindow(L"MultithreadedRendering");
    DXUTCreateDevice(D3D_FEATURE_LEVEL_10_0, true, 800, 600);
    DXUTMainLoop();

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
void InitApp()
{
    D3DSettingsDlg.Init(&DialogResourceManager);
    HUD.Init(&DialogResourceManager);
    SampleUI.Init(&DialogResourceManager);

    HUD.SetCallback(OnGUIEvent);
    int iY = 30;
    int iYo = 26;
    HUD.AddButton(IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 22);
    HUD.AddButton(IDC_TOGGLEWIRE, L"Toggle Wires (F6)", 0, iY += iYo, 170, 22, VK_F6);
    HUD.AddRadioButton(IDC_DEVICECONTEXT_IMMEDIATE, IDC_DEVICECONTEXT_GROUP, L"Immediate", 0, iY += iYo, 170, 22);
    HUD.AddRadioButton(IDC_DEVICECONTEXT_ST_DEFERRED_PER_SCENE, IDC_DEVICECONTEXT_GROUP, L"ST Def/Scene", 0, iY += iYo, 170, 22);
    HUD.AddRadioButton(IDC_DEVICECONTEXT_MT_DEFERRED_PER_SCENE, IDC_DEVICECONTEXT_GROUP, L"MT Def/Scene", 0, iY += iYo, 170, 22);
    HUD.AddRadioButton(IDC_DEVICECONTEXT_ST_DEFERRED_PER_CHUNK, IDC_DEVICECONTEXT_GROUP, L"ST Def/Chunk", 0, iY += iYo, 170, 22);
    HUD.AddRadioButton(IDC_DEVICECONTEXT_MT_DEFERRED_PER_CHUNK, IDC_DEVICECONTEXT_GROUP, L"MT Def/Chunk", 0, iY += iYo, 170, 22);

    CDXUTRadioButton* pRadioButton = HUD.GetRadioButton(IDC_DEVICECONTEXT_IMMEDIATE);
    pRadioButton->SetChecked(true);

    SampleUI.SetCallback(OnGUIEvent); iY = 10;
}

//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
    return true;
}

//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
    static float fTotalTime = 0.0f;
    fTotalTime += fElapsedTime;

    Renderer.AnimateLights(fTotalTime);

    Camera.FrameMove(fElapsedTime);
}

//--------------------------------------------------------------------------------------
void RenderText()
{
    TxtHelper->Begin();
    TxtHelper->SetInsertionPos(2, 0);
    TxtHelper->SetForegroundColor(Colors::Yellow);
    TxtHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
    TxtHelper->DrawTextLine(DXUTGetDeviceStats());
    TxtHelper->End();
}

//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext)
{
    *pbNoFurtherProcessing = DialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
        return 0;

    if (D3DSettingsDlg.IsActive())
    {
        D3DSettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
        return 0;
    }

    *pbNoFurtherProcessing = HUD.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
        return 0;
    *pbNoFurtherProcessing = SampleUI.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
        return 0;

    Camera.HandleMessages(hWnd, uMsg, wParam, lParam);

    return 0;
}

//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
    switch (nControlID)
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEWIRE:
            Renderer.ToggleWireFrame();
            break;
        case IDC_DEVICECONTEXT_IMMEDIATE:
            Renderer.SetDeviceContextType(DEVICECONTEXT_IMMEDIATE);
            break;
        case IDC_DEVICECONTEXT_ST_DEFERRED_PER_SCENE:
            Renderer.SetDeviceContextType(DEVICECONTEXT_ST_DEFERRED_PER_SCENE);
            break;
        case IDC_DEVICECONTEXT_MT_DEFERRED_PER_SCENE:
            Renderer.SetDeviceContextType(DEVICECONTEXT_MT_DEFERRED_PER_SCENE);
            break;
        case IDC_DEVICECONTEXT_ST_DEFERRED_PER_CHUNK:
            Renderer.SetDeviceContextType(DEVICECONTEXT_ST_DEFERRED_PER_CHUNK);
            break;
        case IDC_DEVICECONTEXT_MT_DEFERRED_PER_CHUNK:
            Renderer.SetDeviceContextType(DEVICECONTEXT_MT_DEFERRED_PER_CHUNK);
            break;
    }
}

//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext)
{
    return true;
}

//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN(DialogResourceManager.OnD3D11CreateDevice(pd3dDevice, pd3dImmediateContext));
    V_RETURN(D3DSettingsDlg.OnD3D11CreateDevice(pd3dDevice));
    TxtHelper = new CDXUTTextHelper(pd3dDevice, pd3dImmediateContext, &DialogResourceManager, 15);

    V_RETURN(Renderer.Create(pd3dDevice, pd3dImmediateContext));

    // 카메라 뷰 파라미터
    Camera.SetViewParams(DefaultEye, DefaultLookAt);
    Camera.SetRadius(DefaultCameraRadius, MinCameraRadius, MaxCameraRadius);

    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
    HRESULT hr;

    V_RETURN(DialogResourceManager.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc));
    V_RETURN(D3DSettingsDlg.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc));

    float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    Camera.SetProjParams(FOV, fAspectRatio, NearPlane, FarPlane);
    Camera.SetWindow(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);
    Camera.SetButtonMasks(MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON);

    HUD.SetLocation(pBackBufferSurfaceDesc->Width - 170, 0);
    HUD.SetSize(170, 170);
    SampleUI.SetLocation(pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300);
    SampleUI.SetSize(170, 300);

    return S_OK;
}

//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext)
{
    Renderer.BeginFrame(pd3dImmediateContext);

    // 설정 대화상자가 활성이면 씬 대신 그것을 렌더
    if (D3DSettingsDlg.IsActive())
    {
        D3DSettingsDlg.OnRender(fElapsedTime);
        return;
    }

    Renderer.Render(pd3dImmediateContext, Camera);

    DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD / Stats");
    HUD.OnRender(fElapsedTime);
    SampleUI.OnRender(fElapsedTime);
    RenderText();
    DXUT_EndPerfEvent();
}

//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
    DialogResourceManager.OnD3D11ReleasingSwapChain();
}

//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
    DialogResourceManager.OnD3D11DestroyDevice();
    D3DSettingsDlg.OnD3D11DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE(TxtHelper);

    Renderer.Destroy();
}
