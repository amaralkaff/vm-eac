#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <functional>
#include <cstdint>

//
// Xbox Game Bar Overlay Hijack
// Creates an overlay window in the same rendering band as the Game Bar
// This allows rendering above fullscreen exclusive games
// without creating a suspicious standalone overlay window
//

// Window band IDs (undocumented)
enum ZBID : DWORD
{
    ZBID_DEFAULT                    = 0,
    ZBID_DESKTOP                    = 1,
    ZBID_UIACCESS                   = 2,
    ZBID_IMMERSIVE_IHM              = 3,
    ZBID_IMMERSIVE_NOTIFICATION     = 4,
    ZBID_IMMERSIVE_APPCHROME        = 5,
    ZBID_IMMERSIVE_MOGO             = 6,
    ZBID_IMMERSIVE_EDGY             = 7,
    ZBID_IMMERSIVE_INACTIVEMOBODY   = 8,
    ZBID_IMMERSIVE_INACTIVEDOCK     = 9,
    ZBID_IMMERSIVE_ACTIVEMOBODY     = 10,
    ZBID_IMMERSIVE_ACTIVEDOCK       = 11,
    ZBID_IMMERSIVE_BACKGROUND       = 12,
    ZBID_IMMERSIVE_SEARCH           = 13,
    ZBID_GENUINE_WINDOWS            = 14,
    ZBID_IMMERSIVE_RESTRICTED       = 15,
    ZBID_SYSTEM_TOOLS               = 16,
    ZBID_LOCK                       = 17,
    ZBID_ABOVELOCK_UX               = 18,
};

// CreateWindowInBand - undocumented user32 export
typedef HWND( WINAPI* fnCreateWindowInBand )(
    DWORD dwExStyle,
    LPCWSTR lpClassName,
    LPCWSTR lpWindowName,
    DWORD dwStyle,
    int X, int Y, int nWidth, int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hInstance,
    LPVOID lpParam,
    DWORD dwBand
);

class GameBarOverlay
{
public:
    GameBarOverlay( ) = default;
    ~GameBarOverlay( );

    // Initialize and run the overlay render loop (blocking)
    bool Run( HWND gameWindow, std::function<void( )> renderCallback );
    void Stop( );

    HWND GetWindow( ) const { return m_hwnd; }

private:
    bool CreateStealthWindow( );
    bool InitD3D( );
    bool InitImGui( );
    bool UpdatePosition( );
    void MainLoop( );
    void Cleanup( );

    // Game window we're overlaying
    HWND m_gameHwnd         = nullptr;

    // Our overlay window
    HWND m_hwnd             = nullptr;
    HINSTANCE m_hInstance    = nullptr;
    WNDCLASSEXW m_wc        = {};

    // D3D11
    ID3D11Device*            m_device    = nullptr;
    ID3D11DeviceContext*     m_context   = nullptr;
    IDXGISwapChain*          m_swapChain = nullptr;
    ID3D11RenderTargetView*  m_rtv       = nullptr;

    // Render callback
    std::function<void( )>   m_callback;
    bool                     m_running   = false;

    // Resolved CreateWindowInBand
    fnCreateWindowInBand     m_createWindowInBand = nullptr;
};
