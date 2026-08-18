//--------------------------------------------------------------------------------------
// File: VertexBuffer.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "VertexBuffer.h"

bool VertexBuffer::CreateDynamic(ID3D11Device* device, UINT byteWidth, const char* debugName)
{
    Destroy();

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = byteWidth;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;

    if (FAILED(device->CreateBuffer(&desc, nullptr, &buffer)))
        return false;

    if (debugName)
    {
        DXUT_SetDebugName(buffer, debugName);
    }

    return true;
}

void VertexBuffer::Destroy()
{
    SAFE_RELEASE(buffer);
}
