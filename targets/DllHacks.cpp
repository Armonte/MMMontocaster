#include "GameConfigInstance.hpp"
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

using namespace std;
using namespace AsmHacks;
using namespace DllHacks;


DEFINE_GUID ( GUID_DEVINTERFACE_HID, 0x4D1E55B2L, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 );

void stopDllMain ( const string& error );


namespace DllHacks
{

void initializePreLoad()
{
    LOG ( "🔧 initializePreLoad() - Applying assembly hacks..." );
    LOG ( "   Game: %s", GameConfigInstance::isMBAC() ? "MBAC" : "MBAA" );
    
    if ( GameConfigInstance::isMBAC() )
    {
        // MBAC: TESTING MAIN LOOP HOOK with verified addresses!
        LOG ( "   🔧 MBAC MODE: Testing main loop hook with code caves" );
        
        // Get the full hookMainLoop (contains 4 patches: intro skip + 3 main loop hooks)
        const AsmList& hooks = g_gameConfig.getHookMainLoop();
        LOG ( "   📦 hookMainLoop contains %d patches", (int)hooks.size() );
        
        // Apply ALL hookMainLoop patches - VERIFIED working via CE!
        for ( size_t i = 0; i < hooks.size(); ++i )
        {
            const char* patchName = "UNKNOWN";
            if ( i == 0 ) patchName = "INTRO_SKIP";
            else if ( i == 1 ) patchName = "HOOK_WRAPPER";  // Wrapper at code cave
            else if ( i == 2 ) patchName = "UNUSED";
            else if ( i == 3 ) patchName = "LOOP_REDIRECT"; // Redirect call to wrapper
            
            LOG ( "   ✅ [%s] Writing patch %d at 0x%08X (%d bytes)",
                  patchName, (int)i, (uintptr_t)hooks[i].addr, (int)hooks[i].bytes.size() );
            WRITE_ASM_HACK ( hooks[i] );
        }
        
        LOG ( "   🎯 Hook strategy VERIFIED via CE: call wrapper{callback+WindowsMessagePump}; ret" );
        
        // MULTIWINDOW: ✅ ENABLED (MBAC address verified!)
        LOG ( "   ✅ [MULTI_WINDOW] Applying MBAC multi-window patch @ 0x45F82D" );
        WRITE_ASM_HACK ( g_gameConfig.getMultiWindow() );
        
        // INPUT HIJACK: ✅ ENABLED (MBAC addresses verified via IDA!)
        LOG ( "   🎮 [HIJACK_CONTROLS] Applying MBAC input hijack (NOPing ProcessPlayerInput @ 0x44B9C0)" );
        const AsmList& inputHooks = g_gameConfig.getHijackControls();
        for ( size_t i = 0; i < inputHooks.size(); ++i )
        {
            LOG ( "   ✅ [INPUT_HOOK] Writing input patch %d at 0x%08X (%d bytes)",
                  (int)i, (uintptr_t)inputHooks[i].addr, (int)inputHooks[i].bytes.size() );
            WRITE_ASM_HACK ( inputHooks[i] );
        }
        
        // ALL OTHER PATCHES: ❌ DISABLED (addresses not verified for MBAC)
        LOG ( "   ❌ [DISABLED] hijackMenu - not verified for MBAC" );
        LOG ( "   ❌ [DISABLED] detectRoundStart - not verified for MBAC" );
        LOG ( "   ❌ [DISABLED] filterRepeatedSfx - not verified for MBAC" );
        LOG ( "   ❌ [DISABLED] muteSpecificSfx - not verified for MBAC" );
        LOG ( "   ❌ [DISABLED] addExtraTextures - not verified for MBAC" );
        LOG ( "   ❌ [DISABLED] loadCustomPalettesAsm - not verified for MBAC" );
        LOG ( "   ❌ [DISABLED] detectAutoReplaySave - not verified for MBAC" );
        LOG ( "   ❌ [DISABLED] hijackEscapeKey - not verified for MBAC" );
        LOG ( "   ❌ [DISABLED] disableTrainingMusicReset - not verified for MBAC" );
        LOG ( "   ❌ [DISABLED] fixBossStageSuperFlashOverlay - not verified for MBAC" );
    }
    else
    {
        // MBAA: Apply all patches normally
        LOG ( "   Applying %d hookMainLoop patches", (int)g_gameConfig.getHookMainLoop().size() );
        
        for ( const Asm& hack : g_gameConfig.getHookMainLoop() )
        {
            LOG ( "   Writing patch at 0x%08X (%d bytes)", (uintptr_t)hack.addr, (int)hack.bytes.size() );
            WRITE_ASM_HACK ( hack );
        }
        
        // Apply other hooks using config
        for ( const Asm& hack : g_gameConfig.getHijackControls() )
            WRITE_ASM_HACK ( hack );

        for ( const Asm& hack : g_gameConfig.getHijackMenu() )
            WRITE_ASM_HACK ( hack );

        for ( const Asm& hack : g_gameConfig.getDetectRoundStart() )
            WRITE_ASM_HACK ( hack );

        for ( const Asm& hack : g_gameConfig.getFilterRepeatedSfx() )
            WRITE_ASM_HACK ( hack );

        for ( const Asm& hack : g_gameConfig.getMuteSpecificSfx() )
            WRITE_ASM_HACK ( hack );

        for ( const Asm& hack : g_gameConfig.getAddExtraTextures() )
            WRITE_ASM_HACK ( hack );

        for ( const Asm& hack : g_gameConfig.getLoadCustomPalettesAsm() )
            WRITE_ASM_HACK ( hack );

        WRITE_ASM_HACK ( g_gameConfig.getMultiWindow() );
        WRITE_ASM_HACK ( g_gameConfig.getDetectAutoReplaySave() );
        WRITE_ASM_HACK ( g_gameConfig.getHijackEscapeKey() );
        WRITE_ASM_HACK ( g_gameConfig.getDisableTrainingMusicReset() );
        WRITE_ASM_HACK ( g_gameConfig.getFixBossStageSuperFlashOverlay() );
    }
    
    LOG ( "✅ Assembly hacks applied successfully!" );

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

    if ( GameConfigInstance::isMBAC() )
    {
        // MBAC: Apply essential post-load patches
        LOG ( "🟡 MBAC: Applying essential post-load patches" );
        
        // Skip stage patches - need MBAC-specific addresses
        LOG ( "❌ [ENABLE_STAGES] Skipping stage patches (MBAC addresses not verified)" );
        
        // Get the handle to the main window (MBAC has different title)
        // Try progressively less specific window titles
        const char* mbacTitle = "MELTY BLOOD Act Cadenza";  // Generic prefix for any version
        if ( ! ( windowHandle = ProcessManager::findWindow ( mbacTitle, false ) ) )
        {
            LOG ( "⚠️ Couldn't find MBAC window with partial match for '%s'", mbacTitle );
            LOG ( "⚠️ DirectX hooks will fail without valid window handle" );
        }
        else
        {
            LOG ( "✅ Found MBAC window" );
        }
        
        // Device notifications for controller detection
        DEV_BROADCAST_DEVICEINTERFACE dbh;
        memset ( &dbh, 0, sizeof ( dbh ) );
        dbh.dbcc_size = sizeof ( dbh );
        dbh.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        dbh.dbcc_classguid = GUID_DEVINTERFACE_HID;

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

        // Hook DirectX for MBAC (no game-specific addresses needed!)
        // BUT: Skip DllFrameRate::enable() for now - it accesses bad addresses
        bool loadFramestep = ( GetAsyncKeyState ( VK_F8 ) & 0x8000 ) == 0x8000;
        if ( !ProcessManager::isWine() && !loadFramestep && windowHandle )
        {
            LOG ( "🔧 [DIRECTX] Hooking DirectX for MBAC..." );
            string err;
            if ( ! ( err = InitDirectX ( windowHandle ) ).empty() )
                LOG ( "❌ [DIRECTX] InitDirectX failed: %s", err.c_str() );
            else if ( ! ( err = HookDirectX() ).empty() )
                LOG ( "❌ [DIRECTX] HookDirectX failed: %s", err.c_str() );
            else
                LOG ( "✅ [DIRECTX] DirectX hooks installed successfully!" );
            
            // Enable frame rate control for MBAC
            // We've disabled MBAC's limiter and will use DirectX-based timing
            DllFrameRate::enable();
            LOG ( "✅ [FRAMERATE] Enabled DllFrameRate (MBAC's limiter disabled)" );
        }
        else if ( !windowHandle )
        {
            LOG ( "⚠️ [DIRECTX] Skipping DirectX hooks - no window handle" );
        }
        
        LOG ( "✅ MBAC initializePostLoad completed" );
        return;
    }

    // MBAA: Apply all post-load patches normally
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

    if ( notifyHandle )
        UnregisterDeviceNotification ( notifyHandle );

    if ( WindowProc )
    {
        MH_DisableHook ( ( void * ) WindowProc );
        MH_REMOVE_HOOK ( WindowProc );
        MH_Uninitialize();
        WindowProc = 0;
    }

    // Only revert patches that were actually applied
    if ( GameConfigInstance::isMBAC() )
    {
        LOG ( "Reverting MBAC patches (hookMainLoop + hijackControls + multiWindow)..." );
        // Revert all hookMainLoop patches (intro skip + main loop hooks)
        for ( int i = g_gameConfig.getHookMainLoop().size() - 1; i >= 0; --i )
            g_gameConfig.getHookMainLoop()[i].revert();
        // Revert input hijack patches
        for ( int i = g_gameConfig.getHijackControls().size() - 1; i >= 0; --i )
            g_gameConfig.getHijackControls()[i].revert();
        // Revert multi-window patch
        g_gameConfig.getMultiWindow().revert();
    }
    else
    {
        // MBAA: Revert all hookMainLoop patches
        for ( int i = g_gameConfig.getHookMainLoop().size() - 1; i >= 0; --i )
            g_gameConfig.getHookMainLoop()[i].revert();
    }
}

} // namespace DllHacks


// The following constructors should only be called when running in the DLL, ie MBAA's memory space
InitialGameState::InitialGameState ( IndexedFrame indexedFrame, uint8_t netplayState, bool isTraining )
    : indexedFrame ( indexedFrame )
    , stage ( *g_gameConfig.getStageSelectorAddr() )
    , netplayState ( netplayState )
    , isTraining ( isTraining )
{
    chara[0] = ( uint8_t ) * CC_P1_CHARACTER_ADDR;
    chara[1] = ( uint8_t ) * CC_P2_CHARACTER_ADDR;

    moon[0] = ( uint8_t ) * g_gameConfig.getP1MoonSelectorAddr();
    moon[1] = ( uint8_t ) * g_gameConfig.getP2MoonSelectorAddr();

    color[0] = ( uint8_t ) * g_gameConfig.getP1ColorSelectorAddr();
    color[1] = ( uint8_t ) * g_gameConfig.getP2ColorSelectorAddr();
}

SyncHash::SyncHash ( IndexedFrame indexedFrame )
{
    this->indexedFrame = indexedFrame;

    char data [ sizeof ( uint32_t ) * 3 + g_gameConfig.getRngState3Size() ];

    memcpy ( &data[0], CC_RNG_STATE0_ADDR, sizeof ( uint32_t ) );
    memcpy ( &data[4], CC_RNG_STATE1_ADDR, sizeof ( uint32_t ) );
    memcpy ( &data[8], CC_RNG_STATE2_ADDR, sizeof ( uint32_t ) );
    memcpy ( &data[12], CC_RNG_STATE3_ADDR, g_gameConfig.getRngState3Size() );

    getMD5 ( data, sizeof ( data ), hash );

    if ( *g_gameConfig.getGameModeAddr() != CC_GAME_MODE_IN_GAME )
    {
        memset ( &chara[0], 0, sizeof ( CharaHash ) );
        memset ( &chara[1], 0, sizeof ( CharaHash ) );
        chara[0].chara = ( uint16_t ) * CC_P1_CHARACTER_ADDR;
        chara[0].moon  = ( uint16_t ) * g_gameConfig.getP1MoonSelectorAddr();
        chara[1].chara = ( uint16_t ) * CC_P2_CHARACTER_ADDR;
        chara[1].moon  = ( uint16_t ) * g_gameConfig.getP2MoonSelectorAddr();
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
