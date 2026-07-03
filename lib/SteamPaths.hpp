#pragma once

#include <string>


// Locate a Steam game's install folder WITHOUT hardcoding any drive/path. The chain is the same one
// Steam itself uses:
//   registry (Steam root) -> steamapps\libraryfolders.vdf (every library, any drive)
//   -> appmanifest_<appId>.acf ("installdir") -> <library>\steamapps\common\<installdir>
//
// Returns the folder WITH a trailing backslash (e.g. "C:\...\common\MELTY BLOOD Actress Again Current
// Code\") iff that folder actually contains exeName; otherwise "". Safe to call on any machine -- if
// Steam isn't installed or the game isn't found it just returns "".
std::string findSteamGameDir ( const std::string& appId, const std::string& exeName );

// The Steam install root (e.g. "C:\Program Files (x86)\Steam"), or "" if Steam isn't installed.
std::string findSteamRoot();
