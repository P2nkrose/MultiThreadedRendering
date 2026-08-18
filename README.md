# MultiThread Deferred Context Rendering

DirectX 11의 [DeferredContext](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-render)를 이용한 **MultiThreaded Rendering** 구현입니다.
하나의 씬(SquidRoom)을 **5가지 렌더링 경로**로 그려, <br> Immediate Context 와 DeferredContext 멀티스레딩의 차이를 실시간으로 비교할 수 있도록 구현했습니다.

<img width="600" height="400" alt="squid3" src="https://github.com/user-attachments/assets/b45afa3d-87a4-4a60-8d63-4bf36737046f" />


---

## Important Implementations

- [MultiThreaded Rendering](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-intro) with [DeferredContext](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-render) — 여러 스레드에서 Command List를 기록하고 Immediate Context가 실행
- 작업 분할 두 가지 방식 — **per-Scene**(렌더 패스 단위) / **per-Chunk**(메시 드로우콜 단위)
- Shadow Map — 광원 시점 깊이 렌더링을 통한 그림자
- Planar Mirror Reflection — Stencil 기법 기반 거울 반사(4개)

---

## Render Paths

우측 라디오 버튼으로 실시간 전환하며, 동일한 프레임을 아래 5가지 경로로 렌더링합니다.<br>
**ST** = Single-Thread, **MT** = Multi-Thread, **Def** = Deferred Context.

| 경로 | 스레드 | 분할 단위 | 설명 |
|------|--------|-----------|------|
| **Immediate** | 1 | — | 즉시 컨텍스트로 전부 렌더링 (기준선) |
| **ST Def/Scene** | 1 | 씬(패스) | 씬마다 Deferred Context 하나, 한 스레드가 순차 기록 (API 시연) |
| **MT Def/Scene** | N | 씬(패스) | 메인·그림자·거울 씬마다 스레드 하나씩 병렬 기록 |
| **ST Def/Chunk** | 1 | 청크(메시 조각) | 청크를 여러 Deferred Context에 분배, 한 스레드가 처리 |
| **MT Def/Chunk** | 코어 수 | 청크(메시 조각) | 청크를 물리 코어 수만큼의 워커 스레드에 분산 (최고 확장성) |

병렬성이 **패스 개수**에 묶이는 per-Scene과 달리, per-Chunk는 **CPU 코어 수**에 맞춰 확장됩니다.

---

## Important Classes

### SquidRenderer
렌더링의 전체 흐름을 관리하는 메인 렌더러입니다.
셰이더·상태·상수 버퍼·그림자·거울과 워커 스레드를 모두 소유하며,<br>
5가지 렌더 경로 선택, Command List 기록/실행, 씬 셋업까지 담당합니다.

**Main Functions**
- `SquidRenderer::Render`
- `SquidRenderer::RenderScene` / `RenderSceneSetup`
- `SquidRenderer::PerSceneThreadProc` / `PerChunkThreadProc`

---

### Per-Scene / Per-Chunk Worker Threads
DeferredContext에 Command List를 기록하는 워커 스레드 풀입니다.<br>
per-Scene은 씬(패스)마다 스레드 하나(그림자 + 거울 + 메인),
per-Chunk는 물리 코어 수만큼의 스레드가 메시 청크를 나눠 기록합니다.

**Main Code**
- `SquidRenderer::PerChunkThreadProc`
- 작업 큐: `WorkQueue.h` (Setup / Chunk / Finalize 항목)

---

### ShadowMap
광원 시점의 Shadow Depth Map을 생성합니다.<br>
`R32_TYPELESS` 텍스처 + DSV + SRV를 RAII로 관리하며, Scene Pass에서 그림자에 사용됩니다.

**Main Code**
- `ShadowMap::Create`

---

### MirrorSet
Stencil 기법으로 거울 평면 반사를 렌더링합니다.<br>
거울 사각형을 Stencil에 기록 → 반사 투영으로 스텐실 영역에 월드를 그림 → Stencil 정리 순으로 처리합니다.

**Main Code**
- `SquidRenderer::RenderMirror`

---

### RAII Wrappers
`VertexShader`, `PixelShader`, `ConstantBuffer`, `VertexBuffer`,
`SamplerState`, `RasterizerState`, `DepthStencilState` 등<br>
D3D11 리소스를 `Create()` / `Destroy()` 로 감싼 래퍼 클래스들입니다.


---

## Files

이 포트폴리오는 실행 가능한 바이너리와 함께,  
직접 빌드할 수 있는 Visual Studio 솔루션을 포함하고 있습니다.

- **bin/**  
  실행 가능한 바이너리 파일 폴더

- **MultithreadedRendering_x64_release.exe**  
  Windows 64bit 전용 실행 파일  
  (실행 시 추가 DLL이 필요할 수 있습니다)

- **dxut/**  
  DXUT 프레임워크 프로젝트 폴더 (빌드 시 필요)

- **media/**  
  쉐이더 코드 및 그래픽 리소스 데이터

- **source/**  
  C++ 소스 코드 및 프로젝트 파일

- **MultithreadedRendering.sln**  
  Visual Studio 2022 솔루션 파일

---

## Download Release

<img width="600" height="400" alt="squidroom" src="https://github.com/user-attachments/assets/6b0b9c0e-554e-4fd9-8282-9b8d83bf7744" />


빌드 과정 없이 포트폴리오를 실행하려면 아래 링크된 파일을 다운받으시면 됩니다.
[https://github.com/P2nkrose/MultiThreadedRendering/releases/download/1/MultiThreadSquidRoomRelease.zip](https://github.com/P2nkrose/MultiThreadedRendering/releases/download/1/MultiThreadSquidRoomRelease.zip)

- Release 된 바이너리는 Windows 64bit 전용입니다.
- 압축 해제 후 `bin` 폴더의 `MultithreadedRendering_x64_release.exe` 를 실행하세요.
- **카메라 회전** : 마우스 좌클릭 드래그
- **줌** : 마우스 휠
