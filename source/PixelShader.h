//--------------------------------------------------------------------------------------
// File: PixelShader.h
//
// 픽셀 셰이더 RAII 래퍼.
//--------------------------------------------------------------------------------------
#pragma once

#include "Shader.h"

class PixelShader : public Shader
{
public:
    Type GetType() const override { return Shader::Type::Pixel; }
    void Destroy() override;

    bool Create(ID3D11Device* device, const wchar_t* file, const char* entryPoint, const char* profile);

    operator ID3D11PixelShader* () const { return shader; }

    PixelShader() : shader(nullptr) {}
    ~PixelShader() { Destroy(); }

private:
    PixelShader(const PixelShader&) = delete;
    const PixelShader& operator = (const PixelShader&) = delete;

    ID3D11PixelShader* shader;
};
