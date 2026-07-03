#include "SteamPaths.hpp"

#include <windows.h>

#include <fstream>
#include <vector>
#include <algorithm>

using namespace std;


// Read a REG_SZ value, or "" on any failure. cccaster's frontend is a 32-bit process, so an HKLM
// "SOFTWARE\Valve\Steam" read is WOW64-redirected to Wow6432Node automatically; we also try the
// explicit Wow6432Node path for robustness.
static string regReadString ( HKEY root, const char* subKey, const char* value )
{
    HKEY key;
    if ( RegOpenKeyExA ( root, subKey, 0, KEY_READ, &key ) != ERROR_SUCCESS )
        return "";

    char buffer[MAX_PATH] = { 0 };
    DWORD size = sizeof ( buffer );
    DWORD type = 0;
    const LONG result = RegQueryValueExA ( key, value, 0, &type, ( LPBYTE ) buffer, &size );
    RegCloseKey ( key );

    if ( result != ERROR_SUCCESS || ( type != REG_SZ && type != REG_EXPAND_SZ ) )
        return "";

    return string ( buffer );
}

// Normalize to backslashes and drop any trailing slash.
static string cleanPath ( string path )
{
    replace ( path.begin(), path.end(), '/', '\\' );
    while ( ! path.empty() && path.back() == '\\' )
        path.pop_back();
    return path;
}

static bool pathExists ( const string& path )
{
    return GetFileAttributesA ( path.c_str() ) != INVALID_FILE_ATTRIBUTES;
}

// Extract the value of the FIRST `"<key>"  "<value>"` pair found in a VDF/ACF line, un-escaping the
// `\\` that Steam writes for path separators. Returns "" if this line doesn't hold that key.
static string vdfValueOnLine ( const string& line, const string& key )
{
    const string token = "\"" + key + "\"";
    size_t k = line.find ( token );
    if ( k == string::npos )
        return "";

    size_t q1 = line.find ( '"', k + token.size() );
    if ( q1 == string::npos )
        return "";
    size_t q2 = line.find ( '"', q1 + 1 );
    if ( q2 == string::npos )
        return "";

    const string raw = line.substr ( q1 + 1, q2 - q1 - 1 );

    string out;
    for ( size_t i = 0; i < raw.size(); ++i )
    {
        if ( raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '\\' )
        {
            out += '\\';
            ++i;
        }
        else
        {
            out += raw[i];
        }
    }
    return out;
}


string findSteamRoot()
{
    string root = regReadString ( HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath" );
    if ( root.empty() )
        root = regReadString ( HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Valve\\Steam", "InstallPath" );
    if ( root.empty() )
        root = regReadString ( HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", "InstallPath" );
    return cleanPath ( root );
}

// Every Steam library root: the Steam install itself plus each "path" listed in libraryfolders.vdf
// (modern format, post-2021). The user may have libraries on multiple drives.
static vector<string> steamLibraries ( const string& root )
{
    vector<string> libs;
    if ( root.empty() )
        return libs;

    libs.push_back ( root );

    ifstream file ( root + "\\steamapps\\libraryfolders.vdf" );
    string line;
    while ( getline ( file, line ) )
    {
        const string path = cleanPath ( vdfValueOnLine ( line, "path" ) );
        if ( ! path.empty() )
            libs.push_back ( path );
    }
    return libs;
}


string findSteamGameDir ( const string& appId, const string& exeName )
{
    const string root = findSteamRoot();
    if ( root.empty() )
        return "";

    for ( const string& lib : steamLibraries ( root ) )
    {
        const string acf = lib + "\\steamapps\\appmanifest_" + appId + ".acf";
        if ( ! pathExists ( acf ) )
            continue;

        string installDir;
        {
            ifstream file ( acf );
            string line;
            while ( getline ( file, line ) )
            {
                const string value = vdfValueOnLine ( line, "installdir" );
                if ( ! value.empty() )
                {
                    installDir = value;
                    break;
                }
            }
        }
        if ( installDir.empty() )
            continue;

        const string gameDir = lib + "\\steamapps\\common\\" + installDir + "\\";
        if ( pathExists ( gameDir + exeName ) )
            return gameDir;
    }

    return "";
}
