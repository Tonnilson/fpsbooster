// dllmain.cpp : Defines the entry point for the DLL application.
#include <pe/module.h>
#include <xorstr.hpp>
#include "pluginsdk.h"
#include "searchers.h"
#include <tchar.h>
#include <iostream>
#include <ShlObj.h>
#include <KnownFolders.h>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <wil/stl.h>
#include <wil/win32_helpers.h>
#include "detours.h"
#include "UIElement.h"

static uintptr_t mainTextParser = NULL;
static uintptr_t mainChatNotification = NULL;
static uintptr_t parseCombatLog = NULL;
static uintptr_t aWorldPointer = NULL;

static BYTE mainTextParserBytes[] = { 0x00, 0x00 };
static BYTE mainNotificationBytes[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static BYTE mainNotificationJmp[] = { 0xE9, 0x00, 0x00, 0x00, 0x00, 0x90 };
static BYTE parseCombatLogBytes[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static BYTE parseCombatLogJmp[] = { 0xE9, 0x00, 0x00, 0x00, 0x00, 0x90 };

static int hideAll = 0;
static std::filesystem::path filterPath;
static int PlayerZoneID = NULL;

const std::filesystem::path& documents_path()
{
	static std::once_flag once_flag;
	static std::filesystem::path path;

	std::call_once(once_flag, [](std::filesystem::path& path) {
		wil::unique_cotaskmem_string result;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &result)))
			path = result.get();
		}, path);
	return path;
}

#define IS_NUMERIC(string) (!string.empty() && std::find_if(string.begin(), string.end(), [](unsigned char c) { return !std::isdigit(c); }) == string.end())
#define IS_KEY_DOWN(key) ((GetAsyncKeyState(key) & (1 << 16)))
#define IN_RANGE(low, high, x) ((low <= x && x <= high))

std::vector<int> uiElementIds = {
	3450, // Chat
	5507, //Active Buffs
	1264, //icons small
	1268, //Icons big
	1267, //Name & Money
	1269, //XP Bar
	6139, //Quest ID
	5514, //Auto Combat
	6126, //Map
	6222, //Basin specific
	5496 //Party
};

#ifdef _M_X64
static std::vector<__int64> uiPointers;
#else
static std::vector<int> uiPointers;
#endif

static void loadFilter() {
	std::string fileString;
	std::ifstream file(filterPath.c_str());

	if (!uiElementIds.empty())
		uiElementIds.clear();

	while (std::getline(file, fileString)) {
		if (!fileString.empty() && IS_NUMERIC(fileString))
			uiElementIds.push_back(std::stoi(fileString));
	}

	return;
}

static void HotKeyMonitor() {
	static bool mainTextPressed;
	static bool notificationsPressed;
	static bool chatPressed;
	static bool uiElementsPressed;
	static bool reloadFilterList;
	static bool keyWasDown = false;
	static bool keyDown = false;
	while (true) {
		if (IS_KEY_DOWN(VK_MENU)) {
			mainTextPressed = IS_KEY_DOWN(0x31);
			chatPressed = IS_KEY_DOWN(0x33);
			uiElementsPressed = IS_KEY_DOWN(0x58);
			reloadFilterList = IS_KEY_DOWN(VK_INSERT);

			if (reloadFilterList) {
				if (std::filesystem::exists(filterPath))
					loadFilter();

				std::this_thread::sleep_for(std::chrono::milliseconds(1000)); //Delay by 1s to prevent excessive execution
			}
			else if (mainTextPressed && mainTextParser) {
				if (*(BYTE*)mainTextParser == 0x90) {
					memcpy((LPVOID)mainTextParser, mainTextParserBytes, sizeof(mainTextParserBytes));
				}
				else {
					memset((LPVOID)mainTextParser, 0x90, 2);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(500)); //Delay by 500ms to prevent excessive execution
			}
			else if (chatPressed && mainChatNotification) {
#ifdef _M_X64
				if (*(BYTE*)mainChatNotification == 0x74)
					*(BYTE*)mainChatNotification = 0xEB;
				else
					*(BYTE*)mainChatNotification = 0x74;
#else
				if (*(BYTE*)mainChatNotification == 0x72)
					*(BYTE*)mainChatNotification = 0xEB;
				else
					*(BYTE*)mainChatNotification = 0x72;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(500)); //Delay by 500ms to prevent excessive execution
			}
			else if (uiElementsPressed) {
				if (hideAll == 1) {
					hideAll = 0;
					UIElement* element;
					bool SkipID = false;

					for (auto x : uiPointers) {
						element = reinterpret_cast<UIElement*>(x);
						//Check to make sure the pointer and ID is valid / not 0
						if (element != nullptr && element->ID != NULL) {
							//Make sure visibility is between 2->3 (2 for visible | 3 for hidden)
							if (IN_RANGE(2, 3, element->Visibility)) {
								auto idSearch = std::find(uiElementIds.begin(), uiElementIds.end(), element->ID);
								if (idSearch != uiElementIds.end()) {
									//This bit was about checking player zone and making sure to unhide the correct Quest ID
									if (IN_RANGE(6400, 6405, PlayerZoneID) && element->ID == 6139)
										SkipID = true;
									else if (element->ID == 6222 && !IN_RANGE(6400, 6405, PlayerZoneID))
										SkipID = true;
									else
										SkipID = false;

									if (!SkipID && element->Visibility != NULL)
										element->Visibility = 2;
								}
							}
						}
					}
					/*
						Empty out the vector on each hide, why? Well when the player loads new zones
						or switches characters etc the game reallocates chunks of the UI so things get
						shifted around, we clear it just to make life easy and to keep the array from
						building up with excessive junk elements. Could make it better and make a
						multi dimensional vector containing desired Element ID and pointer but
						I stopped caring about blade and shit
					*/
					if (!uiPointers.empty())
						uiPointers.clear();
				}
				else
					hideAll = 1;

				std::this_thread::sleep_for(std::chrono::milliseconds(500)); //Delay by 500ms to prevent excessive execution
			}
		}
		//Create some slack for our thread, was going to use a low-level keyboard hook for event based checking but had issues with ALT key so dropped it.
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}
}

#ifdef _M_X64
bool(__fastcall* oWorldThread)(__int64);
bool __fastcall hkWorldThread(__int64 localPVtable) {
	__int64 localPlayerWorld = *(unsigned __int64*)(localPVtable + 0xC8);
	if (localPlayerWorld) {
		int zoneID = *reinterpret_cast<int*>(localPlayerWorld + 0xB0);
		if (zoneID != PlayerZoneID) {
			PlayerZoneID = zoneID;
		}
	}
	return oWorldThread(localPVtable);
}

bool(__fastcall* oInitMainLobby)(__int64, unsigned int);
bool __fastcall hkInitMainLobby(__int64 a1, unsigned int a2) {
	if (!uiPointers.empty())
		uiPointers.clear();

	if (hideAll)
		hideAll = 0;

	return oInitMainLobby(a1, a2);
}

bool(__fastcall* oIsElementVisible)(__int64, __int64);
bool __fastcall hkIsElementVisisble(__int64 uiElement, __int64 a2) {

	if (hideAll == 1) {
		if (*reinterpret_cast<int*>(uiElement + 0xC) == 1048600) {
			UIElement* element = reinterpret_cast<UIElement*>(uiElement);
			auto idSearch = std::find(uiElementIds.begin(), uiElementIds.end(), element->ID);
			if (idSearch != uiElementIds.end()) {
				if ((std::find(uiPointers.begin(), uiPointers.end(), uiElement) == uiPointers.end()))
					uiPointers.push_back(uiElement);

				if (element->Visibility == 2)
					element->Visibility = 3;
			}
		}
	}

	return oIsElementVisible(uiElement, a2);
}
#else

bool Detour32(char* src, char* dst, const uintptr_t len)
{
	if (len < 5) return false;
	DWORD  curProtection;
	VirtualProtect(src, len, PAGE_EXECUTE_READWRITE, &curProtection);
	uintptr_t  relativeAddress = (uintptr_t)(dst - (uintptr_t)src) - 5;

	*src = (char)'\xE9';
	*(uintptr_t*)((uintptr_t)src + 1) = relativeAddress;
	for (uintptr_t i = 1 + sizeof(uintptr_t); i < len; i++) {
		*(src + i) = (char)'\x90';
	}

	VirtualProtect(src, len, curProtection, &curProtection);
	return true;
}

int elementP = NULL;
uintptr_t oElementAddr = NULL;

bool(_stdcall* oParseCombatLog)(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int);
bool __stdcall hkParseCombatLog(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12, int a13, int a14, int a15, int a16, int a17, int a18, int a19) {
	return 0;
}

void CheckAndHideUIElement(int uiElement) {
	UIElement* element = reinterpret_cast<UIElement*>(uiElement);
	auto idSearch = std::find(uiElementIds.begin(), uiElementIds.end(), element->ID);
	if (idSearch != uiElementIds.end()) {
		if ((std::find(uiPointers.begin(), uiPointers.end(), uiElement) == uiPointers.end()))
			uiPointers.push_back(uiElement);

		if (element->Visibility == 2 && element->ID != 5453)
			element->Visibility = 3;

	}
}

static uintptr_t oWorldData = 0;
static uintptr_t oInitMainLobby = 0;

__declspec(naked) void hkWorldData() {
	__asm {
		mov eax, [eax + 0x90]
		mov[PlayerZoneID], eax
		jmp[oWorldData]
	}
}

__declspec(naked) void hkInitMainLobby() {
	__asm {
		push esp
		push edx
		push ebp
		push eax
		push ecx
		push esp
		push esi
	}

	if (!uiPointers.empty())
		uiPointers.clear();

	if (hideAll)
		hideAll = 0;

	__asm {
		pop esi
		pop esp
		pop ecx
		pop eax
		pop ebp
		pop edx
		pop esp
		cmp esi, [edi+0x588]
		jmp [oInitMainLobby]
	}
}

__declspec(naked) void hkIsElementVisisble() {

	__asm {
		push esp
		push edx
		push ebp
		push eax
		push ecx
		push esp
		push esi
		mov[elementP], ebp
	}
	if (hideAll == 1)
		CheckAndHideUIElement(elementP);

	__asm {
		pop esi
		pop esp
		pop ecx
		pop eax
		pop ebp
		pop edx
		pop esp
		mov eax, [edx + 0x2F8]
		jmp[oElementAddr]
	}
}

#endif

void __cdecl oep_notify([[maybe_unused]] const version_t client_version)
{
	if (const auto module = pe::get_module()) {
		DetourTransactionBegin();
		DetourUpdateThread(NtCurrentThread());

		//If filter exists, load custom filter set.
		if (std::filesystem::exists(filterPath))
			loadFilter();

		uintptr_t handle = module->handle();
		const auto sections2 = module->segments();
		const auto& s2 = std::find_if(sections2.begin(), sections2.end(), [](const IMAGE_SECTION_HEADER& x) {
			return x.Characteristics & IMAGE_SCN_CNT_CODE;
			});
		const auto data = s2->as_bytes();

#ifdef _M_X64

		/*
			One of two main text handlers for parsing, converting and eventually
			displaying text to screen.
			Used for Alt + 1

			Patch 169
			Original pattern: 48 89 44 24 48 48 8B 44 24 48 48 8B 00 48 8B 54 24 68 48 8B 4C 24 48
		*/
		auto sMainText = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("48 89 44 24 48 48 8B 44 24 48 48 8B 00 48 8B 54 24 68 48 8B 4C 24 48")));
		if (sMainText != data.end()) {
			mainTextParser = (uintptr_t)&sMainText[0] - 0xE;
			memcpy((LPVOID)mainTextParserBytes, (LPVOID)mainTextParser, 2);
		}

		/*
			Find condition for saying parse text to chatbox
			Used for Alt + 3

			Patch 169
			Original pattern: 48 8B 53 1C 48 85 D2 74 ?? 4C 8B C6 48 8D 4C 24 40
		*/
		auto sChatNotif = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("48 8B 53 1C 48 85 D2 74 ?? 4C 8B C6 48 8D 4C 24 40")));
		if (sChatNotif != data.end()) {
			mainChatNotification = (uintptr_t)&sChatNotif[0] + 0x7;
			//*(BYTE*)mainChatNotification = 0xEB;
		}

		/*
			Find the caller for saying X skill used and parse text
			for combat log.

			Patch 169
			Original pattern: CC 48 85 D2 0F 84 ?? ?? ?? ?? 55 56 57 41 54 41 55 41 56 41 57 48 81 EC C0 00 00 00 48 C7 84 24 B0 00 00 00 FE FF FF FF
		*/
		auto sCombatLog = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("CC 48 85 D2 0F 84 ?? ?? ?? ?? 55 56 57 41 54 41 55 41 56 41 57 48 81 EC C0 00 00 00 48 C7 84 24 B0 00 00 00 FE FF FF FF")));
		if (sCombatLog != data.end()) {
			parseCombatLog = (uintptr_t)&sCombatLog[0] + 0x4;
			memcpy((LPVOID)(parseCombatLogJmp + 0x1), (LPVOID)(parseCombatLog + 0x2), 4);
			*(BYTE*)(parseCombatLogJmp + 0x1) += 0x01;
			memcpy((LPVOID)parseCombatLogBytes, (LPVOID)parseCombatLog, sizeof(parseCombatLogBytes));
			memcpy((LPVOID)parseCombatLog, parseCombatLogJmp, sizeof(parseCombatLogJmp));
		}

		/*
			Hook a thread to accurately retrieve the Character World Pointer
			Used for checking players current zoneID but this pointer contains
			more information about the player like character name, health etc.

			Patch 169
			Original pattern: 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 50 48 89 74 24 58 48 8B F1 48 8B 91 C8 00 00 00 48 85 D2 75
		*/
		auto sWorldThread = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("48 C7 44 24 20 FE FF FF FF 48 89 5C 24 50 48 89 74 24 58 48 8B F1 48 8B 91 C8 00 00 00 48 85 D2 75")));
		uintptr_t aWorldThread = NULL;
		if (sWorldThread != data.end()) {
			aWorldThread = (uintptr_t)&sWorldThread[0] - 0x6;
			oWorldThread = module->rva_to<std::remove_pointer_t<decltype(oWorldThread)>>(aWorldThread - handle);
			DetourAttach(&(PVOID&)oWorldThread, &hkWorldThread);
		}

		/*
			Hook main lobby initialization
			check / reset our uiPointer array and turn off hideAll
		*/
		auto sInitMainLobby = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 8B DA")));
		if (sInitMainLobby != data.end()) {
			uintptr_t aInitMainLobby = (uintptr_t)&sInitMainLobby[0];
			oInitMainLobby = module->rva_to<std::remove_pointer_t<decltype(oInitMainLobby)>>(aInitMainLobby - handle);
			DetourAttach(&(PVOID&)oInitMainLobby, &hkInitMainLobby);
		}

#else
		/*
			One of two main text handlers for parsing, converting and eventually
			displaying text to screen.
			Used for Alt + 1

			Patch 169
			Original pattern: 8D 44 24 10 64 A3 00 00 00 00 8B 44 24 24 85 C0
		*/
		auto sMainText = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("8D 44 24 10 64 A3 00 00 00 00 8B 44 24 24 85 C0")));
		if (sMainText != data.end()) {
			mainTextParser = (uintptr_t)&sMainText[0] + 0x14;
			memcpy((LPVOID)mainTextParserBytes, (LPVOID)mainTextParser, 2);
		}

		/*
			Find condition for saying parse text to chatbox
			Used for Alt + 3

			Patch 169
			Original pattern: 8B 45 1C 85 C0 8B 4D 18 72 ??
		*/
		auto sChatNotif = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("8B 45 1C 85 C0 8B 4D 18 72 ??")));
		if (sChatNotif != data.end()) {
			mainChatNotification = (uintptr_t)&sChatNotif[0] + 0x8;
			//*(BYTE*)mainChatNotification = 0xEB;
		}

		/*
			Hook a thread to accurately retrieve the Character World Pointer
			Used for checking players current zoneID but this pointer contains
			more information about the player like character name, health etc.

			Patch 169
			Original pattern: 8B 89 2C 4B 00 00 8B 31 99 52 8B 56 64 50 FF D2 8B F0
		*/
		auto sWorldPointer = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("8B 89 2C 4B 00 00 8B 31 99 52 8B 56 64 50 FF D2 8B F0")));
		if (sWorldPointer != data.end()) {
			aWorldPointer = (uintptr_t)&sWorldPointer[0] - 0xC;
			//8B 80 90 00 00 00
			oWorldData = aWorldPointer + 0x6;
			Detour32((char*)aWorldPointer, (char*)hkWorldData, 6);
		}

		/*
			Do a trampoline jump on the Main Lobby loader
			to check / reset our uiPointer array and turn off hideAll
		*/
		auto sInitMainLobby = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("57 8B F8 3B B7 88 05 00 00")));
		if (sInitMainLobby != data.end()) {
			uintptr_t aInitMainLobby = (uintptr_t)&sInitMainLobby[0] + 0x3;
			//3B B7 88 05 00 00
			oInitMainLobby = aInitMainLobby + 0x6;
			Detour32((char*)aInitMainLobby, (char*)hkInitMainLobby, 6);
		}

		/*
			Find the caller for saying X skill used and parse text
			for combat log.

			Patch 169
			Original pattern: 6A 00 6A 08 51 52 50 E8
		*/
		auto sCombatLog = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("6A 00 6A 08 51 52 50 E8")));
		if (sCombatLog != data.end()) {
			uintptr_t aCombatLog = (uintptr_t)&sCombatLog[0] - 0x5C;
			oParseCombatLog = module->rva_to<std::remove_pointer_t<decltype(oParseCombatLog)>>(aCombatLog - handle);
			DetourAttach(&(PVOID&)oParseCombatLog, &hkParseCombatLog);
			DetourTransactionCommit();
			//memset((void*)aCombatLog, 0x90, 5);
		}
#endif
		DetourTransactionCommit();
#ifdef _M_X64
		if (const auto module = pe::get_module(L"bsengine_Shipping64.dll")) {
#else
		if (const auto module = pe::get_module(L"bsengine_Shipping.dll")) {
#endif
			DetourTransactionBegin();
			DetourUpdateThread(NtCurrentThread());

			uintptr_t handle = module->handle();
			const auto sections = module->segments();
			const auto& s2 = std::find_if(sections.begin(), sections.end(), [](const IMAGE_SECTION_HEADER& x) {
				return x.Characteristics & IMAGE_SCN_CNT_CODE;
				});
			const auto data = s2->as_bytes();

			/*
				The function we will be hooking to get all UI Elements
				and hide them from the players screen.

				This function is checking thousands of UI Elements per cycle
				so take care in what you do in this function, It's not actually
				checking visibility of the UI element, it does a lot more than that
				but this is just a dumb down name.
			*/
#ifdef _M_X64
			//40 53 48 83 EC 20 F6 81 84 00 00 00 01 48 8B D9
			auto sElementVis = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("40 53 48 83 EC 20 F6 81 84 00 00 00 01 48 8B D9")));
			uintptr_t aElementVis = NULL;
			if (sElementVis != data.end()) {
				aElementVis = (uintptr_t)&sElementVis[0];
				oIsElementVisible = module->rva_to<std::remove_pointer_t<decltype(oIsElementVisible)>>(aElementVis - handle);
				DetourAttach(&(PVOID&)oIsElementVisible, &hkIsElementVisisble);
				DetourTransactionCommit();
			}
#else
			auto sElementVis = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("8B 82 F8 02 00 00 8B CB FF D0 8B F0")));
			uintptr_t aElementVis = NULL;
			if (sElementVis != data.end()) {
				aElementVis = (uintptr_t)&sElementVis[0];
				oElementAddr = aElementVis + 0x6;
				Detour32((char*)aElementVis, (char*)hkIsElementVisisble, 6);
			}
#endif
		}

		//Create our hotkey thread and detach from current thread
		std::thread t1(HotKeyMonitor);
		t1.detach();
		}
	}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		/*
			Check if booster.txt file path was set if not set the path
			We're checking in dllmain to avoid directx mods like d912prxy and Reshade
			limiting viewing access outside of game directory.

			May not be needed anymore with loader3 but I can't reproduce the issue
			so leaving it as is. Harms nothing.
		*/
		if (filterPath.empty())
			filterPath = documents_path() / xorstr_(L"BnS\\booster.txt");

		DisableThreadLibraryCalls(hInstance);
	}

	return TRUE;
}

bool __cdecl init([[maybe_unused]] const version_t client_version)
{
	NtCurrentPeb()->BeingDebugged = FALSE;
	return true;
}

extern "C" __declspec(dllexport) plugin_info_t GPluginInfo = {
  .hide_from_peb = true,
  .erase_pe_header = true,
  .init = init,
  .oep_notify = oep_notify,
  .priority = 1
};