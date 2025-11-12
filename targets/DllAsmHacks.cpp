#include "DllAsmHacks.hpp"
#include "Messages.hpp"
#include "DllNetplayManager.hpp"
#include "CharacterSelect.hpp"
#include "Logger.hpp"
#include "DllTrialManager.hpp"

#include <windows.h>
#include <d3dx9.h>
#include <fstream>
#include <vector>
#include <iterator>
#include <array>
#include <map>
#include <atomic>
#include <regex>
#include <optional>
#include <filesystem>
#include <cstring>
namespace fs = std::filesystem;

using namespace std;


static int memwrite ( void *dst, const void *src, size_t len )
{
    DWORD old, tmp;

    if ( ! VirtualProtect ( dst, len, PAGE_READWRITE, &old ) )
        return GetLastError();

    memcpy ( dst, src, len );

    if ( ! VirtualProtect ( dst, len, old, &tmp ) )
        return GetLastError();

    return 0;
}


namespace AsmHacks
{

uint32_t currentMenuIndex = 0;

uint32_t menuConfirmState = 0;

uint32_t roundStartCounter = 0;

char *replayName = 0;

uint32_t *autoReplaySaveStatePtr = 0;

uint8_t enableEscapeToExit = true;

uint8_t sfxFilterArray[CC_SFX_ARRAY_LEN] = { 0 };

uint8_t sfxMuteArray[CC_SFX_ARRAY_LEN] = { 0 };

uint32_t numLoadedColors = 0;

// VS Result Menu globals
uint32_t* gVsResultMenuMode = (uint32_t*)0x77BF2C;
uint32_t* gStoryModeClearFlag = (uint32_t*)0x5585F4;
uint32_t* gVsResultMenuInputState = (uint32_t*)0x774C10;
void** gVsResultMenuHandle = (void**)0x774C38;

// YES/NO dialog state for "ONCE AGAIN" prompt
static void* gOnceAgainYesNoDialog = nullptr;
static bool gOnceAgainYesNoDialogActive = false;
static int gOnceAgainYesNoDialogResult = -1; // -1=no selection, 0=NO, 1=YES
static void* gBattleContextForDialog = nullptr; // Store battleContext for dialog handling

// Function pointer types for YES/NO dialog functions
typedef int (__stdcall* CTM_YesNo_Init_t)(void* menuManager);
static CTM_YesNo_Init_t CTM_YesNo_Init = (CTM_YesNo_Init_t)0x47D070;

typedef int (__stdcall* VsResultMenu_Create_t)(int skipQuickRetry);
static VsResultMenu_Create_t VsResultMenu_Create_Original = (VsResultMenu_Create_t)0x482CD0;


// The team order is always (initial) point character first
static unordered_map<uint32_t, pair<uint32_t, uint32_t>> teamOrders =
{
    {  4, {  5,  6 } }, // Maids -> Hisui, Kohaku
    { 34, { 14, 20 } }, // NekoMech -> M.Hisui, Neko
    { 35, {  6, 14 } }, // KohaMech -> Kohaku, M.Hisui
};

extern "C" void charaSelectColorCb()
{
    uint32_t *edi;

    asm ( "movl %%edi,%0" : "=r" ( edi ) );

    Sleep ( 20 ); // This is code that was replaced

    uint32_t *ptrBase = ( uint32_t * ) 0x74D808;

    if ( ! *ptrBase )
        return;

    uint32_t *ptr1     = ( uint32_t * ) ( *ptrBase + 0x1AC ); // P1 color table reference
    uint32_t *partner1 = ( uint32_t * ) ( *ptrBase + 0x1B8 ); // P1 partner color table reference
    uint32_t *ptr2     = ( uint32_t * ) ( *ptrBase + 0x388 ); // P2 color table reference
    uint32_t *partner2 = ( uint32_t * ) ( *ptrBase + 0x394 ); // P2 partner color table reference

    LOG ( "edi=%08X; ptr1=%08X; partner1=%08X; ptr2=%08X; partner2=%08X", edi, ptr1, partner1, ptr2, partner2 );

    const uint32_t chara1 = *CC_P1_CHARACTER_ADDR;
    const uint32_t chara2 = *CC_P2_CHARACTER_ADDR;

    const auto& team1 = teamOrders.find ( chara1 );
    const auto& team2 = teamOrders.find ( chara2 );

    const bool hasTeam1 = ( team1 != teamOrders.end() );
    const bool hasTeam2 = ( team2 != teamOrders.end() );

    if ( edi + 1 == ptr1 && *ptr1 )
    {
        colorLoadCallback ( 1, ( hasTeam1 ? team1->second.first : chara1 ), ( ( uint32_t * ) *ptr1 ) + 1 );
    }
    else if ( edi + 1 == ptr2 && *ptr2 )
    {
        colorLoadCallback ( 2, ( hasTeam2 ? team2->second.first : chara2 ), ( ( uint32_t * ) *ptr2 ) + 1 );
    }
    else if ( edi + 1 == partner1 && *partner1 )
    {
        colorLoadCallback ( 1, ( hasTeam1 ? team1->second.second : chara1 ), ( ( uint32_t * ) *partner1 ) + 1 );
    }
    else if ( edi + 1 == partner2 && *partner2 )
    {
        colorLoadCallback ( 2, ( hasTeam2 ? team2->second.second : chara2 ), ( ( uint32_t * ) *partner2 ) + 1 );
    }
}

static void loadingStateColorCb2 ( uint32_t *singlePaletteData )
{
    const uint32_t chara1 = *CC_P1_CHARACTER_ADDR;
    const uint32_t chara2 = *CC_P2_CHARACTER_ADDR;

    const auto& team1 = teamOrders.find ( chara1 );
    const auto& team2 = teamOrders.find ( chara2 );

    const bool hasTeam1 = ( team1 != teamOrders.end() );
    const bool hasTeam2 = ( team2 != teamOrders.end() );

    if ( hasTeam1 || hasTeam2 )
    {
        uint32_t player = ( numLoadedColors % 2 ) + 1;

        if ( ! hasTeam1 && hasTeam2 )
            player = ( numLoadedColors < 1 ? 1 : 2 );

        uint32_t chara = ( player == 1 ? chara1 : chara2 );

        if ( hasTeam1 && player == 1 )
            chara = ( numLoadedColors < 2 ? team1->second.first : team1->second.second );
        else if ( hasTeam2 && player == 2 )
            chara = ( numLoadedColors < 2 ? team2->second.first : team2->second.second );

        colorLoadCallback (
            player,
            chara,
            * ( player == 1 ? CC_P1_COLOR_SELECTOR_ADDR : CC_P2_COLOR_SELECTOR_ADDR ),
            singlePaletteData );
    }
    else if ( numLoadedColors < 2 )
    {
        colorLoadCallback (
            numLoadedColors + 1,
            ( numLoadedColors == 0 ? chara1 : chara2 ),
            * ( numLoadedColors == 0 ? CC_P1_COLOR_SELECTOR_ADDR : CC_P2_COLOR_SELECTOR_ADDR ),
            singlePaletteData );
    }

    ++numLoadedColors;
}

extern "C" void saveReplayCb()
{
    //netManPtr->exportInputs();
}

extern "C" void loadingStateColorCb()
{
    uint32_t *ebx, *esi;

    asm ( "movl %%ebx,%0" : "=r" ( ebx ) );
    asm ( "movl %%esi,%0" : "=r" ( esi ) );

    uint32_t *ptr = ( uint32_t * ) ( ( uint32_t ( esi ) << 10 ) + uint32_t ( ebx ) + 4 );

    LOG ( "ebx=%08X; esi=%08X; ptr=%08X", ebx, esi, ptr );

    loadingStateColorCb2 ( ptr );
}
extern "C" void (*drawInputHistory) () = (void(*)()) 0x479460;

extern "C" int CallDrawText ( int width, int height, int xAddr, int yAddr, char* text, int textAlpha, int textShade, int textShade2, void* addr, int spacing, int layer, char* out );
/*
      A ------- B
      |         |
      |         |
      C --------D
*/
extern "C" int CallDrawRect ( int screenXAddr, int screenYAddr, int width, int height, int A, int B, int C, int D, int layer );
extern "C" int CallDrawSprite ( int spriteWidth, int dxdevice, int texAddr, int screenXAddr, int screenYAddr, int spriteHeight, int texXAddr, int texYAddr, int texXSize, int texYSize, int flags, int unk, int layer );

extern "C" void renderCallback();
// ARGB
extern "C" void addExtraDrawCallsCb() {
    renderCallback();

    //inputDisplay
    /*
    *(int*) 0x5585f8 = 0x1;
    drawInputHistory();
    *(int*) 0x55df0f = 0x1;
    drawInputHistory();
    *(int*) 0x55df0f = 0x0;
    */
}

extern "C" int loadTextureFromMemory( char* imgbuf1, int img1size, char* imgbuf2, int img2size, int param4 );

extern "C" void addExtraTexturesCb() {
    //MessageBoxA(0, "a", "a", 0);
    string filename = ".//GRP//arrows.png";
    string filename3 = ".//GRP//inputs.png";
    ifstream input( filename.c_str(), ios::binary );
    vector<char> buffer( istreambuf_iterator<char>(input), {} );
    int imgsize = buffer.size();
    char* rawimg = &buffer[0];
    ifstream input3( filename3.c_str(), ios::binary );
    vector<char> buffer3( istreambuf_iterator<char>(input3), {} );
    int imgsize3 = buffer3.size();
    char* rawimg3 = &buffer3[0];
    TrialManager::trialBGTextures = loadTextureFromMemory(rawimg, imgsize, 0, 0, 0);
    TrialManager::trialInputTextures = loadTextureFromMemory(rawimg3, imgsize3, 0, 0, 0);
}
int Asm::write() const
{
    backup.resize ( bytes.size() );
    memcpy ( &backup[0], addr, backup.size() );
    return memwrite ( addr, &bytes[0], bytes.size() );
}

int Asm::revert() const
{
    return memwrite ( addr, &backup[0], backup.size() );
}

// ----- all of this really should be moved to a different file.

#define swap32(v) __builtin_bswap32(v)

std::map<int, std::map<int, std::array<DWORD, 256>>> palettes;

typedef std::array<DWORD, 256> Palette;

int getIndexFromCharName(const std::string& name) {

    std::map<std::string, int> lookup = {
        {"SION",0},
        {"ARC",1},
        {"CIEL",2},
        {"AKIHA",3},
        
        {"HISUI",5},

        {"KOHAKU",6},
        {"KOHAKU_M",6},

        {"SHIKI",7},
        {"MIYAKO",8},
        {"WARAKIA",9},
        {"NERO",10},
        {"V_SION",11},
        {"WARC",12},
        {"AKAAKIHA",13},
        
        {"M_HISUI",14},
        {"M_HISUI_P",14},
        {"M_HISUI_M",14},

        {"NANAYA",15},
        {"SATSUKI",17},
        {"LEN",18},
        {"P_CIEL",19},
        
        {"NECO",20},
        {"NECO_P",20},

        {"AOKO",22},
        {"WLEN",23},
        {"NECHAOS",25},
        {"KISHIMA",28},
        {"S_AKIHA",29},
        {"RIES",30},
        {"ROA",31},
        {"RYOUGI",33},
        {"P_ARC",51},
        {"P_ARC_D",-1}, // i remember having some issues with P_ARC_D,, not doing it
    };

    if(!lookup.contains(name)) {
        log("couldnt find \"%s\"", name.c_str());
        return -1; 
    }

    return lookup[name];
}

class PNGChunk {
public:

	// watch out! values are in big endian!!

	PNGChunk(BYTE* PNGChunkStart) {
		len = (DWORD*)(PNGChunkStart + 0x0);
		PNGChunkType = (DWORD*)(PNGChunkStart + 0x4);
		data = (BYTE*)(PNGChunkStart + 0x8);

		DWORD tempLen = *len;
		tempLen = swap32(tempLen);
		crc = (DWORD*)(PNGChunkStart + 0x8 + tempLen);
	}

	PNGChunk getNextPNGChunk() {
		return PNGChunk((BYTE*)(crc) + 4);
	}

	void display() {

		char tempBuffer[5];
		memcpy(&tempBuffer, PNGChunkType, 4);
		tempBuffer[4] = 0;

		printf("PNGChunk len: %08X name: %s\n", swap32(*len), tempBuffer);

		if(strncmp(tempBuffer, "IHDR", 4) == 0) {
			printf("\t(%d, %d) d:%d type:%d\n", swap32(*(DWORD*)(data + 0x0)), swap32(*(DWORD*)(data + 0x4)), *(BYTE*)(data + 0x8), *(BYTE*)(data + 0x9));
		}
	}

	bool isIndexed() {
		if(strncmp((char*)PNGChunkType, "IHDR", 4) == 0) {
			return *(BYTE*)(data + 0x9) == 3;
		}
		return false;
	}

	std::optional<Palette> getPalette() {
		
		int bpp = 8; // this is an assumption.

		std::optional<Palette> res;

		if(strncmp((char*)PNGChunkType, "PLTE", 4) != 0) {
			return res;
		}

		Palette tempRes;
		for(int i=0; i<256; i++) {
			BYTE r = data[(i * 3) + 0];
			BYTE g = data[(i * 3) + 1];
			BYTE b = data[(i * 3) + 2];
			tempRes[i] = 0x01000000 | (b << 16) | (g << 8) | (r << 0); // if i remember correctly, melty uses ABGR
		}

		res = tempRes;

		return res;
	}

	bool isValid() {
		if(len == NULL || *len == 0) {
			return false;
		}
		return true;
	}

	DWORD* len = NULL;
	DWORD* PNGChunkType = NULL;
	BYTE* data = NULL;
	DWORD* crc = NULL;

};

std::vector<std::string> getPaletteFiles(const std::string& inputPath) {

	std::vector<std::string> res;
	
	std::regex re(R"((.+\.png)$)", std::regex::icase); 

    for (const auto & entry : fs::directory_iterator(inputPath)) {
		std::string p = entry.path().string();
        if(std::regex_match(p, re)) {
			res.push_back(p);
		}
	}

	return res;
}

std::optional<Palette> getPalette(const std::string& filePath) {

	// read https://en.wikipedia.org/wiki/PNG

	std::optional<Palette> res;

	std::ifstream file(filePath, std::ios::binary | std::ios::ate);
	if (!file.good()) {
		printf("couldnt find %s\n", filePath.c_str());
		return res;
	}

	int bufferSize = file.tellg();
	file.seekg(0, std::ios::beg);

	BYTE* buffer = (BYTE*)malloc(bufferSize);

	file.read((char*)buffer, bufferSize);

	if(strncmp((char*)buffer + 1, "PNG", 3) != 0) {
		printf("%s wasnt a png!\n", filePath.c_str());
		free(buffer);
		return res;
	}

	bool isIndexed = false; // checks IHDR 3
	int bpp = -1;

	PNGChunk PNGChunk(buffer + 0x8);
	while(PNGChunk.isValid()) {
		PNGChunk.display();
		std::optional<Palette> optPal = PNGChunk.getPalette();
		if(isIndexed && optPal.has_value()) {
			printf("\tgot palette!\n");
			res = optPal.value();
		}
		isIndexed |= PNGChunk.isIndexed();
		PNGChunk = PNGChunk.getNextPNGChunk();
	}

	if(res.has_value()) {
		printf("\treturning palette\n");
	} 
	
	free(buffer);

	return res;
}

void loadCustomPalettes() {

    static std::atomic<bool> loaded = false; // 0: unloaded, 1: loading, 2: loaded

    if(loaded) {
        return;
    }

    loaded = true;

    std::string pathString = "./cccaster/palettes/";
    fs::path dirPath(pathString);
    
    if (!fs::exists(dirPath)) {
        // create folder and instructions
        fs::create_directory(dirPath);
        
        std::ofstream outFile(pathString + "instructions.txt");

        std::string instructions = R"(
Instructions:

putting palettes in here will automatically load them into melty

please put PNG files from palmod in here, and give them the following naming scheme
    [characterID]_[paletteNum].png

example, for warc color 27 would be
    12_27.png

list of character IDs:

SION      0
ARC       1
CIEL      2
AKIHA     3
HISUI     5
KOHAKU    6
SHIKI     7
MIYAKO    8
WARAKIA   9
NERO     10
V_SION   11
WARC     12
VAKIHA   13
M_HISUI  14
NANAYA   15
SATSUKI  17
LEN      18
P_CIEL   19
NECO     20
AOKO     22
WLEN     23
NECHAOS  25
KISHIMA  28
S_AKIHA  29
RIES     30
ROA      31
RYOUGI   33
HIME     51
)";

        outFile << instructions;

        outFile.close();
    }
    
    std::vector<std::string> paletteList = getPaletteFiles(pathString);

	for(const std::string& filePath : paletteList) {
		std::optional<Palette> tempPalette = getPalette(filePath);
		
        std::regex pattern(R"((\d+)_([\d]+)\.png)");
        std::smatch matches;

        std::string filename = filePath.substr(filePath.find_last_of("/\\") + 1);

        if(tempPalette.has_value() && std::regex_match(filename, matches, pattern)) {
            
            int charID = std::stoi(matches[1]);
            int palNum = std::stoi(matches[2]);

            if(!palettes.contains(charID)) {
                palettes.insert({charID,  std::map<int, std::array<DWORD, 256>>()});
            }

            log("adding palette %s %d %d", filePath.c_str(), charID, palNum);
            palettes[charID].insert({palNum, tempPalette.value()});
		}
	}
}

void palettePatcher(DWORD EAX, DWORD EBX) {

    loadCustomPalettes();

    // can this func be called in different threads?

    if(EBX == 0) {
        return;
    }

    char ebxBuffer[256];
    strncpy(ebxBuffer, (char*)EBX, 256);

    std::string ebx(ebxBuffer);
    if(ebx.substr(MAX(0, ebx.size() - 4)) != ".pal") {
        return;
    }

    size_t lastBackslash = ebx.find_last_of('\\');
    if(lastBackslash == std::string::npos) {
        return;
    }
    std::string charName = ebx.substr(lastBackslash+1, ebx.size() - (lastBackslash+1) - 4);

    int charIndex = getIndexFromCharName(charName);
    
    if(charIndex == -1) {
        return;
    }

    DWORD* colors = (DWORD*)(EAX + 4); // the first dword is the array size

    if(palettes.contains(charIndex)) {
        for(auto it = palettes[charIndex].begin(); it != palettes[charIndex].end(); ++it) {
            if(it->first >= 1 && it->first <= 36) {
                memcpy(&colors[256 * (it->first - 1)], (it->second).data(), sizeof(DWORD) * 256); 
            }
        }
    }
}

void _naked_paletteCallback() {

    // patched at 0x0041f87a

    PUSH_ALL;
    __asmStart R"(
        push ebx;
        push eax;
        call _palettePatcher;
        add esp, 0x8;
    )" __asmEnd
    POP_ALL;

    ASMRET;
}

// -----

// VS Result Menu hooks for Once Again plugin

// Hook 1: VsResultMenu_Init - Force skipQuickRetryGate = 0 for offline versus
extern "C" void VsResultMenu_Init_Hook(void* manager, void* context) {
    // Check if offline versus (not network/replay/story)
    if (gVsResultMenuMode && *gVsResultMenuMode == 0) {
        if (gStoryModeClearFlag && *gStoryModeClearFlag == 0) {
            // Force skipQuickRetryGate = 0 to enable ONCE AGAIN prompt
            // Offset 0xD0 in CVSResultMenuManager struct
            if (manager) {
                int32_t* skipQuickRetryGate = (int32_t*)((char*)manager + 0xD0);
                *skipQuickRetryGate = 0;
            }
        }
    }
    
    // Call original function
    emitCall(0x481D80);
}

// Stub hook placeholders to keep build stable while Once Again integration is refactored.
// These hooks simply forward to the original game code when invoked.
extern "C" int ResultMenu_SetupWinQuote_Hook() {
    __asm__ __volatile__(
        ".att_syntax\n"
        "jmp 0x0043A700\n"  // Jump back to original ResultMenu_SetupWinQuote
    );
    return 0; // Never reached
}

extern "C" int ResultMenu_SetupWinQuote_Hook_Impl(int result, int* preMatchWords, int sceneState, int hasMenuChoice) {
    typedef int(__stdcall* ResultMenu_SetupWinQuote_t)(int, int*, int, int);
    static ResultMenu_SetupWinQuote_t original = reinterpret_cast<ResultMenu_SetupWinQuote_t>(0x0043A700);
    if (!original) {
        return 0;
    }
    return original(result, preMatchWords, sceneState, hasMenuChoice);
}

extern "C" void BattleScene_ProcessResultState_Trampoline(void* ctx, void* battleContext, int sceneState, char forceSkipQuickRetry, int hasMenuChoice, int a6, void* /*originalReturnAddress*/) {
    typedef void(__stdcall* BattleScene_ProcessResultState_t)(void*, void*, int, char, int, int);
    static BattleScene_ProcessResultState_t original = reinterpret_cast<BattleScene_ProcessResultState_t>(0x0043A4C0);
    if (!original) {
        return;
    }
    original(ctx, battleContext, sceneState, forceSkipQuickRetry, hasMenuChoice, a6);
}

extern "C" void BattleScene_ProcessResultState_Hook(void* ctx, void* battleContext, int sceneState, char forceSkipQuickRetry, int hasMenuChoice, int a6) {
    BattleScene_ProcessResultState_Trampoline(ctx, battleContext, sceneState, forceSkipQuickRetry, hasMenuChoice, a6, nullptr);
}

const AsmList hookVsResultMenuInit = {
    { (void*)0x481D80, {
        0xE8, INLINE_DWORD(AsmHacks::detail::rel32(0x481D80, &VsResultMenu_Init_Hook)),
        0x90  // nop (original function prologue will be preserved by hook)
    } }
};

const AsmList hookResultMenuSetupWinQuote = {
    // Placeholder: hook disabled while Once Again dialog is reworked
};

const AsmList hookBattleSceneProcessResultState = {
    // Placeholder: hook disabled while Once Again dialog is reworked
};

} // namespace AsmHacks
