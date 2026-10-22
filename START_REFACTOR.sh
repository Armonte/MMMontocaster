#!/bin/bash
# Multi-Game Refactor Quick Start

echo "======================================"
echo "  CCCaster Multi-Game Refactor"
echo "======================================"
echo ""

# Check if addresses are found
echo "Step 1: Address Finding Status"
echo "--------------------------------"
echo "⚠️  CRITICAL: Need 3 addresses before refactoring:"
echo "   [ ] rngIndex   (find in getRNG at 0x421A80)"
echo "   [ ] outRNG     (find in getRNG at 0x421A80)"
echo "   [ ] frameCount (verify xrefs to 0x76e64c)"
echo ""
echo "Time needed: 30-60 minutes with IDA/Ghidra"
echo ""

read -p "Have you found all 3 critical addresses? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "📖 Read: docs/multi_game_refactor/PRE_REFACTOR_ADDRESS_FINDING.md"
    echo "    Execute Missions 1-3 to find addresses"
    echo ""
    exit 1
fi

echo ""
echo "Step 2: Create Git Branch"
echo "-------------------------"
git checkout -b multi-game-refactor
echo "✅ Branch created"
echo ""

echo "Step 3: Documentation References"
echo "---------------------------------"
echo "📖 Architecture:     docs/multi_game_refactor/CCCASTER_MULTI_GAME_ARCHITECTURE_PLAN.md"
echo "📖 Implementation:   docs/multi_game_refactor/IMPLEMENTATION_QUICK_START.md"
echo "📖 Quick Reference:  docs/multi_game_refactor/QUICK_REFERENCE_CARD.md"
echo ""

echo "Step 4: Create Directory Structure"
echo "-----------------------------------"
mkdir -p netplay/game_configs
mkdir -p targets/asm_hacks
echo "✅ Directories created"
echo ""

echo "======================================"
echo "Ready to start refactoring!"
echo "======================================"
echo ""
echo "Next: Create netplay/game_configs/GameConfig.hpp (interface)"
echo ""
