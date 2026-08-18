//--------------------------------------------------------------------------------------
// File: PixelShader.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "PixelShader.h"

bool PixelShader::Create(ID3D11Device* device, const wchar_t* file, const char* entryPoint, const char* profile)
{
    Destroy();

    ID3DBlob* blob = nullptr;
    if (FAILED(Compile(file, entryPoint, profile, &blob)))
        return false;

    HRESULT hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader);
    SAFE_RELEASE(blob);
    return SUCCEEDED(hr);
}

void PixelShader::Destroy()
{
    SAFE_RELEASE(shader);
}
