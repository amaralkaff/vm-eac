#include "gamebar_hijack.h"
#include "../OS-ImGui/imgui/imgui.h"
#include "../OS-ImGui/imgui/imgui_impl_dx11.h"
#include "../OS-ImGui/imgui/imgui_impl_win32.h"
#include "../game/feature_manager.h"
#include <random>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
extern FeatureManager* g_FeatureManager;

// Global overlay pointer for WndProc
static GameBarOverlay* g_gameBarOverlay = nullptr;

static LRESULT CALLBACK GameBarWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    if ( ImGui_ImplWin32_WndProcHandler( hWnd, msg, wParam, lParam ) )
        return true;

    switch ( msg )
    {
    case WM_HOTKEY:
        if ( g_FeatureManager )
            g_FeatureManager->ProcessHotkeyMessage( ( int ) wParam );
        return 0;
    case WM_CREATE:
    {
        MARGINS margin = { -1 };
        DwmExtendFrameIntoClientArea( hWnd, &margin );
        break;
    }
    case WM_SIZE:
        // Handle resize if needed
        return 0;
    case WM_DESTROY:
        PostQuitMessage( 0 );
        return 0;
    }

    return DefWindowProcW( hWnd, msg, wParam, lParam );
}

// Generate a random class name that looks like a system window
static std::wstring GenerateClassName( )
{
    const wchar_t* prefixes[] = {
        L"Windows.UI.Input.",
        L"Windows.UI.Core.",
        L"DirectUI.Element.",
        L"Shell.TrayWnd.",
    };

    std::random_device rd;
    std::mt19937 gen( rd( ) );
    std::uniform_int_distribution<> prefDist( 0, 3 );
    std::uniform_int_distribution<> numDist( 100, 999 );

    return std::wstring( prefixes[prefDist( gen )] ) + std::to_wstring( numDist( gen ) );
}

GameBarOverlay::~GameBarOverlay( )
{
    Cleanup( );
}

bool GameBarOverlay::Run( HWND gameWindow, std::function<void( )> renderCallback )
{
    m_gameHwnd = gameWindow;
    m_callback = renderCallback;
    g_gameBarOverlay = this;

    // Try to resolve CreateWindowInBand from user32
    HMODULE user32 = GetModuleHandleW( L"user32.dll" );
    if ( user32 )
        m_createWindowInBand = ( fnCreateWindowInBand ) GetProcAddress( user32, "CreateWindowInBand" );

    if ( !CreateStealthWindow( ) )
        return false;

    if ( !InitD3D( ) )
        return false;

    if ( !InitImGui( ) )
        return false;

    m_running = true;
    MainLoop( );

    return true;
}

void GameBarOverlay::Stop( )
{
    m_running = false;
}

bool GameBarOverlay::CreateStealthWindow( )
{
    m_hInstance = GetModuleHandleW( nullptr );

    std::wstring className = GenerateClassName( );

    m_wc = {};
    m_wc.cbSize = sizeof( WNDCLASSEXW );
    m_wc.style = CS_CLASSDC;
    m_wc.lpfnWndProc = GameBarWndProc;
    m_wc.hInstance = m_hInstance;
    m_wc.lpszClassName = _wcsdup( className.c_str( ) );

    if ( !RegisterClassExW( &m_wc ) )
        return false;

    int screenW = GetSystemMetrics( SM_CXSCREEN );
    int screenH = GetSystemMetrics( SM_CYSCREEN );

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    DWORD style = WS_POPUP;

    if ( m_createWindowInBand )
    {
        // Use CreateWindowInBand to place our window in the Game Bar's rendering band
        // ZBID_UIACCESS renders above fullscreen exclusive DirectX games
        m_hwnd = m_createWindowInBand(
            exStyle,
            m_wc.lpszClassName,
            L"",
            style,
            0, 0, screenW, screenH,
            nullptr, nullptr,
            m_hInstance,
            nullptr,
            ZBID_UIACCESS
        );
    }

    if ( !m_hwnd )
    {
        // Fallback: standard window creation with TOPMOST
        m_hwnd = CreateWindowExW(
            exStyle,
            m_wc.lpszClassName,
            L"",
            style,
            0, 0, screenW, screenH,
            nullptr, nullptr,
            m_hInstance,
            nullptr
        );
    }

    if ( !m_hwnd )
        return false;

    // Make window transparent
    SetLayeredWindowAttributes( m_hwnd, 0, 255, LWA_ALPHA );

    // Extend DWM frame for transparent rendering
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea( m_hwnd, &margins );

    // Hide from screenshots and screen capture (anti-stream-snipe + stealth)
    SetWindowDisplayAffinity( m_hwnd, WDA_EXCLUDEFROMCAPTURE );

    ShowWindow( m_hwnd, SW_SHOWDEFAULT );
    UpdateWindow( m_hwnd );

    if ( g_FeatureManager && m_hwnd )
        g_FeatureManager->SetTargetWindow( m_hwnd );

    return true;
}

bool GameBarOverlay::InitD3D( )
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION,
        &sd, &m_swapChain, &m_device, &featureLevel, &m_context
    );

    if ( FAILED( hr ) )
    {
        // Try WARP fallback
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            levels, 2, D3D11_SDK_VERSION,
            &sd, &m_swapChain, &m_device, &featureLevel, &m_context
        );
    }

    if ( FAILED( hr ) )
        return false;

    // Create render target view
    ID3D11Texture2D* backBuffer = nullptr;
    m_swapChain->GetBuffer( 0, IID_PPV_ARGS( &backBuffer ) );
    if ( backBuffer )
    {
        m_device->CreateRenderTargetView( backBuffer, nullptr, &m_rtv );
        backBuffer->Release( );
    }

    return m_rtv != nullptr;
}

bool GameBarOverlay::InitImGui( )
{
    ImGui::CreateContext( );
    ImGuiIO& io = ImGui::GetIO( );
    io.Fonts->AddFontDefault( );

    // Load fonts matching the OS-ImGui setup
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromFileTTF( "c:\\Windows\\Fonts\\msyh.ttc", 20.0f, &fontConfig, io.Fonts->GetGlyphRangesAll( ) );
    io.Fonts->AddFontFromFileTTF( "c:\\Windows\\Fonts\\msyh.ttc", 18.0f, &fontConfig, io.Fonts->GetGlyphRangesAll( ) );

    ImGui::AimStarDefaultStyle( );
    io.LogFilename = nullptr;
    io.IniFilename = nullptr;

    if ( !ImGui_ImplWin32_Init( m_hwnd ) )
        return false;

    if ( !ImGui_ImplDX11_Init( m_device, m_context ) )
        return false;

    return true;
}

bool GameBarOverlay::UpdatePosition( )
{
    if ( !m_gameHwnd || !IsWindow( m_gameHwnd ) )
        return false;

    RECT rect = {};
    POINT point = {};

    GetClientRect( m_gameHwnd, &rect );
    ClientToScreen( m_gameHwnd, &point );

    int w = rect.right;
    int h = rect.bottom;

    SetWindowPos( m_hwnd, HWND_TOPMOST, point.x, point.y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW );

    // Update mouse position for ImGui
    POINT mousePos;
    GetCursorPos( &mousePos );
    ScreenToClient( m_hwnd, &mousePos );
    ImGui::GetIO( ).MousePos.x = static_cast<float>( mousePos.x );
    ImGui::GetIO( ).MousePos.y = static_cast<float>( mousePos.y );

    // Toggle click-through based on whether ImGui wants input
    LONG_PTR exStyle = GetWindowLongPtrW( m_hwnd, GWL_EXSTYLE );
    if ( ImGui::GetIO( ).WantCaptureMouse )
        SetWindowLongPtrW( m_hwnd, GWL_EXSTYLE, exStyle & ( ~WS_EX_TRANSPARENT ) );
    else
        SetWindowLongPtrW( m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT );

    return true;
}

void GameBarOverlay::MainLoop( )
{
    while ( m_running )
    {
        // Process messages
        MSG msg;
        while ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
            if ( msg.message == WM_QUIT )
            {
                m_running = false;
                return;
            }
        }

        // Track game window
        if ( !UpdatePosition( ) )
        {
            m_running = false;
            return;
        }

        // Render frame
        ImGui_ImplDX11_NewFrame( );
        ImGui_ImplWin32_NewFrame( );
        ImGui::NewFrame( );

        if ( m_callback )
            m_callback( );

        ImGui::Render( );

        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_context->OMSetRenderTargets( 1, &m_rtv, nullptr );
        m_context->ClearRenderTargetView( m_rtv, clearColor );
        ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );

        m_swapChain->Present( 1, 0 );
    }
}

void GameBarOverlay::Cleanup( )
{
    if ( m_device )
    {
        ImGui_ImplDX11_Shutdown( );
        ImGui_ImplWin32_Shutdown( );
        ImGui::DestroyContext( );
    }

    if ( m_rtv ) { m_rtv->Release( ); m_rtv = nullptr; }
    if ( m_swapChain ) { m_swapChain->Release( ); m_swapChain = nullptr; }
    if ( m_context ) { m_context->Release( ); m_context = nullptr; }
    if ( m_device ) { m_device->Release( ); m_device = nullptr; }

    if ( m_hwnd )
    {
        DestroyWindow( m_hwnd );
        m_hwnd = nullptr;
    }

    if ( m_wc.lpszClassName )
    {
        UnregisterClassW( m_wc.lpszClassName, m_hInstance );
        free( ( void* ) m_wc.lpszClassName );
        m_wc.lpszClassName = nullptr;
    }

    g_gameBarOverlay = nullptr;
}
