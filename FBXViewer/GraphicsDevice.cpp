#include "stdafx.h"
#include "GraphicsDevice.h"
#include "D3D\TMesh.h"

GraphicsDevice* GraphicsDevice::instance = NULL;

GraphicsDevice::GraphicsDevice(HWND hwnd): m_hWnd(hwnd)
{
    HRESULT hr =  S_OK;

    hr = CoInitialize(0);
    DebugAssert(SUCCEEDED(hr), "CoInitializeʧ��");

    //��ȡ���ڴ�С
    GetWindowRect(m_hWnd, &WindowRect);

    //��ȡ�ͻ�����С
    GetClientRect(m_hWnd, &ClientRect);
    mClientSize.width = ClientRect.right - ClientRect.left;
    mClientSize.height = ClientRect.bottom - ClientRect.top;

    //��ȡDirect3D9�ӿ�
    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    DebugAssert(m_pD3D!=NULL, "Direct3DCreate9ʧ��");

    m_d3ddm.Width = (UINT)mClientSize.width;     //��Ļ�߶ȺͿͻ����߶�һ��
    m_d3ddm.Height = (UINT)mClientSize.height;   //��Ļ��ȺͿͻ������һ��
    ZeroMemory(&m_d3dpp, sizeof(D3DPRESENT_PARAMETERS));
    m_d3dpp.Windowed = TRUE;    //����ģʽ

    //��ȡ��ʾ������ʾģʽ
    hr = m_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &m_d3ddm);
    DebugAssert(SUCCEEDED(hr), "GetAdapterDisplayModeʧ��");

    //����Ƿ�֧��Z-Buffer
    hr = m_pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                          m_d3ddm.Format,
                                          D3DUSAGE_DEPTHSTENCIL,
                                          D3DRTYPE_SURFACE,
                                          D3DFMT_D24S8);
    DebugAssert(SUCCEEDED(hr), "�Կ���֧��Z-Buffer");

    // ����Ƿ�֧��multi-sampling
    D3DMULTISAMPLE_TYPE mst = D3DMULTISAMPLE_2_SAMPLES;
    DWORD quality = 0;
    // Is it supported for the back buffer format?
    if(SUCCEEDED(m_pD3D->CheckDeviceMultiSampleType(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_A8R8G8B8,
        true, mst, &quality)))
    {
        // Is it supported for the depth format?
        if(SUCCEEDED(m_pD3D->CheckDeviceMultiSampleType(
            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_D24S8,
            true, mst, &quality)))
        {
            m_d3dpp.MultiSampleType = mst;
            m_d3dpp.MultiSampleQuality = quality - 1;
        }
    }

    m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;   //back buffer����֮ǰ���������������
    m_d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    m_d3dpp.EnableAutoDepthStencil = TRUE;
    m_d3dpp.AutoDepthStencilFormat  = D3DFMT_D24S8;

    //�����豸�ӿ�IDirect3DDevice9
    hr = m_pD3D->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL, m_hWnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &m_d3dpp, &m_pD3DDevice);
    DebugAssert(SUCCEEDED(hr), "CreateDeviceʧ��");

    //����viewport
    //Ĭ������viewportΪ�����ͻ���
    mDefaultViewport.X = 0;
    mDefaultViewport.Y = 0;
    mDefaultViewport.Width = mClientSize.width;
    mDefaultViewport.Height = mClientSize.height;
    mDefaultViewport.MinZ = 0;
    mDefaultViewport.MaxZ = 1;
    hr = m_pD3DDevice->SetViewport(&mDefaultViewport);
    DebugAssert(SUCCEEDED(hr), "SetViewportʧ��");

    //���ͼ��
    hr = m_pD3DDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    DebugAssert(SUCCEEDED(hr), "SetRenderState D3DRS_FILLMODE D3DFILL_SOLIDʧ��");
}

GraphicsDevice::~GraphicsDevice(void)
{
    CoUninitialize();
}

void GraphicsDevice::BuildViewports()
{
    //����Axis��ʾ����
    mAxisViewport.X = 20;
    mAxisViewport.Y = mClientSize.height - 120;
    mAxisViewport.Width = 100;
    mAxisViewport.Height = 100;
    mAxisViewport.MinZ = 0.0f;
    mAxisViewport.MaxZ = 1.0f;
    D3DXMatrixPerspectiveFovLH(&m_matAxisProj, D3DX_PI / 4.0f,
        (float)mAxisViewport.Width / (float)mAxisViewport.Height, 1.0f, 1000.0f);

    //����Cube��ʾ����
    mCubeViewport.X = mClientSize.width - 220;
    mCubeViewport.Y = 20;
    mCubeViewport.Width = 200;
    mCubeViewport.Height = 200;
    mCubeViewport.MinZ = 0.0f;
    mCubeViewport.MaxZ = 1.0f;
    D3DXMatrixPerspectiveFovLH(&m_matCubeProj, D3DX_PI / 4.0f,
        (float)mCubeViewport.Width / (float)mCubeViewport.Height, 1.0f, 1000.0f);
}

//�������
void GraphicsDevice::Clear()
{
	HRESULT hr = m_pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255,200,200,200), 1.0f, 0);
    DebugAssert(SUCCEEDED(hr), "Clearʧ��");
}

void GraphicsDevice::BeginScene()
{
    //��ʼ����
    HRESULT hr = m_pD3DDevice->BeginScene();
    DebugAssert(SUCCEEDED(hr), "BeginSceneʧ��");
}

void GraphicsDevice::EndScene()
{
    //��������
    HRESULT hr = m_pD3DDevice->EndScene();
    DebugAssert(SUCCEEDED(hr), "EndSceneʧ��");
}

void GraphicsDevice::Present()
{
    //��ʾ����
    HRESULT hr = m_pD3DDevice->Present(NULL, NULL, NULL, NULL);
    DebugAssert(SUCCEEDED(hr), "Presentʧ��");
}

void GraphicsDevice::SetViewport( const D3DVIEWPORT9& viewport )
{
    HRESULT hr = m_pD3DDevice->SetViewport(&viewport);
    DebugAssert(SUCCEEDED(hr), "SetViewportʧ��");
}

void GraphicsDevice::ResetViewport()
{
    HRESULT hr = m_pD3DDevice->SetViewport(&mDefaultViewport);
    DebugAssert(SUCCEEDED(hr), "SetViewportʧ��");
}


