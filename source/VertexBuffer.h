//--------------------------------------------------------------------------------------
// File: VertexBuffer.h
//
// 동적 정점 버퍼 RAII 래퍼. Map/Unmap 은 호출부에서 직접 수행한다.
//--------------------------------------------------------------------------------------
#pragma once

#include <d3d11.h>

class VertexBuffer
{
public:
    bool CreateDynamic(ID3D11Device* device, UINT byteWidth, const char* debugName = nullptr);
    void Destroy();

    operator ID3D11Buffer* () const { return buffer; }
    ID3D11Buffer* const* GetAddressOf() const { return &buffer; }

    VertexBuffer() : buffer(nullptr) {}
    ~VertexBuffer() { Destroy(); }

private:
    VertexBuffer(const VertexBuffer&) = delete;
    const VertexBuffer& operator = (const VertexBuffer&) = delete;

    ID3D11Buffer* buffer;
};
