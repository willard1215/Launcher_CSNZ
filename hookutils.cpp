#include "hookutils.h"
#include "Detours/detours.h"

hook_t* g_pHookBase = NULL;


DWORD GetModuleBase(HMODULE hModule)
{
	MEMORY_BASIC_INFORMATION mem;

	if (!VirtualQuery(hModule, &mem, sizeof(MEMORY_BASIC_INFORMATION)))
		return 0;

	return (DWORD)mem.AllocationBase;
}

DWORD GetModuleSize(HMODULE hModule)
{
	return ((IMAGE_NT_HEADERS*)((DWORD)hModule + ((IMAGE_DOS_HEADER*)hModule)->e_lfanew))->OptionalHeader.SizeOfImage;
}

hook_t* NewHook(void)
{
	hook_t* h = new hook_t;
	memset(h, 0, sizeof(hook_t));
	h->pNext = g_pHookBase;
	g_pHookBase = h;
	return h;
}

hook_t* FindInlineHooked(void* pOldFuncAddr)
{
	for (hook_t* h = g_pHookBase; h; h = h->pNext)
	{
		if (h->pOldFuncAddr == pOldFuncAddr)
			return h;
	}

	return NULL;
}

hook_t* FindVFTHooked(void* pClass, int iTableIndex, int iFuncIndex)
{
	for (hook_t* h = g_pHookBase; h; h = h->pNext)
	{
		if (h->pClass == pClass && h->iTableIndex == iTableIndex && h->iFuncIndex == iFuncIndex)
			return h;
	}

	return NULL;
}

hook_t* FindIATHooked(HMODULE hModule, const char* pszModuleName, const char* pszFuncName)
{
	for (hook_t* h = g_pHookBase; h; h = h->pNext)
	{
		if (h->hModule == hModule && h->pszModuleName == pszModuleName && h->pszFuncName == pszFuncName)
			return h;
	}

	return NULL;
}

#pragma pack(push, 1)

struct tagIATDATA
{
	void* pAPIInfoAddr;
};

struct tagCLASS
{
	DWORD* pVMT;
};

struct tagVTABLEDATA
{
	tagCLASS* pInstance;
	void* pVFTInfoAddr;
};

#pragma pack(pop)

void FreeHook(hook_t* pHook)
{
	if (pHook->pClass)
	{
		tagVTABLEDATA* info = (tagVTABLEDATA*)pHook->pInfo;
		WriteMemory(info->pVFTInfoAddr, (BYTE*)pHook->pOldFuncAddr, sizeof(DWORD));
	}
	else if (pHook->hModule)
	{
		tagIATDATA* info = (tagIATDATA*)pHook->pInfo;
		WriteMemory(info->pAPIInfoAddr, (BYTE*)pHook->pOldFuncAddr, sizeof(DWORD));
	}
	else
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(void*&)pHook->pOldFuncAddr, pHook->pNewFuncAddr);
		DetourTransactionCommit();
	}

	if (pHook->pInfo)
		delete pHook->pInfo;

	delete pHook;
}

void FreeAllHook(void)
{
	hook_t* next = NULL;

	for (hook_t* h = g_pHookBase; h; h = next)
	{
		next = h->pNext;
		FreeHook(h);
	}

	g_pHookBase = NULL;
}

BOOL UnHook(hook_t* pHook)
{
	if (!g_pHookBase)
		return FALSE;

	if (!g_pHookBase->pNext)
	{
		FreeHook(pHook);
		g_pHookBase = NULL;
		return TRUE;
	}

	hook_t* last = NULL;

	for (hook_t* h = g_pHookBase->pNext; h; h = h->pNext)
	{
		if (h->pNext != pHook)
		{
			last = h;
			continue;
		}

		last->pNext = h->pNext;
		FreeHook(h);
		return TRUE;
	}

	return FALSE;
}

hook_t* InlineHook(void* pOldFuncAddr, void* pNewFuncAddr, void*& pCallBackFuncAddr)
{
	hook_t* h = NewHook();
	h->pOldFuncAddr = pOldFuncAddr;
	h->pNewFuncAddr = pNewFuncAddr;

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(void*&)h->pOldFuncAddr, pNewFuncAddr);
	DetourTransactionCommit();

	pCallBackFuncAddr = h->pOldFuncAddr;
	return h;
}

hook_t* InlineHookFromCallOpcode(void* pOldFuncAddr, void* pNewFuncAddr, void*& pCallBackFuncAddr, void*& pFuncAddr)
{
	if (!pOldFuncAddr)
		return NULL;

	DWORD dwRelCall = *(DWORD*)((DWORD)pOldFuncAddr + 1);
	if (!dwRelCall)
		return NULL;

	pFuncAddr = (void*)((DWORD)pOldFuncAddr + 1 + 4 + dwRelCall);
	if (!pFuncAddr)
		return NULL;

	return InlineHook(pFuncAddr, pNewFuncAddr, pCallBackFuncAddr);
}

hook_t* VFTHook(void* pClass, int iTableIndex, int iFuncIndex, void* pNewFuncAddr, void*& pCallBackFuncAddr)
{
	tagVTABLEDATA* info = new tagVTABLEDATA;
	info->pInstance = (tagCLASS*)pClass;

	DWORD* pVMT = ((tagCLASS*)pClass + iTableIndex)->pVMT;
	info->pVFTInfoAddr = pVMT + iFuncIndex;

	hook_t* h = NewHook();
	h->pOldFuncAddr = (void*)pVMT[iFuncIndex];
	h->pNewFuncAddr = pNewFuncAddr;
	h->pInfo = info;
	h->pClass = pClass;
	h->iTableIndex = iTableIndex;
	h->iFuncIndex = iFuncIndex;

	pCallBackFuncAddr = h->pOldFuncAddr;
	WriteMemory(info->pVFTInfoAddr, (BYTE*)&pNewFuncAddr, sizeof(DWORD));
	return 0;
}

hook_t* IATHook(HMODULE hModule, const char* pszModuleName, const char* pszFuncName, void* pNewFuncAddr, void*& pCallBackFuncAddr)
{
	IMAGE_NT_HEADERS* pHeader = (IMAGE_NT_HEADERS*)((DWORD)hModule + ((IMAGE_DOS_HEADER*)hModule)->e_lfanew);
	DWORD importRva = pHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
	if (!importRva)
		return NULL;

	IMAGE_IMPORT_DESCRIPTOR* pImport = (IMAGE_IMPORT_DESCRIPTOR*)((DWORD)hModule + importRva);
	while (pImport->Name && _stricmp((const char*)((DWORD)hModule + pImport->Name), pszModuleName))
		pImport++;

	if (!pImport->Name)
		return NULL;

	HMODULE hImportModule = GetModuleHandle(pszModuleName);
	if (!hImportModule)
		hImportModule = LoadLibrary(pszModuleName);
	if (!hImportModule)
		return NULL;

	DWORD dwFuncAddr = (DWORD)GetProcAddress(hImportModule, pszFuncName);
	if (!dwFuncAddr)
		return NULL;

	IMAGE_THUNK_DATA* pThunk = (IMAGE_THUNK_DATA*)((DWORD)hModule + pImport->FirstThunk);

	while (pThunk->u1.Function && pThunk->u1.Function != dwFuncAddr)
		pThunk++;

	if (!pThunk->u1.Function)
		return NULL;

	tagIATDATA* info = new tagIATDATA;
	info->pAPIInfoAddr = &pThunk->u1.Function;

	hook_t* h = NewHook();
	h->pOldFuncAddr = (void*)pThunk->u1.Function;
	h->pNewFuncAddr = pNewFuncAddr;
	h->pInfo = info;
	h->hModule = hModule;
	h->pszModuleName = pszModuleName;
	h->pszFuncName = pszFuncName;

	pCallBackFuncAddr = h->pOldFuncAddr;
	WriteMemory(info->pAPIInfoAddr, (BYTE*)&pNewFuncAddr, sizeof(DWORD));
	return h;
}

hook_t* IATHookOrdinal(HMODULE hModule, const char* pszModuleName, int ordinal, void* pNewFuncAddr, void*& pCallBackFuncAddr)
{
	IMAGE_NT_HEADERS* pHeader = (IMAGE_NT_HEADERS*)((DWORD)hModule + ((IMAGE_DOS_HEADER*)hModule)->e_lfanew);
	IMAGE_IMPORT_DESCRIPTOR* pImport = (IMAGE_IMPORT_DESCRIPTOR*)((DWORD)hModule + pHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	while (pImport->Name && _stricmp((const char*)((DWORD)hModule + pImport->Name), pszModuleName))
		pImport++;

	IMAGE_THUNK_DATA* pThunk = (IMAGE_THUNK_DATA*)((DWORD)hModule + pImport->FirstThunk);
	IMAGE_THUNK_DATA* pOrigThunk = (IMAGE_THUNK_DATA*)((DWORD)hModule + pImport->OriginalFirstThunk);

	// find needed function by ordinal
	while (pOrigThunk->u1.Function && !(IMAGE_SNAP_BY_ORDINAL(pOrigThunk->u1.Ordinal) && IMAGE_ORDINAL(pOrigThunk->u1.Ordinal) == ordinal))
	{
		pOrigThunk++;
		pThunk++;
	}

	if (!pOrigThunk->u1.Function)
	{
		// import function not found
		return NULL;
	}

	tagIATDATA* info = new tagIATDATA;
	info->pAPIInfoAddr = &pThunk->u1.Function;

	hook_t* h = NewHook();
	h->pOldFuncAddr = (void*)pThunk->u1.Function;
	h->pNewFuncAddr = pNewFuncAddr;
	h->pInfo = info;
	h->hModule = hModule;
	h->pszModuleName = pszModuleName;
	h->pszFuncName = "";

	pCallBackFuncAddr = h->pOldFuncAddr;
	WriteMemory(info->pAPIInfoAddr, (BYTE*)&pNewFuncAddr, sizeof(DWORD));
	return h;

}

void* GetClassFuncAddr(...)
{
	DWORD address;

	__asm
	{
		lea eax, address
		mov edx, [ebp + 8]
		mov[eax], edx
	}

	return (void*)address;
}

DWORD FindPattern(PCHAR pattern, PCHAR mask, DWORD start, DWORD end, DWORD offset)
{
	int patternLength = strlen(mask);
	bool found = false;

	for (DWORD i = start; i < end - patternLength; i++)
	{
		found = true;
		for (int idx = 0; idx < patternLength; idx++)
		{
			if (mask[idx] == 'x' && pattern[idx] != *(PCHAR)(i + idx))
			{
				found = false;
				break;
			}
		}
		if (found)
		{
			return i + offset;
		}
	}

	return 0;
}

DWORD FindPattern(PCHAR pattern, DWORD patternLength, DWORD start, DWORD end, DWORD offset, DWORD refNumber)
{
	bool found = false;

	for (DWORD i = start; i < end - patternLength; i++)
	{
		found = true;

		for (size_t idx = 0; idx < patternLength; idx++)
		{
			if (pattern[idx] != *(PCHAR)(i + idx))
			{
				found = false;
				break;
			}
		}

		if (found)
		{
			refNumber--;
			if (!refNumber)
				return i + offset;
		}
	}

	return 0;
}

DWORD FindPush(DWORD start, DWORD end, PCHAR Message, DWORD refNumber)
{
	char bPushAddrPattern[] = { 0x68 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 };
	DWORD Address = FindPattern(Message, strlen(Message), start, end, 0);
	*(PDWORD)&bPushAddrPattern[1] = Address;
	Address = FindPattern(bPushAddrPattern, sizeof(bPushAddrPattern) - 1, start, end, 0, refNumber);
	return Address;
}

void WriteDWORD(void* pAddress, DWORD dwValue)
{
	DWORD dwProtect;

	if (VirtualProtect((void*)pAddress, 4, PAGE_EXECUTE_READWRITE, &dwProtect))
	{
		*(DWORD*)pAddress = dwValue;
		VirtualProtect((void*)pAddress, 4, dwProtect, &dwProtect);
	}
}

DWORD ReadDWORD(void* pAddress)
{
	DWORD dwProtect;
	DWORD dwValue = 0;

	if (VirtualProtect((void*)pAddress, 4, PAGE_EXECUTE_READWRITE, &dwProtect))
	{
		dwValue = *(DWORD*)pAddress;
		VirtualProtect((void*)pAddress, 4, dwProtect, &dwProtect);
	}

	return dwValue;
}

DWORD WriteMemory(void* pAddress, BYTE* pData, DWORD dwDataSize)
{
	static DWORD dwProtect;

	if (VirtualProtect(pAddress, dwDataSize, PAGE_EXECUTE_READWRITE, &dwProtect))
	{
		memcpy(pAddress, pData, dwDataSize);
		VirtualProtect(pAddress, dwDataSize, dwProtect, &dwProtect);
	}

	return dwDataSize;
}

DWORD ReadMemory(void* pAddress, BYTE* pData, DWORD dwDataSize)
{
	static DWORD dwProtect;

	if (VirtualProtect(pAddress, dwDataSize, PAGE_EXECUTE_READWRITE, &dwProtect))
	{
		memcpy(pData, pAddress, dwDataSize);
		VirtualProtect(pAddress, dwDataSize, dwProtect, &dwProtect);
	}

	return dwDataSize;
}
