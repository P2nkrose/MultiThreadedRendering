//--------------------------------------------------------------------------------------
// File: VertexShader.h
//
// 정점 셰이더 RAII 래퍼. 컴파일된 바이트코드를 보관하여 여러 개의 입력 레이아웃을
// 만들 수 있게 한다(이 샘플은 동일 VS 로 압축/비압축 두 레이아웃을 생성).
//--------------------------------------------------------------------------------------
#pragma once

#include "Shader.h"

class VertexShader : public Shader
{
public:
    Type GetType() const override { return Shader::Type::Vertex; }
    void Destroy() override;

    bool Create(ID3D11Device* device, const wchar_t* file, const char* entryPoint, const char* profile);

    // 보관한 바이트코드로 입력 레이아웃을 생성한다.
    bool CreateInputLayout(ID3D11Device* device, const D3D11_INPUT_ELEMENT_DESC* elems,
                            UINT numElems, ID3D11InputLayout** outLayout) const;

    operator ID3D11VertexShader* () const { return shader; }

    VertexShader() : shader(nullptr), byteCode(nullptr) {}
    ~VertexShader() { Destroy(); }

private:
    VertexShader(const VertexShader&) = delete;
    const VertexShader& operator = (const VertexShader&) = delete;

    ID3D11VertexShader* shader;
    ID3DBlob*           byteCode;
};
