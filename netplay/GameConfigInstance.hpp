#pragma once

#include "GameConfig.hpp"
#include "GameConfigMBAA.hpp"
#include "GameConfigMBAC.hpp"

/**
 * Global game configuration instance
 * 
 * This is selected at compile time via build flags:
 *  - BUILD_MBAA (default): Uses GameConfigMBAA for MBAACC v1.07
 *  - BUILD_MBAC: Uses GameConfigMBAC for MBAC v1.03a
 *  - BUILD_MB: Uses GameConfigMB for MB v1.00 (future)
 *  - BUILD_MBREACT: Uses GameConfigMBReact for MBR (future)
 */

// Singleton pattern - single global instance
class GameConfigInstance {
private:
    static GameConfig* instance;
    
    GameConfigInstance() = delete;  // No construction
    
public:
    static GameConfig& get() {
        if (!instance) {
            // Select configuration based on build flag
#ifdef BUILD_MBAC
            static GameConfigMBAC configMBAC;
            instance = &configMBAC;
#elif defined(BUILD_MB)
            #error "BUILD_MB not yet implemented"
#elif defined(BUILD_MBREACT)
            #error "BUILD_MBREACT not yet implemented"
#else
            // Default to MBAACC
            static GameConfigMBAA configMBAA;
            instance = &configMBAA;
#endif
        }
        return *instance;
    }
    
    // Check which game is active (for runtime branching if needed)
    static bool isMBAA() {
#ifdef BUILD_MBAC
        return false;
#else
        return true;
#endif
    }
    
    static bool isMBAC() {
#ifdef BUILD_MBAC
        return true;
#else
        return false;
#endif
    }
};

// Convenience macro for accessing the global config
#define g_gameConfig GameConfigInstance::get()

