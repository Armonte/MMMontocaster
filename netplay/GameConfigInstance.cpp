#include "GameConfigInstance.hpp"

// Initialize static member - must be in .cpp to avoid multiple definitions
GameConfig* GameConfigInstance::instance = nullptr;

