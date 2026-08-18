//--------------------------------------------------------------------------------------
// File: Shader.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "Shader.h"

HRESULT Shader::Compile(const wchar_t* file, const char* entryPoint, const char* profile, ID3DBlob** outBlob)
{
    return DXUTCompileFromFile(file, nullptr, entryPoint, profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, outBlob);
}
