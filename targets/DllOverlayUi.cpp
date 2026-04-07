// NOLINTBEGIN
// cpplint: disable=build/include_what_you_use
#include "DllOverlayUi.hpp"
#include "PluginHost/DetourManager.hpp"
#include "../lib/Logger.hpp"  // NOLINT(build/include) IWYU pragma: keep

#ifdef _WIN32
#include <windows.h>
#endif

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
#endif

using namespace std;
using namespace DllOverlayUi;


bool initalizedDirectX = false;

static bool shouldInitDirectX = false;
static bool overlay_text_initialized = false;
#ifdef LOGGING
static bool logged_imgui_wait = false;
#endif

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

bool initImGui( IDirect3DDevice9 *device );
void ImGuiInvalidateDeviceObjects();
bool ImGuiCreateDeviceObjects(IDirect3DDevice9* device);

void InitializeDirectX ( IDirect3DDevice9 *device )
{
#if CCCASTER_HAS_D3D9
    if ( ! shouldInitDirectX )
        return;

    if ( overlay_text_initialized == false )
    {
        initOverlayText ( device );
        overlay_text_initialized = true;
    }

#ifdef LOGGING
    const bool imgui_ok = initImGui ( device );
    if ( ! imgui_ok )
    {
        if ( ! logged_imgui_wait )
        {
            LOG ( "Overlay: InitializeDirectX - waiting for game window before initializing ImGui" );
            logged_imgui_wait = true;
        }
        cccaster::plugin::DetourManager::instance().set_render_callbacks_enabled ( false );
        return;
    }
    logged_imgui_wait = false;
#endif

    initalizedDirectX = true;

    if ( ! cccaster::plugin::DetourManager::instance().render_callbacks_enabled() )
    {
#ifdef LOGGING
        LOG ( "Overlay: InitializeDirectX - enabling render detours" );
#endif
        cccaster::plugin::DetourManager::instance().set_render_callbacks_enabled ( true );
    }
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
#ifdef LOGGING
    ImGuiInvalidateDeviceObjects();
    logged_imgui_wait = false;
#endif
    overlay_text_initialized = false;

    LOG ( "Overlay: InvalidateDeviceObjects - suspending render detours" );
    cccaster::plugin::DetourManager::instance().set_render_callbacks_enabled ( false );
#endif
}

// Note: this is called on the SAME thread as the main application thread
void PresentFrameBegin ( IDirect3DDevice9 *device )
{
#if CCCASTER_HAS_D3D9
    if ( ! initalizedDirectX )
    {
        InitializeDirectX ( device );
        if ( ! initalizedDirectX )
            return;
    }

    D3DVIEWPORT9 viewport;
    device->GetViewport ( &viewport );

    // Always invoke Menu-layer plugin callbacks (e.g. widescreen sidebars)
    // regardless of viewport, so they render during movies and all screens.
    if ( initalizedDirectX && cccaster::plugin::DetourManager::instance().render_callbacks_enabled ( cccaster::plugin::RenderLayerId::Menu ) )
    {
        cccaster::plugin::RenderContext render_context{};
        render_context.device = device;
        render_context.viewport_width = viewport.Width;
        render_context.viewport_height = viewport.Height;
        cccaster::plugin::DetourManager::instance().invoke_render ( cccaster::plugin::RenderLayerId::Menu, render_context );
    }

    // Only draw text overlays and ImGui in the main viewport
    static DWORD* const kCcScreenWidthAddr = reinterpret_cast<DWORD*>(0x54D048);
    if ( viewport.Width != *kCcScreenWidthAddr )
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

    if ( initalizedDirectX && cccaster::plugin::DetourManager::instance().render_callbacks_enabled ( cccaster::plugin::RenderLayerId::Overlay ) )
    {
        cccaster::plugin::RenderContext render_context{};
        render_context.device = device;
        render_context.viewport_width = viewport.Width;
        render_context.viewport_height = viewport.Height;
        cccaster::plugin::DetourManager::instance().invoke_render ( cccaster::plugin::RenderLayerId::Overlay, render_context );
    }
#endif
}
// NOLINTEND
