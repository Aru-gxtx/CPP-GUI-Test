#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cmath> 
#include <ctime>  
#include <cstdlib> 
#include <algorithm> // For max/min logic

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

static HWND g_hwnd = nullptr; 
static bool g_MouseCaptured = false;

struct TerrainConfig {
    float seedX, seedZ;
};
TerrainConfig g_Terrain;

struct Camera {
    float x = 0.0f;
    float y = 10.0f; 
    float z = 0.0f;
    float yaw = 0.0f;
    float pitch = 0.4f;
    
    float velY = 0.0f;       // Vertical speed (gravity)
    bool isGrounded = false; // Are we touching the ground?
};
Camera g_Cam;

float GetTerrainHeight(float worldX, float worldZ) {
    float biome = sin((worldX + g_Terrain.seedX) * 0.05f) * cos((worldZ + g_Terrain.seedZ) * 0.05f);
    float base = sin(worldX * 0.15f) + cos(worldZ * 0.15f);
    float detail = sin(worldX * 0.6f) * cos(worldZ * 0.6f);
    
    float mountainFactor = biome; 
    if (mountainFactor < 0.0f) mountainFactor = 0.0f; 
    mountainFactor = mountainFactor * mountainFactor; 

    float rawHeight = (base + detail) * mountainFactor * 5.0f;
    
    return floor(rawHeight) + 1.0f; 
}

void UpdateCamera(float dt) {
    float moveSpeed = 10.0f * dt;
    float gravity = 25.0f * dt;
    float jumpForce = 9.0f;
    float playerEyeHeight = 1.8f; 
    float bodyRadius = 0.2f; 

    if (g_MouseCaptured && g_hwnd) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        RECT rect; GetClientRect(g_hwnd, &rect);
        POINT center = { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
        ClientToScreen(g_hwnd, &center);
        POINT cur; GetCursorPos(&cur);
        float deltaX = (float)(cur.x - center.x);
        float deltaY = (float)(cur.y - center.y);

        g_Cam.yaw   += deltaX * 0.005f;
        g_Cam.pitch -= deltaY * 0.005f; 
        if (g_Cam.pitch > 1.5f) g_Cam.pitch = 1.5f;
        if (g_Cam.pitch < -1.5f) g_Cam.pitch = -1.5f;
        SetCursorPos(center.x, center.y);
    }

    float fwdX = sin(g_Cam.yaw); float fwdZ = cos(g_Cam.yaw);
    float rgtX = cos(g_Cam.yaw); float rgtZ = -sin(g_Cam.yaw);
    
    float inputX = 0.0f; float inputZ = 0.0f;
    if (ImGui::IsKeyDown(ImGuiKey_W)) { inputX += fwdX; inputZ += fwdZ; }
    if (ImGui::IsKeyDown(ImGuiKey_S)) { inputX -= fwdX; inputZ -= fwdZ; }
    if (ImGui::IsKeyDown(ImGuiKey_D)) { inputX += rgtX; inputZ += rgtZ; }
    if (ImGui::IsKeyDown(ImGuiKey_A)) { inputX -= rgtX; inputZ -= rgtZ; }

    if (inputX != 0.0f || inputZ != 0.0f) {
        float len = sqrt(inputX * inputX + inputZ * inputZ);
        inputX /= len; inputZ /= len;

        float currentGround = GetTerrainHeight(g_Cam.x, g_Cam.z);
        float footY = g_Cam.y - playerEyeHeight;
        
        float clearance = footY - (currentGround - 1.0f); // Assuming ground is drawn 1 unit down?

        float nextX = g_Cam.x + inputX * moveSpeed;
        float checkX = nextX + (inputX > 0 ? bodyRadius : -bodyRadius);
        float groundAtX = GetTerrainHeight(checkX, g_Cam.z);
        float stepHeightX = groundAtX - currentGround;

        if (stepHeightX <= 0.0f || clearance > stepHeightX) {
            g_Cam.x = nextX;
        }

        float nextZ = g_Cam.z + inputZ * moveSpeed;
        float checkZ = nextZ + (inputZ > 0 ? bodyRadius : -bodyRadius);
        float groundAtZ = GetTerrainHeight(g_Cam.x, checkZ); // Use current X (or updated X)
        float stepHeightZ = groundAtZ - currentGround;

        if (stepHeightZ <= 0.0f || clearance > stepHeightZ) {
            g_Cam.z = nextZ;
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Space) && g_Cam.isGrounded) {
        g_Cam.velY = jumpForce;
        g_Cam.isGrounded = false;
    }

    float groundHeight = GetTerrainHeight(g_Cam.x, g_Cam.z) - 1.0f + playerEyeHeight;
    
    g_Cam.velY -= gravity;
    g_Cam.y += g_Cam.velY * dt;

    if (g_Cam.y < groundHeight && g_Cam.velY <= 0.0f) {
        g_Cam.y = groundHeight; 
        g_Cam.velY = 0.0f;
        g_Cam.isGrounded = true;
    } else {
        g_Cam.isGrounded = false;
    }
}

class SimpleFrameBuffer {
public:
    ID3D11RenderTargetView* RenderTargetView = nullptr;
    ID3D11ShaderResourceView* ShaderResourceView = nullptr;
    ID3D11DepthStencilView* DepthStencilView = nullptr;
    D3D11_VIEWPORT Viewport = {};

    SimpleFrameBuffer(ID3D11Device* device, int width, int height) {
        Resize(device, width, height);
    }
    void Resize(ID3D11Device* device, int width, int height) {
        if (RenderTargetView) { RenderTargetView->Release(); RenderTargetView = nullptr; }
        if (ShaderResourceView) { ShaderResourceView->Release(); ShaderResourceView = nullptr; }
        if (DepthStencilView) { DepthStencilView->Release(); DepthStencilView = nullptr; }

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width; texDesc.Height = height;
        texDesc.MipLevels = 1; texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1; texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        ID3D11Texture2D* pTexture = nullptr;
        device->CreateTexture2D(&texDesc, nullptr, &pTexture);
        device->CreateRenderTargetView(pTexture, nullptr, &RenderTargetView);
        device->CreateShaderResourceView(pTexture, nullptr, &ShaderResourceView);
        pTexture->Release();

        D3D11_TEXTURE2D_DESC depthDesc = texDesc;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ID3D11Texture2D* pDepthTexture = nullptr;
        device->CreateTexture2D(&depthDesc, nullptr, &pDepthTexture);
        device->CreateDepthStencilView(pDepthTexture, nullptr, &DepthStencilView);
        pDepthTexture->Release();

        Viewport.TopLeftX = 0; Viewport.TopLeftY = 0;
        Viewport.Width = (float)width; Viewport.Height = (float)height;
        Viewport.MinDepth = 0.0f; Viewport.MaxDepth = 1.0f;
    }
    void Bind(ID3D11DeviceContext* context) {
        context->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
        context->RSSetViewports(1, &Viewport);
    }
    void Unbind(ID3D11DeviceContext* context, ID3D11RenderTargetView* mainRTV) {
        context->OMSetRenderTargets(1, &mainRTV, nullptr);
    }
};

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

struct CBufferData {
    float camX, camY, camZ, padding1;
    float camYaw, camPitch, padding2, padding3;
    float objX, objY, objZ, padding4;
    float isTopBlock; 
    float padding5, padding6, padding7;
};

ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr;
ID3D11RasterizerState* g_pRasterizerState = nullptr;

const char* shaderCode = R"(
cbuffer CBuf : register(b0) { 
    float camX; float camY; float camZ; float p1;
    float camYaw; float camPitch; float p2; float p3;
    float objX; float objY; float objZ; float p4;
    float isTopBlock; float p5; float p6; float p7;
}
struct VS_Input { float3 pos : POSITION; float4 col : COLOR; };
struct PS_Input { float4 pos : SV_POSITION; float4 col : COLOR; float worldY : TEXCOORD0; };

PS_Input VS(VS_Input input) {
    PS_Input output;
    float3 worldPos = input.pos + float3(objX, objY, objZ);
    output.worldY = worldPos.y; 
    
    float3 viewPos = worldPos - float3(camX, camY, camZ);
    float c = cos(camYaw); float s = sin(camYaw);
    float3 t; t.x = viewPos.x * c - viewPos.z * s; t.y = viewPos.y; t.z = viewPos.x * s + viewPos.z * c;
    c = cos(camPitch); s = sin(camPitch);
    float3 f; f.x = t.x; f.y = t.y * c - t.z * s; f.z = t.y * s + t.z * c;
    
    float fov = 1.3; 
    output.pos.x = f.x * fov;
    output.pos.y = f.y * fov * 1.33; 
    output.pos.z = f.z - 0.1; 
    output.pos.w = f.z; 
    
    output.col = input.col;
    return output;
}

float4 PS(PS_Input input) : SV_Target {
    float h = input.worldY;
    float4 tint = float4(1,1,1,1);
    
    if (isTopBlock > 0.5) {
        if (h < -1.5)       tint = float4(0.0, 0.4, 0.8, 1.0); // Water
        else if (h < 0.5)   tint = float4(0.2, 0.8, 0.2, 1.0); // Grass
        else if (h < 4.0)   tint = float4(0.5, 0.5, 0.5, 1.0); // Stone
        else                tint = float4(1.0, 1.0, 1.0, 1.0); // Snow
    } else {
        if (h < -3.0)       tint = float4(0.1, 0.1, 0.1, 1.0); // Bedrock
        else                tint = float4(0.35, 0.25, 0.2, 1.0); // Dirt
    }
    return input.col * 0.4 + tint * 0.6;
}
)";

float RandomFloat() { return (float)(rand() % 1000) / 10.0f; }

bool InitGraphics(ID3D11Device* device) {
    srand((unsigned int)time(0)); 
    g_Terrain.seedX = RandomFloat(); 
    g_Terrain.seedZ = RandomFloat();

    HRESULT hr;
    ID3DBlob* pVSBlob = nullptr; ID3DBlob* pPSBlob = nullptr;
    D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &pVSBlob, nullptr);
    device->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &pPSBlob, nullptr);
    device->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    pPSBlob->Release();

    D3D11_INPUT_ELEMENT_DESC ied[] = { {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}, {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0} };
    device->CreateInputLayout(ied, 2, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pInputLayout);
    pVSBlob->Release();

    float l = -0.5f; float r = 0.5f; 
    Vertex vertices[] = {
        { l,l,l, 0.7f,0.7f,0.7f,1 }, { l,r,l, 0.7f,0.7f,0.7f,1 }, { r,r,l, 0.7f,0.7f,0.7f,1 }, { l,l,l, 0.7f,0.7f,0.7f,1 }, { r,r,l, 0.7f,0.7f,0.7f,1 }, { r,l,l, 0.7f,0.7f,0.7f,1 }, 
        { l,l,r, 0.6f,0.6f,0.6f,1 }, { r,r,r, 0.6f,0.6f,0.6f,1 }, { l,r,r, 0.6f,0.6f,0.6f,1 }, { l,l,r, 0.6f,0.6f,0.6f,1 }, { r,l,r, 0.6f,0.6f,0.6f,1 }, { r,r,r, 0.6f,0.6f,0.6f,1 }, 
        { l,l,r, 0.5f,0.5f,0.5f,1 }, { l,r,l, 0.5f,0.5f,0.5f,1 }, { l,l,l, 0.5f,0.5f,0.5f,1 }, { l,l,r, 0.5f,0.5f,0.5f,1 }, { l,r,r, 0.5f,0.5f,0.5f,1 }, { l,r,l, 0.5f,0.5f,0.5f,1 }, 
        { r,l,l, 0.5f,0.5f,0.5f,1 }, { r,r,l, 0.5f,0.5f,0.5f,1 }, { r,l,r, 0.5f,0.5f,0.5f,1 }, { r,l,r, 0.5f,0.5f,0.5f,1 }, { r,r,l, 0.5f,0.5f,0.5f,1 }, { r,r,r, 0.5f,0.5f,0.5f,1 }, 
        { l,r,l, 1.0f,1.0f,1.0f,1 }, { l,r,r, 1.0f,1.0f,1.0f,1 }, { r,r,r, 1.0f,1.0f,1.0f,1 }, { l,r,l, 1.0f,1.0f,1.0f,1 }, { r,r,r, 1.0f,1.0f,1.0f,1 }, { r,r,l, 1.0f,1.0f,1.0f,1 }, 
        { l,l,l, 0.3f,0.3f,0.3f,1 }, { r,l,r, 0.3f,0.3f,0.3f,1 }, { l,l,r, 0.3f,0.3f,0.3f,1 }, { l,l,l, 0.3f,0.3f,0.3f,1 }, { r,l,l, 0.3f,0.3f,0.3f,1 }, { r,l,r, 0.3f,0.3f,0.3f,1 }, 
    };

    D3D11_BUFFER_DESC bd = {}; bd.Usage = D3D11_USAGE_DEFAULT; bd.ByteWidth = sizeof(vertices); bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = {}; initData.pSysMem = vertices;
    device->CreateBuffer(&bd, &initData, &g_pVertexBuffer);

    D3D11_BUFFER_DESC cbd = {}; cbd.Usage = D3D11_USAGE_DYNAMIC; cbd.ByteWidth = 64; cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    D3D11_RASTERIZER_DESC rsDesc = {}; rsDesc.FillMode = D3D11_FILL_SOLID; rsDesc.CullMode = D3D11_CULL_BACK; 
    device->CreateRasterizerState(&rsDesc, &g_pRasterizerState);

    return true;
}

void RenderGraphics(ID3D11DeviceContext* context) {
    context->RSSetState(g_pRasterizerState);
    UINT stride = sizeof(Vertex); UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    context->IASetInputLayout(g_pInputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(g_pVertexShader, nullptr, 0);
    context->PSSetShader(g_pPixelShader, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

    int camGridX = (int)floor(g_Cam.x);
    int camGridZ = (int)floor(g_Cam.z);
    int range = 32; 

    for (int x = camGridX - range; x <= camGridX + range; x++) {
        for (int z = camGridZ - range; z <= camGridZ + range; z++) {
            
            float worldX = (float)x;
            float worldZ = (float)z;

            int stackHeight = (int)(GetTerrainHeight(worldX, worldZ) - 1.0f);
            int floorLevel = -6; 

            for (int y = floorLevel; y <= stackHeight; y++) {
                D3D11_MAPPED_SUBRESOURCE mappedResource;
                context->Map(g_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
                CBufferData* dataPtr = (CBufferData*)mappedResource.pData;
                
                dataPtr->camX = g_Cam.x; dataPtr->camY = g_Cam.y; dataPtr->camZ = g_Cam.z;
                dataPtr->camYaw = g_Cam.yaw; dataPtr->camPitch = g_Cam.pitch;
                dataPtr->objX = worldX; dataPtr->objY = (float)y; dataPtr->objZ = worldZ;
                dataPtr->isTopBlock = (y == stackHeight) ? 1.0f : 0.0f; 

                context->Unmap(g_pConstantBuffer, 0);
                context->Draw(36, 0);
            }
        }
    }
}

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
SimpleFrameBuffer* myFB = nullptr;
UINT g_ResizeWidth = 0, g_ResizeHeight = 0;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int main(int, char**) {
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    g_hwnd = ::CreateWindowW(wc.lpszClassName, L"CPP 3D GUI Test", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1280 * main_scale), (int)(800 * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(g_hwnd)) { CleanupDeviceD3D(); return 1; }
    
    InitGraphics(g_pd3dDevice); 
    myFB = new SimpleFrameBuffer(g_pd3dDevice, 800, 600);

    ::ShowWindow(g_hwnd, SW_SHOWDEFAULT); ::UpdateWindow(g_hwnd);
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark(); ImGui::GetStyle().ScaleAllSizes(main_scale);
    ImGui_ImplWin32_Init(g_hwnd); ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg); ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        UpdateCamera(io.DeltaTime);

        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        myFB->Bind(g_pd3dDeviceContext);
        float bgColor[4] = { 0.6f, 0.8f, 1.0f, 1.0f }; 
        g_pd3dDeviceContext->ClearRenderTargetView(myFB->RenderTargetView, bgColor);
        g_pd3dDeviceContext->ClearDepthStencilView(myFB->DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
        
        RenderGraphics(g_pd3dDeviceContext);
        
        myFB->Unbind(g_pd3dDeviceContext, g_mainRenderTargetView);

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::Begin("3D Viewport", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
            ImGui::PopStyleVar();
            ImVec2 size = ImGui::GetContentRegionAvail();
            if (size.x != myFB->Viewport.Width || size.y != myFB->Viewport.Height)
                myFB->Resize(g_pd3dDevice, (int)size.x, (int)size.y);
            ImGui::Image((void*)myFB->ShaderResourceView, size);
            
            ImGui::SetCursorPos(ImVec2(20, 20));
            ImGui::TextColored(ImVec4(1,1,0,1), "X: %.1f Y: %.1f Z: %.1f", g_Cam.x, g_Cam.y, g_Cam.z);

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) g_MouseCaptured = !g_MouseCaptured;
        ImGui::End();

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        float clear_color[4] = {0,0,0,1};
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    CleanupDeviceD3D(); ::DestroyWindow(g_hwnd);
    return 0;
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2; sd.BufferDesc.Width = 0; sd.BufferDesc.Height = 0; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60; sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;
    CreateRenderTarget(); return true;
}
void CleanupDeviceD3D() { CleanupRenderTarget(); if (g_pSwapChain) g_pSwapChain->Release(); if (g_pd3dDeviceContext) g_pd3dDeviceContext->Release(); if (g_pd3dDevice) g_pd3dDevice->Release(); }
void CreateRenderTarget() { ID3D11Texture2D* pBackBuffer; g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)); g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView); pBackBuffer->Release(); }
void CleanupRenderTarget() { if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; } }
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE: if (wParam != SIZE_MINIMIZED) { g_ResizeWidth = (UINT)LOWORD(lParam); g_ResizeHeight = (UINT)HIWORD(lParam); } return 0;
    case WM_DESTROY: ::PostQuitMessage(0); return 0;
    } return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}