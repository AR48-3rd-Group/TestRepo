#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define TriangleCount 3
#include <windows.h>
#include <d3d11_1.h>
#pragma comment(lib, "d3d11.lib")
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include <assert.h>
#include <wrl.h>

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
static bool             global_windowDidResize = false;
HWND                    g_hwnd;
ID3D11Device1*          g_d3d11Device;
ID3D11DeviceContext1*   g_d3d11DeviceContext;
IDXGISwapChain1*        g_d3d11SwapChain;
ID3D11RenderTargetView* g_d3d11FrameBufferView;
ID3D11VertexShader*     g_vertexShader;
ID3D11PixelShader*      g_pixelShader;
ID3D11InputLayout*      g_inputLayout;
ID3D11Buffer*           g_vertexBuffers[TriangleCount];
UINT                    g_numVerts;
UINT                    g_stride;
UINT                    g_offset;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
HRESULT InitWindow(HINSTANCE hInstance);
HRESULT InitDevice();
void CleanupDevice();
void Render();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    if (FAILED(InitWindow(hInstance)))
    {
        return GetLastError();
    }

    if (FAILED(InitDevice()))
    {
        CleanupDevice();
        return GetLastError();
    }
  
    // Main message loop
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            if (global_windowDidResize)
            {
                g_d3d11DeviceContext->OMSetRenderTargets(0, 0, 0);
                g_d3d11FrameBufferView->Release();

                HRESULT res = g_d3d11SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
                assert(SUCCEEDED(res));

                ID3D11Texture2D* d3d11FrameBuffer;
                res = g_d3d11SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3d11FrameBuffer);
                assert(SUCCEEDED(res));

                res = g_d3d11Device->CreateRenderTargetView(d3d11FrameBuffer, NULL,
                    &g_d3d11FrameBufferView);
                assert(SUCCEEDED(res));
                d3d11FrameBuffer->Release();

                global_windowDidResize = false;
            }

            Render();
        }
    }

    CleanupDevice();

    return (int)msg.wParam;
}

// 윈도우 프로시저 
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    switch (msg)
    {
    case WM_KEYDOWN:
    {
        if (wparam == VK_ESCAPE)
            DestroyWindow(hwnd);
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    case WM_SIZE:
    {
        // 윈도우 사이즈 가 변경되었을경우 true로 변경
        global_windowDidResize = true;
        break;
    }
    default:
        result = DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return result;
}

HRESULT InitWindow(HINSTANCE hInstance)
{
    // Open a window    
    WNDCLASSEXW winClass = {};
    winClass.cbSize = sizeof(WNDCLASSEXW);
    winClass.style = CS_HREDRAW | CS_VREDRAW;
    winClass.lpfnWndProc = &WndProc;
    winClass.hInstance = hInstance;
    winClass.hIcon = LoadIconW(0, IDI_APPLICATION);
    winClass.hCursor = LoadCursorW(0, IDC_ARROW);
    winClass.lpszClassName = L"MyWindowClass";
    winClass.hIconSm = LoadIconW(0, IDI_APPLICATION);

    if (!RegisterClassExW(&winClass)) {
        MessageBoxA(0, "RegisterClassEx failed", "Fatal Error", MB_OK);
        return E_FAIL;
    }

    RECT initialRect = { 0, 0, 1024, 768 };
    AdjustWindowRectEx(&initialRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);
    LONG initialWidth = initialRect.right - initialRect.left;
    LONG initialHeight = initialRect.bottom - initialRect.top;

    g_hwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
        winClass.lpszClassName,
        L"Drawing a Triangle",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        initialWidth,
        initialHeight,
        0, 0, hInstance, 0);

    if (!g_hwnd)
    {
        MessageBoxA(0, "CreateWindowEx failed", "Fatal Error", MB_OK);
        return E_FAIL;
    }    

    return S_OK;
}

HRESULT InitDevice()
{
    // Create D3D11 Device and Context
    {
        ID3D11Device* baseDevice;
        ID3D11DeviceContext* baseDeviceContext;
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        #if defined(DEBUG) || defined(_DEBUG)
                creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif

        HRESULT hResult = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE,
            0, creationFlags, featureLevels, ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION, &baseDevice,
            0, &baseDeviceContext);

        if (FAILED(hResult)) {
            MessageBoxA(0, "D3D11CreateDevice() failed", "Fatal Error", MB_OK);
            return E_FAIL;
        }

        // Get 1.1 Interface of D3D11 Device and Context
        hResult = baseDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&g_d3d11Device);
        assert(SUCCEEDED(hResult));
        baseDevice->Release();

        hResult = baseDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&g_d3d11DeviceContext);
        assert(SUCCEEDED(hResult));
        baseDeviceContext->Release();
    }

    #if defined(DEBUG) || defined(_DEBUG)
        // Set up debug layer to break on D3D11 errors
        ID3D11Debug* d3dDebug = nullptr;
        g_d3d11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug);
        if (d3dDebug)
        {
            ID3D11InfoQueue* d3dInfoQueue = nullptr;
            if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue)))
            {
                d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
                d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
                d3dInfoQueue->Release();
            }
            d3dDebug->Release();
        }
    #endif

    // Create Swap Chain
    {
        // Get DXGI Factory (needed to create Swap Chain)
        IDXGIFactory2* dxgiFactory;
        {
            IDXGIDevice1* dxgiDevice;
            HRESULT hResult = g_d3d11Device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDevice);
            assert(SUCCEEDED(hResult));

            IDXGIAdapter* dxgiAdapter;
            hResult = dxgiDevice->GetAdapter(&dxgiAdapter);
            assert(SUCCEEDED(hResult));
            dxgiDevice->Release();

            DXGI_ADAPTER_DESC adapterDesc;
            dxgiAdapter->GetDesc(&adapterDesc);

            OutputDebugStringA("Graphics Device: ");
            OutputDebugStringW(adapterDesc.Description);

            hResult = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);
            assert(SUCCEEDED(hResult));

            dxgiAdapter->Release();
        }

        // SwapChain description 클래스
        DXGI_SWAP_CHAIN_DESC1 d3d11SwapChainDesc = {};
        d3d11SwapChainDesc.Width = 0; // use window width
        d3d11SwapChainDesc.Height = 0; // use window height
        d3d11SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        d3d11SwapChainDesc.SampleDesc.Count = 1;
        d3d11SwapChainDesc.SampleDesc.Quality = 0;
        d3d11SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        d3d11SwapChainDesc.BufferCount = 2;
        d3d11SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        d3d11SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        d3d11SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        d3d11SwapChainDesc.Flags = 0;

        // SwapChain 생성
        HRESULT hResult = dxgiFactory->CreateSwapChainForHwnd(g_d3d11Device, g_hwnd, &d3d11SwapChainDesc, 0, 0, &g_d3d11SwapChain);
        assert(SUCCEEDED(hResult));

        dxgiFactory->Release();
    }

    // Create Framebuffer Render Target
    {
        ID3D11Texture2D* d3d11FrameBuffer;
        HRESULT hResult = g_d3d11SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3d11FrameBuffer);
        assert(SUCCEEDED(hResult));

        hResult = g_d3d11Device->CreateRenderTargetView(d3d11FrameBuffer, 0, &g_d3d11FrameBufferView);
        assert(SUCCEEDED(hResult));
        d3d11FrameBuffer->Release();
    }

    // Create Vertex Shader
    ID3DBlob* vsBlob;
    {
        ID3DBlob* shaderCompileErrorsBlob;
        HRESULT hResult = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VS_main", "vs_5_0", 0, 0, &vsBlob, &shaderCompileErrorsBlob);
        if (FAILED(hResult))
        {
            const char* errorString = NULL;
            if (hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                errorString = "Could not compile shader; file not found";
            else if (shaderCompileErrorsBlob) {
                errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();
                shaderCompileErrorsBlob->Release();
            }
            MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
            return E_FAIL;
        }

        hResult = g_d3d11Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vertexShader);
        assert(SUCCEEDED(hResult));
    }

    // Create Pixel Shader
    {
        ID3DBlob* psBlob;
        ID3DBlob* shaderCompileErrorsBlob;
        HRESULT hResult = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS_main", "ps_5_0", 0, 0, &psBlob, &shaderCompileErrorsBlob);
        if (FAILED(hResult))
        {
            const char* errorString = NULL;
            if (hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                errorString = "Could not compile shader; file not found";
            else if (shaderCompileErrorsBlob) {
                errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();
                shaderCompileErrorsBlob->Release();
            }
            MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
            return E_FAIL;
        }

        hResult = g_d3d11Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pixelShader);
        assert(SUCCEEDED(hResult));
        psBlob->Release();
    }

    // Create Input Layout
    {
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        HRESULT hResult = g_d3d11Device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_inputLayout);
        assert(SUCCEEDED(hResult));
        vsBlob->Release();
    }

    // Create Vertex Buffers
    {
        float vertexData[] = { // x, y, r, g, b, a
            -1.f,  -1.f, 0.f, 1.f, 0.f, 1.f,
            -0.5f, 0.f, 1.f, 0.f, 0.f, 1.f,
            0.f, -1.f, 0.f, 0.f, 1.f, 1.f
        };
        g_stride = 6 * sizeof(float);
        g_numVerts = sizeof(vertexData) / g_stride;
        g_offset = 0;

        D3D11_BUFFER_DESC vertexBufferDesc = {};
        vertexBufferDesc.ByteWidth = sizeof(vertexData);
        vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexSubresourceData = { vertexData };

        HRESULT hResult = g_d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &g_vertexBuffers[0]);
        assert(SUCCEEDED(hResult));
    }

    {
        float vertexData[] = { // x, y, r, g, b, a
            0.0f,  -1.f, 1.f, 0.f, 0.f, 1.f,
            0.5f, 0.f, 0.f, 1.f, 0.f, 1.f,
            1.f , -1.f, 0.f, 0.f, 1.f, 1.f
        };
        g_stride = 6 * sizeof(float);
        g_numVerts = sizeof(vertexData) / g_stride;
        g_offset = 0;

        D3D11_BUFFER_DESC vertexBufferDesc = {};
        vertexBufferDesc.ByteWidth = sizeof(vertexData);
        vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexSubresourceData = { vertexData };

        HRESULT hResult = g_d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &g_vertexBuffers[1]);
        assert(SUCCEEDED(hResult));
    }

    {
        float vertexData[] = { // x, y, r, g, b, a
            -0.5f,  0.f, 0.f, 0.f, 1.f, 1.f,
            0.f, 1.f, 0.f, 1.f, 0.f, 1.f,
            0.5f , 0.f, 1.f, 0.f, 0.f, 1.f
        };
        g_stride = 6 * sizeof(float);
        g_numVerts = sizeof(vertexData) / g_stride;
        g_offset = 0;

        D3D11_BUFFER_DESC vertexBufferDesc = {};
        vertexBufferDesc.ByteWidth = sizeof(vertexData);
        vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexSubresourceData = { vertexData };

        HRESULT hResult = g_d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &g_vertexBuffers[2]);
        assert(SUCCEEDED(hResult));
    }
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
//// COM 객체 메모리 누수 체크
//#if defined(DEBUG) || defined(_DEBUG)
//    Microsoft::WRL::ComPtr<ID3D11Debug> dxgiDebug;
//
//    if (SUCCEEDED(g_d3d11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&dxgiDebug)))
//    {
//        dxgiDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
//        dxgiDebug = nullptr;
//    }
//#endif


    if (g_d3d11DeviceContext) g_d3d11DeviceContext->ClearState();

    for (size_t i = 0; i < TriangleCount; i++)
    {
        if (g_vertexBuffers[i]) g_vertexBuffers[i]->Release();
    }

    if (g_inputLayout) g_inputLayout->Release();
    if (g_vertexShader) g_vertexShader->Release();
    if (g_pixelShader) g_pixelShader->Release();
    if (g_d3d11FrameBufferView) g_d3d11FrameBufferView->Release();
    if (g_d3d11SwapChain) g_d3d11SwapChain->Release();
    if (g_d3d11DeviceContext) g_d3d11DeviceContext->Release();
    if (g_d3d11Device) g_d3d11Device->Release();
}

//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
    FLOAT backgroundColor[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
    g_d3d11DeviceContext->ClearRenderTargetView(g_d3d11FrameBufferView, backgroundColor);

    RECT winRect;
    GetClientRect(g_hwnd, &winRect);
    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)(winRect.right - winRect.left), (FLOAT)(winRect.bottom - winRect.top), 0.0f, 1.0f };
    g_d3d11DeviceContext->RSSetViewports(1, &viewport);

    g_d3d11DeviceContext->OMSetRenderTargets(1, &g_d3d11FrameBufferView, nullptr);

    // 삼각형 렌더링
    for (size_t i = 0; i < TriangleCount; i++)
    {
        g_d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_d3d11DeviceContext->IASetInputLayout(g_inputLayout);

        g_d3d11DeviceContext->VSSetShader(g_vertexShader, nullptr, 0);
        g_d3d11DeviceContext->PSSetShader(g_pixelShader, nullptr, 0);

        g_d3d11DeviceContext->IASetVertexBuffers(0, 1, &g_vertexBuffers[i], &g_stride, &g_offset);

        g_d3d11DeviceContext->Draw(g_numVerts, 0);
    }

    g_d3d11SwapChain->Present(1, 0);
}
