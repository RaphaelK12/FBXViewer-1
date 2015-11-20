#include "stdafx.h"
#include "Axis.h"
#include "..\GraphicsDevice.h"

namespace StaticMesh
{

    AxisMesh::AxisMesh( void ) : pXTexture(0),pYTexture(0),pZTexture(0)
    {

    }

    AxisMesh::~AxisMesh( void )
    {
        DestroyXYZ();
    }

	void AxisMesh::SetVertexData(int meshType)
    {
        mName = "������";
        mPrimitiveType = D3DPT_LINELIST;
        mVertexType = PC;
        nVertex = 6;
        nPrimitive = 3;

        D3DXVECTOR3 Origin(0.0f, 0.0f, 0.0f);
        Positions.resize(nVertex);
        Colors.resize(nVertex);
        IndexBuf.resize(nPrimitive*2);
        Positions[0] = Origin;
        Positions[1] = Origin + 15.0f*D3DXVECTOR3(1.0f, 0.0f, 0.0f); //x
        Positions[2] = Origin;
        Positions[3] = Origin + 15.0f*D3DXVECTOR3(0.0f, 1.0f, 0.0f); //y
        Positions[4] = Origin;
        Positions[5] = Origin + 15.0f*D3DXVECTOR3(0.0f, 0.0f, 1.0f); //z
        Colors[0] = D3DCOLOR_ARGB(255, 255, 0, 0);
        Colors[1] = D3DCOLOR_ARGB(255, 255, 0, 0);  //x ��
        Colors[2] = D3DCOLOR_ARGB(255, 0, 255, 0);
        Colors[3] = D3DCOLOR_ARGB(255, 0, 255, 0);  //y ��
        Colors[4] = D3DCOLOR_ARGB(255, 0, 0, 255);
        Colors[5] = D3DCOLOR_ARGB(255, 0, 0, 255);  //z ��
        for (size_t i=0; i<nPrimitive*2; i++)
        {
            IndexBuf[i] = i;
        }
    }

    void AxisMesh::CreateXYZ(IDirect3DDevice9* pDevice)
    {
        //׼������
        D3DXVECTOR3 XEndPos = Positions[1];
        D3DXVECTOR3 YEndPos = Positions[3];
        D3DXVECTOR3 ZEndPos = Positions[5];
        D3DXVECTOR3 lXPos[]={
            XEndPos,
            XEndPos + D3DXVECTOR3(16.0f, 0.0f , 0.0f),
            XEndPos + D3DXVECTOR3( 0.0f, 16.0f, 0.0f),
            XEndPos + D3DXVECTOR3(16.0f, 16.0f, 0.0f),
        };
        D3DXVECTOR3 lYPos[]={
            YEndPos,
            YEndPos + D3DXVECTOR3(16.0f, 0.0f , 0.0f),
            YEndPos + D3DXVECTOR3( 0.0f, 16.0f, 0.0f),
            YEndPos + D3DXVECTOR3(16.0f, 16.0f, 0.0f),
        };
        D3DXVECTOR3 lZPos[]={
            ZEndPos,
            ZEndPos + D3DXVECTOR3(16.0f, 0.0f , 0.0f),
            ZEndPos + D3DXVECTOR3( 0.0f, 16.0f, 0.0f),
            ZEndPos + D3DXVECTOR3(16.0f, 16.0f, 0.0f),
        };
        D3DXVECTOR2 lUV[]={
            D3DXVECTOR2(0.0f,0.0f),
            D3DXVECTOR2(1.0f,0.0f),
            D3DXVECTOR2(0.0f,1.0f),
            D3DXVECTOR2(1.0f,1.0f)
        };
        WORD lIndex[]={
            0,1,2, 1,3,2
        };
        for (unsigned int i=0; i<6; ++i)
        {
            XQuad[i].Pos = lXPos[lIndex[i]];
            XQuad[i].TC = lUV[lIndex[i]];

            YQuad[i].Pos = lYPos[lIndex[i]];
            YQuad[i].TC = lUV[lIndex[i]];

            ZQuad[i].Pos = lZPos[lIndex[i]];
            ZQuad[i].TC = lUV[lIndex[i]];
        }

        //Texture
        {
            HRESULT hr =D3DXCreateTextureFromFileEx(pDevice, "axisX.png",
                0, 0, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
                NULL, NULL, &pXTexture);
            if (FAILED(hr))
            {
                DebugPrintf("Diffuse texture: %s ����ʧ��\n", "axisX.png");
            }

            hr =D3DXCreateTextureFromFileEx(pDevice, "axisY.png",
                0, 0, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
                NULL, NULL, &pYTexture);
            if (FAILED(hr))
            {
                DebugPrintf("Diffuse texture: %s ����ʧ��\n", "axisY.png");
            }

            hr =D3DXCreateTextureFromFileEx(pDevice, "axisZ.png",
                0, 0, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
                NULL, NULL, &pZTexture);
            if (FAILED(hr))
            {
                DebugPrintf("Diffuse texture: %s ����ʧ��\n", "axisZ.png");
            }
        }
    }

    void AxisMesh::UpdateXYZ( const D3DXVECTOR3& eyePoint )
    {
        //����X Quad�ĳ��򣬼�Billboard�ķ�����
        D3DXVECTOR3 XQuadNormal;
        D3DXVec3Cross(&XQuadNormal, &(XQuad[2].Pos - XQuad[0].Pos), &(XQuad[1].Pos - XQuad[0].Pos));
        D3DXVec3Normalize(&XQuadNormal, &XQuadNormal);

        //����X Quad����XCenter
        D3DXVECTOR3 XCenter = (XQuad[0].Pos + XQuad[4].Pos)*0.5f;
        //����XCenter��Eye point������
        D3DXVECTOR3 XCenter2EyePoint = eyePoint - XCenter;
        D3DXVec3Normalize(&XCenter2EyePoint, &XCenter2EyePoint);

        //����X Quad��Ҫ��������ת�ĽǶ�
        float XAngle = acosf(D3DXVec3Dot(&XCenter2EyePoint,&XQuadNormal));

        //��תX Quad��ʹ���䷨������XCenter2EyePoint��ͬ
        D3DXMATRIX matXRotation;
        D3DXMatrixRotationY(&matXRotation, XAngle);
        D3DXVECTOR4 tmp;
        for (unsigned int i=0; i<6; ++i)
        {
            D3DXVECTOR3 tmpPos = XQuad[i].Pos;
            tmpPos.x -= XCenter.x;
            tmpPos.z -= XCenter.z;
            D3DXVec3Transform(&tmp, &tmpPos, &matXRotation);
            XQuad[i].Pos = D3DXVECTOR3(tmp.x, tmp.y, tmp.z) + XCenter;
        }
    }

    void AxisMesh::DestroyXYZ()
    {
        SAFE_RELEASE(pXTexture);
        SAFE_RELEASE(pYTexture);
        SAFE_RELEASE(pZTexture);
    }

    namespace
    {
        D3DMATERIAL9 DefaultMatrial()
        {
            D3DMATERIAL9 matrial;
            matrial.Diffuse = D3DXCOLOR(1.0f,1.0f,1.0f,1.0f);
            matrial.Ambient = D3DXCOLOR(1.0f,1.0f,1.0f,1.0f);
            matrial.Specular = D3DXCOLOR(0.0f,0.0f,0.0f,0.0f);
            matrial.Power = 0.0f;
            return matrial;
        }
    }
    void AxisMesh::DrawXYZ()
    {
        GraphicsDevice* pGDevice = GraphicsDevice::getInstance();
        IDirect3DDevice9* pDevice = pGDevice->m_pD3DDevice;
        pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_TEX0);
        pDevice->SetMaterial(&DefaultMatrial());
        pDevice->SetTexture(0, pXTexture);
        pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, XQuad, sizeof(PCTVertex));
        pDevice->SetTexture(0, pYTexture);
        pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, YQuad, sizeof(PCTVertex));
        pDevice->SetTexture(0, pZTexture);
        pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, YQuad, sizeof(PCTVertex));

        pDevice->SetFVF(0);
    }
}
