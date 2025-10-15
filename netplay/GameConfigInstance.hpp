#pragma once

#include "GameConfig.hpp"
#include "GameConfigMBAA.hpp"
#include "GameConfigMBAC.hpp"
#include <string>

/**
 * Global game configuration instance with RUNTIME game detection
 * 
 * CCCaster now supports multiple games in a single build!
 * The game is auto-detected by scanning for executables in the game folder:
 *  - MBAA.exe → Melty Blood Actress Again Current Code v1.07
 *  - mbacPC.exe → Melty Blood Act Cadenza v1.03a
 *  - MB.exe → Melty Blood v1.00 (future)
 *  - MBREACT.exe → Melty Blood ReACT (future)
 */

// Supported Melty Blood games
enum class MeltyGame {
    Unknown = 0,
    MBAA_CurrentCode,  // Melty Blood Actress Again Current Code v1.07
    MBAC_ActCadenza,   // Melty Blood Act Cadenza v1.03a
    MB_Original,       // Melty Blood v1.00 (future)
    MBRA_ReACT         // Melty Blood ReACT (future)
};

// Game metadata for detection and display
struct GameInfo {
    MeltyGame type;
    const char* executableName;
    const char* displayName;
    const char* shortName;
};

// Singleton pattern - single global instance with runtime game selection
class GameConfigInstance {
private:
    static GameConfig* instance;
    static MeltyGame currentGame;
    
    // Static instances of all game configs (allocated once)
    static GameConfigMBAA configMBAA;
    static GameConfigMBAC configMBAC;
    // Add more configs here as games are supported
    
    GameConfigInstance() = delete;  // No construction
    
public:
    // All supported games
    static const GameInfo supportedGames[];
    static const int supportedGameCount;
    
    /**
     * Set the active game configuration
     * This should be called during initialization after detecting the game
     */
    static void setGame(MeltyGame game);
    
    /**
     * Auto-detect game by scanning the given directory for known executables
     * Returns the detected game type, or Unknown if none found
     */
    static MeltyGame detectGame(const std::string& gameDir);
    
    /**
     * Get the currently active game configuration
     */
    static GameConfig& get() {
        if (!instance) {
            // Default to MBAACC if no game was explicitly set
            setGame(MeltyGame::MBAA_CurrentCode);
        }
        return *instance;
    }
    
    /**
     * Get the current game type
     */
    static MeltyGame getCurrentGame() {
        return currentGame;
    }
    
    /**
     * Get executable name for a specific game type
     */
    static const char* getExecutableName(MeltyGame game);
    
    /**
     * Get display name for a specific game type
     */
    static const char* getDisplayName(MeltyGame game);
    
    // Convenience type checks
    static bool isMBAA() { return currentGame == MeltyGame::MBAA_CurrentCode; }
    static bool isMBAC() { return currentGame == MeltyGame::MBAC_ActCadenza; }
    static bool isMB() { return currentGame == MeltyGame::MB_Original; }
    static bool isMBRA() { return currentGame == MeltyGame::MBRA_ReACT; }
};

// Convenience macro for accessing the global config
#define g_gameConfig GameConfigInstance::get()

