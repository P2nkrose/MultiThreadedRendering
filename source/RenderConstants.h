//--------------------------------------------------------------------------------------
// File: RenderConstants.h
//
// 씬 전역에서 공유하는 상수와 렌더 경로(디바이스 컨텍스트) 종류 열거형.
//--------------------------------------------------------------------------------------
#pragma once

// 일반적으로 처음 n개의 광원만 그림자를 생성하며, 이후 광원은 그림자 없이 단순 조명만 한다.
constexpr int NUM_LIGHTS  = 4;
constexpr int NUM_SHADOWS = 1;
constexpr int NUM_MIRRORS = 4;

// UI 오른쪽 라디오 버튼에 대응하는 5가지 렌더링 경로.
enum DEVICECONTEXT_TYPE
{
    DEVICECONTEXT_IMMEDIATE,                // 단일 스레드, 즉시(immediate) 컨텍스트 사용
    DEVICECONTEXT_ST_DEFERRED_PER_SCENE,    // 단일 스레드, 씬 단위 디퍼드 컨텍스트
    DEVICECONTEXT_MT_DEFERRED_PER_SCENE,    // 멀티스레드, 씬 단위 디퍼드 컨텍스트
    DEVICECONTEXT_ST_DEFERRED_PER_CHUNK,    // 단일 스레드, 청크 단위 디퍼드 컨텍스트
    DEVICECONTEXT_MT_DEFERRED_PER_CHUNK,    // 멀티스레드, 청크 단위 디퍼드 컨텍스트 (가장 고급)
};
