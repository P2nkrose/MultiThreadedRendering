//--------------------------------------------------------------------------------------
// File: Shader.h
//
// 셰이더 래퍼의 공통 베이스. HLSL 컴파일 헬퍼를 제공한다.
//--------------------------------------------------------------------------------------
#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>

class Shader
{
public:
    enum class Type
    {
        Vertex,
        Pixel,
    };

    virtual Type GetType() const = 0;
    virtual void Destroy() = 0;
    virtual ~Shader() {}

protected:
    Shader() {}

    // DXUTCompileFromFile 로 HLSL 을 컴파일한다. 성공 시 *outBlob 에 바이트코드를 채운다.
    static HRESULT Compile(const wchar_t* file, const char* entryPoint, const char* profile, ID3DBlob** outBlob);
};
