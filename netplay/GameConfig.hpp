#pragma once

#include <cstdint>
#include <vector>
#include <string>

// Forward declarations
struct Asm;

/**
 * GameConfig - Abstract interface for game-specific configuration
 * 
 * This interface provides all game-specific addresses, offsets, and constants
 * needed for CCCaster to support multiple Melty Blood games.
 * 
 * Implementations:
 *  - GameConfigMBAA: MBAACC v1.07 (current)
 *  - GameConfigMBAC: MBAC v1.03a (in progress)
 *  - GameConfigMB: MB v1.00 (future)
 *  - GameConfigMBReact: MB Actress Again Current Code Type Lumina (future)
 */
class GameConfig {
public:
    virtual ~GameConfig() = default;
    
    // ===================================================================
    // GAME IDENTIFICATION
    // ===================================================================
    
    virtual const char* getExecutableName() const = 0;     // "MBAA.exe", "mbacPC.exe", etc
    virtual const char* getGameName() const = 0;           // "Melty Blood Actress Again Current Code"
    virtual const char* getGameVersion() const = 0;        // "v1.07", "v1.03a", etc
    virtual const char* getGameTitle() const = 0;          // Full window title
    virtual const char* getNetworkConfigFile() const = 0;  // "System\\NetConnect.dat"
    virtual const char* getAppConfigFile() const = 0;      // "System\\_App.ini"
    
    // ===================================================================
    // GAME LOOP & HOOKS
    // ===================================================================
    
    virtual char* getLoopStartAddr() const = 0;       // CC_LOOP_START_ADDR - Main event loop
    virtual char* getHookCall1Addr() const = 0;       // MM_HOOK_CALL1_ADDR - First hook point
    virtual char* getHookCall2Addr() const = 0;       // MM_HOOK_CALL2_ADDR - Second hook point
    virtual char* getWindowProcAddr() const = 0;      // CC_WINDOW_PROC_ADDR - WindowProc location
    virtual char* getMultipleMeltyAddr() const = 0;   // MULTIPLE_MELTY - Allow multiple instances
    
    // ===================================================================
    // INPUT SYSTEM
    // ===================================================================
    
    virtual char** getInputBufferPtrAddr() const = 0;  // CC_PTR_TO_WRITE_INPUT_ADDR
    virtual uint32_t getP1DirectionOffset() const = 0; // CC_P1_OFFSET_DIRECTION
    virtual uint32_t getP1ButtonsOffset() const = 0;   // CC_P1_OFFSET_BUTTONS
    virtual uint32_t getP2DirectionOffset() const = 0; // CC_P2_OFFSET_DIRECTION
    virtual uint32_t getP2ButtonsOffset() const = 0;   // CC_P2_OFFSET_BUTTONS
    
    // ===================================================================
    // RNG SYSTEM (Critical for deterministic rollback!)
    // ===================================================================
    
    virtual uint32_t* getRngState0Addr() const = 0;    // CC_RNG_STATE0_ADDR - outRNG
    virtual uint32_t* getRngState1Addr() const = 0;    // CC_RNG_STATE1_ADDR - totalRNGCalls
    virtual uint32_t* getRngState2Addr() const = 0;    // CC_RNG_STATE2_ADDR - rngIndex
    virtual char* getRngState3Addr() const = 0;        // CC_RNG_STATE3_ADDR - state array
    virtual uint32_t getRngState3Size() const = 0;     // CC_RNG_STATE3_SIZE - array size
    
    // ===================================================================
    // FRAME CONTROL & TIMING
    // ===================================================================
    
    virtual uint32_t* getWorldTimerAddr() const = 0;      // CC_WORLD_TIMER_ADDR - Frame counter
    virtual uint8_t* getPauseFlagAddr() const = 0;        // CC_PAUSE_FLAG_ADDR - Pause state
    virtual uint32_t* getSkipFramesAddr() const = 0;      // CC_SKIP_FRAMES_ADDR - Frame skip
    virtual uint32_t* getRoundTimerAddr() const = 0;      // CC_ROUND_TIMER_ADDR - Round timer
    virtual uint32_t* getRealTimerAddr() const = 0;       // CC_REAL_TIMER_ADDR - Real timer
    virtual uint8_t* getAliveFlagAddr() const = 0;        // CC_ALIVE_FLAG_ADDR - Game alive
    virtual uint64_t* getPerfFreqAddr() const = 0;        // CC_PERF_FREQ_ADDR - Performance frequency
    virtual uint32_t* getFpsCounterAddr() const = 0;      // CC_FPS_COUNTER_ADDR - FPS display
    
    // ===================================================================
    // GAME MODE & STATE
    // ===================================================================
    
    virtual uint32_t* getGameModeAddr() const = 0;        // CC_GAME_MODE_ADDR
    virtual uint32_t* getGameStateAddr() const = 0;       // CC_GAME_STATE_ADDR
    virtual uint8_t* getIntroStateAddr() const = 0;       // CC_INTRO_STATE_ADDR
    virtual uint32_t* getSkippableFlagAddr() const = 0;   // CC_SKIPPABLE_FLAG_ADDR
    virtual uint32_t* getMenuStateCounterAddr() const = 0; // CC_MENU_STATE_COUNTER_ADDR
    
    // ===================================================================
    // PLAYER STRUCTURES (P1)
    // ===================================================================
    
    virtual uint8_t* getP1EnabledFlagAddr() const = 0;    // CC_P1_ENABLED_FLAG_ADDR
    virtual uint32_t* getP1SequenceAddr() const = 0;      // CC_P1_SEQUENCE_ADDR
    virtual uint32_t* getP1SeqStateAddr() const = 0;      // CC_P1_SEQ_STATE_ADDR
    virtual uint32_t* getP1HealthAddr() const = 0;        // CC_P1_HEALTH_ADDR
    virtual uint32_t* getP1RedHealthAddr() const = 0;     // CC_P1_RED_HEALTH_ADDR
    virtual float* getP1GuardBarAddr() const = 0;         // CC_P1_GUARD_BAR_ADDR
    virtual float* getP1GuardQualityAddr() const = 0;     // CC_P1_GUARD_QUALITY_ADDR
    virtual uint32_t* getP1MeterAddr() const = 0;         // CC_P1_METER_ADDR
    virtual uint32_t* getP1HeatAddr() const = 0;          // CC_P1_HEAT_ADDR
    virtual uint8_t* getP1NoInputFlagAddr() const = 0;    // CC_P1_NO_INPUT_FLAG_ADDR
    virtual uint8_t* getP1PuppetStateAddr() const = 0;    // CC_P1_PUPPET_STATE_ADDR
    virtual int32_t* getP1XPositionAddr() const = 0;      // CC_P1_X_POSITION_ADDR
    virtual int32_t* getP1YPositionAddr() const = 0;      // CC_P1_Y_POSITION_ADDR
    virtual int32_t* getP1XPrevPosAddr() const = 0;       // CC_P1_X_PREV_POS_ADDR
    virtual int32_t* getP1YPrevPosAddr() const = 0;       // CC_P1_Y_PREV_POS_ADDR
    virtual int32_t* getP1XVelocityAddr() const = 0;      // CC_P1_X_VELOCITY_ADDR
    virtual int32_t* getP1YVelocityAddr() const = 0;      // CC_P1_Y_VELOCITY_ADDR
    virtual int16_t* getP1XAccelerationAddr() const = 0;  // CC_P1_X_ACCELERATION_ADDR
    virtual int16_t* getP1YAccelerationAddr() const = 0;  // CC_P1_Y_ACCELERATION_ADDR
    virtual uint32_t* getP1SpriteAngleAddr() const = 0;   // CC_P1_SPRITE_ANGLE_ADDR
    virtual uint8_t* getP1FacingFlagAddr() const = 0;     // CC_P1_FACING_FLAG_ADDR
    virtual uint8_t* getP1ComboOffsetAddr() const = 0;    // CC_P1_COMBO_OFFSET_ADDR
    virtual uint32_t* getP1ComboHitBaseAddr() const = 0;  // CC_P1_COMBO_HIT_BASE_ADDR
    
    // ===================================================================
    // PLAYER STRUCTURES (P2, P3, P4)
    // ===================================================================
    
    virtual uint8_t* getP2EnabledFlagAddr() const = 0;    // CC_P2_ENABLED_FLAG_ADDR
    virtual uint32_t* getP2SequenceAddr() const = 0;      // CC_P2_SEQUENCE_ADDR
    virtual uint32_t* getP2SeqStateAddr() const = 0;      // CC_P2_SEQ_STATE_ADDR
    virtual uint32_t* getP2HealthAddr() const = 0;        // CC_P2_HEALTH_ADDR
    virtual uint32_t* getP2RedHealthAddr() const = 0;     // CC_P2_RED_HEALTH_ADDR
    virtual float* getP2GuardBarAddr() const = 0;         // CC_P2_GUARD_BAR_ADDR
    virtual float* getP2GuardQualityAddr() const = 0;     // CC_P2_GUARD_QUALITY_ADDR
    virtual uint32_t* getP2MeterAddr() const = 0;         // CC_P2_METER_ADDR
    virtual uint32_t* getP2HeatAddr() const = 0;          // CC_P2_HEAT_ADDR
    virtual uint8_t* getP2NoInputFlagAddr() const = 0;    // CC_P2_NO_INPUT_FLAG_ADDR
    virtual uint8_t* getP2PuppetStateAddr() const = 0;    // CC_P2_PUPPET_STATE_ADDR
    virtual int32_t* getP2XPositionAddr() const = 0;      // CC_P2_X_POSITION_ADDR
    virtual int32_t* getP2YPositionAddr() const = 0;      // CC_P2_Y_POSITION_ADDR
    virtual uint8_t* getP2FacingFlagAddr() const = 0;     // CC_P2_FACING_FLAG_ADDR
    
    virtual uint8_t* getP3EnabledFlagAddr() const = 0;    // CC_P3_ENABLED_FLAG_ADDR
    virtual uint32_t* getP3SequenceAddr() const = 0;      // CC_P3_SEQUENCE_ADDR
    virtual uint8_t* getP3NoInputFlagAddr() const = 0;    // CC_P3_NO_INPUT_FLAG_ADDR
    virtual uint8_t* getP3PuppetStateAddr() const = 0;    // CC_P3_PUPPET_STATE_ADDR
    
    virtual uint8_t* getP4EnabledFlagAddr() const = 0;    // CC_P4_ENABLED_FLAG_ADDR
    virtual uint8_t* getP4NoInputFlagAddr() const = 0;    // CC_P4_NO_INPUT_FLAG_ADDR
    virtual uint8_t* getP4PuppetStateAddr() const = 0;    // CC_P4_PUPPET_STATE_ADDR
    
    virtual uint32_t getPlayerStructSize() const = 0;     // CC_PLR_STRUCT_SIZE
    
    // ===================================================================
    // CHARACTER SELECT
    // ===================================================================
    
    virtual uint32_t* getP1SelectorModeAddr() const = 0;   // CC_P1_SELECTOR_MODE_ADDR
    virtual uint32_t* getP1CharaSelectorAddr() const = 0;  // CC_P1_CHARA_SELECTOR_ADDR
    virtual uint32_t* getP1CharacterAddr() const = 0;      // CC_P1_CHARACTER_ADDR
    virtual uint32_t* getP1MoonSelectorAddr() const = 0;   // CC_P1_MOON_SELECTOR_ADDR
    virtual uint32_t* getP1ColorSelectorAddr() const = 0;  // CC_P1_COLOR_SELECTOR_ADDR
    virtual uint8_t* getP1RandomColorAddr() const = 0;     // CC_P1_RANDOM_COLOR_ADDR
    
    virtual uint32_t* getP2SelectorModeAddr() const = 0;   // CC_P2_SELECTOR_MODE_ADDR
    virtual uint32_t* getP2CharaSelectorAddr() const = 0;  // CC_P2_CHARA_SELECTOR_ADDR
    virtual uint32_t* getP2CharacterAddr() const = 0;      // CC_P2_CHARACTER_ADDR
    virtual uint32_t* getP2MoonSelectorAddr() const = 0;   // CC_P2_MOON_SELECTOR_ADDR
    virtual uint32_t* getP2ColorSelectorAddr() const = 0;  // CC_P2_COLOR_SELECTOR_ADDR
    virtual uint8_t* getP2RandomColorAddr() const = 0;     // CC_P2_RANDOM_COLOR_ADDR
    
    // ===================================================================
    // GAME SETTINGS
    // ===================================================================
    
    virtual uint32_t* getScreenWidthAddr() const = 0;      // CC_SCREEN_WIDTH_ADDR
    virtual uint32_t* getDamageLevelAddr() const = 0;      // CC_DAMAGE_LEVEL_ADDR
    virtual uint32_t* getWinCountVsAddr() const = 0;       // CC_WIN_COUNT_VS_ADDR
    virtual uint32_t* getTimerSpeedAddr() const = 0;       // CC_TIMER_SPEED_ADDR
    virtual uint32_t* getAutoReplaySaveAddr() const = 0;   // CC_AUTO_REPLAY_SAVE_ADDR
    virtual uint32_t* getStageSelectorAddr() const = 0;    // CC_STAGE_SELECTOR_ADDR
    virtual uint32_t* getStageAnimationOffAddr() const = 0; // CC_STAGE_ANIMATION_OFF_ADDR
    
    // ===================================================================
    // MATCH STATUS
    // ===================================================================
    
    virtual uint32_t* getP1GamePointFlagAddr() const = 0;  // CC_P1_GAME_POINT_FLAG_ADDR
    virtual uint32_t* getP2GamePointFlagAddr() const = 0;  // CC_P2_GAME_POINT_FLAG_ADDR
    virtual uint32_t* getP1WinsAddr() const = 0;           // CC_P1_WINS_ADDR
    virtual uint32_t* getP2WinsAddr() const = 0;           // CC_P2_WINS_ADDR
    virtual uint32_t* getRoundCountAddr() const = 0;       // CC_ROUND_COUNT_ADDR
    
    // ===================================================================
    // TRAINING MODE
    // ===================================================================
    
    virtual uint32_t* getTrainingPauseAddr() const = 0;    // CC_TRAINING_PAUSE_ADDR
    virtual uint32_t* getVersusPauseAddr() const = 0;      // CC_VERSUS_PAUSE_ADDR
    virtual int32_t* getDummyStatusAddr() const = 0;       // CC_DUMMY_STATUS_ADDR
    virtual uint32_t* getP1ComboGuardAddr() const = 0;     // CC_P1_COMBO_GUARD_ADDR
    
    // ===================================================================
    // CAMERA & DISPLAY
    // ===================================================================
    
    virtual int* getCameraXAddr() const = 0;               // CC_CAMERA_X_ADDR
    virtual int* getCameraYAddr() const = 0;               // CC_CAMERA_Y_ADDR
    virtual uint32_t* getHitSparksAddr() const = 0;        // CC_HIT_SPARKS_ADDR
    virtual int* getShowAttackDisplayAddr() const = 0;     // CC_SHOW_ATTACK_DISPLAY
    virtual int* getShowInputDisplayAddr() const = 0;      // CC_SHOW_INPUT_DISPLAY
    
    // ===================================================================
    // SOUND
    // ===================================================================
    
    virtual uint8_t* getSfxArrayAddr() const = 0;          // CC_SFX_ARRAY_ADDR
    virtual uint32_t getSfxArrayLen() const = 0;           // CC_SFX_ARRAY_LEN
    
    // ===================================================================
    // GRAPHICS
    // ===================================================================
    
    virtual uint32_t* getD3DX9ObjAddr() const = 0;         // CC_D3DX9_OBJ_ADDR
    virtual uint32_t* getReplayCreatedAddr() const = 0;    // CC_REPLAY_CREATED_ADDR
    virtual void* getRepRoundTblEndPtrAddr() const = 0;    // CC_REPROUND_TBL_ENDPTR_ADDR
    
    // ===================================================================
    // SPRITE & TEXT RESOURCES
    // ===================================================================
    
    virtual uint32_t getButtonSpriteTexAddr() const = 0;   // BUTTON_SPRITE_TEX
    virtual uint32_t getFont0Addr() const = 0;             // FONT0
    virtual uint32_t getFont1Addr() const = 0;             // FONT1
    virtual uint32_t getFont2Addr() const = 0;             // FONT2
    
    // ===================================================================
    // KEYBOARD CONFIG
    // ===================================================================
    
    virtual uint32_t getKeyboardConfigOffset() const = 0;  // CC_KEYBOARD_CONFIG_OFFSET
    
    // ===================================================================
    // GAME FEATURES (capabilities differ between games)
    // ===================================================================
    
    virtual bool hasPuppetCharacters() const = 0;          // MBAACC has puppets, MBAC/MB/MBR don't
    virtual bool hasMoonStyles() const = 0;                // Only MBAACC has moon styles (Crescent/Half/Full)
    virtual uint32_t getCharacterCount() const = 0;        // Number of playable characters
    virtual uint32_t getStageCount() const = 0;            // Number of stages
    virtual uint32_t getPreGameIntroFrames() const = 0;    // CC_PRE_GAME_INTRO_FRAMES
    
    // ===================================================================
    // ASSEMBLY HACKS (game-specific)
    // ===================================================================
    
    virtual const std::vector<Asm>& getHookMainLoop() const = 0;
    virtual const std::vector<Asm>& getEnableDisabledStages() const = 0;
    virtual const Asm& getDisableFpsLimit() const = 0;
    virtual const Asm& getDisableFpsCounter() const = 0;
    virtual const std::vector<Asm>& getHijackControls() const = 0;
    virtual const std::vector<Asm>& getHijackMenu() const = 0;
    virtual const std::vector<Asm>& getDetectRoundStart() const = 0;
    virtual const std::vector<Asm>& getSaveReplay() const = 0;
    virtual const Asm& getDetectAutoReplaySave() const = 0;
    virtual const Asm& getMultiWindow() const = 0;
    virtual const Asm& getForceGotoVersus() const = 0;
    virtual const Asm& getForceGotoVersusCPU() const = 0;
    virtual const Asm& getForceGotoTraining() const = 0;
    virtual const Asm& getForceGotoReplay() const = 0;
    virtual const Asm& getHijackEscapeKey() const = 0;
    virtual const std::vector<Asm>& getFilterRepeatedSfx() const = 0;
    virtual const std::vector<Asm>& getMuteSpecificSfx() const = 0;
    virtual const Asm& getHijackIntroState() const = 0;
    virtual const Asm& getDisableTrainingMusicReset() const = 0;
    virtual const Asm& getFixBossStageSuperFlashOverlay() const = 0;
    virtual const Asm& getHijackCharaSelectColors() const = 0;
    virtual const std::vector<Asm>& getHijackLoadingStateColors() const = 0;
    virtual const std::vector<Asm>& getDisableHealthBars() const = 0;
    virtual const std::vector<Asm>& getAddExtraDraws() const = 0;
    virtual const std::vector<Asm>& getAddExtraTextures() const = 0;
    virtual const std::vector<Asm>& getLoadCustomPalettesAsm() const = 0;
};

