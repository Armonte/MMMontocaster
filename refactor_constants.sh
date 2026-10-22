#!/bin/bash
# Batch replace all CC_ constants with g_gameConfig calls

cd /home/teo/dev/MMMontocaster/MMMontocaster

echo "Refactoring CC_ constants to use GameConfig..."

# Define replacements (pattern → replacement)
declare -A replacements=(
    # Already done, but ensure consistency:
    ["CC_GAME_MODE_ADDR"]="g_gameConfig.getGameModeAddr()"
    ["CC_SKIP_FRAMES_ADDR"]="g_gameConfig.getSkipFramesAddr()"
    
    # Remaining addresses - systematic batch
    ["CC_PAUSE_FLAG_ADDR"]="g_gameConfig.getPauseFlagAddr()"
    ["CC_SCREEN_WIDTH_ADDR"]="g_gameConfig.getScreenWidthAddr()"
    ["CC_GAME_STATE_ADDR"]="g_gameConfig.getGameStateAddr()"
    ["CC_SKIPPABLE_FLAG_ADDR"]="g_gameConfig.getSkippableFlagAddr()"
    ["CC_MENU_STATE_COUNTER_ADDR"]="g_gameConfig.getMenuStateCounterAddr()"
    ["CC_TRAINING_PAUSE_ADDR"]="g_gameConfig.getTrainingPauseAddr()"
    ["CC_VERSUS_PAUSE_ADDR"]="g_gameConfig.getVersusPauseAddr()"
    ["CC_DUMMY_STATUS_ADDR"]="g_gameConfig.getDummyStatusAddr()"
    ["CC_AUTO_REPLAY_SAVE_ADDR"]="g_gameConfig.getAutoReplaySaveAddr()"
    ["CC_STAGE_SELECTOR_ADDR"]="g_gameConfig.getStageSelectorAddr()"
    ["CC_REPLAY_CREATED_ADDR"]="g_gameConfig.getReplayCreatedAddr()"
    ["CC_D3DX9_OBJ_ADDR"]="g_gameConfig.getD3DX9ObjAddr()"
    ["CC_REPROUND_TBL_ENDPTR_ADDR"]="g_gameConfig.getRepRoundTblEndPtrAddr()"
    
    # Player 1 addresses
    ["CC_P1_ENABLED_FLAG_ADDR"]="g_gameConfig.getP1EnabledFlagAddr()"
    ["CC_P1_SEQUENCE_ADDR"]="g_gameConfig.getP1SequenceAddr()"
    ["CC_P1_SEQ_STATE_ADDR"]="g_gameConfig.getP1SeqStateAddr()"
    ["CC_P1_HEALTH_ADDR"]="g_gameConfig.getP1HealthAddr()"
    ["CC_P1_RED_HEALTH_ADDR"]="g_gameConfig.getP1RedHealthAddr()"
    ["CC_P1_GUARD_BAR_ADDR"]="g_gameConfig.getP1GuardBarAddr()"
    ["CC_P1_GUARD_QUALITY_ADDR"]="g_gameConfig.getP1GuardQualityAddr()"
    ["CC_P1_METER_ADDR"]="g_gameConfig.getP1MeterAddr()"
    ["CC_P1_HEAT_ADDR"]="g_gameConfig.getP1HeatAddr()"
    ["CC_P1_NO_INPUT_FLAG_ADDR"]="g_gameConfig.getP1NoInputFlagAddr()"
    ["CC_P1_PUPPET_STATE_ADDR"]="g_gameConfig.getP1PuppetStateAddr()"
    ["CC_P1_Y_POSITION_ADDR"]="g_gameConfig.getP1YPositionAddr()"
    ["CC_P1_X_PREV_POS_ADDR"]="g_gameConfig.getP1XPrevPosAddr()"
    ["CC_P1_Y_PREV_POS_ADDR"]="g_gameConfig.getP1YPrevPosAddr()"
    ["CC_P1_X_VELOCITY_ADDR"]="g_gameConfig.getP1XVelocityAddr()"
    ["CC_P1_Y_VELOCITY_ADDR"]="g_gameConfig.getP1YVelocityAddr()"
    ["CC_P1_X_ACCELERATION_ADDR"]="g_gameConfig.getP1XAccelerationAddr()"
    ["CC_P1_Y_ACCELERATION_ADDR"]="g_gameConfig.getP1YAccelerationAddr()"
    ["CC_P1_SPRITE_ANGLE_ADDR"]="g_gameConfig.getP1SpriteAngleAddr()"
    ["CC_P1_FACING_FLAG_ADDR"]="g_gameConfig.getP1FacingFlagAddr()"
    ["CC_P1_COMBO_GUARD_ADDR"]="g_gameConfig.getP1ComboGuardAddr()"
    
    # Player 2 addresses
    ["CC_P2_ENABLED_FLAG_ADDR"]="g_gameConfig.getP2EnabledFlagAddr()"
    ["CC_P2_SEQUENCE_ADDR"]="g_gameConfig.getP2SequenceAddr()"
    ["CC_P2_SEQ_STATE_ADDR"]="g_gameConfig.getP2SeqStateAddr()"
    ["CC_P2_HEALTH_ADDR"]="g_gameConfig.getP2HealthAddr()"
    ["CC_P2_RED_HEALTH_ADDR"]="g_gameConfig.getP2RedHealthAddr()"
    ["CC_P2_GUARD_BAR_ADDR"]="g_gameConfig.getP2GuardBarAddr()"
    ["CC_P2_GUARD_QUALITY_ADDR"]="g_gameConfig.getP2GuardQualityAddr()"
    ["CC_P2_METER_ADDR"]="g_gameConfig.getP2MeterAddr()"
    ["CC_P2_HEAT_ADDR"]="g_gameConfig.getP2HeatAddr()"
    ["CC_P2_NO_INPUT_FLAG_ADDR"]="g_gameConfig.getP2NoInputFlagAddr()"
    ["CC_P2_PUPPET_STATE_ADDR"]="g_gameConfig.getP2PuppetStateAddr()"
    ["CC_P2_Y_POSITION_ADDR"]="g_gameConfig.getP2YPositionAddr()"
    ["CC_P2_FACING_FLAG_ADDR"]="g_gameConfig.getP2FacingFlagAddr()"
    
    # Character select
    ["CC_P1_SELECTOR_MODE_ADDR"]="g_gameConfig.getP1SelectorModeAddr()"
    ["CC_P1_CHARA_SELECTOR_ADDR"]="g_gameConfig.getP1CharaSelectorAddr()"
    ["CC_P1_MOON_SELECTOR_ADDR"]="g_gameConfig.getP1MoonSelectorAddr()"
    ["CC_P1_COLOR_SELECTOR_ADDR"]="g_gameConfig.getP1ColorSelectorAddr()"
    ["CC_P2_SELECTOR_MODE_ADDR"]="g_gameConfig.getP2SelectorModeAddr()"
    ["CC_P2_CHARA_SELECTOR_ADDR"]="g_gameConfig.getP2CharaSelectorAddr()"
    ["CC_P2_MOON_SELECTOR_ADDR"]="g_gameConfig.getP2MoonSelectorAddr()"
    ["CC_P2_COLOR_SELECTOR_ADDR"]="g_gameConfig.getP2ColorSelectorAddr()"
    
    # Game point flags
    ["CC_P1_GAME_POINT_FLAG_ADDR"]="g_gameConfig.getP1GamePointFlagAddr()"
    ["CC_P2_GAME_POINT_FLAG_ADDR"]="g_gameConfig.getP2GamePointFlagAddr()"
    ["CC_P1_WINS_ADDR"]="g_gameConfig.getP1WinsAddr()"
    ["CC_P2_WINS_ADDR"]="g_gameConfig.getP2WinsAddr()"
    ["CC_ROUND_COUNT_ADDR"]="g_gameConfig.getRoundCountAddr()"
)

# Apply replacements to all target files
for file in targets/Dll*.cpp targets/Dll*.hpp; do
    if [ -f "$file" ]; then
        echo "  Refactoring $file..."
        for pattern in "${!replacements[@]}"; do
            replacement="${replacements[$pattern]}"
            sed -i "s/$pattern/$replacement/g" "$file"
        done
    fi
done

# Also refactor ProcessManager and other netplay files  
for file in netplay/*.cpp; do
    if [ -f "$file" ]; then
        echo "  Refactoring $file..."
        for pattern in "${!replacements[@]}"; do
            replacement="${replacements[$pattern]}"
            sed -i "s/$pattern/$replacement/g" "$file"
        done
    fi
done

echo "✅ Refactoring complete!"
echo ""
echo "Total replacements made:"
grep -r "g_gameConfig\." targets/ netplay/ --include='*.cpp' --include='*.hpp' | wc -l

