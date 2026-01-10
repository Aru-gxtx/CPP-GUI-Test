#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <math.h> // For sqrt()
#include <stdlib.h> // For rand()

#include <vector>

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct Enemy {
    ImVec2 position; 
    ImVec2 velocity; 
};

static std::vector<Enemy> enemies;

static float spawn_timer = 0.0f;
static float current_spawn_rate = 2.0f; // Start: Spawn 1 enemy every 2 seconds

static float pos_x = 100.0f;
static float pos_y = 100.0f;

const float MAX_SPRINT_TIME = 1.0f;  // Can run for 1 seconds
const float COOLDOWN_TIME   = 1.5f;  // Must wait 1.5 seconds if overheated

static float sprint_timer   = 0.0f;  // How long we've been running
static float cooldown_timer = 0.0f;  // Timer for the penalty

int game_state = 0; 

int score = 0;
int high_score = 0;

void ResetGame() {
    score = 0;
    high_score = (score > high_score) ? score : high_score;
    
    pos_x = 100.0f;
    pos_y = 100.0f;
    sprint_timer = 0.0f;
    cooldown_timer = 0.0f;

    enemies.clear();

    spawn_timer = 0.0f;
    current_spawn_rate = 2.0f; // Reset to easy mode
}

bool LoadTextureFromFile(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA subResource; 
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    
    ID3D11Texture2D* pTexture = NULL;
    g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    
    g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
    pTexture->Release();

    *out_width = image_width;
    *out_height = image_height;
    stbi_image_free(image_data);

    return true;
}

int main(int, char**)
{
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"cpp_gui_test_ID", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"CPP GUI Test", WS_OVERLAPPEDWINDOW, 100, 100, (int)(960 * main_scale), (int)(600 * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        
    style.FontScaleDpi = main_scale;        

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    int my_image_width = 0;
    int my_image_height = 0;
    ID3D11ShaderResourceView* my_texture = NULL;
    bool ret = LoadTextureFromFile("gallegodz.png", &my_texture, &my_image_width, &my_image_height);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("MainCanvas", nullptr, window_flags);
        
        ImVec2 win_size = ImGui::GetWindowSize();
        float dt = io.DeltaTime;
        if (game_state == 0)
        {
            const char* title = "GalleDodge";
            float text_width = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPos(ImVec2((win_size.x - text_width) * 0.5f, win_size.y * 0.4f));
            ImGui::Text(title);

            float btn_width = 100.0f;
            ImGui::SetCursorPos(ImVec2((win_size.x - btn_width) * 0.5f, win_size.y * 0.5f));
            
            if (ImGui::Button("START GAME", ImVec2(btn_width, 40.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space))
            {
                ResetGame();
                game_state = 1; // Switch to Playing
            }
        }
        else if (game_state == 1)
        {
            float PLAYER_SIZE = 30.0f; 

            float move_speed = 150.0f; // Default "Walk" speed every frame

            if (cooldown_timer > 0.0f) {
                cooldown_timer -= dt;
            }
            else if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
                move_speed = 300.0f;    
                sprint_timer += dt;     
                
                if (sprint_timer >= MAX_SPRINT_TIME) {
                    cooldown_timer = COOLDOWN_TIME; 
                    sprint_timer = 0.0f;            
                }
            }
            else {
                float recovery_speed = 1.5f; // Recover faster than you drain
                sprint_timer -= recovery_speed * dt;
                if (sprint_timer < 0.0f) sprint_timer = 0.0f;
            }

            float step = move_speed * dt;

            if (ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_A)) pos_x -= step;
            if (ImGui::IsKeyDown(ImGuiKey_RightArrow)|| ImGui::IsKeyDown(ImGuiKey_D)) pos_x += step;
            if (ImGui::IsKeyDown(ImGuiKey_UpArrow)   || ImGui::IsKeyDown(ImGuiKey_W)) pos_y -= step;
            if (ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_S)) pos_y += step;

            if (pos_x < 0) pos_x = 0;
            if (pos_y < 0) pos_y = 0;
            if (pos_x > win_size.x - PLAYER_SIZE) pos_x = win_size.x - PLAYER_SIZE;
            if (pos_y > win_size.y - PLAYER_SIZE) pos_y = win_size.y - PLAYER_SIZE;

            spawn_timer -= dt;

            if (spawn_timer <= 0.0f)
            {
                Enemy new_enemy;
                
                int edge = rand() % 4;
                ImVec2 spawn;
                if (edge == 0) spawn = ImVec2((float)(rand() % (int)win_size.x), -20);
                else if (edge == 1) spawn = ImVec2(win_size.x + 20, (float)(rand() % (int)win_size.y));
                else if (edge == 2) spawn = ImVec2((float)(rand() % (int)win_size.x), win_size.y + 20);
                else spawn = ImVec2(-20, (float)(rand() % (int)win_size.y));

                new_enemy.position = spawn;

                float dx = pos_x - spawn.x;
                float dy = pos_y - spawn.y;
                float len = sqrtf(dx*dx + dy*dy);
                if (len > 0) { dx /= len; dy /= len; }
                
                new_enemy.velocity = ImVec2(dx * 500.0f, dy * 500.0f);
                
                enemies.push_back(new_enemy);

                spawn_timer = current_spawn_rate;

                current_spawn_rate *= 0.98f; 
                
                if (current_spawn_rate < 0.2f) current_spawn_rate = 0.2f;
            }

            float player_center_x = pos_x + (PLAYER_SIZE * 0.5f);
            float player_center_y = pos_y + (PLAYER_SIZE * 0.5f);
            float player_radius = PLAYER_SIZE * 0.4f; 
            float enemy_radius = 20.0f;

            for (int i = enemies.size() - 1; i >= 0; i--)
            {
                enemies[i].position.x += enemies[i].velocity.x * dt;
                enemies[i].position.y += enemies[i].velocity.y * dt;

                ImGui::GetWindowDrawList()->AddCircleFilled(enemies[i].position, enemy_radius, IM_COL32(255, 0, 0, 255));

                float dx = player_center_x - enemies[i].position.x;
                float dy = player_center_y - enemies[i].position.y;
                if (sqrtf(dx*dx + dy*dy) < (player_radius + enemy_radius))
                {
                    if (score > high_score) high_score = score;
                    game_state = 2; 
                }

                if (enemies[i].position.x < -100 || enemies[i].position.x > win_size.x + 100 ||
                    enemies[i].position.y < -100 || enemies[i].position.y > win_size.y + 100) 
                {
                    score++; // Score when you dodge them successfully
                    enemies.erase(enemies.begin() + i); // Remove from list
                }
            }

            ImGui::SetCursorPos(ImVec2(pos_x, pos_y));
            if (my_texture) ImGui::Image((void*)my_texture, ImVec2(PLAYER_SIZE, PLAYER_SIZE));
            else ImGui::Button("P", ImVec2(PLAYER_SIZE, PLAYER_SIZE));

            ImGui::SetCursorPos(ImVec2(20, 20));
            ImGui::Text("SCORE: %d", score);
            ImGui::Text("Spawn Rate: %.2fs", current_spawn_rate); // Debug text so you can see it getting faster

            float bar_w = 300.0f;
            ImGui::SetCursorPos(ImVec2((win_size.x - bar_w)*0.5f, win_size.y - 40));
            if (cooldown_timer > 0) {
                 ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1,0,0,1));
                 ImGui::ProgressBar(cooldown_timer/COOLDOWN_TIME, ImVec2(bar_w, 20), "OVERHEATED");
                 ImGui::PopStyleColor();
            } else {
                 ImGui::ProgressBar(sprint_timer/MAX_SPRINT_TIME, ImVec2(bar_w, 20), "Sprint");
            }
        }
        else if (game_state == 2)
        {
            const char* title = "GAME OVER";
            float w = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPos(ImVec2((win_size.x - w) * 0.5f, win_size.y * 0.3f));
            ImGui::TextColored(ImVec4(1, 0, 0, 1), title); // Red Text

            char score_text[64];
            sprintf(score_text, "Score: %d", score);
            w = ImGui::CalcTextSize(score_text).x;
            ImGui::SetCursorPos(ImVec2((win_size.x - w) * 0.5f, win_size.y * 0.4f));
            ImGui::Text(score_text);

            sprintf(score_text, "High Score: %d", high_score);
            w = ImGui::CalcTextSize(score_text).x;
            ImGui::SetCursorPos(ImVec2((win_size.x - w) * 0.5f, win_size.y * 0.45f));
            ImGui::TextColored(ImVec4(0, 1, 0, 1), score_text); // Green Text

            float btn_width = 120.0f;
            ImGui::SetCursorPos(ImVec2((win_size.x - btn_width) * 0.5f, win_size.y * 0.6f));
            if (ImGui::Button("TRY AGAIN", ImVec2(btn_width, 40.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space))
            {
                ResetGame();
                game_state = 1; // Restart
            }
        }

        ImGui::End();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}