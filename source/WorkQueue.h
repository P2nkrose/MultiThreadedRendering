//--------------------------------------------------------------------------------------
// File: WorkQueue.h
//
// 멀티스레드 per-chunk 렌더링에서 워커 스레드에 전달하는 작업 큐 항목 정의.
//--------------------------------------------------------------------------------------
#pragma once

#include <windows.h>
#include "SceneParams.h"

// 각 워커 스레드가 처리할 수 있는 작업의 종류.
enum WorkQueueEntryType
{
    WORK_QUEUE_ENTRY_TYPE_SETUP,      // 0. 씬 셋업(상태 설정)
    WORK_QUEUE_ENTRY_TYPE_CHUNK,      // 1. 한 청크(chunk) 그리기
    WORK_QUEUE_ENTRY_TYPE_FINALIZE,   // 2. 커맨드 리스트 마무리

    WORK_QUEUE_ENTRY_TYPE_COUNT
};

// 작업 큐에 담기는 기본 구조체. Setup/Chunk/Finalize 파생 구조체가 이를 상속한다.
struct WorkQueueEntryBase
{
    WorkQueueEntryType Type;
};

// 씬 셋업 작업 파라미터
struct WorkQueueEntrySetup : public WorkQueueEntryBase
{
    const SceneParamsStatic*    SceneParamsStatic;
    SceneParamsDynamic          SceneParamsDynamic;
};

// 청크 렌더 작업 파라미터
struct WorkQueueEntryChunk : public WorkQueueEntryBase
{
    int Mesh;
};

// 씬 마무리 작업 파라미터
struct WorkQueueEntryFinalize : public WorkQueueEntryBase
{
};

// per-chunk 워커 스레드별 작업 큐 (고정 크기 바이트 버퍼)
constexpr int SceneQueueSizeInBytes = 16 * 1024;
typedef BYTE ChunkQueue[SceneQueueSizeInBytes];
