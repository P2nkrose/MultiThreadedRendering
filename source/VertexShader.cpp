//--------------------------------------------------------------------------------------
// File: VertexShader.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "VertexShader.h"

bool VertexShader::Create(ID3D11Device* device, const wchar_t* file, const char* entryPoint, const char* profile)
{
    Destroy();

    ID3DBlob* blob = nullptr;
    if (FAILED(Compile(file, entryPoint, profile, &blob)))
        return false;

    if (FAILED(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader)))
    {
        SAFE_RELEASE(blob);
        return false;
    }

    byteCode = blob;    // 바이트코드는 입력 레이아웃 생성을 위해 보관한다.
    return true;
}

bool VertexShader::CreateInputLayout(ID3D11Device* device, const D3D11_INPUT_ELEMENT_DESC* elems,
                                      UINT numElems, ID3D11InputLayout** outLayout) const
{
    if (!byteCode)
        return false;

    return SUCCEEDED(device->CreateInputLayout(elems, numElems,
        byteCode->GetBufferPointer(), byteCode->GetBufferSize(), outLayout));
}

void VertexShader::Destroy()
{
    SAFE_RELEASE(shader);
    SAFE_RELEASE(byteCode);
}
