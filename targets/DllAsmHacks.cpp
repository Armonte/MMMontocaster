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

typedef char (__thiscall* CTM_YesNo_Update_t)(void* menuManager);
static CTM_YesNo_Update_t CTM_YesNo_Update = (CTM_YesNo_Update_t)0x44CB80;

typedef int (__thiscall* CTM_YesNo_SetMessage_t)(void* menuManager, const char* text);
static CTM_YesNo_SetMessage_t CTM_YesNo_SetMessage = (CTM_YesNo_SetMessage_t)0x4D8CA0;

typedef uint8_t (__thiscall* CTM_YesNo_Render_t)(void* menuManager);
static CTM_YesNo_Render_t CTM_YesNo_Render = (CTM_YesNo_Render_t)0x4D8E40;

static constexpr size_t kCTMYesNoSize = 0xC8;
static constexpr uintptr_t kCTMYesNoVTable = 0x005378E0;
static const char* kOnceAgainPromptString = reinterpret_cast<const char*>(0x00538AC8);

static void ResetOnceAgainDialogState() {
    gOnceAgainYesNoDialog = nullptr;
    gOnceAgainYesNoDialogActive = false;
    gOnceAgainYesNoDialogResult = -1;
    gBattleContextForDialog = nullptr;
}

static void DestroyOnceAgainDialog() {
    if (!gOnceAgainYesNoDialog) {
        ResetOnceAgainDialogState();
        return;
    }

    void* dialog = gOnceAgainYesNoDialog;
    gOnceAgainYesNoDialog = nullptr;

    try {
        auto** vtable = reinterpret_cast<void***>(dialog);
        if (vtable && *vtable && (*vtable)[0]) {
            using DestructorFn = void(__thiscall*)(void*, int);
            auto destroy = reinterpret_cast<DestructorFn>((*vtable)[0]);
            destroy(dialog, 1);
        }
    } catch (...) {
        LOG("DestroyOnceAgainDialog: Exception while invoking dialog destructor");
    }

    operator delete(dialog);
    ResetOnceAgainDialogState();
}

static bool CreateOnceAgainDialog(void* preMatchWords) {
    if (gOnceAgainYesNoDialogActive && gOnceAgainYesNoDialog) {
        gBattleContextForDialog = preMatchWords;
        return true;
    }

    void* dialogMem = operator new(kCTMYesNoSize);
    if (!dialogMem) {
        LOG("CreateOnceAgainDialog: operator new failed");
        return false;
    }

    std::memset(dialogMem, 0, kCTMYesNoSize);

    int dialogResult = 0;
    try {
        dialogResult = CTM_YesNo_Init(dialogMem);
    } catch (...) {
        LOG("CreateOnceAgainDialog: CTM_YesNo_Init threw");
        operator delete(dialogMem);
        return false;
    }

    if (!dialogResult || dialogResult != reinterpret_cast<int>(dialogMem)) {
        LOG("CreateOnceAgainDialog: CTM_YesNo_Init unexpected result=%p", (void*)dialogResult);
        operator delete(dialogMem);
        return false;
    }

    auto* dialogWords = reinterpret_cast<uint32_t*>(dialogMem);
    dialogWords[33] = 0; // state
    dialogWords[34] = 0; // timer
    dialogWords[40] = 0; // selection flag (0 = YES)

    if (CTM_YesNo_SetMessage && kOnceAgainPromptString) {
        try {
            CTM_YesNo_SetMessage(dialogMem, kOnceAgainPromptString);
        } catch (...) {
            LOG("CreateOnceAgainDialog: CTM_YesNo_SetMessage threw");
        }
    }

    gOnceAgainYesNoDialog = dialogMem;
    gOnceAgainYesNoDialogActive = true;
    gOnceAgainYesNoDialogResult = -1;
    gBattleContextForDialog = preMatchWords;

    LOG("CreateOnceAgainDialog: Dialog created at %p", dialogMem);
    return true;
}

static bool UpdateOnceAgainDialog() {
    if (!gOnceAgainYesNoDialogActive || !gOnceAgainYesNoDialog) {
        return false;
    }

    try {
        CTM_YesNo_Update(gOnceAgainYesNoDialog);
    } catch (...) {
        LOG("UpdateOnceAgainDialog: CTM_YesNo_Update threw");
        DestroyOnceAgainDialog();
        return false;
    }

    const auto* dialogWords = reinterpret_cast<const uint32_t*>(gOnceAgainYesNoDialog);
    const uint32_t state = dialogWords[33];
    const uint32_t selectionIsNo = dialogWords[40];

    if (state == 4 && gOnceAgainYesNoDialogResult == -1) {
        gOnceAgainYesNoDialogResult = (selectionIsNo == 0) ? 1 : 0;
        LOG("UpdateOnceAgainDialog: Completed with result=%d", gOnceAgainYesNoDialogResult);
        return true;
    }

    return false;
}

static void RenderOnceAgainDialog() {
    if (!gOnceAgainYesNoDialogActive || !gOnceAgainYesNoDialog) {
        return;
    }

    try {
        CTM_YesNo_Render(gOnceAgainYesNoDialog);
    } catch (...) {
        LOG("RenderOnceAgainDialog: CTM_YesNo_Render threw");
    }
}

static bool ShouldShowOnceAgainDialog(int* preMatchWords, char forceSkipQuickRetry, int hasMenuChoice) {
    if (!preMatchWords) {
        LOG("ShouldShow: preMatchWords is null");
        return false;
    }

    LOG("ShouldShow: forceSkip=%d hasMenu=%d state=%d framesSinceLoad=%d roundTimer=%d",
        (int)forceSkipQuickRetry, hasMenuChoice, preMatchWords[21], preMatchWords[17], preMatchWords[20]);

    // Only reject if explicitly forced to skip - hasMenuChoice=1 is normal for offline VS
    if (forceSkipQuickRetry != 0) {
        LOG("ShouldShow: REJECTED - forceSkip set");
        return false;
    }

    if (gStoryModeClearFlag && *gStoryModeClearFlag != 0) {
        LOG("ShouldShow: REJECTED - story mode");
        return false;
    }

    if (gVsResultMenuMode && *gVsResultMenuMode != 0) {
        LOG("ShouldShow: REJECTED - menu mode=%d", *gVsResultMenuMode);
        return false;
    }

    if (preMatchWords[21] != 0) {
        LOG("ShouldShow: REJECTED - state != 0");
        return false;
    }

    if (preMatchWords[17] < 30) {
        LOG("ShouldShow: REJECTED - framesSinceLoad < 30");
        return false;
    }

    if (preMatchWords[20] <= 300) {
        LOG("ShouldShow: REJECTED - roundTimer <= 300");
        return false;
    }

    LOG("ShouldShow: ACCEPTED - will create dialog");
    return true;
}

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

// MinHook function pointer for BattleScene_ProcessResultState
// Original function uses __userpurge: edx=battleContext, ecx=sceneContext, eax=sceneState, stack=rest
typedef void (__cdecl* BattleScene_ProcessResultState_t)(
    void* battleContext, void* sceneContext, int sceneState,
    char forceSkipQuickRetry, int hasMenuChoice, int a6
);
BattleScene_ProcessResultState_t BattleScene_ProcessResultState_Original = nullptr;

// Helper to get original function pointer from assembly
extern "C" void* GetBattleSceneOriginalPtr() {
    return (void*)BattleScene_ProcessResultState_Original;
}

// Wrapper to call original function with correct register-based calling convention
// MUST be naked because original uses "retn 0Ch" (stdcall - callee cleans stack)
extern "C" __attribute__((naked)) void CallOriginalBattleSceneProcessResultState(
    void* battleContext, void* sceneContext, int sceneState,
    char forceSkipQuickRetry, int hasMenuChoice, int a6)
{
    __asm__ (
        // Stack: [retaddr][bc][sc][state][forceSkip][hasMenu][a6]
        "push %esi\n\t"
        "push %edi\n\t"
        "push %ebx\n\t"
        // Load params from stack (offsets adjusted for 3 pushes = +12)
        "movl 16(%esp), %edx\n\t"      // battleContext
        "movl 20(%esp), %ecx\n\t"      // sceneContext
        "movl 24(%esp), %eax\n\t"      // sceneState
        "movl 36(%esp), %esi\n\t"      // a6
        "movl 32(%esp), %edi\n\t"      // hasMenuChoice
        "movzbl 28(%esp), %ebx\n\t"    // forceSkipQuickRetry
        // Push stack params for original
        "pushl %esi\n\t"               // a6
        "pushl %edi\n\t"               // hasMenuChoice
        "pushl %ebx\n\t"               // forceSkipQuickRetry
        // Get function pointer via helper (no args, returns in eax)
        "call _GetBattleSceneOriginalPtr\n\t"
        "test %eax, %eax\n\t"
        "jz 1f\n\t"
        // Save function pointer
        "movl %eax, %esi\n\t"
        // Restore edx, ecx, eax (note esp shifted by 3 more pushes = +24 total)
        "movl 28(%esp), %edx\n\t"      // battleContext
        "movl 32(%esp), %ecx\n\t"      // sceneContext
        "movl 36(%esp), %eax\n\t"      // sceneState
        // Call original via saved pointer
        "call *%esi\n\t"               // Original does "retn 0Ch"
        "1:\n\t"
        "pop %ebx\n\t"
        "pop %edi\n\t"
        "pop %esi\n\t"
        "ret"
    );
}

// Hook 1: VsResultMenu_Init - Force skipQuickRetryGate = 0 for offline versus
static void VsResultMenu_Init_Hook_Impl_Internal(void* prevMenu, void* managerPtr, void* contextPtr) {
    (void)prevMenu;

    int32_t initialSkipGate = -1;
    auto* skipQuickRetryGate =
        managerPtr ? reinterpret_cast<int32_t*>(reinterpret_cast<char*>(managerPtr) + 0xD0) : nullptr;
    if (skipQuickRetryGate) {
        initialSkipGate = *skipQuickRetryGate;
    }

    LOG("VsResultMenu_Init_Hook: prev=%p manager=%p context=%p mode=%d storyClear=%d skipGate(before)=%d",
        prevMenu,
        managerPtr,
        contextPtr,
        gVsResultMenuMode ? *gVsResultMenuMode : -1,
        gStoryModeClearFlag ? *gStoryModeClearFlag : -1,
        initialSkipGate);

    if (managerPtr &&
        gVsResultMenuMode && *gVsResultMenuMode == 0 &&
        gStoryModeClearFlag && *gStoryModeClearFlag == 0) {
        if (skipQuickRetryGate && *skipQuickRetryGate != 0) {
            LOG("VsResultMenu_Init_Hook: forcing skipQuickRetryGate -> 0 (was %d)", *skipQuickRetryGate);
            *skipQuickRetryGate = 0;
        }
    }
}

extern "C" void __attribute__((naked)) VsResultMenu_Init_CallHook() {
    __asm__ __volatile__(
        ".intel_syntax noprefix\n"
        "push ecx\n"
        "push eax\n"
        "push edx\n"
        "mov eax, [esp+16]\n"   // manager pointer
        "mov edx, [esp+20]\n"   // context pointer
        "mov ecx, [esp+8]\n"    // previous menu pointer (saved ecx)
        "push edx\n"
        "push eax\n"
        "push ecx\n"
        "call _VsResultMenu_Init_Hook_Impl\n"
        "add esp, 12\n"
        "pop edx\n"
        "pop eax\n"
        "pop ecx\n"
        "mov eax, 0x00481D80\n"
        "jmp eax\n"
        ".att_syntax prefix\n"
    );
}

// Stub hook placeholders to keep build stable while Once Again integration is refactored.
// These hooks simply forward to the original game code when invoked.
extern "C" int ResultMenu_SetupWinQuote_Hook() {
    __asm__ __volatile__(
        ".intel_syntax noprefix\n"
        "mov eax, 0x0043A700\n"  // Load original ResultMenu_SetupWinQuote
        "jmp eax\n"
        ".att_syntax prefix\n"
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

// Internal C++ implementation (called by naked wrapper below)
extern "C" void BattleScene_ProcessResultState_Hook_Impl(void* battleContext, void* sceneContext, int sceneState, char forceSkipQuickRetry, int hasMenuChoice, int a6) {
    // Parameters correctly extracted from registers by naked wrapper
    static int logCounter = 0;
    if (logCounter < 5) {
        LOG("BattleScene_ProcessResultState_Hook: battleContext=%p sceneContext=%p sceneState=%d forceSkip=%d hasMenu=%d a6=%d",
            battleContext, sceneContext, sceneState, static_cast<int>(forceSkipQuickRetry), hasMenuChoice, a6);
        ++logCounter;
    }

    try {
        if (!battleContext) {
            LOG("BattleScene_ProcessResultState_Hook: battleContext=null, calling original");
            CallOriginalBattleSceneProcessResultState(battleContext, sceneContext, sceneState,
                                                      forceSkipQuickRetry, hasMenuChoice, a6);
            return;
        }

        int* bc = (int*)battleContext;
        if (!bc) {
            LOG("BattleScene_ProcessResultState_Hook: battleContext cast failed, calling original");
            CallOriginalBattleSceneProcessResultState(battleContext, sceneContext, sceneState,
                                                      forceSkipQuickRetry, hasMenuChoice, a6);
            return;
        }

        int state = bc[21];
        LOG("BattleScene_ProcessResultState_Hook: state=%d sceneState=%d forceSkip=%d hasMenu=%d a6=%d dialogActive=%d dialogResult=%d",
            state,
            sceneState,
            forceSkipQuickRetry,
            hasMenuChoice,
            a6,
            gOnceAgainYesNoDialogActive ? 1 : 0,
            gOnceAgainYesNoDialogResult);

        LOG("BattleScene_ProcessResultState_Hook: After state log, before ShouldShow check");

        // Attempt to create dialog when conditions are met and we are in the initial state
        if (!gOnceAgainYesNoDialogActive && ShouldShowOnceAgainDialog(bc, forceSkipQuickRetry, hasMenuChoice)) {
            LOG("BattleScene_ProcessResultState_Hook: ShouldShowOnceAgainDialog -> true");
            if (CreateOnceAgainDialog(bc)) {
                bc[21] = 20;
                state = 20;
                LOG("BattleScene_ProcessResultState_Hook: dialog created; forcing state=20");
            } else {
                LOG("BattleScene_ProcessResultState_Hook: dialog creation failed");
            }
        }

        if (gOnceAgainYesNoDialogActive && gOnceAgainYesNoDialog) {
            gBattleContextForDialog = bc;

            if (state == 0) {
                bc[21] = 20;
                state = 20;
                LOG("BattleScene_ProcessResultState_Hook: adjusted state 0 -> 20 while dialog active");
            }

            if (gOnceAgainYesNoDialogResult == -1) {
                UpdateOnceAgainDialog();
                RenderOnceAgainDialog();
                if (gOnceAgainYesNoDialogResult == 1) {
                    LOG("BattleScene_ProcessResultState_Hook: dialog YES selected -> rematch");
                    bc[21] = 8;
                    DestroyOnceAgainDialog();
                    try {
                        if (netManPtr) {
                            netManPtr->exportInputs();
                            LOG("BattleScene_ProcessResultState_Hook: exportInputs succeeded (YES)");
                        }
                    } catch (...) {
                        LOG("BattleScene_ProcessResultState_Hook: exportInputs threw");
                    }
                    // Call original with register setup
                    CallOriginalBattleSceneProcessResultState(battleContext, sceneContext, sceneState,
                                                              forceSkipQuickRetry, hasMenuChoice, a6);
                    return;
                }

                if (gOnceAgainYesNoDialogResult == 0) { // NO selected
                    LOG("BattleScene_ProcessResultState_Hook: dialog NO selected -> returning to results");
                    VsResultMenu_Create_Original(0);
                    DestroyOnceAgainDialog();
                    bc[21] = 0;
                    // Call original with register setup
                    CallOriginalBattleSceneProcessResultState(battleContext, sceneContext, sceneState,
                                                              forceSkipQuickRetry, hasMenuChoice, a6);
                    return;
                }

                RenderOnceAgainDialog();
                LOG("BattleScene_ProcessResultState_Hook: dialog pending result (state=%d)", bc[21]);
                return; // Keep dialog active without advancing state machine
            }
        }
    } catch (...) {
        LOG("BattleScene_ProcessResultState_Hook: Exception caught, calling original");
    }

    // Call original function for all other cases via our trampoline wrapper
    // (Original function needs registers set up, not just stack params)
    CallOriginalBattleSceneProcessResultState(battleContext, sceneContext, sceneState,
                                              forceSkipQuickRetry, hasMenuChoice, a6);
}

// Naked wrapper to extract register parameters (edx, ecx, eax) and stack params
// Original function uses custom __userpurge convention:
//   edx = battleContext, ecx = sceneContext, eax = sceneState
//   Stack layout (BEFORE our function): [esp+4]=forceSkipQuickRetry, [esp+8]=a5, [esp+12]=a6, [esp+20]=hasMenuChoice, [esp+24]=transitionArg
//   NOTE: Gap at [esp+16]!
extern "C" __attribute__((naked)) void BattleScene_ProcessResultState_Hook() {
    __asm__ (
        "push %ebx\n\t"
        "push 16(%esp)\n\t"          // a6 - was [esp+12]
        "push 24(%esp)\n\t"          // hasMenuChoice - was [esp+20]
        "movzbl 8(%esp), %ebx\n\t"   // forceSkipQuickRetry - was [esp+4]
        "push %ebx\n\t"
        "push %eax\n\t"              // sceneState from register
        "push %ecx\n\t"              // sceneContext from register
        "push %edx\n\t"              // battleContext from register
        "call _BattleScene_ProcessResultState_Hook_Impl\n\t"
        "add $24, %esp\n\t"          // Clean our 6 params
        "pop %ebx\n\t"
        "ret $12"                    // Return and clean 12 bytes like original "retn 0Ch"
    );
}

// Hook 2: VsResultMenu_FinalizeSelection - Intercept ONCE_AGAIN selection
namespace {
    struct MenuString {
        int32_t base;
        union {
            char* pLongString;
            char shortString[0x10];
        };
        int32_t length;
        int32_t maxLength;
    };

    static const char* resolveMenuString(MenuString* menuString) {
        if (!menuString) {
            return nullptr;
        }
        if (menuString->maxLength < static_cast<int32_t>(sizeof(menuString->shortString))) {
            return menuString->shortString;
        }
        return menuString->pLongString;
    }
}

static void VsResultMenu_FinalizeSelection_Hook_Impl_Internal(void* manager) {
    bool isOnceAgain = false;
    MenuString* hoveredTag = manager ? reinterpret_cast<MenuString*>(reinterpret_cast<uintptr_t>(manager) + 0x20) : nullptr;

    const char* tagStr = resolveMenuString(hoveredTag);
    if (tagStr) {
        if (strcmp(tagStr, "ONCE_AGAIN") == 0 || strcmp(tagStr, "ONCE AGAIN") == 0 || strncmp(tagStr, "ONCE", 4) == 0) {
            isOnceAgain = true;
        }
    }

    LOG("VsResultMenu_FinalizeSelection_Hook: manager=%p tag='%s' length=%d maxLength=%d",
        manager,
        tagStr ? tagStr : "<null>",
        hoveredTag ? hoveredTag->length : -1,
        hoveredTag ? hoveredTag->maxLength : -1);

    if (isOnceAgain && netManPtr) {
        try {
            netManPtr->exportInputs();
            LOG("VsResultMenu_FinalizeSelection_Hook: exportInputs succeeded");
        } catch (...) {
            LOG("VsResultMenu_FinalizeSelection_Hook: exportInputs threw");
        }
    } else {
        LOG("VsResultMenu_FinalizeSelection_Hook: not ONCE AGAIN (isOnceAgain=%d)", isOnceAgain ? 1 : 0);
    }

    const int32_t skipGate = manager ? *(int32_t*)(reinterpret_cast<uintptr_t>(manager) + 0xD0) : -1;
    const uint32_t currentState = gVsResultMenuInputState ? *gVsResultMenuInputState : 0;

    LOG("VsResultMenu_FinalizeSelection_Hook: gVsResultMenuInputState=%u skipGate=%d",
        currentState,
        skipGate);
}

extern "C" void __attribute__((naked)) VsResultMenu_FinalizeSelection_CallHook() {
    __asm__ __volatile__(
        ".intel_syntax noprefix\n"
        "push esi\n"
        "push edi\n"
        "mov edi, dword ptr [0x00774C38]\n"
        "pushad\n"
        "push edi\n"
        "call %0\n"
        "add esp, 4\n"
        "popad\n"
        "push 0x00482E88\n"
        "ret\n"
        ".att_syntax prefix\n"
        :
        : "r"(AsmHacks::VsResultMenu_FinalizeSelection_Hook_Impl_Internal)
        : "eax"
    );
}

// Hook 3: BattleScene_ApplyResultSelection - currently passthrough
extern "C" void BattleScene_ApplyResultSelection_Hook(uint32_t inputState) {
    if (inputState == 0 && netManPtr) {
        try {
            netManPtr->exportInputs();
        } catch (...) {
            LOG("BattleScene_ApplyResultSelection_Hook: exportInputs threw");
        }
    }

    // Original call disabled previously due to recursion; continue to fall through safely.
    LOG("BattleScene_ApplyResultSelection_Hook: original call skipped to avoid recursion");
    return;
}

// Hook 4: BattleScene_PostMatchTransition - Inject YES/NO dialog BEFORE VsResultMenu_Create
extern "C" int __stdcall BattleScene_PostMatchTransition_VsResultMenuCreate_Hook(int skipQuickRetry) {
    try {
        if (!gVsResultMenuMode || !gStoryModeClearFlag) {
            return VsResultMenu_Create_Original(skipQuickRetry);
        }

        LOG("BattleScene_PostMatchTransition_VsResultMenuCreate_Hook: skipQuickRetry=%d, mode=%d, storyFlag=%d", skipQuickRetry, *gVsResultMenuMode, *gStoryModeClearFlag);

        if (*gVsResultMenuMode == 0 && *gStoryModeClearFlag == 0 && skipQuickRetry == 0) {
            LOG("BattleScene_PostMatchTransition_VsResultMenuCreate_Hook: Conditions met for Once Again dialog");
        }
    } catch (...) {
        LOG("BattleScene_PostMatchTransition_VsResultMenuCreate_Hook: Exception, calling original");
    }

    LOG("BattleScene_PostMatchTransition_VsResultMenuCreate_Hook: Calling original VsResultMenu_Create");
    return VsResultMenu_Create_Original(skipQuickRetry);
}

const AsmList hookVsResultMenuInit = {
    { (void*)0x482D4B, {
        0xE8, INLINE_DWORD(AsmHacks::detail::rel32(0x482D4B, &VsResultMenu_Init_CallHook))
    } }
};

const AsmList hookVsResultMenuFinalizeSelection = {
    { (void*)0x482E80, {
        0xE9, INLINE_DWORD(AsmHacks::detail::rel32(0x482E80, &VsResultMenu_FinalizeSelection_CallHook)),
        0x90, 0x90, 0x90
    } }
};

const AsmList hookBattleSceneApplyResultSelection = {
    { (void*)0x439420, {
        0xE8, INLINE_DWORD(AsmHacks::detail::rel32(0x439420, &BattleScene_ApplyResultSelection_Hook)),
        0x90
    } }
};

const AsmList hookBattleScenePostMatchTransition = {
    { (void*)0x4396C5, {
        0xE8, INLINE_DWORD(AsmHacks::detail::rel32(0x4396C5, &BattleScene_PostMatchTransition_VsResultMenuCreate_Hook)),
        0x90
    } }
};

// NOTE: hookBattleSceneProcessResultState is now installed via MinHook in DllMain.cpp
// No byte patches needed - MinHook handles the detour automatically

} // namespace AsmHacks

extern "C" __attribute__((used)) void VsResultMenu_Init_Hook_Impl(void* prevMenu, void* managerPtr, void* contextPtr) {
    AsmHacks::VsResultMenu_Init_Hook_Impl_Internal(prevMenu, managerPtr, contextPtr);
}
