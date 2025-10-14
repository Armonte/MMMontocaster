#pragma once

#include "GameConfig.hpp"
#include "Constants.hpp"
#include "../targets/DllAsmHacks.hpp"

/**
 * GameConfigMBAA - Configuration for MBAACC v1.07
 * 
 * This class provides all MBAACC-specific addresses and assembly hacks
 * that were previously hard-coded in Constants.hpp and DllAsmHacks.hpp
 */
class GameConfigMBAA : public GameConfig {
public:
    // ===================================================================
    // GAME IDENTIFICATION
    // ===================================================================
    
    const char* getExecutableName() const override {
        return "MBAA.exe";
    }
    
    const char* getGameName() const override {
        return "Melty Blood Actress Again Current Code";
    }
    
    const char* getGameVersion() const override {
        return CC_VERSION;  // "1.4.0"
    }
    
    const char* getGameTitle() const override {
        return CC_TITLE;  // Full title with version
    }
    
    const char* getNetworkConfigFile() const override {
        return CC_NETWORK_CONFIG_FILE;
    }
    
    const char* getAppConfigFile() const override {
        return CC_APP_CONFIG_FILE;
    }
    
    // ===================================================================
    // GAME LOOP & HOOKS
    // ===================================================================
    
    char* getLoopStartAddr() const override {
        return CC_LOOP_START_ADDR;  // 0x40D330
    }
    
    char* getHookCall1Addr() const override {
        return MM_HOOK_CALL1_ADDR;  // 0x40D032
    }
    
    char* getHookCall2Addr() const override {
        return MM_HOOK_CALL2_ADDR;  // 0x40D411
    }
    
    char* getWindowProcAddr() const override {
        return CC_WINDOW_PROC_ADDR;  // 0x40D4C0
    }
    
    char* getMultipleMeltyAddr() const override {
        return MULTIPLE_MELTY;  // 0x40D25A
    }
    
    // ===================================================================
    // INPUT SYSTEM
    // ===================================================================
    
    char** getInputBufferPtrAddr() const override {
        return (char**)CC_PTR_TO_WRITE_INPUT_ADDR;  // 0x76E6AC
    }
    
    uint32_t getP1DirectionOffset() const override {
        return CC_P1_OFFSET_DIRECTION;  // 0x18
    }
    
    uint32_t getP1ButtonsOffset() const override {
        return CC_P1_OFFSET_BUTTONS;  // 0x24
    }
    
    uint32_t getP2DirectionOffset() const override {
        return CC_P2_OFFSET_DIRECTION;  // 0x2C
    }
    
    uint32_t getP2ButtonsOffset() const override {
        return CC_P2_OFFSET_BUTTONS;  // 0x38
    }
    
    // ===================================================================
    // RNG SYSTEM
    // ===================================================================
    
    uint32_t* getRngState0Addr() const override {
        return CC_RNG_STATE0_ADDR;  // 0x563778 - outRNG
    }
    
    uint32_t* getRngState1Addr() const override {
        return CC_RNG_STATE1_ADDR;  // 0x56377C - totalRNGCalls
    }
    
    uint32_t* getRngState2Addr() const override {
        return CC_RNG_STATE2_ADDR;  // 0x564068 - rngIndex
    }
    
    char* getRngState3Addr() const override {
        return CC_RNG_STATE3_ADDR;  // 0x564070 - state array
    }
    
    uint32_t getRngState3Size() const override {
        return CC_RNG_STATE3_SIZE;  // 220 bytes
    }
    
    // ===================================================================
    // FRAME CONTROL & TIMING
    // ===================================================================
    
    uint32_t* getWorldTimerAddr() const override {
        return CC_WORLD_TIMER_ADDR;  // 0x55D1D4
    }
    
    uint8_t* getPauseFlagAddr() const override {
        return CC_PAUSE_FLAG_ADDR;  // 0x55D203
    }
    
    uint32_t* getSkipFramesAddr() const override {
        return CC_SKIP_FRAMES_ADDR;  // 0x55D25C
    }
    
    uint32_t* getRoundTimerAddr() const override {
        return CC_ROUND_TIMER_ADDR;  // 0x562A3C
    }
    
    uint32_t* getRealTimerAddr() const override {
        return CC_REAL_TIMER_ADDR;  // 0x562A40
    }
    
    uint8_t* getAliveFlagAddr() const override {
        return CC_ALIVE_FLAG_ADDR;  // 0x76E650
    }
    
    uint64_t* getPerfFreqAddr() const override {
        return CC_PERF_FREQ_ADDR;  // 0x774A80
    }
    
    uint32_t* getFpsCounterAddr() const override {
        return CC_FPS_COUNTER_ADDR;  // 0x774A70
    }
    
    // ===================================================================
    // GAME MODE & STATE
    // ===================================================================
    
    uint32_t* getGameModeAddr() const override {
        return CC_GAME_MODE_ADDR;  // 0x54EEE8
    }
    
    uint32_t* getGameStateAddr() const override {
        return CC_GAME_STATE_ADDR;  // 0x74D598
    }
    
    uint8_t* getIntroStateAddr() const override {
        return CC_INTRO_STATE_ADDR;  // 0x55D20B
    }
    
    uint32_t* getSkippableFlagAddr() const override {
        return CC_SKIPPABLE_FLAG_ADDR;  // 0x74D99C
    }
    
    uint32_t* getMenuStateCounterAddr() const override {
        return CC_MENU_STATE_COUNTER_ADDR;  // 0x767440
    }
    
    // ===================================================================
    // PLAYER STRUCTURES (P1)
    // ===================================================================
    
    uint8_t* getP1EnabledFlagAddr() const override {
        return CC_P1_ENABLED_FLAG_ADDR;  // 0x555130
    }
    
    uint32_t* getP1SequenceAddr() const override {
        return CC_P1_SEQUENCE_ADDR;  // 0x555140
    }
    
    uint32_t* getP1SeqStateAddr() const override {
        return CC_P1_SEQ_STATE_ADDR;  // 0x555144
    }
    
    uint32_t* getP1HealthAddr() const override {
        return CC_P1_HEALTH_ADDR;  // 0x5551EC
    }
    
    uint32_t* getP1RedHealthAddr() const override {
        return CC_P1_RED_HEALTH_ADDR;  // 0x5551F0
    }
    
    float* getP1GuardBarAddr() const override {
        return CC_P1_GUARD_BAR_ADDR;  // 0x5551F4
    }
    
    float* getP1GuardQualityAddr() const override {
        return CC_P1_GUARD_QUALITY_ADDR;  // 0x555208
    }
    
    uint32_t* getP1MeterAddr() const override {
        return CC_P1_METER_ADDR;  // 0x555210
    }
    
    uint32_t* getP1HeatAddr() const override {
        return CC_P1_HEAT_ADDR;  // 0x555214
    }
    
    uint8_t* getP1NoInputFlagAddr() const override {
        return CC_P1_NO_INPUT_FLAG_ADDR;  // 0x5552A7
    }
    
    uint8_t* getP1PuppetStateAddr() const override {
        return CC_P1_PUPPET_STATE_ADDR;  // 0x5552A8
    }
    
    int32_t* getP1XPositionAddr() const override {
        return CC_P1_X_POSITION_ADDR;  // 0x555238
    }
    
    int32_t* getP1YPositionAddr() const override {
        return CC_P1_Y_POSITION_ADDR;  // 0x55523C
    }
    
    int32_t* getP1XPrevPosAddr() const override {
        return CC_P1_X_PREV_POS_ADDR;  // 0x555244
    }
    
    int32_t* getP1YPrevPosAddr() const override {
        return CC_P1_Y_PREV_POS_ADDR;  // 0x555248
    }
    
    int32_t* getP1XVelocityAddr() const override {
        return CC_P1_X_VELOCITY_ADDR;  // 0x55524C
    }
    
    int32_t* getP1YVelocityAddr() const override {
        return CC_P1_Y_VELOCITY_ADDR;  // 0x555250
    }
    
    int16_t* getP1XAccelerationAddr() const override {
        return CC_P1_X_ACCELERATION_ADDR;  // 0x555254
    }
    
    int16_t* getP1YAccelerationAddr() const override {
        return CC_P1_Y_ACCELERATION_ADDR;  // 0x555256
    }
    
    uint32_t* getP1SpriteAngleAddr() const override {
        return CC_P1_SPRITE_ANGLE_ADDR;  // 0x555430
    }
    
    uint8_t* getP1FacingFlagAddr() const override {
        return CC_P1_FACING_FLAG_ADDR;  // 0x555444
    }
    
    uint8_t* getP1ComboOffsetAddr() const override {
        return CC_P1_COMBO_OFFSET_ADDR;  // 0x557E59
    }
    
    uint32_t* getP1ComboHitBaseAddr() const override {
        return CC_P1_COMBO_HIT_BASE_ADDR;  // 0x557E5C
    }
    
    // ===================================================================
    // PLAYER STRUCTURES (P2, P3, P4)
    // ===================================================================
    
    uint8_t* getP2EnabledFlagAddr() const override {
        return CC_P2_ENABLED_FLAG_ADDR;
    }
    
    uint32_t* getP2SequenceAddr() const override {
        return CC_P2_SEQUENCE_ADDR;
    }
    
    uint32_t* getP2SeqStateAddr() const override {
        return CC_P2_SEQ_STATE_ADDR;
    }
    
    uint32_t* getP2HealthAddr() const override {
        return CC_P2_HEALTH_ADDR;
    }
    
    uint32_t* getP2RedHealthAddr() const override {
        return CC_P2_RED_HEALTH_ADDR;
    }
    
    float* getP2GuardBarAddr() const override {
        return CC_P2_GUARD_BAR_ADDR;
    }
    
    float* getP2GuardQualityAddr() const override {
        return CC_P2_GUARD_QUALITY_ADDR;
    }
    
    uint32_t* getP2MeterAddr() const override {
        return CC_P2_METER_ADDR;
    }
    
    uint32_t* getP2HeatAddr() const override {
        return CC_P2_HEAT_ADDR;
    }
    
    uint8_t* getP2NoInputFlagAddr() const override {
        return CC_P2_NO_INPUT_FLAG_ADDR;
    }
    
    uint8_t* getP2PuppetStateAddr() const override {
        return CC_P2_PUPPET_STATE_ADDR;
    }
    
    int32_t* getP2XPositionAddr() const override {
        return CC_P2_X_POSITION_ADDR;
    }
    
    int32_t* getP2YPositionAddr() const override {
        return CC_P2_Y_POSITION_ADDR;
    }
    
    uint8_t* getP2FacingFlagAddr() const override {
        return CC_P2_FACING_FLAG_ADDR;
    }
    
    uint8_t* getP3EnabledFlagAddr() const override {
        return CC_P3_ENABLED_FLAG_ADDR;
    }
    
    uint32_t* getP3SequenceAddr() const override {
        return CC_P3_SEQUENCE_ADDR;
    }
    
    uint8_t* getP3NoInputFlagAddr() const override {
        return CC_P3_NO_INPUT_FLAG_ADDR;
    }
    
    uint8_t* getP3PuppetStateAddr() const override {
        return CC_P3_PUPPET_STATE_ADDR;
    }
    
    uint8_t* getP4EnabledFlagAddr() const override {
        return CC_P4_ENABLED_FLAG_ADDR;
    }
    
    uint8_t* getP4NoInputFlagAddr() const override {
        return CC_P4_NO_INPUT_FLAG_ADDR;
    }
    
    uint8_t* getP4PuppetStateAddr() const override {
        return CC_P4_PUPPET_STATE_ADDR;
    }
    
    uint32_t getPlayerStructSize() const override {
        return CC_PLR_STRUCT_SIZE;  // 0xAFC
    }
    
    // ===================================================================
    // CHARACTER SELECT
    // ===================================================================
    
    uint32_t* getP1SelectorModeAddr() const override {
        return CC_P1_SELECTOR_MODE_ADDR;  // 0x74D8EC
    }
    
    uint32_t* getP1CharaSelectorAddr() const override {
        return CC_P1_CHARA_SELECTOR_ADDR;  // 0x74D8F8
    }
    
    uint32_t* getP1CharacterAddr() const override {
        return CC_P1_CHARACTER_ADDR;  // 0x74D8FC
    }
    
    uint32_t* getP1MoonSelectorAddr() const override {
        return CC_P1_MOON_SELECTOR_ADDR;  // 0x74D900
    }
    
    uint32_t* getP1ColorSelectorAddr() const override {
        return CC_P1_COLOR_SELECTOR_ADDR;  // 0x74D904
    }
    
    uint8_t* getP1RandomColorAddr() const override {
        return CC_P1_RANDOM_COLOR_ADDR;
    }
    
    uint32_t* getP2SelectorModeAddr() const override {
        return CC_P2_SELECTOR_MODE_ADDR;  // 0x74D910
    }
    
    uint32_t* getP2CharaSelectorAddr() const override {
        return CC_P2_CHARA_SELECTOR_ADDR;  // 0x74D91C
    }
    
    uint32_t* getP2CharacterAddr() const override {
        return CC_P2_CHARACTER_ADDR;  // 0x74D920
    }
    
    uint32_t* getP2MoonSelectorAddr() const override {
        return CC_P2_MOON_SELECTOR_ADDR;  // 0x74D924
    }
    
    uint32_t* getP2ColorSelectorAddr() const override {
        return CC_P2_COLOR_SELECTOR_ADDR;  // 0x74D928
    }
    
    uint8_t* getP2RandomColorAddr() const override {
        return CC_P2_RANDOM_COLOR_ADDR;
    }
    
    // ===================================================================
    // GAME SETTINGS
    // ===================================================================
    
    uint32_t* getScreenWidthAddr() const override {
        return CC_SCREEN_WIDTH_ADDR;  // 0x54D048
    }
    
    uint32_t* getDamageLevelAddr() const override {
        return CC_DAMAGE_LEVEL_ADDR;  // 0x553FCC
    }
    
    uint32_t* getWinCountVsAddr() const override {
        return CC_WIN_COUNT_VS_ADDR;  // 0x553FDC
    }
    
    uint32_t* getTimerSpeedAddr() const override {
        return CC_TIMER_SPEED_ADDR;  // 0x553FD0
    }
    
    uint32_t* getAutoReplaySaveAddr() const override {
        return CC_AUTO_REPLAY_SAVE_ADDR;  // 0x553FE8
    }
    
    uint32_t* getStageSelectorAddr() const override {
        return CC_STAGE_SELECTOR_ADDR;  // 0x74FD98
    }
    
    uint32_t* getStageAnimationOffAddr() const override {
        return CC_STAGE_ANIMATION_OFF_ADDR;  // 0x554124
    }
    
    // ===================================================================
    // MATCH STATUS
    // ===================================================================
    
    uint32_t* getP1GamePointFlagAddr() const override {
        return CC_P1_GAME_POINT_FLAG_ADDR;  // 0x559548
    }
    
    uint32_t* getP2GamePointFlagAddr() const override {
        return CC_P2_GAME_POINT_FLAG_ADDR;  // 0x55954C
    }
    
    uint32_t* getP1WinsAddr() const override {
        return CC_P1_WINS_ADDR;  // 0x559550
    }
    
    uint32_t* getP2WinsAddr() const override {
        return CC_P2_WINS_ADDR;  // 0x559580
    }
    
    uint32_t* getRoundCountAddr() const override {
        return CC_ROUND_COUNT_ADDR;  // 0x5550E0
    }
    
    // ===================================================================
    // TRAINING MODE
    // ===================================================================
    
    uint32_t* getTrainingPauseAddr() const override {
        return CC_TRAINING_PAUSE_ADDR;  // 0x562A64
    }
    
    uint32_t* getVersusPauseAddr() const override {
        return CC_VERSUS_PAUSE_ADDR;  // 0x564B30
    }
    
    int32_t* getDummyStatusAddr() const override {
        return CC_DUMMY_STATUS_ADDR;  // 0x74D7F8
    }
    
    uint32_t* getP1ComboGuardAddr() const override {
        return CC_P1_COMBO_GUARD_ADDR;  // 0x76E708
    }
    
    // ===================================================================
    // CAMERA & DISPLAY
    // ===================================================================
    
    int* getCameraXAddr() const override {
        return CC_CAMERA_X_ADDR;  // 0x564B14
    }
    
    int* getCameraYAddr() const override {
        return CC_CAMERA_Y_ADDR;  // 0x564B18
    }
    
    uint32_t* getHitSparksAddr() const override {
        return CC_HIT_SPARKS_ADDR;  // 0x67BD78
    }
    
    int* getShowAttackDisplayAddr() const override {
        return CC_SHOW_ATTACK_DISPLAY;  // 0x5595B8
    }
    
    int* getShowInputDisplayAddr() const override {
        return CC_SHOW_INPUT_DISPLAY;  // 0x5585F8
    }
    
    // ===================================================================
    // SOUND
    // ===================================================================
    
    uint8_t* getSfxArrayAddr() const override {
        return CC_SFX_ARRAY_ADDR;  // 0x76E008
    }
    
    uint32_t getSfxArrayLen() const override {
        return CC_SFX_ARRAY_LEN;  // 1500
    }
    
    // ===================================================================
    // GRAPHICS
    // ===================================================================
    
    uint32_t* getD3DX9ObjAddr() const override {
        return CC_D3DX9_OBJ_ADDR;  // 0x76E7D4
    }
    
    uint32_t* getReplayCreatedAddr() const override {
        return CC_REPLAY_CREATED_ADDR;  // 0x774C30
    }
    
    void* getRepRoundTblEndPtrAddr() const override {
        return CC_REPROUND_TBL_ENDPTR_ADDR;  // 0x77BF9C
    }
    
    // ===================================================================
    // SPRITE & TEXT RESOURCES
    // ===================================================================
    
    uint32_t getButtonSpriteTexAddr() const override {
        return BUTTON_SPRITE_TEX;  // 0x74D5E8
    }
    
    uint32_t getFont0Addr() const override {
        return FONT0;  // 0x55D680
    }
    
    uint32_t getFont1Addr() const override {
        return FONT1;  // 0x55D260
    }
    
    uint32_t getFont2Addr() const override {
        return FONT2;  // 0x55DAA0
    }
    
    // ===================================================================
    // KEYBOARD CONFIG
    // ===================================================================
    
    uint32_t getKeyboardConfigOffset() const override {
        return CC_KEYBOARD_CONFIG_OFFSET;  // 0x14D2C0
    }
    
    // ===================================================================
    // GAME FEATURES
    // ===================================================================
    
    bool hasPuppetCharacters() const override {
        return true;  // MBAACC has puppet characters (Maids, NekoMech, KohaMech)
    }
    
    bool hasMoonStyles() const override {
        return true;  // MBAACC has moon styles (Crescent, Half, Full)
    }
    
    uint32_t getCharacterCount() const override {
        return 31;  // MBAACC has 31 characters
    }
    
    uint32_t getStageCount() const override {
        return 36;  // MBAACC stage count
    }
    
    uint32_t getPreGameIntroFrames() const override {
        return CC_PRE_GAME_INTRO_FRAMES;  // 224
    }
    
    // ===================================================================
    // ASSEMBLY HACKS (from DllAsmHacks.hpp)
    // ===================================================================
    
    const std::vector<Asm>& getHookMainLoop() const override {
        return AsmHacks::hookMainLoop;
    }
    
    const std::vector<Asm>& getEnableDisabledStages() const override {
        return AsmHacks::enableDisabledStages;
    }
    
    const Asm& getDisableFpsLimit() const override {
        return AsmHacks::disableFpsLimit;
    }
    
    const Asm& getDisableFpsCounter() const override {
        return AsmHacks::disableFpsCounter;
    }
    
    const std::vector<Asm>& getHijackControls() const override {
        return AsmHacks::hijackControls;
    }
    
    const std::vector<Asm>& getHijackMenu() const override {
        return AsmHacks::hijackMenu;
    }
    
    const std::vector<Asm>& getDetectRoundStart() const override {
        return AsmHacks::detectRoundStart;
    }
    
    const std::vector<Asm>& getSaveReplay() const override {
        return AsmHacks::saveReplay;
    }
    
    const Asm& getDetectAutoReplaySave() const override {
        return AsmHacks::detectAutoReplaySave;
    }
    
    const Asm& getMultiWindow() const override {
        return AsmHacks::multiWindow;
    }
    
    const Asm& getForceGotoVersus() const override {
        return AsmHacks::forceGotoVersus;
    }
    
    const Asm& getForceGotoVersusCPU() const override {
        return AsmHacks::forceGotoVersusCPU;
    }
    
    const Asm& getForceGotoTraining() const override {
        return AsmHacks::forceGotoTraining;
    }
    
    const Asm& getForceGotoReplay() const override {
        return AsmHacks::forceGotoReplay;
    }
    
    const Asm& getHijackEscapeKey() const override {
        return AsmHacks::hijackEscapeKey;
    }
    
    const std::vector<Asm>& getFilterRepeatedSfx() const override {
        return AsmHacks::filterRepeatedSfx;
    }
    
    const std::vector<Asm>& getMuteSpecificSfx() const override {
        return AsmHacks::muteSpecificSfx;
    }
    
    const Asm& getHijackIntroState() const override {
        return AsmHacks::hijackIntroState;
    }
    
    const Asm& getDisableTrainingMusicReset() const override {
        return AsmHacks::disableTrainingMusicReset;
    }
    
    const Asm& getFixBossStageSuperFlashOverlay() const override {
        return AsmHacks::fixBossStageSuperFlashOverlay;
    }
    
    const Asm& getHijackCharaSelectColors() const override {
        return AsmHacks::hijackCharaSelectColors;
    }
    
    const std::vector<Asm>& getHijackLoadingStateColors() const override {
        return AsmHacks::hijackLoadingStateColors;
    }
    
    const std::vector<Asm>& getDisableHealthBars() const override {
        return AsmHacks::disableHealthBars;
    }
    
    const std::vector<Asm>& getAddExtraDraws() const override {
        return AsmHacks::addExtraDraws;
    }
    
    const std::vector<Asm>& getAddExtraTextures() const override {
        return AsmHacks::addExtraTextures;
    }
    
    const std::vector<Asm>& getLoadCustomPalettesAsm() const override {
        return AsmHacks::loadCustomPalettesAsm;
    }
};

