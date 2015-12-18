#include "stdafx.h"
#include "Windowsx.h"
#include "FBXViewer.h"
#include "GraphicsDevice.h"
#include "D3D/SkinnedMesh.h"
#include "D3D/TMesh.h"
#include "D3D/Camera.h"
#include "D3D/Axis.h"

#define MAX_LOADSTRING 100

// ȫ�ֱ���:
HINSTANCE hInst;								// ��ǰʵ��
HWND hWnd;                                      // �����ھ��
TCHAR szTitle[MAX_LOADSTRING];					// �������ı�
TCHAR szWindowClass[MAX_LOADSTRING];			// ����������
HANDLE HTimer;                                  // ʱ��
unsigned int FrameCount;                        // ֡����
// �������ز���
bool Pause = false, bStepMode = true;
bool bStepForward = false;
// ͼ���豸
GraphicsDevice* pGDevice;

// Camera���
D3DXMATRIX identity, view, proj, viewproj;
D3DXMATRIX fixedView;

// ��Ҫ���Ƶ�Mesh
StaticMesh::AxisMesh axis;
StaticMesh::CubeMesh cube;
SkinnedMesh skinMesh;
TMesh::MeshType meshType = TMesh::RIGHTHANDED_ZUP;

//���������ز���
D3DXVECTOR2 lastCursorPos(0.0f, 0.0f);
D3DXVECTOR2 currentCursorPos(0.0f, 0.0f);
bool g_bMouseTrack = false;
bool dragging = false;
bool hovering = false;
bool moving = false;

// �˴���ģ���а����ĺ�����ǰ������:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
void Update( unsigned int _dt );
void Render( unsigned int _dt );
void Destroy();

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: �ڴ˷��ô��롣
    MSG msg;
    ZeroMemory(&msg, sizeof(MSG));
	HACCEL hAccelTable;
    DWORD time = 0;

	// ��ʼ��ȫ���ַ���
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_FBXVIEWER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// ִ��Ӧ�ó����ʼ��:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_FBXVIEWER));

	// ����Ϣѭ��:
	while (msg.message!=WM_QUIT)
    {
        //����Windows��Ϣ
        if(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            //ʹ��WaitableTimer������֡
            DWORD result = MsgWaitForMultipleObjects(
                1,
                &HTimer,
                FALSE,INFINITE,QS_ALLEVENTS);
            DebugAssert(result!=WAIT_FAILED, "MsgWaitForMultipleObjects�ȴ�ʧ��: %d", GetLastError());

            if(GetFocus()!=hWnd || Pause)    //ʧȥ���㡢��ͣ��������֡
            {
                continue;
            }

            switch (result)
            {
            case WAIT_OBJECT_0:
                {
                    if (!bStepMode)
                    {
                        FrameCount++;
                    }
                    else    //��֡����ģʽ
                    {
                        if (bStepForward)
                        {
                            FrameCount++;
                            bStepForward = false;
                        }
                        Update(FrameCount);
                        Render(FrameCount);
                    }
                }
                break;
            default:
                break;
            }//END switch
        }
	}
	return (int) msg.wParam;
}



//
//  ����: MyRegisterClass()
//
//  Ŀ��: ע�ᴰ���ࡣ
//
//  ע��:
//
//    ����ϣ��
//    �˴�������ӵ� Windows 95 �еġ�RegisterClassEx��
//    ����֮ǰ�� Win32 ϵͳ����ʱ������Ҫ�˺��������÷������ô˺���ʮ����Ҫ��
//    ����Ӧ�ó���Ϳ��Ի�ù�����
//    ����ʽ��ȷ�ġ�Сͼ�ꡣ
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FBXVIEWER));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_FBXVIEWER);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   ����: InitInstance(HINSTANCE, int)
//
//   Ŀ��: ����ʵ�����������������
//
//   ע��:
//
//        �ڴ˺����У�������ȫ�ֱ����б���ʵ�������
//        ��������ʾ�����򴰿ڡ�
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // ��ʵ������洢��ȫ�ֱ�����
    
    //�������������ھ���洢��ȫ�ֱ�����
    hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
       CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
    
    if (!hWnd)
    {
       return FALSE;
    }
    
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);


    /*
     timer
    */
    //����WaitableTimer
    HTimer = CreateWaitableTimer(NULL,FALSE,NULL);
    DebugAssert(NULL!=HTimer, "CreateWaitableTimer ʧ��: %d", GetLastError());
    //��ʼ��WaitableTimer
    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -1i64;   //1���ʼ��ʱ
    SetWaitableTimer(HTimer, &liDueTime, 100, NULL, NULL, 0);  //����200ms = 0.2s
    FrameCount = 0;

    /*
        GraphicsDevice
    */
    pGDevice = GraphicsDevice::getInstance(hWnd);
    pGDevice->BuildViewports();
    /*
        Camera
    */
    InitCamera();
    cube.SetVertexData(0);
    cube.Create(pGDevice->m_pD3DDevice);
    /*
        Axis
    */
	axis.SetVertexData(0);  //����ʵ���ϲ������� TODO: �Ľ����
    axis.Create(pGDevice->m_pD3DDevice);
    //axis.CreateXYZ(pGDevice->m_pD3DDevice);
    //axis.UpdateXYZ(fixedEyePoint);
    /*
        ��ȡfbx������Skinned mesh
    */
    // ��ȡ����ļ�·�� ������
    char fileSrc[MAX_PATH];
    GetTestFileName(fileSrc);
    skinMesh.Load(fileSrc, pGDevice->m_pD3DDevice);
    /*
        Matrix
    */
    D3DXMatrixIdentity(&identity);    
    D3DXMatrixPerspectiveFovLH(&proj, D3DX_PI / 4.0f,
        (float)pGDevice->mDefaultViewport.Width / (float)pGDevice->mDefaultViewport.Height, 1.0f, 1000.0f);    
    return TRUE;
}

//
//  ����: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  Ŀ��: ���������ڵ���Ϣ��
//
//  WM_COMMAND	- ����Ӧ�ó���˵�
//  WM_PAINT	- ����������
//  WM_DESTROY	- �����˳���Ϣ������
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// �����˵�ѡ��:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: �ڴ���������ͼ����...
		EndPaint(hWnd, &ps);
        break;
    case WM_MOUSEMOVE:
        moving = true;
        hovering = false;
        lastCursorPos = currentCursorPos;
        currentCursorPos.x = (float)GET_X_LPARAM(lParam);
		currentCursorPos.y = (float)GET_Y_LPARAM(lParam);
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_HOVER;
        tme.dwHoverTime = 100;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
        break;
    case WM_LBUTTONDOWN:
        lastCursorPos = currentCursorPos;
		currentCursorPos.x = (float)GET_X_LPARAM(lParam);
		currentCursorPos.y = (float)GET_Y_LPARAM(lParam);
        if (lastCursorPos!=currentCursorPos)
        {
            dragging = true;
        }
        else
        {
            dragging = false;
        }
        break;
    case WM_MOUSEHOVER:
        lastCursorPos = currentCursorPos;
		currentCursorPos.x = (float)GET_X_LPARAM(lParam);
		currentCursorPos.y = (float)GET_Y_LPARAM(lParam);
        hovering = true;
        moving = false;
        break;
    case WM_KEYDOWN:
        switch(wParam)
        {
        case VK_ESCAPE:    //��Esc�˳�
            PostQuitMessage(0);
            break;
        case 'P': //��'P'��ͣ/�ָ�
            Pause = Pause ? false : true;
            break;
        //camera ����
        case 'W':
            MoveCameraForward(3.0f);
            break;
        case 'S':
            MoveCameraBackward(3.0f);
            break;
        case 'A':
            MoveCameraLeft(3.0f);
            break;
        case 'D':
            MoveCameraRight(3.0f);
            break;
        case 'R':
            ResetCamera();
            break;
        //�ο���http://forums.codeguru.com/showthread.php?302925-Where-is-Page-Up-Down-Accelerator-Key&p=981700#post981700
        //Page Up : VK_PRIOR
        //Page Down : VK_NEXT
        case VK_PRIOR:
            MoveCameraUpward(3.0f);
            break;
        case VK_NEXT:
            MoveCameraDownward(3.0f);
            break;
        case 'Q':
            RotateCameraHorizontally(-D3DX_PI/45);
            break;
        case 'E':
            RotateCameraHorizontally(D3DX_PI/45);
            break;
        case VK_HOME:
            RotateCameraVertically(-D3DX_PI/45);
            break;
        case VK_END:
            RotateCameraVertically(D3DX_PI/45);
            break;
        case VK_SPACE:
            bStepForward = true;
            break;
        }
        break;
	case WM_DESTROY:
        Destroy();
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// �����ڡ������Ϣ�������
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}


void Update( unsigned int _dt )
{
    //Update camera    
    D3DXMatrixLookAtLH(&view, &eyePoint, &lookAt, &up);
    D3DXMatrixLookAtLH(&fixedView, &fixedEyePoint, &fixedLookAt, &fixedUp);
    cube.Update(currentCursorPos, identity, fixedView, pGDevice->m_matCubeProj);
    if (dragging)
    {
        //�϶�ʱ������ת
        D3DXVECTOR2 mousePosDetla(currentCursorPos-lastCursorPos);
        RotateCameraHorizontally(D3DX_PI/4*mousePosDetla.x);
        RotateCameraVertically(D3DX_PI/4*mousePosDetla.y);
    }
    axis.Update();
    //axis.UpdateXYZ(fixedEyePoint);
    
    skinMesh.Update(identity, _dt);
}


void Render( unsigned int _dt )
{
    //DebugPrintf("Frame count: %d\n", FrameCount);
    HRESULT hr = S_OK;

    //�������
    pGDevice->Clear();

    //��ʼ����
    pGDevice->BeginScene();

    //Render skinmesh
    skinMesh.Render(pGDevice->m_pD3DDevice, identity, view, proj, eyePoint);

    //Render cube
    {
        pGDevice->SetViewport(pGDevice->mCubeViewport);
        cube.SetConstants(pGDevice->m_pD3DDevice, identity, fixedView, pGDevice->m_matCubeProj);
        cube.Draw(pGDevice->m_pD3DDevice);
        if (hovering || moving)
        {
            //��cube���ڵ�viewport��λ��(top-left)������Ļ����任��(0,0)
            D3DXVECTOR2 tmp(currentCursorPos);
            tmp.x -= pGDevice->mCubeViewport.X;
            tmp.y -= pGDevice->mCubeViewport.Y;
            cube.DrawRay(tmp, identity, fixedView, pGDevice->m_matCubeProj);
        }
        pGDevice->ResetViewport();
    }

    //Render axis
    {
        pGDevice->SetViewport(pGDevice->mAxisViewport);
        axis.SetConstants(pGDevice->m_pD3DDevice, identity, fixedView, pGDevice->m_matAxisProj);
        axis.Draw(pGDevice->m_pD3DDevice);
        //axis.DrawXYZ();
        pGDevice->ResetViewport();
	}

    //��������
    pGDevice->EndScene();

    //��ʾ����
    pGDevice->Present();
}

void Destroy()
{
    axis.Destroy();
    cube.Destroy();
    skinMesh.Destroy();

    GraphicsDevice::ReleaseInstance();
}