#include "GameConfigInstance.hpp"
#include "targets/DllAsmHacks.hpp"       // MBAA assembly hacks
#include "targets/DllAsmHacksMBAC.hpp"  // MBAC assembly hacks
#include <windows.h>

// Initialize static members
GameConfig* GameConfigInstance::instance = nullptr;
MeltyGame GameConfigInstance::currentGame = MeltyGame::Unknown;

// Static game config instances (allocated once, reused)
GameConfigMBAA GameConfigInstance::configMBAA;
GameConfigMBAC GameConfigInstance::configMBAC;

// Supported games list
const GameInfo GameConfigInstance::supportedGames[] = {
    { MeltyGame::MBAA_CurrentCode, "MBAA.exe",    "Melty Blood Actress Again Current Code v1.07", "MBAACC" },
    { MeltyGame::MBAC_ActCadenza,  "mbacPC.exe",  "Melty Blood Act Cadenza v1.03a",              "MBAC" },
    { MeltyGame::MB_Original,      "MB.exe",      "Melty Blood v1.00",                           "MB" },
    { MeltyGame::MBRA_ReACT,       "MBREACT.exe", "Melty Blood ReACT",                           "MBRA" },
};

const int GameConfigInstance::supportedGameCount = sizeof(supportedGames) / sizeof(GameInfo);

// Auto-detect game by checking for executables
MeltyGame GameConfigInstance::detectGame(const std::string& gameDir) {
    // Try each supported game in order
    for (int i = 0; i < supportedGameCount; ++i) {
        std::string exePath = gameDir + supportedGames[i].executableName;
        
        // Check if file exists
        DWORD fileAttr = GetFileAttributes(exePath.c_str());
        if (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
            // Found the executable!
            return supportedGames[i].type;
        }
    }
    
    return MeltyGame::Unknown;
}

// Get executable name for a game type
const char* GameConfigInstance::getExecutableName(MeltyGame game) {
    for (int i = 0; i < supportedGameCount; ++i) {
        if (supportedGames[i].type == game) {
            return supportedGames[i].executableName;
        }
    }
    return "Unknown.exe";
}

// Get display name for a game type
const char* GameConfigInstance::getDisplayName(MeltyGame game) {
    for (int i = 0; i < supportedGameCount; ++i) {
        if (supportedGames[i].type == game) {
            return supportedGames[i].displayName;
        }
    }
    return "Unknown Game";
}

// Set the active game configuration
void GameConfigInstance::setGame(MeltyGame game) {
    currentGame = game;
    
    switch (game) {
        case MeltyGame::MBAA_CurrentCode:
            instance = &configMBAA;
            break;
        case MeltyGame::MBAC_ActCadenza:
            instance = &configMBAC;
            break;
        case MeltyGame::MB_Original:
            // Future: instance = &configMB;
            instance = &configMBAA;  // Fallback for now
            break;
        case MeltyGame::MBRA_ReACT:
            // Future: instance = &configMBRA;
            instance = &configMBAA;  // Fallback for now
            break;
        default:
            // Unknown game - default to MBAA
            instance = &configMBAA;
            break;
    }
}

