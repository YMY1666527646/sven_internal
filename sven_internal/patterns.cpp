// Patterns

#include "patterns.h"

namespace Patterns
{
	namespace Interfaces
	{
		PATTERN(EngineFuncs, "68 ? ? ? ? FF 15 ? ? ? ? A1 ? ? ? ? 83 C4 1C 85 C0 74 0A 68");
		PATTERN(ClientFuncs, "FF E0 68 ? ? ? ? FF 35 ? ? ? ? E8 ? ? ? ? 83 C4 08 A3");
		PATTERN(EngineStudio, "68 ? ? ? ? 68 ? ? ? ? 6A 01 FF D0 83 C4 0C 85 C0");
	}

	namespace Hardware
	{
		PATTERN(flNextCmdTime, "D9 1D ? ? ? ? 75 0A A1");
		PATTERN(Netchan_CanPacket, "D9 05 ? ? ? ? D9 EE DA E9 DF E0 F6 C4 44 8B 44 24 04");
		PATTERN(V_RenderView, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 D9 EE D9 15");
	}

	namespace Client
	{
		PATTERN(CVotePopup__MsgFunc_VoteMenu, "56 57 FF 74 24 10 8B F1 FF 74 24 18 E8 ? ? ? ? E8 ? ? ? ? 89 86 ? ? 00 00 E8");
		PATTERN(READ_BYTE, "8B 0D ? ? ? ? 8D 51 01 3B 15 ? ? ? ? 7E 0E C7 05 ? ? ? ? 01 00 00 00 83 C8 FF C3 A1 ? ? ? ? 89 15 ? ? ? ? 0F B6 04 08 C3");
		PATTERN(READ_STRING, "8B 15 ? ? ? ? 33 C0 53 55 8B 2D ? ? ? ? 56 57 8B 3D ? ? ? ? C6 05 ? ? ? ? 00 90");
		
		PATTERN(CHudBaseTextBlock__Print, "55 8B EC 6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 53 56 57 A1 ? ? ? ? 33 C5 50 8D 45 F4 64 A3 00 00 00 00 8B D9 8B 0D");

		PATTERN(CVoiceBanMgr__SaveState, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 8B 84 24 ? ? 00 00 53 57 FF 35 ? ? ? ? 8B F9 50 8D 44 24 14");
		PATTERN(CVoiceBanMgr__SetPlayerBan, "56 FF 74 24 08 8B F1 E8 ? ? ? ? 80 7C 24 0C 00 74 13 85 C0 75 32 FF 74 24 08 8B CE E8");
		PATTERN(CVoiceBanMgr__InternalFindPlayerSquelch, "53 55 8B 6C 24 0C 56 57 0F 10 4D 00 0F 28 C1 66 0F 73 D8 08 66 0F FC C8 0F 10 C1 66 0F 73 D8 04");

		PATTERN(CVoiceStatus__IsPlayerBlocked, "83 EC 14 A1 ? ? ? ? 33 C4 89 44 24 10 56 8D 44 24 04 8B F1 50 FF 74 24 20 FF 15");
		PATTERN(CVoiceStatus__SetPlayerBlockedState, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 53 68 ? ? ? ? 8B D9 FF 15 ? ? ? ? D9 5C 24 08");
		PATTERN(CVoiceStatus__UpdateServerState, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 53 8B D9 89 5C 24 08");

		PATTERN(HACK_GetPlayerUniqueID, "FF 74 24 08 FF 74 24 08 FF 15 ? ? ? ? 83 C4 08 85 C0 0F 95 C0 C3");

		PATTERN(WeaponsResource__GetFirstPos, "6B 54 24 04 68 56 57 33 F6 8B F9 81 C2 ? ? ? ? 8B 02 85 C0 74");
	}

	namespace GameOverlay
	{
		PATTERN(SetCursorPos_Hook, "55 8B EC 80 3D ? ? ? ? 00 74 19 8B 45 08 A3 ? ? ? ? 8B 45 0C A3 ? ? ? ? B8 01");
		PATTERN(ValveUnhookFunc, "55 8B EC C7");
		//PATTERN(ValveUnhookFunc, "55 8B ? 64 ? ? ? ? ? 6A ? 68 ? ? ? ? 50 64 ? ? ? ? ? ? 81 ? ? ? ? ? 56 8B ? ? 85");
	}
}