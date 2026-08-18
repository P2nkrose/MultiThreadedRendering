//--------------------------------------------------------------------------------------
// File: MultiDeviceContextDXUTMesh.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "MultiDeviceContextDXUTMesh.h"

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT CMultiDeviceContextDXUTMesh::Create(ID3D11Device* pDev11, LPCWSTR szFileName, 
                                             MDC_SDKMESH_CALLBACKS11* pCallbacks)
{
    if (pCallbacks)
    {
        RenderMeshCallback = pCallbacks->pRenderMesh;
    }
    else
    {
        RenderMeshCallback = nullptr;
    }

    return CDXUTSDKMesh::Create(pDev11, szFileName, pCallbacks);
}


_Use_decl_annotations_
void CMultiDeviceContextDXUTMesh::RenderMesh(UINT iMesh,
                                            bool bAdjacent,
                                            ID3D11DeviceContext* pd3dDeviceContext,
                                            UINT iDiffuseSlot,
                                            UINT iNormalSlot,
                                            UINT iSpecularSlot)
{
    CDXUTSDKMesh::RenderMesh(iMesh,
        bAdjacent,
        pd3dDeviceContext,
        iDiffuseSlot,
        iNormalSlot,
        iSpecularSlot);
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void CMultiDeviceContextDXUTMesh::RenderFrame(UINT iFrame,
                                              bool bAdjacent,
                                              ID3D11DeviceContext* pd3dDeviceContext,
                                              UINT iDiffuseSlot,
                                              UINT iNormalSlot,
                                              UINT iSpecularSlot)
{
    if(!m_pStaticMeshData || !m_pFrameArray)
        return;

    if(INVALID_MESH != m_pFrameArray[iFrame].Mesh)
    {
        if (!RenderMeshCallback)
        {
            RenderMesh(m_pFrameArray[iFrame].Mesh,
                bAdjacent,
                pd3dDeviceContext,
                iDiffuseSlot,
                iNormalSlot,
                iSpecularSlot);
        }
        else
        {
            RenderMeshCallback(this, 
                m_pFrameArray[iFrame].Mesh,
                bAdjacent,
                pd3dDeviceContext,
                iDiffuseSlot,
                iNormalSlot,
                iSpecularSlot);
        }
    }

    // Render our children
    if(INVALID_FRAME != m_pFrameArray[iFrame].ChildFrame)
        RenderFrame(m_pFrameArray[iFrame].ChildFrame, bAdjacent, pd3dDeviceContext, iDiffuseSlot, 
        iNormalSlot, iSpecularSlot);

    // Render our siblings
    if(INVALID_FRAME != m_pFrameArray[iFrame].SiblingFrame)
        RenderFrame(m_pFrameArray[iFrame].SiblingFrame, bAdjacent, pd3dDeviceContext, iDiffuseSlot, 
        iNormalSlot, iSpecularSlot);
}

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void CMultiDeviceContextDXUTMesh::Render(ID3D11DeviceContext* pd3dDeviceContext,
                                         UINT iDiffuseSlot,
                                         UINT iNormalSlot,
                                         UINT iSpecularSlot)
{
    RenderFrame(0, false, pd3dDeviceContext, iDiffuseSlot, iNormalSlot, iSpecularSlot);
}

