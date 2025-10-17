#include "GameConfigInstance.hpp"
#include "DllOverlayUi.hpp"
#include "ProcessManager.hpp"
#include "Constants.hpp"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <d3dx9.h>

using namespace std;
using namespace DllOverlayUi;


bool initalizedDirectX = false;

static bool shouldInitDirectX = false;

bool doEndScene = false;

namespace DllOverlayUi
{

void init()
{
    shouldInitDirectX = true;
}

} // namespace DllOverlayUi


void initOverlayText ( IDirect3DDevice9 *device );

void invalidateOverlayText();

void renderOverlayText ( IDirect3DDevice9 *device, const D3DVIEWPORT9& viewport );

void initImGui( IDirect3DDevice9 *device );

void InitializeDirectX ( IDirect3DDevice9 *device )
{
    // Suppress spammy DirectX logs
    // LOG ( "🎨 InitializeDirectX called! shouldInit=%d, device=%p", shouldInitDirectX, device );
    
    if ( ! shouldInitDirectX )
        return;

    initalizedDirectX = true;
    LOG ( "✅ Initializing DirectX overlay (ImGui + text)..." );

    initOverlayText ( device );
#ifdef LOGGING
    initImGui ( device );
#endif
    
    LOG ( "✅ DirectX overlay initialized!" );
}

void InvalidateDeviceObjects()
{
    if ( ! initalizedDirectX )
        return;

    initalizedDirectX = false;

    invalidateOverlayText();
}

// Note: this is called on the SAME thread as the main application thread
void PresentFrameBegin ( IDirect3DDevice9 *device )
{
    static int callCount = 0;
    if ( callCount++ < 10 )
        LOG ( "🎨 PresentFrameBegin called! count=%d, device=%p", callCount, device );
    
    if ( ! initalizedDirectX )
        InitializeDirectX ( device );

    D3DVIEWPORT9 viewport;
    device->GetViewport ( &viewport );
    
    if ( callCount <= 10 )
        LOG ( "   Viewport: Width=%u, Height=%u", viewport.Width, viewport.Height );

    // Only draw in the main viewport; there should only be one with this width
    // MBAC FIX: Temporarily disabled viewport check - screen width address needs verification
    // if ( viewport.Width != * g_gameConfig.getScreenWidthAddr() )
    //     return;

    renderOverlayText ( device, viewport );
#ifdef LOGGING
    doEndScene = true;
    if (device->BeginScene() >= 0)
    {
        // Imgui needs to be called on the EndScene right before present is called
        // because there's about 100 begin/endscene pairs between present calls
        device->EndScene();
    }
#endif
}
