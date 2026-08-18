//--------------------------------------------------------------------------------------
// File: MultiDeviceContextDXUTMesh.h
//--------------------------------------------------------------------------------------
#pragma once

#include "SDKMesh.h"

class CMultiDeviceContextDXUTMesh;
typedef void (*LPRENDERMESH11)(CMultiDeviceContextDXUTMesh* pMesh, 
        UINT iMesh,
        bool bAdjacent,
        ID3D11DeviceContext* pd3dDeviceContext,
        UINT iDiffuseSlot,
        UINT iNormalSlot,
        UINT iSpecularSlot);

struct MDC_SDKMESH_CALLBACKS11 : public SDKMESH_CALLBACKS11
{
    LPRENDERMESH11 pRenderMesh;
};

class CMultiDeviceContextDXUTMesh : public CDXUTSDKMesh
{

public:
    virtual HRESULT Create(_In_ ID3D11Device* pDev11, _In_z_ LPCWSTR szFileName, _In_opt_ MDC_SDKMESH_CALLBACKS11* pLoaderCallbacks = nullptr);

    virtual void Render(_In_ ID3D11DeviceContext* pd3dDeviceContext,
                         _In_ UINT iDiffuseSlot = INVALID_SAMPLER_SLOT,
                         _In_ UINT iNormalSlot = INVALID_SAMPLER_SLOT,
                         _In_ UINT iSpecularSlot = INVALID_SAMPLER_SLOT) override;

    void RenderMesh(_In_ UINT iMesh,
                     _In_ bool bAdjacent,
                     _In_ ID3D11DeviceContext* pd3dDeviceContext,
                     _In_ UINT iDiffuseSlot,
                     _In_ UINT iNormalSlot,
                     _In_ UINT iSpecularSlot);
    void RenderFrame(_In_ UINT iFrame,
                      _In_ bool bAdjacent,
                      _In_ ID3D11DeviceContext* pd3dDeviceContext,
                      _In_ UINT iDiffuseSlot,
                      _In_ UINT iNormalSlot,
                      _In_ UINT iSpecularSlot);

protected:
    LPRENDERMESH11                  RenderMeshCallback;
};