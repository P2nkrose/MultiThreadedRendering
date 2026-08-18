//--------------------------------------------------------------------------------------
// File: ConstantBuffer.h
//
// 동적(D3D11_USAGE_DYNAMIC) 상수 버퍼 RAII 래퍼. Map/Unmap 은 호출부에서 직접 수행하며,
// 여기서는 생성/해제와 포인터 접근만 담당한다.
//--------------------------------------------------------------------------------------
#pragma once

#include <d3d11.h>

class ConstantBuffer
{
public:
    bool Create(ID3D11Device* device, UINT byteWidth, const char* debugName = nullptr);
    void Destroy();

    operator ID3D11Buffer* () const { return buffer; }
    ID3D11Buffer* const* GetAddressOf() const { return &buffer; }

    ConstantBuffer() : buffer(nullptr) {}
    ~ConstantBuffer() { Destroy(); }

private:
    ConstantBuffer(const ConstantBuffer&) = delete;
    const ConstantBuffer& operator = (const ConstantBuffer&) = delete;

    ID3D11Buffer* buffer;
};
