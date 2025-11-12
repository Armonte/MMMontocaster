#pragma once
#include <stdint.h>

/*
 * MBAACC VS Result Menu structures extracted from analysis.
 * These definitions are kept in 32-bit layout (pack(1)) so they can be
 * imported directly into IDA (File -> Load File -> Parse C header).
 * 
 * Based on analysis in docs/mbaa_vs_result_menu.md
 */

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)

// SSO String structure (matches MenuString from mbaa_menu_structs.h)
typedef struct _SSOString {
    int32_t base;
    union {
        char* pLongString;
        char shortString[0x10];
    };
    int32_t length;
    int32_t maxLength;
} SSOString;

// CVSResultMenuManager structure
// Based on offsets documented in docs/mbaa_vs_result_menu.md lines 79-92
typedef struct _CVSResultMenuManager {
    void* vftable;                      // +0x00
    uint8_t pad_0x04[0x18];            // +0x04 - padding
    int32_t selectionActive;           // +0x1C - latched when menu has highlighted tag ready
    SSOString highlightedTag;          // +0x20 - SSO string backing currently highlighted tag
    uint8_t pad_0x38[0x44];            // +0x38 - padding
    int32_t selectionLatched;          // +0x7C - debounce state for confirm animations
    int32_t selectionFrameCounter;      // +0x80 - frame counter for confirm animations
    int32_t state;                      // +0x84 - high-level state machine parameter
    int32_t stateTimer;                 // +0x88 - timer for state machine
    int32_t statePayload;               // +0x8C - payload for state machine
    int32_t highlightResetCountdown;    // +0x90 - countdown for highlight reset
    uint8_t pad_0x94[0x28];            // +0x94 - padding
    void* replayDialog;                 // +0xBC - pointer to replay save UI (non-null when state 5 active)
    void* netConfirmDialog;             // +0xC0 - pointer to "Return to network menu?" confirmation dialog (state 6)
    int32_t replayDialogActive;         // +0xC4 - flag gating transitions after dialogs close
    int32_t replaySaveTimer;            // +0xC8 - timer for replay save flow
    SSOString* replayName;              // +0xCC - pointer to SSO string buffer for filename assembly
    int32_t skipQuickRetryGate;         // +0xD0 - gate seeded by VsResultMenu_Create (0, 1, 100, 255 observed)
    void* infoWindow;                   // +0xD4 - info-window instance for result text display
    int32_t netPromptMode;               // +0xD8 - netplay-specific flag
    int32_t netMatchState;              // +0xDC - netplay match state flag
} CVSResultMenuManager;

#pragma pack(pop)

/* Base addresses within mbaa.exe (image base 0x400000). */
#define MBAA_BASE_RVA                 0x00400000

/* VS Result Menu function RVAs */
#define VsResultMenu_Init_RVA          0x00481D80
#define VsResultMenu_Create_RVA        0x00482CD0
#define VsResultMenuManager_Update_RVA 0x004825A0
#define VsResultMenu_FinalizeSelection_RVA 0x00482E80
#define BattleScene_ApplyResultSelection_RVA 0x00439420
#define BattleScene_PostMatchTransition_RVA 0x00439420

/* VS Result Menu globals (RVAs) */
#define gVsResultMenuHandle_RVA        0x00774C38
#define gVsResultMenuContext_RVA       0x00774C1C
#define gVsResultMenuInputState_RVA    0x00774C10
#define gVsResultMenuReplaySlot_RVA    0x00774C18
#define gVsResultMenuMode_RVA          0x0077BF2C
#define gStoryModeClearFlag_RVA        0x005585F4

#ifdef __cplusplus
}
#endif


