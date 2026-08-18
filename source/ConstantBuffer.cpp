//--------------------------------------------------------------------------------------
// File: ConstantBuffer.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "ConstantBuffer.h"

bool ConstantBuffer::Create(ID3D11Device* device, UINT byteWidth, const char* debugName)
{
    Destroy();

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;
    desc.ByteWidth = byteWidth;

    if (FAILED(device->CreateBuffer(&desc, nullptr, &buffer)))
        return false;

    if (debugName)
    {
        DXUT_SetDebugName(buffer, debugName);
    }

    return true;
}

void ConstantBuffer::Destroy()
{
    SAFE_RELEASE(buffer);
}
