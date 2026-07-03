#include "DllHacks.hpp"
#include "DllAsmHacks.hpp"
#include "Exceptions.hpp"
#include "ProcessManager.hpp"
#include "Algorithms.hpp"
#include "KeyboardManager.hpp"
#include "MouseManager.hpp"
#include "ControllerManager.hpp"
#include "DllFrameRate.hpp"

#include "D3DHook.h"

#define INITGUID
#include <windows.h>
#include <windowsx.h>
#include <dbt.h>
#include <MinHook.h>

// d3d9.h is needed for IDirect3DTexture9::LockRect in the uninitialized-texture fix below. Include it
// with INITGUID temporarily off so we don't emit a second set of D3D IID objects (dxguid/d3d9.lib already
// provide them; INITGUID is on in this TU only for GUID_DEVINTERFACE_HID). push/pop keeps HID working.
#pragma push_macro ( "INITGUID" )
#undef INITGUID
#include <d3d9.h>
#pragma pop_macro ( "INITGUID" )

#include "CCGameAddrs.hpp"      // runtime CE/Steam address table (ccAddr / InitCCAddrs / CCK_*)
#include "safetyhook.hpp"

using namespace std;
using namespace AsmHacks;
using namespace DllHacks;

// SafetyHook mid-function hook for the main message loop, an alternative to the hand-built
// `hookMainLoop` AsmList trampoline. MidHook auto-relocates the displaced bytes and preserves register
// context, so it needs only the (runtime CE/Steam) target address -- no code caves, no per-build bytes.
// Opt-in via CC_SAFETYHOOK_MAINLOOP; default keeps the AsmList path so CE behavior is unchanged.
#ifdef CC_SAFETYHOOK_MAINLOOP
namespace
{
    safetyhook::MidHook mainLoopMidHook;

    void mainLoopMidHookFn ( safetyhook::Context& )
    {
        static bool firstFire = true;
        if ( firstFire )
        {
            firstFire = false;
            LOG ( "SafetyHook mainloop MidHook FIRED (first call)" );
        }
        callback();   // AsmHacks::callback (per-loop frame work) -- same target as the AsmList hook
    }
}
#endif

// SafetyHook MidHook alternative to the `detectRoundStart` AsmList trampoline. The AsmList diverts CE
// 0x440CC5 through free space to `inc roundStartCounter`; the MidHook just increments the counter in
// place at the runtime-resolved site (CE 0x440CC5 / Steam 0x48A534, epilogue of SceneProcess_FrameBegin),
// letting SafetyHook relocate the displaced bytes -- no trampoline, ASLR-safe. Opt-in via
// CC_SAFETYHOOK_ROUNDSTART; default keeps the AsmList so CE behavior is unchanged.
#ifdef CC_SAFETYHOOK_ROUNDSTART
namespace
{
    safetyhook::MidHook roundStartMidHook;

    void roundStartMidHookFn ( safetyhook::Context& )
    {
        static bool firstFire = true;
        if ( firstFire )
        {
            firstFire = false;
            LOG ( "SafetyHook roundstart MidHook FIRED (first call)" );
        }
        ++roundStartCounter;   // AsmHacks::roundStartCounter -- same global the AsmList increments
    }
}
#endif

// Steam variant of `hijackControls`. The Steam build MERGED joystick+keyboard handling into
// Input_MergeJoystickAndKeyboard (0x4F0140) with the output struct in [esi] (CE used a separate fn with
// [edi]), so the CE patch addresses don't apply. Here we NOP the native input-STORE instructions so
// DllControllerManager drives input instead. RVAs are off the runtime module base (g_ccBase) -> ASLR-safe
// (MBAA_steam.exe has DYNAMIC_BASE). Bytes verified against MBAA_steam.exe; lengths drive the NOP count.
// CE's keyboard-zero (0x54D2C0) is intentionally DROPPED (Steam handles keyboard differently).
// Source: _steam302/CASTER_HACKS_PORT.md (hack #5j, derived + content-verified).
namespace
{
    std::vector<Asm> buildHijackControlsSteam()
    {
        struct Site { DWORD rva; int len; };        // rva = steamVA - 0x400000 ; len = bytes to NOP
        static const Site sites[] =
        {
            { 0x0F0165, 2 }, // 89 06        mov [esi],eax      (joy dir)
            { 0x0F0174, 3 }, // 89 46 04     mov [esi+4],eax
            { 0x0F017C, 3 }, // 89 46 08     mov [esi+8],eax
            { 0x0F018C, 3 }, // 09 46 0C     or  [esi+C],eax    (joy buttons)
            { 0x0F0196, 3 }, // 09 46 0C
            { 0x0F01A0, 3 }, // 09 46 0C
            { 0x0F01AA, 3 }, // 09 46 0C
            { 0x0F01D7, 2 }, // 89 06        mov [esi],eax      (keyboard merge)
            { 0x0F01DE, 3 }, // 09 46 0C     or  [esi+C],eax
        };

        std::vector<Asm> list;
        for ( const Site& s : sites )
            list.push_back ( Asm { ( void * ) ( g_ccBase + s.rva ), std::vector<uint8_t> ( s.len, 0x90 ) } );
        return list;
    }
}


// Fix for the driver-dependent texture garbage in MBAACC (snow/blotchy text boxes + pause menu on
// NVIDIA, black splotches on "transparent" sprite effects like wallbounce on AMD).
//
// Root cause: the game creates nearly all of its dynamic textures through ONE helper,
// D3D_CreateTextureWithRetry @ CE 0x4BF060, which hands back a D3DPOOL_MANAGED texture WITHOUT clearing
// it. Callers then write only part of the surface -- Dialog_CreateFromTextBuffer (0x4C2A80) fills only
// the glyph pixels of a 1024x512 texture; SpriteFrame_ComposeToCache (0x406750) copies only the sprite
// sub-rects (D3DXLoadSurfaceFromSurface, filter=NONE). Every untouched texel is undefined memory, which
// modern drivers surface differently: NVIDIA returns stale VRAM/heap (-> snow), AMD zero-fills (-> black
// where the sprite edge blends in the neighbour texel). The game itself already does the right thing in
// CreateTextureFromBuffer via Texture_FillWithD3DX; the dialog/sprite paths just skip it.
//
// Fix: a safetyhook MidHook at the success epilogue (0x4BF11F, after `mov eax,[esp+..var_4]`), where EAX
// holds the freshly-created IDirect3DTexture9* (0 on the failure path, which rets separately at 0x4BF084).
// We clear the whole level to all-zero bytes = transparent black in A8R8G8B8 / A4R4G4B4 / DXT5 (an
// all-zero DXT5 block is alpha 0 + colour 0). The clear runs at creation, before the caller writes its
// sub-rects, so real content is untouched. These textures are cached (ResourceCache_*), so it's a
// one-time cost per texture, not per frame. CE-only for now (Steam has a different, ASLR'd layout).
namespace
{
    safetyhook::MidHook textureClearMidHook;

    void textureClearMidHookFn ( safetyhook::Context& ctx )
    {
        IDirect3DTexture9 *tex = ( IDirect3DTexture9 * ) ctx.eax;
        if ( !tex )
            return;

        D3DSURFACE_DESC desc;
        if ( FAILED ( tex->GetLevelDesc ( 0, &desc ) ) )
            return;

        // Only lockable pools can be zeroed via LockRect. Render targets created here (D3DUSAGE_RENDERTARGET
        // -> forced D3DPOOL_DEFAULT) are not lockable and get fully drawn each frame anyway -- skip them.
        if ( desc.Pool == D3DPOOL_DEFAULT )
            return;

        // Block-compressed formats store (Height/4) rows of blocks; using Height would overrun the lock.
        UINT rows = desc.Height;
        switch ( desc.Format )
        {
            case D3DFMT_DXT1: case D3DFMT_DXT2: case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5:
                rows = ( desc.Height + 3 ) / 4;
                break;
            default:
                break;
        }

        D3DLOCKED_RECT lr;
        if ( SUCCEEDED ( tex->LockRect ( 0, &lr, NULL, 0 ) ) )
        {
            memset ( lr.pBits, 0, ( size_t ) lr.Pitch * rows );
            tex->UnlockRect ( 0 );
        }
    }

    // Installed CE-only from initializePreLoad. Idempotent-ish: create_mid returns an empty hook on failure.
    void installTextureClearHook()
    {
        void *target = ( void * ) 0x4BF11F;   // D3D_CreateTextureWithRetry success epilogue (EAX = new texture)
        textureClearMidHook = safetyhook::create_mid ( target, &textureClearMidHookFn );
        LOG ( "TextureClear MidHook @ 0x4BF11F: %s", textureClearMidHook ? "OK" : "FAILED" );
    }
}


DEFINE_GUID ( GUID_DEVINTERFACE_HID, 0x4D1E55B2L, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 );

void stopDllMain ( const string& error );


namespace DllHacks
{

void initializePreLoad()
{
    InitCCAddrs();   // populate the CE/Steam variant + address table first (idempotent); ccIsSteam() below

#ifdef CC_SAFETYHOOK_MAINLOOP
    // SafetyHook MidHook at the runtime-resolved main-loop site (CE 0x40D330 / Steam 0x438DD1).
    InitCCAddrs();   // ensure the CE/Steam address table is populated (idempotent)
    void *mainLoopTarget = ccAddr ( CCK_LOOP_START_ADDR );
    LOG ( "SafetyHook mainloop: variant=%s base=0x%08X target=%p",
          ccIsSteam() ? "STEAM" : "CE", g_ccBase, mainLoopTarget );
    mainLoopMidHook = safetyhook::create_mid ( mainLoopTarget, &mainLoopMidHookFn );
    LOG ( "SafetyHook mainloop: create_mid %s", mainLoopMidHook ? "OK" : "FAILED" );
#else
    for ( const Asm& hack : hookMainLoop )
        WRITE_ASM_HACK ( hack );
#endif

    if ( ccIsSteam() )
    {
        // Steam refactored input handling; CE absolute addresses don't apply (and would corrupt under
        // ASLR). Use the runtime base-relative NOP set instead. Keyboard-zero is dropped (see helper).
        LOG ( "hijackControls: STEAM variant (base-relative NOP set, %u sites)",
              ( unsigned ) buildHijackControlsSteam().size() );
        for ( const Asm& hack : buildHijackControlsSteam() )
            WRITE_ASM_HACK ( hack );
    }
    else
    {
        for ( const Asm& hack : hijackControls )
            WRITE_ASM_HACK ( hack );
    }

    // hijackMenu is N/A on Steam (Steam menu/matchmaking differs; lobby via Steamworks). CE-only.
    if ( ! ccIsSteam() )
        for ( const Asm& hack : hijackMenu )
            WRITE_ASM_HACK ( hack );

#ifdef CC_SAFETYHOOK_ROUNDSTART
    // MidHook at the runtime-resolved round-start site (CE 0x440CC5 / Steam 0x48A534).
    InitCCAddrs();   // idempotent
    void *roundStartTarget = ccAddr ( CCK_ROUND_START_HOOK_ADDR );
    LOG ( "SafetyHook roundstart: variant=%s target=%p", ccIsSteam() ? "STEAM" : "CE", roundStartTarget );
    roundStartMidHook = safetyhook::create_mid ( roundStartTarget, &roundStartMidHookFn );
    LOG ( "SafetyHook roundstart: create_mid %s", roundStartMidHook ? "OK" : "FAILED" );
#else
    for ( const Asm& hack : detectRoundStart )
        WRITE_ASM_HACK ( hack );
#endif

    // The remaining asm-hacks below still use CE absolute addresses and have NOT been ported to Steam
    // (per _steam302/CASTER_HACKS_PORT.md several are N/A on Steam; the SFX pair collapses to one Steam
    // injection at 0x52196A; disableTrainingMusicReset/fixBossStageSuperFlashOverlay have Steam addrs
    // derived but aren't wired yet). On an ASLR'd Steam image these would be wild writes, so skip them
    // entirely there -- a Steam boot applies only the ported set (mainloop + roundstart MidHooks, Steam
    // hijackControls). Re-enable per-hack as each Steam variant lands.
    if ( ! ccIsSteam() )
    {
        for ( const Asm& hack : filterRepeatedSfx )
            WRITE_ASM_HACK ( hack );

        for ( const Asm& hack : muteSpecificSfx )
            WRITE_ASM_HACK ( hack );

        for ( const Asm& hack : addExtraTextures )
            WRITE_ASM_HACK ( hack );

        for ( const Asm& hack : loadCustomPalettesAsm )
            WRITE_ASM_HACK ( hack );

        WRITE_ASM_HACK ( multiWindow );
        WRITE_ASM_HACK ( detectAutoReplaySave );
        WRITE_ASM_HACK ( hijackEscapeKey );
        WRITE_ASM_HACK ( disableTrainingMusicReset );
        WRITE_ASM_HACK ( fixBossStageSuperFlashOverlay );
        WRITE_ASM_HACK ( disableScreenshot );

        // Clear newly-created textures to transparent black -> kills the NVIDIA snow / AMD black-splotch
        // artifacts on dialogs, the pause menu, and sprite effects (see textureClearMidHookFn above).
        installTextureClearHook();
    }

    // TODO color hijack is temporary disabled due to some issues
    //
    // for ( const Asm& hack : hijackLoadingStateColors )
    //     WRITE_ASM_HACK ( hack );
    //
    // WRITE_ASM_HACK ( hijackCharaSelectColors );
}

// Note: this is called on the SAME thread as the main application thread
MH_WINAPI_HOOK ( LRESULT, CALLBACK, WindowProc, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch ( msg )
    {
        case WM_SYSCOMMAND:
            // Eat these two events to prevent screensaver and sleep
            switch ( wParam )
            {
                case SC_SCREENSAVE:
                case SC_MONITORPOWER:
                    return 0;

                default:
                    break;
            }
            break;

        case WM_KEYDOWN:
            // Ignore repeated keys
            if ( ( lParam >> 30 ) & 1 )
                break;

        // Intentional fall-through

        case WM_SYSKEYDOWN:
            // Handle Alt + F4
            if ( ( HIWORD ( lParam ) & KF_ALTDOWN ) && ( wParam == VK_F4 ) )
                stopDllMain ( "" );
            break;

        case WM_KEYUP:
        {
            // Only inject keyboard events when hooked
            if ( !KeyboardManager::get().isHooked() || !KeyboardManager::get().owner )
                break;

            const uint32_t vkCode = wParam;
            const uint32_t scanCode = ( lParam >> 16 ) & 127;
            const bool isExtended = ( lParam >> 24 ) & 1;
            const bool isDown = ( lParam >> 31 ) & 1;

            LOG ( "vkCode=0x%02X; scanCode=%u; isExtended=%u; isDown=%u", vkCode, scanCode, isExtended, isDown );

            // Note: this doesn't actually eat the keyboard event, which is actually acceptable
            // for the in-game overlay UI, since we need to mix usage with GetKeyState.
            KeyboardManager::get().owner->keyboardEvent ( vkCode, scanCode, isExtended, isDown );
            break;
        }

        case WM_DEVICECHANGE:
            switch ( wParam )
            {
                case DBT_DEVICEARRIVAL:
                case DBT_DEVICEREMOVECOMPLETE:
                    if ( ( ( DEV_BROADCAST_HDR * ) lParam )->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE )
                    {
                        ControllerManager::get().refreshJoysticks();
                    }
                    break;
            }
            return 0;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MOUSEMOVE:
        {
            // Only inject mouse events when hooked
            if ( ! MouseManager::get().owner )
                break;

            static bool isDown = false;

            if ( msg == WM_LBUTTONDOWN )
                isDown = true;
            else if ( msg ==  WM_LBUTTONUP )
                isDown = false;

            int x = GET_X_LPARAM ( lParam );
            int y = GET_Y_LPARAM ( lParam );

            MouseManager::get().owner->mouseEvent ( x, y, isDown, ( msg == WM_LBUTTONDOWN ), ( msg == WM_LBUTTONUP ) );
            break;
        }

        default:
            break;
    }

    return oWindowProc ( hwnd, msg, wParam, lParam );
}


static pWindowProc WindowProc = ( pWindowProc ) CC_WINDOW_PROC_ADDR;

static HDEVNOTIFY notifyHandle = 0;


void *windowHandle = 0;


void initializePostLoad()
{
    LOG ( "threadId=%08x", GetCurrentThreadId() );

    // Apparently this needs to be applied AFTER the game loads
    for ( const Asm& hack : enableDisabledStages )
        WRITE_ASM_HACK ( hack );

    // Get the handle to the main window
    if ( ! ( windowHandle = ProcessManager::findWindow ( CC_TITLE ) ) )
        LOG ( "Couldn't find window '%s'", CC_TITLE );

    DEV_BROADCAST_DEVICEINTERFACE dbh;
    memset ( &dbh, 0, sizeof ( dbh ) );
    dbh.dbcc_size = sizeof ( dbh );
    dbh.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    dbh.dbcc_classguid = GUID_DEVINTERFACE_HID;

    // Register for device notifications
    if ( ! ( notifyHandle = RegisterDeviceNotification ( windowHandle, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE ) ) )
        LOG ( "RegisterDeviceNotification failed: %s", WinException::getLastError() );

    // Hook the game's WindowProc
    MH_STATUS status = MH_Initialize();
    if ( status != MH_OK )
        LOG ( "Initialize failed: %s", MH_StatusString ( status ) );

    status = MH_CREATE_HOOK ( WindowProc );
    if ( status != MH_OK )
        LOG ( "Create hook failed: %s", MH_StatusString ( status ) );

    status = MH_EnableHook ( ( void * ) WindowProc );
    if ( status != MH_OK )
        LOG ( "Enable hook failed: %s", MH_StatusString ( status ) );

    bool loadFramestep = ( GetAsyncKeyState ( VK_F8 ) & 0x8000 ) == 0x8000;
    // We can't hook DirectX calls on Wine (yet?).
    if ( ProcessManager::isWine() || loadFramestep )
    {
        return;
    }

    // Apparently this needs to be applied AFTER the game loads
    // TODO: Make toggle
    DllFrameRate::enable();

    // Hook the game's DirectX calls
    string err;
    if ( ! ( err = InitDirectX ( windowHandle ) ).empty() )
        LOG ( "InitDirectX failed: %s", err );
    else if ( ! ( err = HookDirectX() ).empty() )
        LOG ( "HookDirectX failed: %s", err );
}

void deinitialize()
{
    UnhookDirectX();

    textureClearMidHook = {};   // RAII reset removes the MidHook + restores the original bytes

    if ( notifyHandle )
        UnregisterDeviceNotification ( notifyHandle );

    if ( WindowProc )
    {
        MH_DisableHook ( ( void * ) WindowProc );
        MH_REMOVE_HOOK ( WindowProc );
        MH_Uninitialize();
        WindowProc = 0;
    }

#ifdef CC_SAFETYHOOK_MAINLOOP
    mainLoopMidHook = {};   // RAII reset removes the hook + restores the original bytes
#else
    for ( int i = hookMainLoop.size() - 1; i >= 0; --i )
        hookMainLoop[i].revert();
#endif

#ifdef CC_SAFETYHOOK_ROUNDSTART
    roundStartMidHook = {};   // RAII reset removes the hook + restores the original bytes
#endif
}

} // namespace DllHacks


// The following constructors should only be called when running in the DLL, ie MBAA's memory space
InitialGameState::InitialGameState ( IndexedFrame indexedFrame, uint8_t netplayState, bool isTraining )
    : indexedFrame ( indexedFrame )
    , stage ( *CC_STAGE_SELECTOR_ADDR )
    , netplayState ( netplayState )
    , isTraining ( isTraining )
{
    chara[0] = ( uint8_t ) * CC_P1_CHARACTER_ADDR;
    chara[1] = ( uint8_t ) * CC_P2_CHARACTER_ADDR;

    moon[0] = ( uint8_t ) * CC_P1_MOON_SELECTOR_ADDR;
    moon[1] = ( uint8_t ) * CC_P2_MOON_SELECTOR_ADDR;

    color[0] = ( uint8_t ) * CC_P1_COLOR_SELECTOR_ADDR;
    color[1] = ( uint8_t ) * CC_P2_COLOR_SELECTOR_ADDR;
}

SyncHash::SyncHash ( IndexedFrame indexedFrame )
{
    this->indexedFrame = indexedFrame;

    char data [ sizeof ( uint32_t ) * 3 + CC_RNG_STATE3_SIZE ];

    memcpy ( &data[0], CC_RNG_STATE0_ADDR, sizeof ( uint32_t ) );
    memcpy ( &data[4], CC_RNG_STATE1_ADDR, sizeof ( uint32_t ) );
    memcpy ( &data[8], CC_RNG_STATE2_ADDR, sizeof ( uint32_t ) );
    memcpy ( &data[12], CC_RNG_STATE3_ADDR, CC_RNG_STATE3_SIZE );

    getMD5 ( data, sizeof ( data ), hash );

    if ( *CC_GAME_MODE_ADDR != CC_GAME_MODE_IN_GAME )
    {
        memset ( &chara[0], 0, sizeof ( CharaHash ) );
        memset ( &chara[1], 0, sizeof ( CharaHash ) );
        chara[0].chara = ( uint16_t ) * CC_P1_CHARACTER_ADDR;
        chara[0].moon  = ( uint16_t ) * CC_P1_MOON_SELECTOR_ADDR;
        chara[1].chara = ( uint16_t ) * CC_P2_CHARACTER_ADDR;
        chara[1].moon  = ( uint16_t ) * CC_P2_MOON_SELECTOR_ADDR;
        return;
    }

    roundTimer = *CC_ROUND_TIMER_ADDR;
    realTimer = *CC_REAL_TIMER_ADDR;
    cameraX = *CC_CAMERA_X_ADDR;
    cameraY = *CC_CAMERA_Y_ADDR;

#define SAVE_CHARA(N)                                                                           \
    chara[N-1].seq          = *CC_P ## N ## _SEQUENCE_ADDR;                                     \
    chara[N-1].seqState     = *CC_P ## N ## _SEQ_STATE_ADDR;                                    \
    chara[N-1].health       = *CC_P ## N ## _HEALTH_ADDR;                                       \
    chara[N-1].redHealth    = *CC_P ## N ## _RED_HEALTH_ADDR;                                   \
    chara[N-1].meter        = *CC_P ## N ## _METER_ADDR;                                        \
    chara[N-1].heat         = *CC_P ## N ## _HEAT_ADDR;                                         \
    chara[N-1].guardBar     = ( *CC_INTRO_STATE_ADDR ? 0 : *CC_P ## N ## _GUARD_BAR_ADDR );     \
    chara[N-1].guardQuality = *CC_P ## N ## _GUARD_QUALITY_ADDR;                                \
    chara[N-1].x            = *CC_P ## N ## _X_POSITION_ADDR;                                   \
    chara[N-1].y            = *CC_P ## N ## _Y_POSITION_ADDR;                                   \
    chara[N-1].chara = ( uint16_t ) * CC_P ## N ## _CHARACTER_ADDR;                             \
    chara[N-1].moon  = ( uint16_t ) * CC_P ## N ## _MOON_SELECTOR_ADDR;

    SAVE_CHARA ( 1 )
    SAVE_CHARA ( 2 )
}
