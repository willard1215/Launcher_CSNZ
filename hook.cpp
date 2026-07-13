#include "hook.h"
#include "hookutils.h"
#include <stdio.h>
#include <locale.h>
#include <ICommandLine.h>
#include <IGameUI.h>
#include <VGUI/IPanel.h>
#include "DediCsv.h"
#include "ChattingManager.h"
#include <IFileSystem.h>

#define MAX_ZIP_SIZE	(1024 * 1024 * 16 )
#include "XZip.h"

#include <vector>
#include <string>
#include <unordered_map>
#include "sys.h"
#include <stdarg.h>
#include "../CSNZ_Server/src/thirdparty/SQLiteCpp/sqlite3/sqlite3.h"

HMODULE g_hEngineModule;
DWORD g_dwEngineBase;
DWORD g_dwEngineSize;

DWORD g_dwGameUIBase;
DWORD g_dwGameUISize;

DWORD g_dwMpBase;
DWORD g_dwMpSize;

DWORD g_dwFileSystemBase;
DWORD g_dwFileSystemSize;

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT "30002"
#define ENABLE_LAUNCHER_CHAT_AUTOLOGIN 0
#define ENABLE_GAMEUI_AUTH_UI 0

#define HW_LOGIN_DLG_CTOR_RVA 0x9505A0
#define HW_LOGIN_DLG_ONCOMMAND_RVA 0x951940
#define HW_AUTH_MANAGER_AUTH_RVA 0x818B20
#define HW_AUTH_STATE_GLOBAL_RVA 0x228C9E4
#define HW_SOCKET_MANAGER_EVENT_RVA 0
#define HW_SOCKET_MANAGER_CONNECT_RVA 0
#define HW_READPACKET_EVENT_CALL_RVA 0xA77290
#define HW_WSARECV_WRAPPER_RVA 0x3AD960
#define HW_ALLOC_STRING_RVA 0xA73780

#define SOCKETMANAGER_SIG_CSNZ23 "\x55\x8B\xEC\x6A\x00\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x51\x53\x56\x57\xA1\x00\x00\x00\x00\x33\xC5\x50\x8D\x45\x00\x64\xA3\x00\x00\x00\x00\x8B\xD9\x89\x5D\x00\x8A\x45"
#define SOCKETMANAGER_MASK_CSNZ23 "xxxx?x????xx????xxxxxx????xxxxx?xx????xxxx?xx"

#define SERVERCONNECT_SIG_CSNZ2019 "\xE8\x00\x00\x00\x00\x85\xC0\x75\x00\x46"
#define SERVERCONNECT_MASK_CSNZ2019 "x????xxx?x"

#define PACKET_METADATA_PARSE_SIG_CSNZ "\x55\x8B\xEC\x6A\x00\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\x00\x56\x57\x50\x8D\x45\x00\x64\xA3\x00\x00\x00\x00\x8B\xF1\x89\xB5\x00\x00\x00\x00\x8B\x45\x00\x89\x85"
#define PACKET_METADATA_PARSE_MASK_CSNZ "xxxx?x????xx????xxx????x????xxxx?xxxxx?xx????xxxx????xx?xx"

#define PACKET_QUEST_PARSE_SIG_CSNZ "\x55\x8B\xEC\x6A\x00\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x00\x53\x56\x57\xA1\x00\x00\x00\x00\x33\xC5\x50\x8D\x45\x00\x64\xA3\x00\x00\x00\x00\x8B\xF9\x8B\x45\x00\x89\x45\x00\x8B\x45\x00\xC7\x45\x00\x00\x00\x00\x00\xC7\x45\x00\x00\x00\x00\x00\x89\x45\x00\x6A\x00\x8D\x45\x00\xC7\x45\x00\x00\x00\x00\x00\x50\x8D\x4D\x00\xE8\x00\x00\x00\x00\x0F\xB6\x45\x00\x89\x47\x00\xE8\x00\x00\x00\x00\x8B\x47\x00\x48"
#define PACKET_QUEST_PARSE_MASK_CSNZ "xxxx?x????xx????xxx?xxxx????xxxxx?xx????xxxx?xx?xx?xx?????xx?????xx?x?xx?xx?????xxx?x????xxx?xx?x????xx?x"

#define PACKET_UMSG_PARSE_SIG_CSNZ "\x55\x8B\xEC\x6A\x00\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\xB8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\x00\x56\x57\x50\x8D\x45\x00\x64\xA3\x00\x00\x00\x00\x8B\xF9\x89\xBD"
#define PACKET_UMSG_PARSE_MASK_CSNZ "xxxx?x????xx????xx????x????x????xxxx?xxxxx?xx????xxxx"

#define PACKET_ALARM_PARSE_SIG_CSNZ "\x55\x8B\xEC\x6A\x00\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\x00\x56\x57\x50\x8D\x45\x00\x64\xA3\x00\x00\x00\x00\x8B\xF9\x89\xBD\x00\x00\x00\x00\x8B\x45\x00\xC7\x85\x00\x00\x00\x00\x00\x00\x00\x00\x89\x85\x00\x00\x00\x00\x8B\x45\x00\xC7\x85\x00\x00\x00\x00\x00\x00\x00\x00\xC7\x85\x00\x00\x00\x00\x00\x00\x00\x00\x89\x85\x00\x00\x00\x00\x6A\x00\x8D\x85\x00\x00\x00\x00\xC7\x45\x00\x00\x00\x00\x00\x50\x8D\x8D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x0F\xB6\x85\x00\x00\x00\x00\x83\xF8"
#define PACKET_ALARM_PARSE_MASK_CSNZ "xxxx?x????xx????xxx????x????xxxx?xxxxx?xx????xxxx????xx?xx????????xx????xx?xx????????xx????????xx????x?xx????xx?????xxx????x????xxx????xx"

#define PACKET_ITEM_PARSE_SIG_CSNZ "\x55\x8B\xEC\x6A\x00\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\x00\x56\x57\x50\x8D\x45\x00\x64\xA3\x00\x00\x00\x00\x8B\xF1\x8B\x45\x00\xC7\x85"
#define PACKET_ITEM_PARSE_MASK_CSNZ "xxxx?x????xx????xxx????x????xxxx?xxxxx?xx????xxxx?xx"

#define PACKET_CRYPT_PARSE_SIG_CSNZ "\x55\x8B\xEC\x6A\x00\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\x00\x53\x56\x57\x50\x8D\x45\x00\x64\xA3\x00\x00\x00\x00\x8B\x45\x00\x89\x85\x00\x00\x00\x00\x8B\x45\x00\xC7\x85\x00\x00\x00\x00\x00\x00\x00\x00\xC7\x85\x00\x00\x00\x00\x00\x00\x00\x00\x89\x85\x00\x00\x00\x00\x6A\x00\x8D\x85\x00\x00\x00\x00\xC7\x45\x00\x00\x00\x00\x00\x50\x8D\x8D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x0F\xB6\x9D"
#define PACKET_CRYPT_PARSE_MASK_CSNZ "xxxx?x????xx????xxx????x????xxxx?xxxxxx?xx????xx?xx????xx?xx????????xx????????xx????x?xx????xx?????xxx????x????xxx"

#define PACKET_HACK_PARSE_SIG_CSNZ "\x55\x8B\xEC\x6A\x00\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x00\x53\x56\x57\xA1\x00\x00\x00\x00\x33\xC5\x50\x8D\x45\x00\x64\xA3\x00\x00\x00\x00\x8B\xD9\x89\x5D\x00\x8B\x45\x00\x89\x45\x00\x8B\x45\x00\xC7\x45\x00\x00\x00\x00\x00\xC7\x45\x00\x00\x00\x00\x00\x89\x45\x00\x6A\x00\x8D\x45\x00\xC7\x45\x00\x00\x00\x00\x00\x50\x8D\x4D\x00\xE8\x00\x00\x00\x00\x0F\xB6\x45\x00\x89\x43\x00\x83\xE8"
#define PACKET_HACK_PARSE_MASK_CSNZ "xxxx?x????xx????xxx?xxxx????xxxxx?xx????xxxx?xx?xx?xx?xx?????xx?????xx?x?xx?xx?????xxx?x????xxx?xx?xx"

#define PACKET_HACK_SEND_SIG_CSNZ "\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xEB\x00\x43\x56\x20\x20\x0D"
#define PACKET_HACK_SEND_MASK_CSNZ "x????x????x?xxxxx"

#define BOT_MANAGER_PTR_SIG_CSNZ "\xA3\x00\x00\x00\x00\xC7\x45\x00\x00\x00\x00\x00\xFF\x15\x00\x00\x00\x00\x83\xC4"
#define BOT_MANAGER_PTR_MASK_CSNZ "x????xx?????xx????xx"

#define CSOMAINPANEL_PTR_SIG_CSNZ "\x8B\x0D\x00\x00\x00\x00\x6A\x01\x8B\x01\xFF\x90\x00\x00\x00\x00\x8B\x0D\x00\x00\x00\x00\x6A\x01\xE8\x00\x00\x00\x00\x8B\x03"
#define CSOMAINPANEL_PTR_MASK_CSNZ "xx????xxxxxx????xx????xxx????xx"

#define CALL_PANEL_FINDCHILDBYNAME_SIG_CSNZ "\xE8\x00\x00\x00\x00\x85\xC0\x74\x00\x83\x7D"
#define CALL_PANEL_FINDCHILDBYNAME_MASK_CSNZ "x????xxx?xx"

#define NGCLIENT_INIT_SIG_CSNZ "\xE8\x00\x00\x00\x00\x84\xC0\x75\x00\xE8\x00\x00\x00\x00\x33\xC0"
#define NGCLIENT_INIT_MASK_CSNZ "x????xxx?x????xx"

#define NGCLIENT_QUIT_SIG_CSNZ "\xE8\x00\x00\x00\x00\x33\xC0\xE9\x00\x00\x00\x00\xEB"
#define NGCLIENT_QUIT_MASK_CSNZ "x????xxx????x"

#define HOLEPUNCH_SETSERVERINFO_SIG_CSNZ "\x55\x8B\xEC\xB8\x00\x00\x00\x00\x66\xA3"
#define HOLEPUNCH_SETSERVERINFO_MASK_CSNZ "xxxx????xx"

#define HOLEPUNCH_GETUSERSOCKETINFO_SIG_CSNZ "\x55\x8B\xEC\x83\xEC\x00\x57\x8B\x7D\x00\x85\xFF\x75\x00\x8B\x45"
#define HOLEPUNCH_GETUSERSOCKETINFO_MASK_CSNZ "xxxxx?xxx?xxx?xx"

#define CREATESTRINGTABLE_SIG_CSNZ "\x55\x8B\xEC\x53\x56\x8B\xF1\xC7\x46"
#define CREATESTRINGTABLE_MASK_CSNZ "xxxxxxxxx"

#define LOADJSON_SIG_CSNZ "\x55\x8B\xEC\x8B\x0D\x00\x00\x00\x00\x53\x56\x8B\x75\x00\x8B\x01\x57\x8B\x50\x00\x8B\x45\x00\x83\x78\x00\x00\x76\x00\x8B\x00\x6A\x00\x68\x00\x00\x00\x00\x50\xFF\xD2\x8B\x0D\x00\x00\x00\x00\x8B\xD8\x53\x8B\x11\xFF\x52\x00\x8B\xF8\x85\xFF\x74"
#define LOADJSON_MASK_CSNZ "xxxxx????xxxx?xxxxx?xx?xx??x?xxx?x????xxxxx????xxxxxxx?xxxxx"
#define LOADJSON_SIG_CSNZ_LATEST "\x55\x8B\xEC\x8B\x0D\x00\x00\x00\x00\x53\x56\x8B\x75\x00\x8B\x01\x57\x8B\x50\x30\x8B\x45\x00\x83\x78\x14\x0F\x76\x00\x8B\x00\x6A\x00\x68\x00\x00\x00\x00\x50\xFF\xD2\x8B\x0D\x00\x00\x00\x00\x8B\xD8\x53\x8B\x11\xFF\x52\x44\x8B\xF8\x85\xFF\x0F\x84"
#define LOADJSON_MASK_CSNZ_LATEST "xxxxx????xxxx?xxxxxxxx?xxxxx?xxxxx????xxxxx????xxxxxxxxxxxxxx"

#define LOGTOERRORLOG_SIG_CSNZ "\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\x00\x56\x57\x8B\x7D\x00\x8D\x45\x00\x50\x6A"
#define LOGTOERRORLOG_MASK_CSNZ "xxxxx????x????xxxx?xxxx?xx?xx"

#define READPACKET_SIG_CSNZ "\xE8\x00\x00\x00\x00\x8B\xF0\x83\xFE\x00\x77"
#define READPACKET_MASK_CSNZ "x????xxxx?x"

#define GETSSLPROTOCOLNAME_SIG_CSNZ "\xE8\x00\x00\x00\x00\xB9\x00\x00\x00\x00\x8A\x10"
#define GETSSLPROTOCOLNAME_MASK_CSNZ "x????x????xx"

#define SOCKETCONSTRUCTOR_SIG_CSNZ "\xE8\x00\x00\x00\x00\xEB\x00\x33\xC0\x53\xC7\x45"
#define SOCKETCONSTRUCTOR_MASK_CSNZ "x????x?xxxxx"
#define SOCKETCONSTRUCTOR_SIG_CSNZ_LATEST "\xE8\x00\x00\x00\x00\xEB\x00\x33\xC0\xFF\x75\x0C\xC7\x45"
#define SOCKETCONSTRUCTOR_MASK_CSNZ_LATEST "x????x?xxxxxxx"

#define EVP_CIPHER_CTX_NEW_SIG_CSNZ "\xE8\x00\x00\x00\x00\x8B\xF8\x89\xBE"
#define EVP_CIPHER_CTX_NEW_MASK_CSNZ "x????xxxx"

char g_pServerIP[16];
char g_pServerPort[6];
char g_pLogin[64];
char g_pPassword[64];

bool g_bUseOriginalServer = false;
bool g_bDumpMetadata = false;
bool g_bIgnoreMetadata = false;
bool g_bDumpQuest = false;
bool g_bDumpUMsg = false;
bool g_bDumpAlarm = false;
bool g_bDumpItem = false;
bool g_bDumpCrypt = false;
bool g_bDumpAll = false;
bool g_bDisableAuthUI = false;
bool g_bUseSSL = true;
bool g_bWriteMetadata = false;
bool g_bLoadDediCsvFromFile = false;
bool g_bRegister = false;
bool g_bNoNGHook = false;
bool g_bLoginCommandSent = false;
bool g_bDisableLocalAuthCheck = false;
volatile LONG g_lAutoLoginAttempted = 0;
void* g_pLatestHWSocketManager = nullptr;
volatile LONG g_lManualReadPumpStarted = 0;

cl_enginefunc_t* g_pEngine;

class CCSBotManager
{
public:
	virtual void Unknown() = NULL;
	virtual void Bot_Add(int side) = NULL;
};

CCSBotManager* g_pBotManager = NULL;

vgui::IPanel* g_pPanel = nullptr;
IGameUI* g_pGameUI = nullptr;
ChattingManager* g_pChattingManager;

WNDPROC oWndProc;
HWND hWnd;

void(__thiscall* g_pfnGameUI_RunFrame)(void* _this);

typedef void* (__thiscall* tPanel_FindChildByName)(void* _this, const char* name, bool recurseDown);
tPanel_FindChildByName g_pfnPanel_FindChildByName;

typedef int(__thiscall* tLoginDlg_OnCommand)(void* _this, const char* command);
tLoginDlg_OnCommand g_pfnLoginDlg_OnCommand;
bool g_bLatestLoginCommandHooked = false;
void* g_pHWLoginDlg = nullptr;
extern void(__thiscall* g_pfnHWLoginDlg_OnCommand)(void* ptr, const char* command);

typedef void(__thiscall* tParseCSV)(int* _this, unsigned char* buffer, int size);
tParseCSV g_pfnParseCSV;

typedef void*(*tEVP_CIPHER_CTX_new)();
tEVP_CIPHER_CTX_new g_pfnEVP_CIPHER_CTX_new;

void LauncherTrace(const char* fmt, ...)
{
	FILE* file = fopen("LauncherTrace.log", "a");
	if (!file)
		return;

	SYSTEMTIME st;
	GetLocalTime(&st);
	fprintf(file, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	va_list va;
	va_start(va, fmt);
	vfprintf(file, fmt, va);
	va_end(va);

	fprintf(file, "\n");
	fclose(file);
}

static void FormatHexPreview(char* out, int outSize, const unsigned char* data, int size, int limit = 48)
{
	static const char* kHex = "0123456789ABCDEF";
	if (!out || outSize <= 0)
		return;
	out[0] = '\0';
	if (!data || size <= 0)
		return;

	int count = size < limit ? size : limit;
	int pos = 0;
	for (int i = 0; i < count; ++i)
	{
		if (pos + 4 >= outSize)
			break;
		if (i)
			out[pos++] = ' ';
		unsigned char b = data[i];
		out[pos++] = kHex[b >> 4];
		out[pos++] = kHex[b & 0x0F];
	}
	if (size > limit && pos + 5 < outSize)
	{
		out[pos++] = ' ';
		out[pos++] = '.';
		out[pos++] = '.';
		out[pos++] = '.';
	}
	out[pos] = '\0';
}

static void DumpHWSocketReadState(void* socket, const char* label)
{
	if (!socket)
		return;

	__try
	{
		unsigned char* bufStart = *(unsigned char**)((DWORD)socket + 0x24);
		unsigned char* bufEnd = *(unsigned char**)((DWORD)socket + 0x2C);
		unsigned char* readPtr = *(unsigned char**)((DWORD)socket + 0x34);
		int buffered = (bufEnd && readPtr && bufEnd >= readPtr) ? (int)(bufEnd - readPtr) : -1;
		int totalBuffered = (bufEnd && bufStart && bufEnd >= bufStart) ? (int)(bufEnd - bufStart) : -1;
		int previewSize = buffered > 0 ? buffered : 0;
		char preview[192];
		FormatHexPreview(preview, sizeof(preview), readPtr, previewSize);
		char fullPreview[256];
		FormatHexPreview(fullPreview, sizeof(fullPreview), bufStart, totalBuffered > 0 ? totalBuffered : 0);
		unsigned char* aroundPtr = readPtr && readPtr >= bufStart + 16 ? readPtr - 16 : bufStart;
		int aroundSize = (bufEnd && aroundPtr && bufEnd >= aroundPtr) ? (int)(bufEnd - aroundPtr) : 0;
		char aroundPreview[256];
		FormatHexPreview(aroundPreview, sizeof(aroundPreview), aroundPtr, aroundSize);

		LauncherTrace("HWSocketReadState[%s] socket=%p flags1c=%u flags1d=%u flag44=%u flag45=%u lastSeq=%d buf=%p read=%p end=%p buffered=%d total=%d preview=%s full=%s around=%s",
			label, socket,
			*(unsigned char*)((DWORD)socket + 0x1C),
			*(unsigned char*)((DWORD)socket + 0x1D),
			*(unsigned char*)((DWORD)socket + 0x44),
			*(unsigned char*)((DWORD)socket + 0x45),
			(int)*(signed char*)((DWORD)socket + 0x39),
			bufStart, readPtr, bufEnd, buffered, totalBuffered, preview, fullPreview, aroundPreview);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("HWSocketReadState[%s] exception socket=%p", label, socket);
	}
}

void DumpVGuiPanelTree(vgui::IPanel* panel, const char* label, int depth = 0, int maxDepth = 4)
{
	if (!g_pPanel || !panel || depth > maxDepth)
		return;

	__try
	{
		int x = 0, y = 0, w = 0, h = 0;
		g_pPanel->GetPos(panel, x, y);
		g_pPanel->GetSize(panel, w, h);

		const char* name = g_pPanel->GetName(panel);
		const char* className = g_pPanel->GetClassName(panel);
		int childCount = g_pPanel->GetChildCount(panel);
		LauncherTrace("PanelTree[%s] depth=%d panel=%p name='%s' class='%s' pos=%d,%d size=%dx%d visible=%d enabled=%d children=%d",
			label, depth, panel, name ? name : "", className ? className : "", x, y, w, h,
			g_pPanel->IsVisible(panel) ? 1 : 0, g_pPanel->IsEnabled(panel) ? 1 : 0, childCount);

		for (int i = 0; i < childCount && i < 80; i++)
		{
			vgui::IPanel* child = g_pPanel->GetChild(panel, i);
			DumpVGuiPanelTree(child, label, depth + 1, maxDepth);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("PanelTree[%s] exception depth=%d panel=%p", label, depth, panel);
	}
}

void DumpClientPanelTree(void* clientPanel, const char* label)
{
	if (!clientPanel)
		return;

	__try
	{
		vgui::IPanel* vguiPanel = (vgui::IPanel*)(**(void* (__thiscall***)(void*))clientPanel)(clientPanel);
		LauncherTrace("PanelTree[%s] client=%p vgui=%p", label, clientPanel, vguiPanel);
		DumpVGuiPanelTree(vguiPanel, label);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("PanelTree[%s] client exception client=%p", label, clientPanel);
	}
}

DWORD FindPatternCompat(PCHAR pattern, PCHAR mask, PCHAR fallbackPattern, PCHAR fallbackMask, DWORD start, DWORD end)
{
	DWORD find = FindPattern(pattern, mask, start, end, NULL);
	if (!find && fallbackPattern && fallbackMask)
		find = FindPattern(fallbackPattern, fallbackMask, start, end, NULL);

	return find;
}

#pragma region Nexon NGClient/NXGSM
char NGClient_Return1()
{
	return 1;
}

void NGClient_Void()
{
}

// logger shit
bool NXGSM_Dummy()
{
	return false;
}

void NXGSM_WriteStageLogA(int a1, char* a2)
{
}

void NXGSM_WriteErrorLogA(int a1, char* a2)
{
}
#pragma endregion

void Pbuf_AddText(const char* text)
{
	g_pEngine->pfnClientCmd((char*)text);
}

void SendLoginCommand()
{
	if (g_bLoginCommandSent || !g_pChattingManager)
		return;

	const char* login = strlen(g_pLogin) ? g_pLogin : "localuser";
	const char* password = strlen(g_pPassword) ? g_pPassword : "localpass1";

	wchar_t buf[256];
	swprintf(buf, g_bRegister ? L"/register %S %S" : L"/login %S %S", login, password);
	g_pChattingManager->PrintToChat(1, buf);
	g_bLoginCommandSent = true;
	LauncherTrace("SendLoginCommand: sent %S", buf);
}

static bool FileExistsA(const std::string& path)
{
	DWORD attr = GetFileAttributesA(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static std::vector<std::string> GetLocalUserDatabaseCandidates()
{
	std::vector<std::string> candidates;

	char modulePath[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, modulePath, sizeof(modulePath));

	std::string dir = modulePath;
	size_t slash = dir.find_last_of("\\/");
	if (slash != std::string::npos)
		dir.resize(slash);

	candidates.push_back(dir + "\\UserDatabase.db3");

	const std::string assetBinSuffix = "\\asset\\Bin";
	if (dir.size() >= assetBinSuffix.size() &&
		_stricmp(dir.c_str() + dir.size() - assetBinSuffix.size(), assetBinSuffix.c_str()) == 0)
	{
		std::string root = dir.substr(0, dir.size() - assetBinSuffix.size());
		candidates.push_back(root + "\\CSNZ_Server\\bin\\UserDatabase.db3");
	}

	candidates.push_back("..\\..\\CSNZ_Server\\bin\\UserDatabase.db3");
	candidates.push_back("..\\CSNZ_Server\\bin\\UserDatabase.db3");
	candidates.push_back("CSNZ_Server\\bin\\UserDatabase.db3");

	return candidates;
}

static bool ValidateLocalUserAccount(const char* login, const char* password)
{
	if (!login || !password || !login[0] || !password[0])
	{
		LauncherTrace("ValidateLocalUserAccount: empty login/password");
		return false;
	}

	for (const std::string& dbPath : GetLocalUserDatabaseCandidates())
	{
		if (!FileExistsA(dbPath))
			continue;

		sqlite3* db = nullptr;
		int rc = sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
		if (rc != SQLITE_OK)
		{
			LauncherTrace("ValidateLocalUserAccount: sqlite open failed path='%s' rc=%d msg='%s'",
				dbPath.c_str(), rc, db ? sqlite3_errmsg(db) : "");
			if (db)
				sqlite3_close(db);
			continue;
		}

		sqlite3_stmt* stmt = nullptr;
		const char* sql = "SELECT userID FROM User WHERE userName = ? AND password = ? LIMIT 1";
		rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
		if (rc != SQLITE_OK)
		{
			LauncherTrace("ValidateLocalUserAccount: sqlite prepare failed path='%s' rc=%d msg='%s'",
				dbPath.c_str(), rc, sqlite3_errmsg(db));
			sqlite3_close(db);
			continue;
		}

		sqlite3_bind_text(stmt, 1, login, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, password, -1, SQLITE_TRANSIENT);
		rc = sqlite3_step(stmt);
		bool ok = rc == SQLITE_ROW;
		sqlite3_finalize(stmt);
		sqlite3_close(db);

		LauncherTrace("ValidateLocalUserAccount: path='%s' login='%s' result=%d", dbPath.c_str(), login, ok ? 1 : 0);
		return ok;
	}

	LauncherTrace("ValidateLocalUserAccount: UserDatabase.db3 not found");
	return false;
}

CreateHookClass(void*, SocketManagerConstructor, bool useSSL)
{
	return g_pfnSocketManagerConstructor(ptr, g_bUseSSL);
}

CreateHookClass(int, ServerConnect, unsigned long ip, unsigned short port, bool validate)
{
	in_addr originalAddr;
	originalAddr.s_addr = ip;
	LauncherTrace("ServerConnect original=%s:%d override=%s:%s validate=%d",
		inet_ntoa(originalAddr), ntohs(port), g_pServerIP, g_pServerPort, validate ? 1 : 0);
	return g_pfnServerConnect(ptr, inet_addr(g_pServerIP), htons(atoi(g_pServerPort)), validate);
}

static void DumpHWSocketManagerHandlers(void* manager)
{
	if (!manager || !g_dwEngineBase)
		return;

	int count = 0;
	for (int id = 0; id < 256; id++)
	{
		DWORD handler = 0;
		DWORD vtbl = 0;
		DWORD parseFn = 0;

		__try
		{
			handler = *(DWORD*)((DWORD)manager + 0x10 + id * 4);
			if (handler)
			{
				vtbl = *(DWORD*)handler;
				parseFn = *(DWORD*)(vtbl + 8);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			handler = 0;
			vtbl = 0;
			parseFn = 0;
		}

		if (handler)
		{
			count++;
			LauncherTrace("HandlerTable id=%d handler=%08X vtbl=%08X parse=%08X rva=%08X",
				id, handler, vtbl, parseFn, parseFn ? parseFn - g_dwEngineBase : 0);
		}
	}

	LauncherTrace("HandlerTable total=%d manager=%p", count, manager);
}

CreateHookClass(int, HWSocketManager_Event, unsigned int eventCode)
{
	DWORD socketPtr = 0;
	DWORD handlerTable = 0;
	BYTE socketPacketSeq = 0;
	WORD networkEvent = LOWORD(eventCode);
	WORD networkError = HIWORD(eventCode);

	__try
	{
		socketPtr = *(DWORD*)((DWORD)ptr + 0x4);
		if (socketPtr)
		{
			handlerTable = *(DWORD*)((DWORD)ptr + 0x10);
			socketPacketSeq = *(BYTE*)(socketPtr + 0x39);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("HWSocketManager::Event state read exception this=%p event=%08X", ptr, eventCode);
	}

	if (networkEvent == FD_READ || networkEvent == FD_WRITE || networkEvent == FD_CONNECT || networkEvent == FD_CLOSE)
		LauncherTrace("HWSocketManager::Event enter this=%p event=%08X eventLow=%u error=%u socket=%08X handlers=%08X seq=%u",
			ptr, eventCode, networkEvent, networkError, socketPtr, handlerTable, socketPacketSeq);

	int result = g_pfnHWSocketManager_Event(ptr, eventCode);

	if (networkEvent == FD_READ || networkEvent == FD_WRITE || networkEvent == FD_CONNECT || networkEvent == FD_CLOSE)
	{
		DWORD socketAfter = 0;
		__try
		{
			socketAfter = *(DWORD*)((DWORD)ptr + 0x4);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		LauncherTrace("HWSocketManager::Event leave this=%p event=%08X eventLow=%u error=%u result=%d socketAfter=%08X",
			ptr, eventCode, networkEvent, networkError, result, socketAfter);
	}

	return result;
}

CreateHookClass(int, HWSocketManager_Connect, unsigned long ip, unsigned short port, bool initialRead)
{
	in_addr addr;
	addr.s_addr = ip;
	LauncherTrace("HWSocketManager::Connect enter this=%p target=%s:%u initialRead=%d",
		ptr, inet_ntoa(addr), ntohs(port), initialRead ? 1 : 0);

	int result = g_pfnHWSocketManager_Connect(ptr, ip, port, initialRead);

	DWORD socketPtr = 0;
	__try
	{
		socketPtr = *(DWORD*)((DWORD)ptr + 0x4);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}

	LauncherTrace("HWSocketManager::Connect leave this=%p result=%d socket=%08X", ptr, result, socketPtr);
	if (!g_bUseOriginalServer && result && socketPtr)
	{
		g_pLatestHWSocketManager = ptr;
		DumpHWSocketManagerHandlers(ptr);
		if (InterlockedCompareExchange(&g_lManualReadPumpStarted, 1, 0) == 0)
		{
			CreateThread(NULL, 0, [](LPVOID) -> DWORD
			{
				Sleep(2500);
				for (int i = 0; i < 120; i++)
				{
					void* manager = g_pLatestHWSocketManager;
					if (!manager || !g_pfnHWSocketManager_Event)
						break;

					__try
					{
						LauncherTrace("Manual FD_READ pump tick=%d manager=%p", i, manager);
						g_pfnHWSocketManager_Event(manager, FD_READ);
					}
					__except (EXCEPTION_EXECUTE_HANDLER)
					{
						LauncherTrace("Manual FD_READ pump exception tick=%d manager=%p", i, manager);
						break;
					}

					Sleep(250);
				}
				InterlockedExchange(&g_lManualReadPumpStarted, 0);
				return 0;
			}, NULL, 0, NULL);
		}
	}
	return result;
}

CreateHookClassType(int, HWSocket_WSARecv, void, DWORD* socketHandlePtr, DWORD* buffers, DWORD bufferCount, DWORD flagsIn, bool skipRecv, void* overlapped)
{
	SOCKET socketHandle = INVALID_SOCKET;
	DWORD firstLen = 0;
	char* firstBuf = nullptr;

	__try
	{
		if (socketHandlePtr)
			socketHandle = (SOCKET)*socketHandlePtr;
		if (buffers && bufferCount > 0)
		{
			firstLen = buffers[0];
			firstBuf = (char*)buffers[1];
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("HWSocket::WSARecv wrapper state read exception this=%p socketPtr=%p", ptr, socketHandlePtr);
	}

	LauncherTrace("HWSocket::WSARecv wrapper enter this=%p socket=%u buffers=%u firstLen=%u flags=%u skip=%d ov=%p",
		ptr, (unsigned int)socketHandle, bufferCount, firstLen, flagsIn, skipRecv ? 1 : 0, overlapped);

	int result = g_pfnHWSocket_WSARecv(ptr, socketHandlePtr, buffers, bufferCount, flagsIn, skipRecv, overlapped);

	DWORD bytesHint = 0;
	unsigned char b0 = 0xFF;
	unsigned char b1 = 0xFF;
	unsigned char b2 = 0xFF;
	unsigned char b3 = 0xFF;
	__try
	{
		if (buffers && bufferCount > 0)
			bytesHint = buffers[0];
		if (firstBuf && firstLen >= 4)
		{
			b0 = (unsigned char)firstBuf[0];
			b1 = (unsigned char)firstBuf[1];
			b2 = (unsigned char)firstBuf[2];
			b3 = (unsigned char)firstBuf[3];
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("HWSocket::WSARecv wrapper result read exception this=%p", ptr);
	}

	LauncherTrace("HWSocket::WSARecv wrapper leave this=%p result=%d firstLenNow=%u firstBytes=%02X %02X %02X %02X",
		ptr, result, bytesHint, b0, b1, b2, b3);
	return result;
}

CreateHook(__cdecl, void, HolePunch_SetServerInfo, unsigned long ip, unsigned short port)
{
	g_pfnHolePunch_SetServerInfo(inet_addr(g_pServerIP), htons(atoi(g_pServerPort)));
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == 0x113 && wParam == 250)
	{
		// handle dropclient msg if the client detected abnormal things
		printf("handle_dropclient\n");
		return 0;
	}
	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

enum dediCsvType {
	TDM_Spawn_Replacement,
	AllStar_Skill,
	AllStar_Status,
	LastStand,
	ProtectionSupplyWeapon,
	RandomRule_Classic,
	RandomRule,
	ZSRogueLiteAbility,
	ZSTransform_Skill,
	ZSTransform_Status,
	FireBombOption,
	ZombieSkillProperty_Crazy,
	ZombieSkillProperty_JumpBuff,
	ZombieSkillProperty_ArmorUp,
	ZombieSkillProperty_Heal,
	ZombieSkillProperty_ShieldBuf,
	ZombieSkillProperty_Cloacking,
	ZombieSkillProperty_Trap,
	ZombieSkillProperty_Smoke,
	ZombieSkillProperty_VoodooHeal,
	ZombieSkillProperty_Shock,
	ZombieSkillProperty_Rush,
	ZombieSkillProperty_Pile,
	ZombieSkillProperty_Bat,
	ZombieSkillProperty_Stiffen,
	ZombieSkillProperty_SelfDestruct,
	ZombieSkillProperty_Penetration,
	ZombieSkillProperty_Revival,
	ZombieSkillProperty_Telleport,
	ZombieSkillProperty_Boost,
	ZombieSkillProperty_BombCreate,
	ZombieSkillProperty_Flying,
	ZombieSkillProperty_Fireball,
	ZombieSkillProperty_DogShoot,
	ZombieSkillProperty_ViolentRush,
	ZombieSkillProperty_WebShooter,
	ZombieSkillProperty_WebBomb,
	ZombieSkillProperty_Protect,
	ZombieSkillProperty_ChargeSlash,
	ZombieSkillProperty_Claw,
	HumanAbilityData,
	HumanAbilityProbData,
	SpecialZombieProb,
	VirusFactorReq,
	ZombiVirusBonus
};

std::unordered_map<std::string, dediCsvType> dediCsv = {
	{ "maps/TDM_Spawn_Replacement_Dedi.csv", TDM_Spawn_Replacement },
	{ "resource/allstar/AllStar_Skill-Dedi.csv", AllStar_Skill },
	{ "resource/allstar/AllStar_Status-Dedi.csv", AllStar_Status },
	{ "resource/ModeEvent/LastStand_Dedi.csv", LastStand },
	{ "resource/ModeEvent/ProtectionSupplyWeapon_Dedi.csv", ProtectionSupplyWeapon },
	{ "resource/ModeEvent/RandomRule_Classic_Dedi.csv", RandomRule_Classic },
	{ "resource/ModeEvent/RandomRule_Dedi.csv", RandomRule },
	{ "resource/ModeEvent/ZSRogueLiteAbility_Dedi.csv", ZSRogueLiteAbility },
	{ "resource/ModeEvent/ZSTransform_Skill-Dedi.csv", ZSTransform_Skill },
	{ "resource/ModeEvent/ZSTransform_Status-Dedi.csv", ZSTransform_Status },
	{ "resource/zombi/FireBombOption_Dedi.csv", FireBombOption },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Crazy.csv", ZombieSkillProperty_Crazy },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_JumpBuff.csv", ZombieSkillProperty_JumpBuff },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_ArmorUp.csv", ZombieSkillProperty_ArmorUp },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Heal.csv", ZombieSkillProperty_Heal },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_ShieldBuf.csv", ZombieSkillProperty_ShieldBuf },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Cloacking.csv", ZombieSkillProperty_Cloacking },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Trap.csv", ZombieSkillProperty_Trap },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Smoke.csv", ZombieSkillProperty_Smoke },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_VoodooHeal.csv", ZombieSkillProperty_VoodooHeal },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Shock.csv", ZombieSkillProperty_Shock },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Rush.csv", ZombieSkillProperty_Rush },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Pile.csv", ZombieSkillProperty_Pile },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Bat.csv", ZombieSkillProperty_Bat },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Stiffen.csv", ZombieSkillProperty_Stiffen },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_SelfDestruct.csv", ZombieSkillProperty_SelfDestruct },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Penetration.csv", ZombieSkillProperty_Penetration },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Revival.csv", ZombieSkillProperty_Revival },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Telleport.csv", ZombieSkillProperty_Telleport },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Boost.csv", ZombieSkillProperty_Boost },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_BombCreate.csv", ZombieSkillProperty_BombCreate },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Flying.csv", ZombieSkillProperty_Flying },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Fireball.csv", ZombieSkillProperty_Fireball },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_DogShoot.csv", ZombieSkillProperty_DogShoot },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_ViolentRush.csv", ZombieSkillProperty_ViolentRush },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_WebShooter.csv", ZombieSkillProperty_WebShooter },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_WebBomb.csv", ZombieSkillProperty_WebBomb },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Protect.csv", ZombieSkillProperty_Protect },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_ChargeSlash.csv", ZombieSkillProperty_ChargeSlash },
	{ "resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Claw.csv", ZombieSkillProperty_Claw },
	{ "resource/zombi5/HumanAbilityData_Dedi.csv", HumanAbilityData },
	{ "resource/zombi5/HumanAbilityProbData_Dedi.csv", HumanAbilityProbData },
	{ "resource/zombi5/SpecialZombieProb_Dedi.csv", SpecialZombieProb },
	{ "resource/zombi5/VirusFactorReq_Dedi.csv", VirusFactorReq },
	{ "resource/zombi5/ZombiVirusBonus_Dedi.csv", ZombiVirusBonus }
};

bool LoadCsv(int* _this, const char* filename, unsigned char* defaultBuf, int defaultBufSize)
{
	unsigned char* buffer = NULL;
	long size = 0;

	if (g_bLoadDediCsvFromFile)
	{
		char path[MAX_PATH];
		snprintf(path, sizeof(path), "%s/Data/%s", Sys_GetLongPathNameWithoutBin(), filename);

		FILE* file = fopen(path, "rb");
		if (!file)
		{
			printf("LoadCsv: %s failed to load from file (file == NULL), loading from filesystem\n", filename);
			goto LoadFileSystem;
		}

		fseek(file, 0, SEEK_END);
		size = ftell(file);
		rewind(file);

		if (size)
		{
			buffer = (unsigned char*)malloc(size);
			if (buffer)
				fread(buffer, 1, size, file);
			else
				printf("LoadCsv: %s failed to load from file (malloc failed), loading from filesystem\n", filename);
		}
		else
			printf("LoadCsv: %s failed to load from file (size <= 0), loading from filesystem\n", filename);

		fclose(file);

		if (buffer)
			goto SetBuffer;
	}

LoadFileSystem:
	FileHandle_t fh = g_pFileSystem->Open(filename, "rb", 0);
	if (!fh)
	{
		printf("LoadCsv: %s failed to load from filesystem (fh == NULL), loading hardcoded values\n", filename);
		goto LoadDefaultBuf;
	}

	size = g_pFileSystem->Size(fh);
	if (size)
	{
		buffer = (unsigned char*)malloc(size);
		if (buffer)
			g_pFileSystem->Read(buffer, size, fh);
		else
			printf("LoadCsv: %s failed to load from filesystem (malloc failed), loading hardcoded values\n", filename);
	}
	else
		printf("LoadCsv: %s failed to load from filesystem (size <= 0), loading hardcoded values\n", filename);

	g_pFileSystem->Close(fh);

	if (buffer)
		goto SetBuffer;

LoadDefaultBuf:
	buffer = defaultBuf;
	size = defaultBufSize;

SetBuffer:
	g_pfnParseCSV(_this, buffer, size);

	bool result = 0;
	if (_this[2])
		result = _this[3] != 0;

	return result;
}

CreateHookClassType(bool, CreateStringTable, int, const char* filename)
{
	std::string filenameStr = filename;
	if (filenameStr.find("maps/BoostingPoints_Dedi_") != std::string::npos)
		return LoadCsv(ptr, filename, NULL, NULL);

	if (dediCsv.find(filename) != dediCsv.end())
	{
		switch (dediCsv[filename])
		{
		case TDM_Spawn_Replacement: return LoadCsv(ptr, filename, g_TDM_Spawn_Replacement, sizeof(g_TDM_Spawn_Replacement));
		case AllStar_Skill: return LoadCsv(ptr, filename, g_AllStar_Skill, sizeof(g_AllStar_Skill));
		case AllStar_Status: return LoadCsv(ptr, filename, g_AllStar_Status, sizeof(g_AllStar_Status));
		case LastStand: return LoadCsv(ptr, filename, g_LastStand, sizeof(g_LastStand));
		case ProtectionSupplyWeapon: return LoadCsv(ptr, filename, g_ProtectionSupplyWeapon, sizeof(g_ProtectionSupplyWeapon));
		case RandomRule_Classic: return LoadCsv(ptr, filename, g_RandomRule_Classic, sizeof(g_RandomRule_Classic));
		case RandomRule: return LoadCsv(ptr, filename, g_RandomRule, sizeof(g_RandomRule));
		case ZSRogueLiteAbility: return LoadCsv(ptr, filename, g_ZSRogueLiteAbility, sizeof(g_ZSRogueLiteAbility));
		case ZSTransform_Skill: return LoadCsv(ptr, filename, g_ZSTransform_Skill, sizeof(g_ZSTransform_Skill));
		case ZSTransform_Status: return LoadCsv(ptr, filename, g_ZSTransform_Status, sizeof(g_ZSTransform_Status));
		case FireBombOption: return LoadCsv(ptr, filename, g_FireBombOption, sizeof(g_FireBombOption));
		case HumanAbilityData: return LoadCsv(ptr, filename, g_HumanAbilityData, sizeof(g_HumanAbilityData));
		case HumanAbilityProbData: return LoadCsv(ptr, filename, g_HumanAbilityProbData, sizeof(g_HumanAbilityProbData));
		case SpecialZombieProb: return LoadCsv(ptr, filename, g_SpecialZombieProb, sizeof(g_SpecialZombieProb));
		case VirusFactorReq: return LoadCsv(ptr, filename, g_VirusFactorReq, sizeof(g_VirusFactorReq));
		case ZombiVirusBonus: return LoadCsv(ptr, filename, g_ZombiVirusBonus, sizeof(g_ZombiVirusBonus));
		}
	}

	return g_pfnCreateStringTable(ptr, filename);
}

bool LoadJson(std::string* filename, std::string* oriBuf, unsigned char* defaultBuf, int defaultBufSize)
{
	unsigned char* buffer = NULL;
	long size = 0;

	if (g_bLoadDediCsvFromFile)
	{
		char path[MAX_PATH];
		snprintf(path, sizeof(path), "%s/Data/%s", Sys_GetLongPathNameWithoutBin(), filename->c_str());

		FILE* file = fopen(path, "rb");
		if (!file)
		{
			printf("LoadJson: %s failed to load from file (file == NULL), loading from filesystem\n", filename->c_str());
			goto LoadFileSystem;
		}

		fseek(file, 0, SEEK_END);
		size = ftell(file);
		rewind(file);

		if (size)
		{
			buffer = (unsigned char*)malloc(size);
			if (buffer)
				fread(buffer, 1, size, file);
			else
				printf("LoadJson: %s failed to load from file (malloc failed), loading from filesystem\n", filename->c_str());
		}
		else
			printf("LoadJson: %s failed to load from file (size <= 0), loading from filesystem\n", filename->c_str());

		fclose(file);

		if (buffer)
			goto SetBuffer;
	}

LoadFileSystem:
	FileHandle_t fh = g_pFileSystem->Open(filename->c_str(), "r", 0);
	if (!fh)
	{
		printf("LoadJson: %s failed to load from filesystem (fh == NULL), loading hardcoded values\n", filename->c_str());
		goto LoadDefaultBuf;
	}

	size = g_pFileSystem->Size(fh);
	if (size)
	{
		buffer = (unsigned char*)malloc(size);
		if (buffer)
			g_pFileSystem->Read(buffer, size, fh);
		else
			printf("LoadJson: %s failed to load from filesystem (malloc failed), loading hardcoded values\n", filename->c_str());
	}
	else
		printf("LoadJson: %s failed to load from filesystem (size <= 0), loading hardcoded values\n", filename->c_str());

	g_pFileSystem->Close(fh);

	if (buffer)
		goto SetBuffer;

LoadDefaultBuf:
	buffer = defaultBuf;
	size = defaultBufSize;

SetBuffer:
	*oriBuf = std::string((char*)buffer, (char*)buffer + size);

	return 1;
}

CreateHook(__stdcall, int, LoadJson, std::string* filename, std::string* buffer)
{
	if (dediCsv.find(*filename) != dediCsv.end())
	{
		switch (dediCsv[*filename])
		{
		case ZombieSkillProperty_Crazy: return LoadJson(filename, buffer, g_ZombieSkillProperty_Crazy, sizeof(g_ZombieSkillProperty_Crazy));
		case ZombieSkillProperty_JumpBuff: return LoadJson(filename, buffer, g_ZombieSkillProperty_JumpBuff, sizeof(g_ZombieSkillProperty_JumpBuff));
		case ZombieSkillProperty_ArmorUp: return LoadJson(filename, buffer, g_ZombieSkillProperty_ArmorUp, sizeof(g_ZombieSkillProperty_ArmorUp));
		case ZombieSkillProperty_Heal: return LoadJson(filename, buffer, g_ZombieSkillProperty_Heal, sizeof(g_ZombieSkillProperty_Heal));
		case ZombieSkillProperty_ShieldBuf: return LoadJson(filename, buffer, g_ZombieSkillProperty_ShieldBuf, sizeof(g_ZombieSkillProperty_ShieldBuf));
		case ZombieSkillProperty_Cloacking: return LoadJson(filename, buffer, g_ZombieSkillProperty_Cloacking, sizeof(g_ZombieSkillProperty_Cloacking));
		case ZombieSkillProperty_Trap: return LoadJson(filename, buffer, g_ZombieSkillProperty_Trap, sizeof(g_ZombieSkillProperty_Trap));
		case ZombieSkillProperty_Smoke: return LoadJson(filename, buffer, g_ZombieSkillProperty_Smoke, sizeof(g_ZombieSkillProperty_Smoke));
		case ZombieSkillProperty_VoodooHeal: return LoadJson(filename, buffer, g_ZombieSkillProperty_VoodooHeal, sizeof(g_ZombieSkillProperty_VoodooHeal));
		case ZombieSkillProperty_Shock: return LoadJson(filename, buffer, g_ZombieSkillProperty_Shock, sizeof(g_ZombieSkillProperty_Shock));
		case ZombieSkillProperty_Rush: return LoadJson(filename, buffer, g_ZombieSkillProperty_Rush, sizeof(g_ZombieSkillProperty_Rush));
		case ZombieSkillProperty_Pile: return LoadJson(filename, buffer, g_ZombieSkillProperty_Pile, sizeof(g_ZombieSkillProperty_Pile));
		case ZombieSkillProperty_Bat: return LoadJson(filename, buffer, g_ZombieSkillProperty_Bat, sizeof(g_ZombieSkillProperty_Bat));
		case ZombieSkillProperty_Stiffen: return LoadJson(filename, buffer, g_ZombieSkillProperty_Stiffen, sizeof(g_ZombieSkillProperty_Stiffen));
		case ZombieSkillProperty_SelfDestruct: return LoadJson(filename, buffer, g_ZombieSkillProperty_SelfDestruct, sizeof(g_ZombieSkillProperty_SelfDestruct));
		case ZombieSkillProperty_Penetration: return LoadJson(filename, buffer, g_ZombieSkillProperty_Penetration, sizeof(g_ZombieSkillProperty_Penetration));
		case ZombieSkillProperty_Revival: return LoadJson(filename, buffer, g_ZombieSkillProperty_Revival, sizeof(g_ZombieSkillProperty_Revival));
		case ZombieSkillProperty_Telleport: return LoadJson(filename, buffer, g_ZombieSkillProperty_Telleport, sizeof(g_ZombieSkillProperty_Telleport));
		case ZombieSkillProperty_Boost: return LoadJson(filename, buffer, g_ZombieSkillProperty_Boost, sizeof(g_ZombieSkillProperty_Boost));
		case ZombieSkillProperty_BombCreate: return LoadJson(filename, buffer, g_ZombieSkillProperty_BombCreate, sizeof(g_ZombieSkillProperty_BombCreate));
		case ZombieSkillProperty_Flying: return LoadJson(filename, buffer, g_ZombieSkillProperty_Flying, sizeof(g_ZombieSkillProperty_Flying));
		case ZombieSkillProperty_Fireball: return LoadJson(filename, buffer, g_ZombieSkillProperty_Fireball, sizeof(g_ZombieSkillProperty_Fireball));
		case ZombieSkillProperty_DogShoot: return LoadJson(filename, buffer, g_ZombieSkillProperty_DogShoot, sizeof(g_ZombieSkillProperty_DogShoot));
		case ZombieSkillProperty_ViolentRush: return LoadJson(filename, buffer, g_ZombieSkillProperty_ViolentRush, sizeof(g_ZombieSkillProperty_ViolentRush));
		case ZombieSkillProperty_WebShooter: return LoadJson(filename, buffer, g_ZombieSkillProperty_WebShooter, sizeof(g_ZombieSkillProperty_WebShooter));
		case ZombieSkillProperty_WebBomb: return LoadJson(filename, buffer, g_ZombieSkillProperty_WebBomb, sizeof(g_ZombieSkillProperty_WebBomb));
		case ZombieSkillProperty_Protect: return LoadJson(filename, buffer, g_ZombieSkillProperty_Protect, sizeof(g_ZombieSkillProperty_Protect));
		case ZombieSkillProperty_ChargeSlash: return LoadJson(filename, buffer, g_ZombieSkillProperty_ChargeSlash, sizeof(g_ZombieSkillProperty_ChargeSlash));
		case ZombieSkillProperty_Claw: return LoadJson(filename, buffer, g_ZombieSkillProperty_Claw, sizeof(g_ZombieSkillProperty_Claw));
		}
	}

	return g_pfnLoadJson(filename, buffer);
}

enum metaDataType
{
	zipMetadata,
	binToJsonMetadata,
	binMetadata
};

metaDataType GetMetadataType(int metaDataID)
{
	switch (metaDataID)
	{
	case 0:
	case 1:
	case 2:
	case 12:
	case 20:
	case 21:
	case 27:
	case 28:
	case 29:
	case 30:
	case 31:
	case 32:
	case 35:
	case 36:
	case 37:
	case 38:
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 47:
	case 48:
	case 51:
	case 53:
	case 54:
	case 55:
	case 56:
	case 59:
	case 61:
	case 62:
	case 63:
	case 64:
	case 65:
	case 66:
	case 69:
		return zipMetadata;
	case 6:
	case 15:
	case 16:
		return binToJsonMetadata;
	default:
		return binMetadata;
	}
}

const char* GetMetadataName(int metaDataID)
{
	switch (metaDataID)
	{
	case 0:
		return "resource/MapList.csv";
	case 1:
		return "resource/itemBox.csv";
	case 2:
		return "resource/MapModeV2/ModeList.csv";
	case 12:
		return "Matching.csv";
	case 20:
		return "weaponparts.csv";
	case 21:
		return "MileageShop.csv";
	case 27:
		return "resource/GameModeList.csv";
	case 28:
		return "badwordadd.csv";
	case 29:
		return "badworddel.csv";
	case 30:
		return "resource/zombiez/progress_unlock.csv";
	case 31:
		return "ReinforceMaxLv.csv";
	case 32:
		return "ReinforceMaxExp.csv";
	case 35:
		return "resource/item.csv";
	case 36:
		return "voxel/voxel_list.csv";
	case 37:
		return "voxel/voxel_item.csv";
	case 38:
		return "resource/codis/codisdata.cso";
	case 39:
		return "HonorShop.csv";
	case 40:
		return "resource/ItemExpireTime.csv";
	case 41:
		return "resource/scenariotx/scenariotx_common.json";
	case 42:
		return "resource/scenariotx/scenariotx_dedi.json";
	case 43:
		return "resource/scenariotx/shopitemlist_dedi.json";
	case 44:
		return "EpicPieceShop.csv";
	case 45:
		return "models/SkinWeaponInfo_server.json";
	case 47:
		return "SeasonBadgeShop.csv";
	case 48:
		return "ppsystem/config.json";
	case 49:
		return "resource/buff/classmastery.json";
	case 51:
		return "resource/zombiecompetitive/ZBCompetitive.json";
	case 53:
		return "resource/ModeEvent/ModeEvent.csv";
	case 54:
		return "resource/CPShop/EventShop.csv";
	case 55:
		return "resource/ClanWar/FamilyTotalWarMap.csv";
	case 56:
		return "resource/ClanWar/FamilyTotalWar.json";
	case 59:
		return "resource/WeaponAscend.csv";
	case 61:
		return "resource/zombi/PerkParam.csv";
	case 62:
		return "AnniversaryShop.csv";
	case 63:
		return "resource/Anniversary/GlobalAnniversary.json";
	case 64:
		return "resource/Anniversary/AnniversaryLottery.json";
	case 65:
		return "resource/Anniversary/AnniversaryTicket.json";
	case 66:
		return "resource/Synthesis/SynthesisItem.csv";
	case 69:
		return "ZCoinShop.csv";
	}
	return NULL;
}

static std::string MakeMetadataDumpPath(int metaDataID, const char* metaDataName, const char* ext)
{
	char prefix[64];
	sprintf_s(prefix, "MetadataDump/Metadata_%03d_", metaDataID);

	std::string safeName = metaDataName ? metaDataName : "unknown";
	for (char& ch : safeName)
	{
		if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' ||
			ch == '"' || ch == '<' || ch == '>' || ch == '|')
		{
			ch = '_';
		}
	}

	return std::string(prefix) + safeName + ext;
}

#pragma region Packet
void* g_pPacketMetadataParse;

CreateHookClass(int, Packet_Metadata_Parse, void* packetBuffer, int packetSize)
{
	g_pPacketMetadataParse = ptr;

	if (g_bIgnoreMetadata)
	{
		return false;
	}

	unsigned char metaDataID = *(unsigned char*)packetBuffer;
	LauncherTrace("Packet_Metadata_Parse this=%p size=%d metadataID=%u", ptr, packetSize, metaDataID);
	printf("Received metadata ID %d\n", metaDataID);

	metaDataType metaDataType = GetMetadataType(metaDataID);
	const char* metaDataName = GetMetadataName(metaDataID);

	if (g_bDumpMetadata)
	{
		FILE* file = NULL;
		unsigned short metaDataSize = 0;

		CreateDirectory("MetadataDump", NULL);

		switch (metaDataType)
		{
		case zipMetadata:
		{
			if (packetSize >= 4)
			{
				unsigned char chunkFlag = *((unsigned char*)((char*)packetBuffer + 1));
				metaDataSize = *((unsigned short*)((char*)packetBuffer + 2));
				LauncherTrace("Packet_Metadata_Parse dump zip metadataID=%u flag=0x%02X zipSize=%u packetSize=%d",
					metaDataID, chunkFlag, metaDataSize, packetSize);
			}
			break;
		}
		case binToJsonMetadata:
		{
			std::string name = MakeMetadataDumpPath(metaDataID, metaDataName, ".json");
			file = fopen(name.c_str(), "wb");
			if (!file)
			{
				printf("Can't open '%s' file to write metadata dump\n", name.c_str());
			}
			else
			{
				switch (metaDataID)
				{
				case 6:
				{
					fwrite("{\n\t\"Version\": 1,\n", 17, 1, file);

					int offset = 1;
					int size = *((unsigned short*)((char*)packetBuffer + offset)); offset += 2;

					for (int i = 0; i < size; i++)
					{
						int weaponID = *((unsigned short*)((char*)packetBuffer + offset)); offset += 2;
						int size2 = *((unsigned short*)((char*)packetBuffer + offset)); offset += 2;

						char weaponIDStr[32];
						int weaponIDSize = sprintf_s(weaponIDStr, "\t\"%d\": {\n\t\t\"Paints\": [\n", weaponID);
						fwrite(weaponIDStr, weaponIDSize, 1, file);

						for (int j = 0; j < size2; j++)
						{
							int paintID = *((unsigned short*)((char*)packetBuffer + offset)); offset += 2;

							char paintIDStr[16];
							int paintIDSize = sprintf_s(paintIDStr, "\t\t\t%d", paintID);
							fwrite(paintIDStr, paintIDSize, 1, file);

							if (size2 - 1 != j)
								fwrite(",", 1, 1, file);

							fwrite("\n", 1, 1, file);
						}

						fwrite("\t\t]\n\t}", 6, 1, file);

						if (size - 1 != i)
							fwrite(",", 1, 1, file);

						fwrite("\n", 1, 1, file);
					}

					fwrite("}", 1, 1, file);
					break;
				}
				case 15:
				{
					fwrite("{\n\t\"Version\": 1,\n\t\"Weapons\": [\n", 31, 1, file);

					int offset = 1;
					int size = *((unsigned short*)((char*)packetBuffer + offset)); offset += 2;

					for (int i = 0; i < size; i++)
					{
						int itemID = *((unsigned long*)((char*)packetBuffer + offset)); offset += 4;

						char itemIDStr[16];
						int itemIDSize = sprintf_s(itemIDStr, "\t\t%d", itemID);
						fwrite(itemIDStr, itemIDSize, 1, file);

						if (size - 1 != i)
							fwrite(",", 1, 1, file);

						fwrite("\n", 1, 1, file);
					}

					fwrite("\t]\n}", 4, 1, file);
					break;
				}
				case 16:
				{
					fwrite("{\n\t\"Version\": 1,\n", 17, 1, file);

					int offset = 1;
					int size = *((unsigned long*)((char*)packetBuffer + offset)); offset += 4;

					for (int i = 0; i < size; i++)
					{
						int itemID = *((unsigned long*)((char*)packetBuffer + offset)); offset += 4;
						int size2 = *((unsigned long*)((char*)packetBuffer + offset)); offset += 4;

						char itemIDStr[16];
						int itemIDSize = sprintf_s(itemIDStr, "\t\"%d\": {\n", itemID);
						fwrite(itemIDStr, itemIDSize, 1, file);

						for (int j = 0; j < size2; j++)
						{
							int modeFlag = *((unsigned char*)((char*)packetBuffer + offset)); offset++;
							int dropRate = *((unsigned long*)((char*)packetBuffer + offset)); offset += 4;
							int enhanceProbability = *((unsigned long*)((char*)packetBuffer + offset)); offset += 4;

							char modeFlagStr[16];
							int modeFlagSize = sprintf_s(modeFlagStr, "\t\t\"%d\": {\n", modeFlag);
							fwrite(modeFlagStr, modeFlagSize, 1, file);

							char dropRateStr[32];
							int dropRateSize = sprintf_s(dropRateStr, "\t\t\t\"DropRate\": %d,\n", dropRate);
							fwrite(dropRateStr, dropRateSize, 1, file);

							char enhanceProbabilityStr[32];
							int enhanceProbabilitySize = sprintf_s(enhanceProbabilityStr, "\t\t\t\"EnhanceProbability\": %d\n", enhanceProbability);
							fwrite(enhanceProbabilityStr, enhanceProbabilitySize, 1, file);

							fwrite("\t\t}", 3, 1, file);

							if (size2 - 1 != j)
								fwrite(",", 1, 1, file);

							fwrite("\n", 1, 1, file);
						}

						fwrite("\t}", 2, 1, file);

						if (size - 1 != i)
							fwrite(",", 1, 1, file);

						fwrite("\n", 1, 1, file);
					}

					fwrite("}", 1, 1, file);
					break;
				}
				}
				fclose(file);
			}
			break;
		}
		case binMetadata:
		{
			break;
		}
		}

		if (metaDataType != binToJsonMetadata)
		{
			const char* ext = metaDataType == zipMetadata ? ".zip" : ".bin";
			std::string name = MakeMetadataDumpPath(metaDataID, metaDataName, ext);
			file = fopen(name.c_str(), "wb");
			if (!file)
			{
				printf("Can't open '%s' file to write metadata dump\n", name.c_str());
			}
			else
			{
				if (metaDataType == zipMetadata)
				{
					if (packetSize >= 4 && metaDataSize <= packetSize - 4)
					{
						fwrite((char*)packetBuffer + 4, metaDataSize, 1, file);
					}
				}
				else
				{
					fwrite(packetBuffer, packetSize, 1, file);
				}
				fclose(file);
			}
		}
	}

	auto CallOriginalMetadataParse = [&](void* buffer, int size) -> int
	{
		LauncherTrace("Packet_Metadata_Parse call original metadataID=%u name=%s size=%d", metaDataID, metaDataName ? metaDataName : "(null)", size);
		int result = g_pfnPacket_Metadata_Parse(ptr, buffer, size);
		LauncherTrace("Packet_Metadata_Parse leave metadataID=%u name=%s result=%d", metaDataID, metaDataName ? metaDataName : "(null)", result);
		return result;
	};

	if (g_bWriteMetadata && metaDataType == zipMetadata)
	{
		HZIP hMetaData = CreateZip(0, MAX_ZIP_SIZE, ZIP_MEMORY);

		if (!hMetaData)
		{
			printf("CreateZip returned NULL.\n");
			return CallOriginalMetadataParse(packetBuffer, packetSize);
		}

		char path[MAX_PATH];
		sprintf(path, "Metadata/%s", metaDataName);
		printf("Writing metadata from %s\n", path);

		if (ZipAdd(hMetaData, metaDataName, path, 0, ZIP_FILENAME))
		{
			printf("ZipAdd returned error.\n");
			return CallOriginalMetadataParse(packetBuffer, packetSize);
		}

		void* buffer;
		unsigned long length = 0;
		ZipGetMemory(hMetaData, &buffer, &length);

		if (length == 0)
		{
			printf("ZipGetMemory returned zero length.\n");
			return CallOriginalMetadataParse(packetBuffer, packetSize);
		}

		std::vector<unsigned char> destBuffer;
		std::vector<unsigned char> metaDataBuffer((char*)buffer, (char*)buffer + length);

		destBuffer.push_back(metaDataID);
		destBuffer.push_back(5);
		destBuffer.push_back((unsigned char)(length & 0xFF));
		destBuffer.push_back((unsigned char)(length >> 8));
		destBuffer.insert(destBuffer.end(), metaDataBuffer.begin(), metaDataBuffer.end());

		CloseZip(hMetaData);

		return CallOriginalMetadataParse(static_cast<void*>(destBuffer.data()), destBuffer.size());
	}

	return CallOriginalMetadataParse(packetBuffer, packetSize);
}

void Metadata_RequestAll()
{
	std::vector<unsigned char> destBuffer;

	for (int i = 0; i < 56; i++)
	{
		destBuffer.push_back(0xFF);
		destBuffer.push_back(i);

		for (int j = 0; j < 16; j++)
			destBuffer.push_back(0x00);

		g_pfnPacket_Metadata_Parse(g_pPacketMetadataParse, static_cast<void*>(destBuffer.data()), destBuffer.size());
		destBuffer.clear();
	}
}

int counter = 0;
void DumpPacket(const char* packetName, void* packetBuffer, int packetSize)
{
	//char subType = *(char*)packetBuffer;

	CreateDirectory(packetName, NULL);

	char name[MAX_PATH];
	sprintf_s(name, "%s/%s_%d.bin", packetName, packetName, counter++);

	FILE* file = fopen(name, "wb");
	if (file)
	{
		fwrite(packetBuffer, packetSize, 1, file);
		fclose(file);
	}
	else
	{
		printf("Can't open '%s' file to write %s dump\n", name, packetName);
	}
}

CreateHookClass(int, Packet_Quest_Parse, void* packetBuffer, int packetSize)
{
	LauncherTrace("Packet_Quest_Parse this=%p size=%d subtype=%u", ptr, packetSize, packetSize > 0 ? *(unsigned char*)packetBuffer : 0xFF);
	DumpPacket("QuestDump", packetBuffer, packetSize);
	return g_pfnPacket_Quest_Parse(ptr, packetBuffer, packetSize);
}

CreateHookClass(int, Packet_UMsg_Parse, void* packetBuffer, int packetSize)
{
	LauncherTrace("Packet_UMsg_Parse this=%p size=%d subtype=%u", ptr, packetSize, packetSize > 0 ? *(unsigned char*)packetBuffer : 0xFF);
	DumpPacket("UMsgDump", packetBuffer, packetSize);
	return g_pfnPacket_UMsg_Parse(ptr, packetBuffer, packetSize);
}

CreateHookClass(int, Packet_Alarm_Parse, void* packetBuffer, int packetSize)
{
	LauncherTrace("Packet_Alarm_Parse this=%p size=%d subtype=%u", ptr, packetSize, packetSize > 0 ? *(unsigned char*)packetBuffer : 0xFF);
	DumpPacket("AlarmDump", packetBuffer, packetSize);
	return g_pfnPacket_Alarm_Parse(ptr, packetBuffer, packetSize);
}

CreateHookClass(int, Packet_Item_Parse, void* packetBuffer, int packetSize)
{
	LauncherTrace("Packet_Item_Parse this=%p size=%d subtype=%u", ptr, packetSize, packetSize > 0 ? *(unsigned char*)packetBuffer : 0xFF);
	DumpPacket("ItemDump", packetBuffer, packetSize);
	return g_pfnPacket_Item_Parse(ptr, packetBuffer, packetSize);
}

CreateHookClass(int, Packet_Crypt_Parse, void* packetBuffer, int packetSize)
{
	LauncherTrace("Packet_Crypt_Parse this=%p size=%d subtype=%u", ptr, packetSize, packetSize > 0 ? *(unsigned char*)packetBuffer : 0xFF);
	DumpPacket("CryptDump", packetBuffer, packetSize);
	return g_pfnPacket_Crypt_Parse(ptr, packetBuffer, packetSize);
}

int __fastcall Hook_Packet_Hack_Parse(void* _this, int a2, void* packetBuffer, int packetSize)
{
	return 1;
}
#pragma endregion

void __fastcall LoginDlg_OnCommand(void* _this, int r, const char* command)
{
	if (!strcmp(command, "Login"))
	{
		DWORD** v3 = (DWORD**)_this;
		char login[256];
		char password[256];

		//void* pLoginTextEntry = g_pfnPanel_FindChildByName(_this, "1");
		//void* pPasswordTextEntry = g_pfnPanel_FindChildByName(_this, "1");
		(*(void(__thiscall**)(DWORD*, char*, signed int))(*v3[109] + 628))(v3[109], login, 256); // textentry->GetText() // before 23.12.23 *v3[109] + 620
		(*(void(__thiscall**)(DWORD*, char*, signed int))(*v3[110] + 628))(v3[110], password, 256);

		wchar_t buf[256];
		swprintf(buf, L"/login %S %S", login, password);
		if (g_pChattingManager)
			g_pChattingManager->PrintToChat(1, buf);
		return;
	}
	else if (!strcmp(command, "Register"))
	{
		DWORD** v3 = (DWORD**)_this;
		char login[256];
		char password[256];

		(*(void(__thiscall**)(DWORD*, char*, signed int))(*v3[109] + 628))(v3[109], login, 256); // textentry->GetText()
		(*(void(__thiscall**)(DWORD*, char*, signed int))(*v3[110] + 628))(v3[110], password, 256);

		wchar_t buf[256];
		swprintf(buf, L"/register %S %S", login, password);
		if (g_pChattingManager)
			g_pChattingManager->PrintToChat(1, buf);
		return;
	}

	g_pfnLoginDlg_OnCommand(_this, command);
}

CreateHookClass(void, LoginDlg_OnCommand_Latest, const char* command)
{
	if (!strcmp(command, "Login"))
	{
		char login[256] = { 0 };
		char password[256] = { 0 };

		void* pLoginTextEntry = *(void**)((DWORD)ptr + 0x1BC);
		void* pPasswordTextEntry = *(void**)((DWORD)ptr + 0x1C0);
		LauncherTrace("LoginDlg_OnCommand_Latest Login: loginEntry=%p passwordEntry=%p chat=%p",
			pLoginTextEntry, pPasswordTextEntry, g_pChattingManager);

		if (pLoginTextEntry)
			(*(void(__thiscall**)(void*, char*, signed int))(*(DWORD*)pLoginTextEntry + 0x290))(pLoginTextEntry, login, 256);
		if (pPasswordTextEntry)
			(*(void(__thiscall**)(void*, char*, signed int))(*(DWORD*)pPasswordTextEntry + 0x290))(pPasswordTextEntry, password, 256);

		wchar_t buf[256];
		swprintf(buf, L"/login %S %S", login, password);
		if (g_pChattingManager)
			g_pChattingManager->PrintToChat(1, buf);
		return;
	}

	g_pfnLoginDlg_OnCommand_Latest(ptr, command);
}

static void ReadHWTextEntry(void* entry, char* out, int outSize)
{
	if (!out || outSize <= 0)
		return;

	out[0] = 0;
	if (!entry)
		return;

	__try
	{
		(*(void(__thiscall**)(void*, char*, signed int))(*(DWORD*)entry + 0x290))(entry, out, outSize);
		out[outSize - 1] = 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		out[0] = 0;
		LauncherTrace("ReadHWTextEntry exception entry=%p", entry);
	}
}

static void SetHWTextEntry(void* entry, const char* text)
{
	if (!entry)
		return;
	if (!text)
		text = "";

	__try
	{
		(*(void(__thiscall**)(void*, const char*))(*(DWORD*)entry + 0x284))(entry, text);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("SetHWTextEntry exception entry=%p textLen=%d", entry, (int)strlen(text));
	}
}

static bool HasCommandLineLogin()
{
	return g_pLogin[0] != 0 && g_pPassword[0] != 0;
}

static char* CreateHWString(const char* text)
{
	if (!text)
		text = "";

	typedef int* (__cdecl* tHWAllocString)(int len);
	tHWAllocString allocString = (tHWAllocString)(g_dwEngineBase + HW_ALLOC_STRING_RVA);
	int len = (int)strlen(text);

	__try
	{
		int* storage = allocString(len);
		if (!storage)
			return nullptr;

		char* data = (char*)(storage + 3);
		memcpy(data, text, len);
		data[len] = 0;
		*storage = len;
		return data;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("CreateHWString exception textLen=%d", len);
	}

	return nullptr;
}

DWORD WINAPI AutoHWLoginThread(LPVOID param)
{
	void* loginDlg = param;
	Sleep(4500);

	if (!loginDlg || !g_pfnHWLoginDlg_OnCommand)
	{
		LauncherTrace("AutoHWLogin skipped: dlg=%p onCommand=%p", loginDlg, g_pfnHWLoginDlg_OnCommand);
		return 0;
	}

	__try
	{
		void* pLoginTextEntry = *(void**)((DWORD)loginDlg + 0x1B8);
		void* pPasswordTextEntry = *(void**)((DWORD)loginDlg + 0x1BC);
		SetHWTextEntry(pLoginTextEntry, g_pLogin);
		SetHWTextEntry(pPasswordTextEntry, g_pPassword);
		LauncherTrace("AutoHWLogin invoking Login command: dlg=%p login='%s' passwordLen=%d",
			loginDlg, g_pLogin, (int)strlen(g_pPassword));
		g_pfnHWLoginDlg_OnCommand(loginDlg, "Login");
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("AutoHWLogin exception: dlg=%p", loginDlg);
	}

	return 0;
}

CreateHookClass(void*, HWLoginDlgConstructor, void* parent)
{
	void* ret = g_pfnHWLoginDlgConstructor(ptr, parent);
	g_pHWLoginDlg = ret ? ret : ptr;

	__try
	{
		LauncherTrace("HWLoginDlg ctor this=%p parent=%p ret=%p idEntry=%p passwordEntry=%p loginBtn=%p saveID=%p",
			ptr, parent, ret,
			*(void**)((DWORD)ptr + 0x1B8),
			*(void**)((DWORD)ptr + 0x1BC),
			*(void**)((DWORD)ptr + 0x1C4),
			*(void**)((DWORD)ptr + 0x1DC));
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("HWLoginDlg ctor log exception this=%p parent=%p ret=%p", ptr, parent, ret);
	}

	if (HasCommandLineLogin() && InterlockedCompareExchange(&g_lAutoLoginAttempted, 1, 0) == 0)
	{
		HANDLE thread = CreateThread(NULL, 0, AutoHWLoginThread, g_pHWLoginDlg, 0, NULL);
		if (thread)
		{
			CloseHandle(thread);
			LauncherTrace("AutoHWLogin scheduled: dlg=%p", g_pHWLoginDlg);
		}
		else
		{
			LauncherTrace("AutoHWLogin CreateThread failed: err=%lu", GetLastError());
		}
	}

	return ret;
}

CreateHookClass(void, HWLoginDlg_OnCommand, const char* command)
{
	const char* safeCommand = command ? command : "";
	LauncherTrace("HWLoginDlg OnCommand this=%p command='%s'", ptr, safeCommand);

	if (!strcmp(safeCommand, "Login"))
	{
		char login[256] = { 0 };
		char password[256] = { 0 };

		__try
		{
			void* pLoginTextEntry = *(void**)((DWORD)ptr + 0x1B8);
			void* pPasswordTextEntry = *(void**)((DWORD)ptr + 0x1BC);
			ReadHWTextEntry(pLoginTextEntry, login, sizeof(login));
			ReadHWTextEntry(pPasswordTextEntry, password, sizeof(password));
			LauncherTrace("HWLoginDlg Login: idEntry=%p passwordEntry=%p login='%s' passwordLen=%d",
				pLoginTextEntry, pPasswordTextEntry, login, (int)strlen(password));
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			LauncherTrace("HWLoginDlg Login read exception this=%p", ptr);
		}
	}

	g_pfnHWLoginDlg_OnCommand(ptr, command);
}

CreateHookClass(bool, HWAuthManager_Auth, char* login, char* password, char* extra)
{
	char* effectiveLogin = login;
	char* effectivePassword = password;
	char* originalLogin = login;
	char* originalPassword = password;
	if (HasCommandLineLogin())
	{
		effectiveLogin = g_pLogin;
		effectivePassword = g_pPassword;
	}

	int authMode = -1;
	void* authState = nullptr;
	__try
	{
		authState = *(void**)(g_dwEngineBase + HW_AUTH_STATE_GLOBAL_RVA);
		if (authState)
			authMode = *(int*)((DWORD)authState + 0x10);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("HWAuthManager::Auth state read exception");
	}

	LauncherTrace("HWAuthManager::Auth enter this=%p mode=%d state=%p login='%s' passwordLen=%d extra=%p cmdlineOverride=%d",
		ptr, authMode, authState, effectiveLogin ? effectiveLogin : "",
		effectivePassword ? (int)strlen(effectivePassword) : 0, extra, HasCommandLineLogin() ? 1 : 0);

	if (!g_bUseOriginalServer && !g_bDisableLocalAuthCheck && !ValidateLocalUserAccount(effectiveLogin, effectivePassword))
	{
		LauncherTrace("HWAuthManager::Auth denied locally: login='%s'", effectiveLogin ? effectiveLogin : "");
		MessageBox(NULL, "Wrong password or username.", "Login Failed", MB_OK | MB_ICONWARNING);
		return false;
	}

	if (!g_bUseOriginalServer && authState && authMode == 0)
	{
		__try
		{
			*(int*)((DWORD)authState + 0x10) = 100;
			LauncherTrace("HWAuthManager::Auth forced local game-server auth mode: 0 -> 100");
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			LauncherTrace("HWAuthManager::Auth failed to force local auth mode");
		}
	}

	if (HasCommandLineLogin())
	{
		char* hwLogin = CreateHWString(g_pLogin);
		char* hwPassword = CreateHWString(g_pPassword);
		if (hwLogin && hwPassword)
		{
			originalLogin = hwLogin;
			originalPassword = hwPassword;
			LauncherTrace("HWAuthManager::Auth using allocated hw strings: login=%p password=%p", hwLogin, hwPassword);
		}
		else
		{
			LauncherTrace("HWAuthManager::Auth failed to allocate hw strings, using original args: login=%p password=%p",
				login, password);
		}
	}

	bool result = g_pfnHWAuthManager_Auth(ptr, originalLogin, originalPassword, extra);

	int authModeAfter = -1;
	int flag20 = -1;
	int flag21 = -1;
	int flag22 = -1;
	void* packetState = nullptr;
	__try
	{
		if (authState)
			authModeAfter = *(int*)((DWORD)authState + 0x10);
		flag20 = *(BYTE*)((DWORD)ptr + 0x20);
		flag21 = *(BYTE*)((DWORD)ptr + 0x21);
		flag22 = *(BYTE*)((DWORD)ptr + 0x22);
		packetState = *(void**)((DWORD)ptr + 0x2C);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("HWAuthManager::Auth state after read exception");
	}

	LauncherTrace("HWAuthManager::Auth leave this=%p result=%d modeAfter=%d flags20=%d flags21=%d flags22=%d packetState=%p",
		ptr, result ? 1 : 0, authModeAfter,
		flag20, flag21, flag22, packetState);

	return result;
}

bool bShowLoginDlg = false;
void __fastcall GameUI_RunFrame(void* _this)
{
	if (!bShowLoginDlg)
	{
		LauncherTrace("GameUI_RunFrame first run: this=%p, disableAuthUI=%d, loginLen=%d, passwordLen=%d, chat=%p",
			_this, g_bDisableAuthUI ? 1 : 0, (int)strlen(g_pLogin), (int)strlen(g_pPassword), g_pChattingManager);

		if (ENABLE_LAUNCHER_CHAT_AUTOLOGIN && (strlen(g_pLogin) != 0 || strlen(g_pPassword) != 0))
		{
			Sleep(500);

			wchar_t buf[256];
			swprintf(buf, g_bRegister ? L"/register %S %S" : L"/login %S %S", g_pLogin, g_pPassword);
			if (g_pChattingManager)
			{
				LauncherTrace("GameUI_RunFrame sending command via ChattingManager");
				g_pChattingManager->PrintToChat(1, buf);
			}
			else
				LauncherTrace("GameUI_RunFrame cannot send command: ChattingManager is NULL");
		}

		if (ENABLE_GAMEUI_AUTH_UI && !g_bDisableAuthUI)
		{
			__try
			{
				DWORD csoMainPanelPattern = FindPattern(CSOMAINPANEL_PTR_SIG_CSNZ, CSOMAINPANEL_PTR_MASK_CSNZ, g_dwGameUIBase, g_dwGameUIBase + g_dwGameUISize, 2);
				LauncherTrace("GameUI_RunFrame CSOMainPanel pattern=%08X", csoMainPanelPattern);
				void* pCSOMainPanel = **((void***)(csoMainPanelPattern));
				LauncherTrace("GameUI_RunFrame CSOMainPanel=%p", pCSOMainPanel);
				if (!pCSOMainPanel)
				{
					MessageBox(NULL, "pCSOMainPanel == NULL!!!\nUse -disableauthui parameter to disable VGUI login dialog", "Error", MB_OK);
					bShowLoginDlg = true;
					g_pfnGameUI_RunFrame(_this);
					return;
				}

				DWORD dwPanel_FindChildByNameRelAddr = FindPattern(CALL_PANEL_FINDCHILDBYNAME_SIG_CSNZ, CALL_PANEL_FINDCHILDBYNAME_MASK_CSNZ, g_dwGameUIBase, g_dwGameUIBase + g_dwGameUISize, 1);
				LauncherTrace("GameUI_RunFrame Panel_FindChildByName call=%08X", dwPanel_FindChildByNameRelAddr);
				if (!dwPanel_FindChildByNameRelAddr)
				{
					MessageBox(NULL, "dwPanel_FindChildByNameRelAddr == NULL!!!\nUse -disableauthui parameter to disable VGUI login dialog", "Error", MB_OK);
					bShowLoginDlg = true;
					g_pfnGameUI_RunFrame(_this);
					return;
				}
				
				g_pfnPanel_FindChildByName = (tPanel_FindChildByName)(dwPanel_FindChildByNameRelAddr + 4 + *(DWORD*)dwPanel_FindChildByNameRelAddr);
				if (!g_pfnPanel_FindChildByName)
				{
					MessageBox(NULL, "g_pfnPanel_FindChildByName == NULL!!!\nUse -disableauthui parameter to disable VGUI login dialog", "Error", MB_OK);
					bShowLoginDlg = true;
					g_pfnGameUI_RunFrame(_this);
					return;
				}

				void* pLoginDlg = *(void**)((DWORD)pCSOMainPanel + 364);
				LauncherTrace("GameUI_RunFrame LoginDlg=%p from CSOMainPanel+364", pLoginDlg);
				if (!pLoginDlg)
				{
					MessageBox(NULL, "pLoginDlg == NULL!!!\nUse -disableauthui parameter to disable VGUI login dialog", "Error", MB_OK);
					bShowLoginDlg = true;
					g_pfnGameUI_RunFrame(_this);
					return;
				}

				VFTHook(pLoginDlg, 0, 99, LoginDlg_OnCommand, (void*&)g_pfnLoginDlg_OnCommand); // before 10.07.2024 iFuncIndex 98
				LauncherTrace("GameUI_RunFrame LoginDlg_OnCommand old=%p", g_pfnLoginDlg_OnCommand);

				void* pRegisterBtn = g_pfnPanel_FindChildByName(pLoginDlg, "RegisterBtn", false);
				void* pFindIDBtn = g_pfnPanel_FindChildByName(pLoginDlg, "FindIDBtn", false);
				void* pFindPWBtn = g_pfnPanel_FindChildByName(pLoginDlg, "FindPWBtn", false);
				void* pImagePanel1 = g_pfnPanel_FindChildByName(pLoginDlg, "ImagePanel1", false);
				LauncherTrace("GameUI_RunFrame login children: register=%p findID=%p findPW=%p image=%p",
					pRegisterBtn, pFindIDBtn, pFindPWBtn, pImagePanel1);

				if (!pRegisterBtn || !pFindIDBtn || !pFindPWBtn || !pImagePanel1)
				{
					MessageBox(NULL, "Invalid ptrs!!!\nUse -disableauthui parameter to disable VGUI login dialog", "Error", MB_OK);
					bShowLoginDlg = true;
					g_pfnGameUI_RunFrame(_this);
					return;
				}

				void* v27 = (**(void* (__thiscall***)(void*))pRegisterBtn)(pRegisterBtn);
				g_pPanel->SetPos((vgui::IPanel*)v27, 50, 141);
				//(*(void(__stdcall**)(void*, int, int))(*(DWORD*)pRegisterBtn + 4))(pRegisterBtn, 50, 141); // button->SetPos()
				(*(void(__thiscall**)(void*, bool))(*(DWORD*)pFindIDBtn + 160))(pFindIDBtn, false); // button->SetVisible()
				(*(void(__thiscall**)(void*, bool))(*(DWORD*)pFindPWBtn + 160))(pFindPWBtn, false); // button->SetVisible()
				(*(void(__thiscall**)(void*, const char*))(*(DWORD*)pRegisterBtn + 620))(pRegisterBtn, "Register"); // button->SetText() // before 23.12.23 pRegisterBtn + 604 // on 10.07.2024 pRegisterBtn + 612 // on 07.08.2024 pRegisterBtn + 620
				//(*(void(__thiscall**)(void*, const char*))(*(DWORD*)pImagePanel1 + 600))(pImagePanel1, "resource/login.tga"); // imagepanel->SetImage()
				(*(void(__thiscall**)(void*))(*(DWORD*)pLoginDlg + 840))(pLoginDlg); // loginDlg->DoModal() // before 23.12.23 pLoginDlg + 832 // on 10.07.2024 pLoginDlg + 840
				LauncherTrace("GameUI_RunFrame LoginDlg DoModal called");

				// i lost fucking g_pfnShowLoginDlg reference...
				/*if (g_pfnShowLoginDlg)
				{
					g_pfnShowLoginDlg(g_pCSOMainPanel);
				}
				else
				{
					MessageBox(NULL, "g_pfnShowLoginDlg == NULL!!!\nUse -disableauthui parameter to disable VGUI login dialog", "Error", MB_OK);
				}*/
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				LauncherTrace("GameUI_RunFrame exception while initializing auth UI");
				MessageBox(NULL, "Something went wrong while initializing the Auth UI!!!\nUse -disableauthui parameter to disable VGUI login dialog", "Error", MB_OK);
			}
		}
		bShowLoginDlg = true;
		LauncherTrace("GameUI_RunFrame auth flow finished, bShowLoginDlg=1");
	}
	g_pfnGameUI_RunFrame(_this);
}

void CSO_Bot_Add()
{
	// get current botmgr ptr
	DWORD dwBotManagerPtr = FindPattern(BOT_MANAGER_PTR_SIG_CSNZ, BOT_MANAGER_PTR_MASK_CSNZ, g_dwMpBase, g_dwMpBase + g_dwMpSize, 1);
	if (!dwBotManagerPtr)
	{
		MessageBox(NULL, "dwBotManagerPtr == NULL!!!", "Error", MB_OK);
		return;
	}
	g_pBotManager = **((CCSBotManager***)(dwBotManagerPtr));

	int side = 0;
	int argc = g_pEngine->Cmd_Argc();
	if (argc > 0)
	{
		side = atoi(g_pEngine->Cmd_Argv(1));
	}
	g_pBotManager->Bot_Add(side);
}

bool TryShowAuthUI()
{
	if (g_bDisableAuthUI)
		return true;

	if (!g_dwGameUIBase || !g_dwGameUISize || !g_pPanel)
	{
		LauncherTrace("TryShowAuthUI: not ready gameui=%08X size=%08X panel=%p", g_dwGameUIBase, g_dwGameUISize, g_pPanel);
		return false;
	}

	__try
	{
		DWORD csoMainPanelPattern = FindPattern(CSOMAINPANEL_PTR_SIG_CSNZ, CSOMAINPANEL_PTR_MASK_CSNZ, g_dwGameUIBase, g_dwGameUIBase + g_dwGameUISize, 2);
		void* pCSOMainPanel = NULL;
		bool latestLayout = csoMainPanelPattern == 0;
		if (csoMainPanelPattern)
			pCSOMainPanel = **((void***)(csoMainPanelPattern));
		else
		{
			DWORD latestMainPanelPtr = g_dwGameUIBase + 0xBAD15C;
			pCSOMainPanel = *(void**)latestMainPanelPtr;
			LauncherTrace("TryShowAuthUI: old CSOMainPanel pattern not found, latest global %08X -> %p", latestMainPanelPtr, pCSOMainPanel);
		}

		if (!pCSOMainPanel)
		{
			LauncherTrace("TryShowAuthUI: CSOMainPanel NULL pattern=%08X", csoMainPanelPattern);
			return false;
		}

		void* pLoginDlg = *(void**)((DWORD)pCSOMainPanel + 364);
		if (!pLoginDlg)
			pLoginDlg = *(void**)((DWORD)pCSOMainPanel + 0x180);
		if (!pLoginDlg)
		{
			LauncherTrace("TryShowAuthUI: LoginDlg NULL main=%p offsets=364/0x180", pCSOMainPanel);
			return false;
		}

		if (!g_bLatestLoginCommandHooked)
		{
			DWORD latestOnCommand = g_dwGameUIBase + 0x57F090;
			if (InlineHook((void*)latestOnCommand, Hook_LoginDlg_OnCommand_Latest, (void*&)g_pfnLoginDlg_OnCommand_Latest))
			{
				g_bLatestLoginCommandHooked = true;
				LauncherTrace("TryShowAuthUI: latest OnCommand hooked at %08X old=%p", latestOnCommand, g_pfnLoginDlg_OnCommand_Latest);
			}
			else
				LauncherTrace("TryShowAuthUI: latest OnCommand hook failed at %08X", latestOnCommand);
		}

		DWORD dwPanel_FindChildByNameRelAddr = FindPattern(CALL_PANEL_FINDCHILDBYNAME_SIG_CSNZ, CALL_PANEL_FINDCHILDBYNAME_MASK_CSNZ, g_dwGameUIBase, g_dwGameUIBase + g_dwGameUISize, 1);
		if (!dwPanel_FindChildByNameRelAddr)
		{
			LauncherTrace("TryShowAuthUI: Panel_FindChildByName call not found");
			return false;
		}

		g_pfnPanel_FindChildByName = (tPanel_FindChildByName)(dwPanel_FindChildByNameRelAddr + 4 + *(DWORD*)dwPanel_FindChildByNameRelAddr);
		if (!g_pfnPanel_FindChildByName)
		{
			LauncherTrace("TryShowAuthUI: Panel_FindChildByName resolved NULL");
			return false;
		}

		if (!latestLayout)
			VFTHook(pLoginDlg, 0, 99, LoginDlg_OnCommand, (void*&)g_pfnLoginDlg_OnCommand);

		void* pRegisterBtn = g_pfnPanel_FindChildByName(pLoginDlg, "RegisterBtn", false);
		void* pFindIDBtn = g_pfnPanel_FindChildByName(pLoginDlg, "FindIDBtn", false);
		void* pFindPWBtn = g_pfnPanel_FindChildByName(pLoginDlg, "FindPWBtn", false);
		void* pImagePanel1 = g_pfnPanel_FindChildByName(pLoginDlg, "ImagePanel1", false);
		LauncherTrace("TryShowAuthUI: main=%p loginDlg=%p children: register=%p findID=%p findPW=%p image=%p onCommand=%p",
			pCSOMainPanel, pLoginDlg, pRegisterBtn, pFindIDBtn, pFindPWBtn, pImagePanel1, g_pfnLoginDlg_OnCommand);

		if (!pRegisterBtn || !pFindIDBtn || !pFindPWBtn || !pImagePanel1)
		{
			LauncherTrace("TryShowAuthUI: invalid login children");
			DumpClientPanelTree(pCSOMainPanel, "CSOMainPanel");
			DumpClientPanelTree(pLoginDlg, "LoginDlg");
			return false;
		}

		void* v27 = (**(void* (__thiscall***)(void*))pRegisterBtn)(pRegisterBtn);
		g_pPanel->SetPos((vgui::IPanel*)v27, 50, 141);
		(*(void(__thiscall**)(void*, bool))(*(DWORD*)pFindIDBtn + 160))(pFindIDBtn, false);
		(*(void(__thiscall**)(void*, bool))(*(DWORD*)pFindPWBtn + 160))(pFindPWBtn, false);
		(*(void(__thiscall**)(void*, const char*))(*(DWORD*)pRegisterBtn + 620))(pRegisterBtn, "Register");
		(*(void(__thiscall**)(void*))(*(DWORD*)pLoginDlg + 840))(pLoginDlg);

		LauncherTrace("TryShowAuthUI: DoModal called");
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LauncherTrace("TryShowAuthUI: exception");
		return false;
	}
}

CreateHookClass(const char*, GetSSLProtocolName)
{
	LauncherTrace("GetSSLProtocolName -> TLSv1");
	return "TLSv1";
}

CreateHookClassType(void*, SocketConstructor, int, int a2, int a3, char a4)
{
	*(DWORD*)((int)ptr + 72) = (DWORD)g_pfnEVP_CIPHER_CTX_new();
	*(DWORD*)((int)ptr + 76) = (DWORD)g_pfnEVP_CIPHER_CTX_new();
	*(DWORD*)((int)ptr + 80) = (DWORD)g_pfnEVP_CIPHER_CTX_new();
	*(DWORD*)((int)ptr + 84) = (DWORD)g_pfnEVP_CIPHER_CTX_new();

	return g_pfnSocketConstructor(ptr, a2, a3, a4);
}

CreateHookClass(int, ReadPacket, char* outBuf, int len, unsigned short* outLen, bool initialMsg)
{
	int result = g_pfnReadPacket(ptr, outBuf, len, outLen, initialMsg);

	// this + 0x34 - read buf

	// 0 - got message, 4 - wrong header, 6 - idk, 7 - got less than 4 bytes, 8 - bad sequence
	unsigned short dataLen = outLen ? *outLen : 0;
	unsigned char packetId = (result == 0 && outBuf && dataLen > 0) ? (unsigned char)outBuf[0] : 0xFF;
	DWORD handler = 0;
	DWORD vtbl = 0;
	DWORD parseFn = 0;
	if (result == 0 && g_pLatestHWSocketManager && packetId != 0xFF)
	{
		__try
		{
			handler = *(DWORD*)((DWORD)g_pLatestHWSocketManager + 0x10 + packetId * 4);
			if (handler)
			{
				vtbl = *(DWORD*)handler;
				parseFn = *(DWORD*)(vtbl + 8);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			handler = 0;
			vtbl = 0;
			parseFn = 0;
		}
	}

	LauncherTrace("ReadPacket this=%p result=%d initial=%d len=%d outLen=%u id=%u handler=%08X vtbl=%08X parse=%08X rva=%08X",
		ptr, result, initialMsg ? 1 : 0, len, dataLen, packetId,
		handler, vtbl, parseFn, parseFn && g_dwEngineBase ? parseFn - g_dwEngineBase : 0);

	if (!initialMsg && result != 0)
		DumpHWSocketReadState(ptr, "read-fail");

	if (result == 0 && (packetId == 1 || packetId == 7 || packetId == 91 || packetId == 109))
	{
		LauncherTrace("ReadPacket post-parse handler table dump after id=%u", packetId);
		DumpHWSocketManagerHandlers(g_pLatestHWSocketManager);
	}

	if (!initialMsg && result == 0)
	{
		// create folder
		CreateDirectory("Packets", NULL);

		static int directoryCounter = 0;
		if (!directoryCounter)
		{
			while (true)
			{
				char directory[MAX_PATH];
				snprintf(directory, sizeof(directory), "Packets/%d", ++directoryCounter);

				DWORD dwAttr = GetFileAttributes(directory);
				if (dwAttr != 0xffffffff && (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
				{
					continue;
				}

				CreateDirectory(directory, NULL);
				break;
			}
		}

		// write file
		unsigned char* buf = (unsigned char*)(outBuf);

		static int packetCounter = 0;

		char filename[MAX_PATH];
		bool moreInfo = true;
		if (moreInfo)
			snprintf(filename, sizeof(filename), "Packets/%d/Packet_%d_ID_%d_%d.bin", directoryCounter, packetCounter++, buf[0], dataLen);
		else
			snprintf(filename, sizeof(filename), "Packets/%d/Packet_%d.bin", directoryCounter, packetCounter++);

		FILE* file = fopen(filename, "wb");
		if (file)
		{
			fwrite(buf, dataLen, 1, file);
			fclose(file);
		}
		else
			LauncherTrace("ReadPacket dump fopen failed: %s", filename);
	}

	return result;
}

CreateHook(__cdecl, void, LogToErrorLog, char* pLogFile, int logFileId, char* fmt, int fmtLen, ...)
{
	char outputString[1024];

	va_list va;
	va_start(va, fmtLen);
	_vsnprintf_s(outputString, sizeof(outputString), fmt, va);
	outputString[1023] = 0;
	va_end(va);

	printf("[LogToErrorLog][%s.log] %s\n", pLogFile, outputString);

	g_pfnLogToErrorLog(pLogFile, logFileId, outputString, fmtLen);
}

CreateHook(WINAPI, void, OutputDebugStringA, LPCSTR lpOutString)
{
	printf("[OutputDebugString] %s\n", lpOutString);
}

CreateHook(__cdecl, int, HolePunch_GetUserSocketInfo, int userID, char* data)
{
	auto ret = g_pfnHolePunch_GetUserSocketInfo(userID, data);

	data[0] = 2; // unsafety method, since other places port are corrected

	short port = (short&)data[14];
	in_addr ip = (in_addr&)data[16];

	printf("[HolePunch_GetUserSocketInfo] ret: %d | UserID: %d, %s:%d\n", ret, userID, inet_ntoa(ip), ntohs(port));

	return ret;
}

void CreateDebugConsole()
{
	AllocConsole();

	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	SetConsoleTitleA("CSO launcher debug console");
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	setlocale(LC_ALL, "");
}

DWORD WINAPI HookThread(LPVOID lpThreadParameter)
{
	LauncherTrace("HookThread start");
	hWnd = FindWindow(NULL, "Counter-Strike Nexon: Studio");
	oWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
	LauncherTrace("HookThread window=%p oldWndProc=%p", hWnd, oWndProc);

	if (!g_bUseOriginalServer)
	{
		while (!g_dwGameUIBase) // wait for gameui module
		{
			g_dwGameUIBase = (DWORD)GetModuleHandle("gameui.dll");
			Sleep(500);
		}
		g_dwGameUISize = GetModuleSize(GetModuleHandle("gameui.dll"));
		LauncherTrace("HookThread gameui base=%08X size=%08X", g_dwGameUIBase, g_dwGameUISize);

		if (g_pEngine)
		{
			g_pChattingManager = g_pEngine->GetChatManager();
			LauncherTrace("HookThread ChattingManager=%p", g_pChattingManager);
			if (!g_pChattingManager)
				MessageBox(NULL, "g_pChattingManager == NULL!!!", "Error", MB_OK);
		}
		else
			LauncherTrace("HookThread engine pointer is NULL");

		CreateInterfaceFn gameui_factory = CaptureFactory("gameui.dll");
		CreateInterfaceFn vgui2_factory = CaptureFactory("vgui2.dll");
		g_pGameUI = (IGameUI*)(CaptureInterface(gameui_factory, GAMEUI_INTERFACE_VERSION));
		g_pPanel = (vgui::IPanel*)(CaptureInterface(vgui2_factory, VGUI_PANEL_INTERFACE_VERSION));
		LauncherTrace("HookThread factories: gameui=%p vgui2=%p interfaces: gameui=%p panel=%p",
			gameui_factory, vgui2_factory, g_pGameUI, g_pPanel);
		VFTHook(g_pGameUI, 0, 6, GameUI_RunFrame, (void*&)g_pfnGameUI_RunFrame);
		LauncherTrace("HookThread GameUI_RunFrame old=%p", g_pfnGameUI_RunFrame);

		Sleep(1500);
		if (ENABLE_LAUNCHER_CHAT_AUTOLOGIN)
			SendLoginCommand();
		if (ENABLE_LAUNCHER_CHAT_AUTOLOGIN && g_bLoginCommandSent)
		{
			bShowLoginDlg = true;
			LauncherTrace("HookThread auth UI skipped after auto login command");
		}

		if (ENABLE_GAMEUI_AUTH_UI && !bShowLoginDlg && !g_bDisableAuthUI)
		{
			for (int i = 0; i < 80 && !bShowLoginDlg; i++)
			{
				if (TryShowAuthUI())
				{
					bShowLoginDlg = true;
					LauncherTrace("HookThread auth UI shown after %d tries", i + 1);
					break;
				}

				Sleep(100);
			}

			if (!bShowLoginDlg)
				LauncherTrace("HookThread auth UI was not shown after retries");
		}

		while (!g_dwMpBase) // wait for mp.dll module
		{
			g_dwMpBase = (DWORD)GetModuleHandle("mp.dll");
			Sleep(500);
		}
		g_dwMpSize = GetModuleSize(GetModuleHandle("mp.dll"));

		{
			DWORD pushStr = 0;
			DWORD patchAddr = 0;
			BYTE patch[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

			// NOP IsDedi() function to load allstar Skill csv
			pushStr = FindPush(g_dwMpBase, g_dwMpBase + g_dwMpSize, (PCHAR)("Failed to Open AllStar_Skill-Dedi Table"));
			if (!pushStr)
				MessageBox(NULL, "AllStar_Skill_Patch == NULL!!!", "Error", MB_OK);
			else
			{
				patchAddr = pushStr - 0x1B;
				WriteMemory((void*)patchAddr, (BYTE*)patch, sizeof(patch));
			}

			// NOP IsDedi() function to load allstar Status csv
			pushStr = FindPush(g_dwMpBase, g_dwMpBase + g_dwMpSize, (PCHAR)("Failed to Open AllStar_Status-Dedi Table"));
			if (!pushStr)
				MessageBox(NULL, "AllStar_Status_Patch == NULL!!!", "Error", MB_OK);
			else
			{
				patchAddr = pushStr - 0x1E;
				WriteMemory((void*)patchAddr, (BYTE*)patch, sizeof(patch));
			}

			// NOP IsDedi() function to spawn zsht_item_box and zbsitem
			pushStr = FindPush(g_dwMpBase, g_dwMpBase + g_dwMpSize, (PCHAR)("zsht_item_box"), 3);
			if (!pushStr)
				MessageBox(NULL, "ZBS_ZSHT_ItemBox_Patch == NULL!!!", "Error", MB_OK);
			else
			{
				patchAddr = pushStr - 0x4C7;
				WriteMemory((void*)patchAddr, (BYTE*)patch, sizeof(patch));
			}
		}

		if (g_pEngine)
			g_pEngine->pfnAddCommand("cso_bot_add", CSO_Bot_Add);
	}

	return TRUE;
}

void Init(HMODULE hEngineModule, HMODULE hFileSystemModule)
{
	printf("Init()\n");
	LauncherTrace("Init cmdline: %s", CommandLine()->GetCmdLine());

	if (CommandLine()->CheckParm("-debug") || CommandLine()->CheckParm("-dev") || CommandLine()->CheckParm("+developer 1") || CommandLine()->CheckParm("-developer"))
		CreateDebugConsole();

	g_hEngineModule = hEngineModule;
	g_dwEngineBase = GetModuleBase(g_hEngineModule);
	g_dwEngineSize = GetModuleSize(g_hEngineModule);

	g_dwFileSystemBase = GetModuleBase(hFileSystemModule);
	g_dwFileSystemSize = GetModuleSize(hFileSystemModule);

	const char* port;
	const char* ip;

	if (CommandLine()->CheckParm("-ip", &ip) && ip)
	{
		strncpy(g_pServerIP, ip, sizeof(g_pServerIP));
	}
	else
	{
		strncpy(g_pServerIP, DEFAULT_IP, sizeof(DEFAULT_IP));
	}

	if (CommandLine()->CheckParm("-port", &port) && port)
	{
		strncpy(g_pServerPort, port, sizeof(g_pServerPort));
	}
	else
	{
		strncpy(g_pServerPort, DEFAULT_PORT, sizeof(DEFAULT_PORT));
	}

	const char* login;
	const char* password;

	g_bRegister = CommandLine()->CheckParm("-register");
	if (CommandLine()->CheckParm("-login", &login) && login)
	{
		strncpy(g_pLogin, login, sizeof(g_pLogin) - 1);
		g_pLogin[sizeof(g_pLogin) - 1] = 0;
		printf("g_pLogin = %s\n", g_pLogin);
	}
	else if (CommandLine()->CheckParm("-id", &login) && login)
	{
		strncpy(g_pLogin, login, sizeof(g_pLogin) - 1);
		g_pLogin[sizeof(g_pLogin) - 1] = 0;
		printf("g_pLogin = %s\n", g_pLogin);
	}
	if (CommandLine()->CheckParm("-password", &password) && password)
	{
		strncpy(g_pPassword, password, sizeof(g_pPassword) - 1);
		g_pPassword[sizeof(g_pPassword) - 1] = 0;
		printf("g_pPassword = %s\n", g_pPassword);
	}
	else if (CommandLine()->CheckParm("-pw", &password) && password)
	{
		strncpy(g_pPassword, password, sizeof(g_pPassword) - 1);
		g_pPassword[sizeof(g_pPassword) - 1] = 0;
		printf("g_pPassword = %s\n", g_pPassword);
	}

	g_bUseOriginalServer = CommandLine()->CheckParm("-useoriginalserver");
	g_bDumpMetadata = CommandLine()->CheckParm("-dumpmetadata");
	g_bIgnoreMetadata = CommandLine()->CheckParm("-ignoremetadata");
	g_bDumpQuest = CommandLine()->CheckParm("-dumpquest");
	g_bDumpUMsg = CommandLine()->CheckParm("-dumpumsg");
	g_bDumpAlarm = CommandLine()->CheckParm("-dumpalarm");
	g_bDumpItem = CommandLine()->CheckParm("-dumpitem");
	g_bDumpAll = CommandLine()->CheckParm("-dumpall");
	g_bDumpCrypt = CommandLine()->CheckParm("-dumpcrypt");
	g_bDisableAuthUI = CommandLine()->CheckParm("-disableauthui");
	g_bUseSSL = !CommandLine()->CheckParm("-nousessl");
	if (CommandLine()->CheckParm("-usessl"))
		g_bUseSSL = true;
	g_bWriteMetadata = CommandLine()->CheckParm("-writemetadata");
	g_bLoadDediCsvFromFile = CommandLine()->CheckParm("-loaddedicsvfromfile");
	g_bNoNGHook = CommandLine()->CheckParm("-nonghook");
	g_bDisableLocalAuthCheck = CommandLine()->CheckParm("-disablelocalauthcheck");

	printf("g_pServerIP = %s, g_pServerPort = %s\n", g_pServerIP, g_pServerPort);
	LauncherTrace("Init server=%s:%s flags: original=%d disableAuthUI=%d useSSL=%d noNG=%d register=%d disableLocalAuth=%d loginLen=%d passwordLen=%d",
		g_pServerIP, g_pServerPort, g_bUseOriginalServer ? 1 : 0, g_bDisableAuthUI ? 1 : 0,
		g_bUseSSL ? 1 : 0, g_bNoNGHook ? 1 : 0, g_bRegister ? 1 : 0, g_bDisableLocalAuthCheck ? 1 : 0,
		(int)strlen(g_pLogin), (int)strlen(g_pPassword));
}

void Hook(HMODULE hEngineModule, HMODULE hFileSystemModule)
{
	Init(hEngineModule, hFileSystemModule);

	DWORD find = NULL;
	void* dummy = NULL;
	
	if (!g_bNoNGHook)
	{
		find = FindPattern(NGCLIENT_INIT_SIG_CSNZ, NGCLIENT_INIT_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			LauncherTrace("NGClient_Init == NULL!!!");
		else
			InlineHookFromCallOpcode((void*)find, NGClient_Return1, dummy, dummy);

		find = FindPattern(NGCLIENT_QUIT_SIG_CSNZ, NGCLIENT_QUIT_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
		{
			//훅 위치 변경
			LauncherTrace("NGClient_Quit == NULL!!!");
		}
		else
			InlineHookFromCallOpcode((void*)find, NGClient_Void, dummy, dummy);

		find = FindPattern(PACKET_HACK_SEND_SIG_CSNZ, PACKET_HACK_SEND_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
		{
			//훅 위치 변경
			LauncherTrace("Packet_Hack_Send == NULL!!!");
		}
		else
		{
			InlineHookFromCallOpcode((void*)find, NGClient_Void, dummy, dummy);
			InlineHookFromCallOpcode((void*)(find + 0x5), NGClient_Return1, dummy, dummy);
		}

		find = FindPattern(PACKET_HACK_PARSE_SIG_CSNZ, PACKET_HACK_PARSE_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
		{
			//훅 위치 변경
			LauncherTrace("Packet_Hack_Parse == NULL!!!");
		}
		else
			InlineHook((void*)find, Hook_Packet_Hack_Parse, dummy);
	}

	IATHook(g_hEngineModule, "nxgsm.dll", "InitializeGameLogManagerA", NXGSM_Dummy, dummy);
	IATHook(g_hEngineModule, "nxgsm.dll", "WriteStageLogA", NXGSM_WriteStageLogA, dummy);
	IATHook(g_hEngineModule, "nxgsm.dll", "WriteErrorLogA", NXGSM_WriteErrorLogA, dummy);
	IATHook(g_hEngineModule, "nxgsm.dll", "FinalizeGameLogManager", NXGSM_Dummy, dummy);
	IATHook(g_hEngineModule, "nxgsm.dll", "SetUserSN", NXGSM_Dummy, dummy);

	if (!g_bUseOriginalServer)
	{
		find = FindPattern(SOCKETMANAGER_SIG_CSNZ23, SOCKETMANAGER_MASK_CSNZ23, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			MessageBox(NULL, "SocketManagerConstructor == NULL!!!", "Error", MB_OK);
		else
			InlineHook((void*)find, Hook_SocketManagerConstructor, (void*&)g_pfnSocketManagerConstructor);

		find = FindPattern(SERVERCONNECT_SIG_CSNZ2019, SERVERCONNECT_MASK_CSNZ2019, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			MessageBox(NULL, "ServerConnect == NULL!!!", "Error", MB_OK);
		else
			InlineHookFromCallOpcode((void*)find, Hook_ServerConnect, (void*&)g_pfnServerConnect, dummy);

		find = FindPattern(HOLEPUNCH_SETSERVERINFO_SIG_CSNZ, HOLEPUNCH_SETSERVERINFO_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			MessageBox(NULL, "HolePunch_SetServerInfo == NULL!!!", "Error", MB_OK);
		else
			InlineHook((void*)find, Hook_HolePunch_SetServerInfo, (void*&)g_pfnHolePunch_SetServerInfo);

		find = FindPattern(HOLEPUNCH_GETUSERSOCKETINFO_SIG_CSNZ, HOLEPUNCH_GETUSERSOCKETINFO_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			MessageBox(NULL, "HolePunch_GetUserSocketInfo == NULL!!!", "Error", MB_OK);
		else
			InlineHook((void*)find, Hook_HolePunch_GetUserSocketInfo, (void*&)g_pfnHolePunch_GetUserSocketInfo);

		find = g_dwEngineBase + HW_LOGIN_DLG_CTOR_RVA;
		if (!InlineHook((void*)find, Hook_HWLoginDlgConstructor, (void*&)g_pfnHWLoginDlgConstructor))
			LauncherTrace("HWLoginDlg constructor hook failed at %08X", find);
		else
			LauncherTrace("HWLoginDlg constructor hooked at %08X old=%p", find, g_pfnHWLoginDlgConstructor);

		find = g_dwEngineBase + HW_LOGIN_DLG_ONCOMMAND_RVA;
		if (!InlineHook((void*)find, Hook_HWLoginDlg_OnCommand, (void*&)g_pfnHWLoginDlg_OnCommand))
			LauncherTrace("HWLoginDlg OnCommand hook failed at %08X", find);
		else
			LauncherTrace("HWLoginDlg OnCommand hooked at %08X old=%p", find, g_pfnHWLoginDlg_OnCommand);

		find = g_dwEngineBase + HW_AUTH_MANAGER_AUTH_RVA;
		if (!InlineHook((void*)find, Hook_HWAuthManager_Auth, (void*&)g_pfnHWAuthManager_Auth))
			LauncherTrace("HWAuthManager::Auth hook failed at %08X", find);
		else
			LauncherTrace("HWAuthManager::Auth hooked at %08X old=%p", find, g_pfnHWAuthManager_Auth);

		if (HW_SOCKET_MANAGER_EVENT_RVA)
		{
			find = g_dwEngineBase + HW_SOCKET_MANAGER_EVENT_RVA;
			if (!InlineHook((void*)find, Hook_HWSocketManager_Event, (void*&)g_pfnHWSocketManager_Event))
				LauncherTrace("HWSocketManager::Event hook failed at %08X", find);
			else
				LauncherTrace("HWSocketManager::Event hooked at %08X old=%p", find, g_pfnHWSocketManager_Event);
		}
		else
		{
			LauncherTrace("HWSocketManager::Event hook skipped: RVA disabled for this hw.dll");
		}

		if (HW_SOCKET_MANAGER_CONNECT_RVA)
		{
			find = g_dwEngineBase + HW_SOCKET_MANAGER_CONNECT_RVA;
			if (!InlineHook((void*)find, Hook_HWSocketManager_Connect, (void*&)g_pfnHWSocketManager_Connect))
				LauncherTrace("HWSocketManager::Connect hook failed at %08X", find);
			else
				LauncherTrace("HWSocketManager::Connect hooked at %08X old=%p", find, g_pfnHWSocketManager_Connect);
		}
		else
		{
			LauncherTrace("HWSocketManager::Connect hook skipped: RVA disabled for this hw.dll");
		}

		find = g_dwEngineBase + HW_WSARECV_WRAPPER_RVA;
		if (*(BYTE*)find == 0x55 && *(BYTE*)(find + 1) == 0x8B && *(BYTE*)(find + 2) == 0xEC)
		{
			if (!InlineHook((void*)find, Hook_HWSocket_WSARecv, (void*&)g_pfnHWSocket_WSARecv))
				LauncherTrace("HWSocket::WSARecv wrapper hook failed at %08X", find);
			else
				LauncherTrace("HWSocket::WSARecv wrapper hooked at %08X old=%p", find, g_pfnHWSocket_WSARecv);
		}
		else
		{
			LauncherTrace("HWSocket::WSARecv wrapper RVA mismatch at %08X bytes=%02X %02X %02X",
				find, *(BYTE*)find, *(BYTE*)(find + 1), *(BYTE*)(find + 2));
		}

		/*
		{
			DWORD pushStr = FindPush(g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, (PCHAR)("resource/zombi/ZombieSkillTable_Dedi.csv"));
			
			// read instruction opcode to know we found valid address
			int opcode = 0;
			ReadMemory((void*)(pushStr + 0xF), (BYTE*)&opcode, 1);

			if (opcode == 0xE8 && pushStr && InlineHookFromCallOpcode((void*)(pushStr + 0xF), CreateStringTable, (void*&)g_pfnCreateStringTable, dummy))
			{
				DWORD parseCsvCallAddr = (DWORD)dummy + 0x71 + 1; // 0x71
				g_pfnParseCSV = (tParseCSV)(parseCsvCallAddr + 4 + *(DWORD*)parseCsvCallAddr);

				// patch LoadZombieSkill function to load csv bypassing filesystem
				DWORD patchAddr = pushStr - 0x1A;
				BYTE patch[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
				WriteMemory((void*)patchAddr, (BYTE*)patch, sizeof(patch));
			}
			else
			{
				MessageBox(NULL, "Failed to patch zombie skill table", "Error", MB_OK);
			}
		}
		*/

		{
			DWORD pushStr = 0;
			DWORD patchAddr = 0;

			// NOP dedi check on Zombie Skills
			pushStr = FindPush(g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, (PCHAR)("resource/zombi/ZombieSkillProperty_Dedi/ZombieSkillProperty_Crazy.csv"));
			if (!pushStr)
				LauncherTrace("ZombieSkillProperty_Patch == NULL!!!");
			else
			{
				patchAddr = pushStr - 0x23;
				BYTE patch[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
				WriteMemory((void*)patchAddr, (BYTE*)patch, sizeof(patch));
			}

			// NOP dedi check on Fire Bomb
			pushStr = FindPush(g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, (PCHAR)("resource/zombi/FireBombOption_Dedi.csv"));
			if (!pushStr)
				LauncherTrace("FireBombOption_Patch == NULL!!!");
			else
			{
				patchAddr = pushStr - 0x8;
				BYTE patch2[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
				WriteMemory((void*)patchAddr, (BYTE*)patch2, sizeof(patch2));
			}

			find = FindPattern(CREATESTRINGTABLE_SIG_CSNZ, CREATESTRINGTABLE_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
			if (!find)
				LauncherTrace("CreateStringTable == NULL!!!");
			else
			{
				InlineHook((void*)find, Hook_CreateStringTable, (void*&)g_pfnCreateStringTable);

				DWORD parseCsvCallAddr = (DWORD)find + 0x71 + 1; // 0x71
				g_pfnParseCSV = (tParseCSV)(parseCsvCallAddr + 4 + *(DWORD*)parseCsvCallAddr);
			}

			find = FindPatternCompat(LOADJSON_SIG_CSNZ_LATEST, LOADJSON_MASK_CSNZ_LATEST, LOADJSON_SIG_CSNZ, LOADJSON_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize);
			if (!find)
				LauncherTrace("LoadJson == NULL!!!");
			else
				InlineHook((void*)find, Hook_LoadJson, (void*&)g_pfnLoadJson);
		}
	}

	find = FindPattern(LOGTOERRORLOG_SIG_CSNZ, LOGTOERRORLOG_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
	if (!find)
		MessageBox(NULL, "LogToErrorLog == NULL!!!", "Error", MB_OK);
	else
		InlineHook((void*)find, Hook_LogToErrorLog, (void*&)g_pfnLogToErrorLog);

	g_pEngine = (cl_enginefunc_t*)(PVOID) * (PDWORD)(FindPush(g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, (PCHAR)("ScreenFade")) + 0x0D);
	if (!g_pEngine)
		MessageBox(NULL, "g_pEngine == NULL!!!", "Error", MB_OK);
	else
		// hook Pbuf_AddText to allow any cvar or cmd input from console
		g_pEngine->Pbuf_AddText = Pbuf_AddText;

	if (g_bDumpMetadata || g_bWriteMetadata || g_bIgnoreMetadata || !g_bUseOriginalServer)
	{
		find = FindPattern(PACKET_METADATA_PARSE_SIG_CSNZ, PACKET_METADATA_PARSE_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			LauncherTrace("Packet_Metadata_Parse == NULL!!!");
		else
		{
			InlineHook((void*)find, Hook_Packet_Metadata_Parse, (void*&)g_pfnPacket_Metadata_Parse);
			LauncherTrace("Packet_Metadata_Parse hooked at %08X old=%p", find, g_pfnPacket_Metadata_Parse);
			if (g_pEngine)
				g_pEngine->pfnAddCommand("metadata_requestall", Metadata_RequestAll);
		}
	}

	if (g_bDumpQuest || !g_bUseOriginalServer)
	{
		find = FindPattern(PACKET_QUEST_PARSE_SIG_CSNZ, PACKET_QUEST_PARSE_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			LauncherTrace("Packet_Quest_Parse == NULL!!!");
		else
		{
			InlineHook((void*)find, Hook_Packet_Quest_Parse, (void*&)g_pfnPacket_Quest_Parse);
			LauncherTrace("Packet_Quest_Parse hooked at %08X old=%p", find, g_pfnPacket_Quest_Parse);
		}
	}

	if (g_bDumpUMsg || !g_bUseOriginalServer)
	{
		find = FindPattern(PACKET_UMSG_PARSE_SIG_CSNZ, PACKET_UMSG_PARSE_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			LauncherTrace("Packet_UMsg_Parse == NULL!!!");
		else
		{
			InlineHook((void*)find, Hook_Packet_UMsg_Parse, (void*&)g_pfnPacket_UMsg_Parse);
			LauncherTrace("Packet_UMsg_Parse hooked at %08X old=%p", find, g_pfnPacket_UMsg_Parse);
		}
	}

	if (g_bDumpAlarm || !g_bUseOriginalServer)
	{
		find = FindPattern(PACKET_ALARM_PARSE_SIG_CSNZ, PACKET_ALARM_PARSE_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			LauncherTrace("Packet_Alarm_Parse == NULL!!!");
		else
		{
			InlineHook((void*)find, Hook_Packet_Alarm_Parse, (void*&)g_pfnPacket_Alarm_Parse);
			LauncherTrace("Packet_Alarm_Parse hooked at %08X old=%p", find, g_pfnPacket_Alarm_Parse);
		}
	}

	if (g_bDumpItem || !g_bUseOriginalServer)
	{
		find = FindPattern(PACKET_ITEM_PARSE_SIG_CSNZ, PACKET_ITEM_PARSE_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			LauncherTrace("Packet_Item_Parse == NULL!!!");
		else
		{
			InlineHook((void*)find, Hook_Packet_Item_Parse, (void*&)g_pfnPacket_Item_Parse);
			LauncherTrace("Packet_Item_Parse hooked at %08X old=%p", find, g_pfnPacket_Item_Parse);
		}
	}

	if (g_bDumpCrypt || !g_bUseOriginalServer)
	{
		find = FindPattern(PACKET_CRYPT_PARSE_SIG_CSNZ, PACKET_CRYPT_PARSE_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			LauncherTrace("Packet_Crypt_Parse == NULL!!!");
		else
		{
			InlineHook((void*)find, Hook_Packet_Crypt_Parse, (void*&)g_pfnPacket_Crypt_Parse);
			LauncherTrace("Packet_Crypt_Parse hooked at %08X old=%p", find, g_pfnPacket_Crypt_Parse);
		}
	}

	if (g_bDumpAll || !g_bUseOriginalServer)
	{
		find = FindPattern(READPACKET_SIG_CSNZ, READPACKET_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			MessageBox(NULL, "ReadPacket == NULL!!!", "Error", MB_OK);
		else
		{
			DWORD readPacketFunc = find + 5 + *(DWORD*)(find + 1);
			LauncherTrace("ReadPacket call located at %08X target=%08X", find, readPacketFunc);
			InlineHookFromCallOpcode((void*)find, Hook_ReadPacket, (void*&)g_pfnReadPacket, dummy);
			LauncherTrace("ReadPacket hooked via call at %08X old=%p", find, g_pfnReadPacket);

			DWORD eventCall = g_dwEngineBase + HW_READPACKET_EVENT_CALL_RVA;
			if (*(BYTE*)eventCall == 0xE8)
			{
				DWORD eventTarget = eventCall + 5 + *(DWORD*)(eventCall + 1);
				if (eventTarget == readPacketFunc)
				{
					void* eventDummy = NULL;
					InlineHookFromCallOpcode((void*)eventCall, Hook_ReadPacket, eventDummy, dummy);
					LauncherTrace("ReadPacket hooked via event call at %08X target=%08X", eventCall, eventTarget);
				}
				else
				{
					LauncherTrace("ReadPacket event call target mismatch at %08X target=%08X expected=%08X",
						eventCall, eventTarget, readPacketFunc);
				}
			}
			else
			{
				LauncherTrace("ReadPacket event call opcode mismatch at %08X opcode=%02X", eventCall, *(BYTE*)eventCall);
			}
		}
	}

	// patch launcher name in hw.dll to fix annoying message box (length of launcher filename must be < original name)
	find = FindPattern("cstrike-online.exe", strlen("cstrike-online.exe"), g_dwEngineBase, g_dwEngineBase + g_dwEngineSize);
	if (!find)
		MessageBox(NULL, "LauncherName_Patch == NULL!!!", "Error", MB_OK);
	else
		WriteMemory((void*)find, (BYTE*)"CSOLauncher.exe", strlen("CSOLauncher.exe") + 1);

	// patch 100 fps limit 
	// 훅 위치 변경
	find = FindPush(g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, "%3i fps -- host(%3.0f) sv(%3.0f) cl(%3.0f) gfx(%3.0f) snd(%3.0f) ents(%d)\n", 2);
	if (!find)
		LauncherTrace("100Fps_Patch == NULL!!!");
	else
	{
		DWORD patchAddr = find - 0x4DA;
		BYTE patch[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
		WriteMemory((void*)patchAddr, (BYTE*)patch, sizeof(patch));
	}

	if (!g_bUseOriginalServer)
	{
		// hook GetSSLProtocolName to make Crypt work
		find = FindPattern(GETSSLPROTOCOLNAME_SIG_CSNZ, GETSSLPROTOCOLNAME_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			MessageBox(NULL, "GetSSLProtocolName == NULL!!!", "Error", MB_OK);
		else
			InlineHookFromCallOpcode((void*)find, Hook_GetSSLProtocolName, (void*&)g_pfnGetSSLProtocolName, dummy);
	}

	if (!g_bUseOriginalServer && !g_bUseSSL)
	{
		// hook SocketConstructor to create ctx objects
		find = FindPatternCompat(SOCKETCONSTRUCTOR_SIG_CSNZ, SOCKETCONSTRUCTOR_MASK_CSNZ, SOCKETCONSTRUCTOR_SIG_CSNZ_LATEST, SOCKETCONSTRUCTOR_MASK_CSNZ_LATEST, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize);
		if (!find)
			MessageBox(NULL, "SocketConstructor == NULL!!!", "Error", MB_OK);
		else
			InlineHookFromCallOpcode((void*)find, Hook_SocketConstructor, (void*&)g_pfnSocketConstructor, dummy);

		find = FindPattern(EVP_CIPHER_CTX_NEW_SIG_CSNZ, EVP_CIPHER_CTX_NEW_MASK_CSNZ, g_dwEngineBase, g_dwEngineBase + g_dwEngineSize, NULL);
		if (!find)
			MessageBox(NULL, "EVP_CIPHER_CTX_new == NULL!!!", "Error", MB_OK);
		else
		{
			DWORD dwCreateCtxAddr = find + 1;
			g_pfnEVP_CIPHER_CTX_new = (tEVP_CIPHER_CTX_new)(dwCreateCtxAddr + 4 + *(DWORD*)dwCreateCtxAddr);
		}
	}

	// create thread to wait for other modules
	CreateThread(NULL, 0, HookThread, NULL, 0, 0);
}

void Unhook()
{
	FreeAllHook();
}
