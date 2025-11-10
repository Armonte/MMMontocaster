#include "DllOverlayUi.hpp"
#include "Constants.hpp"
#include "PluginHost/DetourManager.hpp"
#include "lib/Logger.hpp"

#if !defined(CCCASTER_HAS_D3D9)
#if defined(__MINGW32__) || defined(_MSC_VER)
#if defined(__has_include)
#if __has_include(<d3dx9.h>)
#define CCCASTER_HAS_D3D9 1
#else
#define CCCASTER_HAS_D3D9 0
#endif
#else
#define CCCASTER_HAS_D3D9 1
#endif
#endif
#else
#define CCCASTER_HAS_D3D9 0
#endif

#if CCCASTER_HAS_D3D9
#include <d3d9.h>
#include <d3dx9.h>
#else
struct IDirect3DDevice9;
struct D3DVIEWPORT9 {
    unsigned long X;
    unsigned long Y;
    unsigned long Width;
    unsigned long Height;
    float MinZ;
    float MaxZ;
};
using HRESULT = long;
constexpr HRESULT D3DERR_DEVICELOST = static_cast<HRESULT>(0x88760868L);
constexpr HRESULT D3DERR_DEVICENOTRESET = static_cast<HRESULT>(0x88760869L);
#endif

using namespace std;
using namespace DllOverlayUi;


bool initalizedDirectX = false;

static bool shouldInitDirectX = false;

bool doEndScene = false;
static bool loggedDeviceLost = false;

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
#if CCCASTER_HAS_D3D9
    if ( ! shouldInitDirectX )
        return;

    initalizedDirectX = true;
    loggedDeviceLost = false;

    initOverlayText ( device );
#ifdef LOGGING
    initImGui ( device );
#endif

    LOG ( "Overlay: InitializeDirectX - enabling render detours" );
    cccaster::plugin::DetourManager::instance().set_render_callbacks_enabled ( true );
#else
    (void)device;
#endif
}

void InvalidateDeviceObjects()
{
#if CCCASTER_HAS_D3D9
    if ( ! initalizedDirectX )
        return;

    initalizedDirectX = false;

    invalidateOverlayText();

    LOG ( "Overlay: InvalidateDeviceObjects - suspending render detours" );
    cccaster::plugin::DetourManager::instance().set_render_callbacks_enabled ( false );
#endif
}

// Note: this is called on the SAME thread as the main application thread
void PresentFrameBegin ( IDirect3DDevice9 *device )
{
#if CCCASTER_HAS_D3D9
    if ( device == nullptr )
        return;

    HRESULT cooperativeLevel = device->TestCooperativeLevel();
    if ( cooperativeLevel == D3DERR_DEVICELOST || cooperativeLevel == D3DERR_DEVICENOTRESET )
    {
        if ( ! loggedDeviceLost )
        {
            LOG ( "Overlay: PresentFrameBegin skipping render (device lost, hr=0x%08X)", cooperativeLevel );
            loggedDeviceLost = true;
        }

        // Invalidate device objects when device is lost or needs reset (e.g., fullscreen transition)
        // This ensures proper cleanup before Reset() is called
        if ( initalizedDirectX )
        {
            InvalidateDeviceObjects();
        }

        cccaster::plugin::DetourManager::instance().set_render_callbacks_enabled ( false );
        return;
    }

    if ( ! initalizedDirectX )
        InitializeDirectX ( device );

    if ( ! initalizedDirectX )
        return;

    if ( loggedDeviceLost )
    {
        LOG ( "Overlay: PresentFrameBegin device restored, resuming renders" );
        loggedDeviceLost = false;
        cccaster::plugin::DetourManager::instance().set_render_callbacks_enabled ( true );
    }

    D3DVIEWPORT9 viewport;
    device->GetViewport ( &viewport );

    // Only draw in the main viewport; there should only be one with this width
    if ( viewport.Width != * CC_SCREEN_WIDTH_ADDR )
        return;

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

    if ( cccaster::plugin::DetourManager::instance().render_callbacks_enabled() )
    {
        cccaster::plugin::RenderContext render_context{};
        render_context.device = device;
        render_context.viewport_width = viewport.Width;
        render_context.viewport_height = viewport.Height;
        cccaster::plugin::DetourManager::instance().invoke_render(render_context);
    }
#else
    (void)device;
#endif
}
