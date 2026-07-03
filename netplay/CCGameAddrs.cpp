// Dual-target (CE/Steam) data-address layer for CCCaster. See CCGameAddrs.hpp.
//
// We must NOT include Constants.hpp's runtime CC_*_ADDR macros here (they need g_ccBase/g_ccAddr which
// we define below), so this TU is built with CC_USE_RUNTIME_ADDRS undefined for itself.
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cctype>

enum CCVariant { CC_VARIANT_CE = 0, CC_VARIANT_STEAM = 1 };

enum CCAddrKey {
#define CCADDR(name, ce_rva, steam_rva) CCK_##name,
#include "CCAddrs.gen.inc"
#undef CCADDR
    CCK_COUNT
};

DWORD     g_ccBase    = 0x400000;        // overwritten in InitCCAddrs with the real module base
DWORD     g_ccAddr[CCK_COUNT] = { 0 };
CCVariant g_ccVariant = CC_VARIANT_CE;

// The variant RVA table (CE and Steam columns) straight from the generated answer key.
namespace {
    struct CCAddrRow { DWORD ce, steam; };
    static const CCAddrRow kRows[CCK_COUNT] = {
#define CCADDR(name, ce_rva, steam_rva) { ce_rva, steam_rva },
#include "CCAddrs.gen.inc"
#undef CCADDR
    };

    // Steam build links steam_api.dll and is named MBAA_steam.exe; CE (Community Edition) is neither.
    bool detectSteam()
    {
        if ( GetModuleHandleA ( "steam_api.dll" ) || GetModuleHandleA ( "steam_api64.dll" ) )
            return true;
        char path[MAX_PATH] = { 0 };
        if ( GetModuleFileNameA ( GetModuleHandleA ( 0 ), path, sizeof ( path ) ) )
        {
            for ( char *p = path; *p; ++p ) *p = (char) tolower ( (unsigned char) *p );
            if ( strstr ( path, "steam" ) )
                return true;
        }
        return false;
    }

    void populate()
    {
        const bool steam = ( g_ccVariant == CC_VARIANT_STEAM );
        for ( int i = 0; i < CCK_COUNT; ++i )
            g_ccAddr[i] = steam ? kRows[i].steam : kRows[i].ce;
    }
}

void SetCCVariant ( CCVariant v )
{
    g_ccVariant = v;
    populate();
}

void InitCCAddrs()
{
    g_ccBase    = (DWORD) GetModuleHandleA ( 0 );   // real load base (CE: 0x400000; Steam: ASLR)
    g_ccVariant = detectSteam() ? CC_VARIANT_STEAM : CC_VARIANT_CE;
    populate();
}
