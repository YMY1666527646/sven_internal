// Game Utils

#include "utils.h"
#include "mathlib.h"

#include "../libdasm/libdasm.h"

#include "../utils/signature_scanner.h"
#include "../utils/patcher.h"
#include "../patterns.h"

//-----------------------------------------------------------------------------

extern playermove_s *g_pPlayerMove;

//-----------------------------------------------------------------------------

static float forwardmove, sidemove, upmove; //backup for fixmove
static Vector vViewForward, vViewRight, vViewUp, vAimForward, vAimRight, vAimUp; //backup for fixmove

static float *s_flNextCmdTime = NULL;
static double *s_dbGameSpeed = NULL;

double *dbRealtime = NULL;

MEMORY_PATCHER(GaussTertiaryAttack);
MEMORY_PATCHER(MinigunTertiaryAttack);
MEMORY_PATCHER(HandGrenadeTertiaryAttack);
MEMORY_PATCHER(ShockRifleTertiaryAttack);

MEMORY_PATCHER(GaussTertiaryAttack_Server);
MEMORY_PATCHER(MinigunTertiaryAttack_Server);
MEMORY_PATCHER(HandGrenadeTertiaryAttack_Server);
MEMORY_PATCHER(ShockRifleTertiaryAttack_Server);
MEMORY_PATCHER(GluonGunTertiaryAttack_Server);

static bool s_bTertiaryAttackGlitchInitialized = false;
static bool s_bTertiaryAttackGlitchInitialized_Server = false;

static BYTE s_TertiaryAttackPatchedBytes[] =
{
	0xC3, // RETURN
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 // NOP
};

//-----------------------------------------------------------------------------

void Msg(const char *pszMsg, ...)
{
	va_list args;

	va_start(args, pszMsg);
	g_pEngineFuncs->Con_Printf(pszMsg, args);
	va_end(args);
}

template<typename... Args>
void T_Msg(const char *pszMsg, Args... args)
{
	g_pEngineFuncs->Con_Printf(pszMsg, args...);
}

//-----------------------------------------------------------------------------

bool WorldToScreen(float *pflOrigin, float *pflVecScreen)
{
	int iResult = g_pEngineFuncs->pTriAPI->WorldToScreen(pflOrigin, pflVecScreen);

	if (pflVecScreen[0] <= 1 && pflVecScreen[1] <= 1 && pflVecScreen[0] >= -1 && pflVecScreen[1] >= -1 && !iResult)
	{
		SCREEN_STRUCT;

		pflVecScreen[0] = (SCREEN_WIDTH / 2 * pflVecScreen[0]) + (pflVecScreen[0] + SCREEN_WIDTH / 2);
		pflVecScreen[1] = -(SCREEN_HEIGHT / 2 * pflVecScreen[1]) + (pflVecScreen[1] + SCREEN_HEIGHT / 2);

		return true;
	}

	return false;
}

void ScreenToWorld(float *pflNDC, float *pflWorldOrigin)
{
	g_pEngineFuncs->pTriAPI->ScreenToWorld(pflNDC, pflWorldOrigin);
}

//-----------------------------------------------------------------------------

static void FixMoveStart(struct usercmd_s *cmd)
{
	forwardmove = cmd->forwardmove;
	sidemove = cmd->sidemove;
	upmove = cmd->upmove;

	if (g_pPlayerMove->iuser1 == 0)
		g_pEngineFuncs->pfnAngleVectors(Vector(0.0f, cmd->viewangles.y, 0.0f), vViewForward, vViewRight, vViewUp);
	else
		g_pEngineFuncs->pfnAngleVectors(cmd->viewangles, vViewForward, vViewRight, vViewUp);
}

static void FixMoveEnd(struct usercmd_s *cmd)
{
	NormalizeAngles(cmd->viewangles);

	if (g_pPlayerMove->iuser1 == 0)
		g_pEngineFuncs->pfnAngleVectors(Vector(0.0f, cmd->viewangles.y, 0.0f), vAimForward, vAimRight, vAimUp);
	else
		g_pEngineFuncs->pfnAngleVectors(cmd->viewangles, vAimForward, vAimRight, vAimUp);

	Vector forwardmove_normalized = forwardmove * vViewForward;
	Vector sidemove_normalized = sidemove * vViewRight;
	Vector upmove_normalized = upmove * vViewUp;

	cmd->forwardmove = DotProduct(forwardmove_normalized, vAimForward) + DotProduct(sidemove_normalized, vAimForward) + DotProduct(upmove_normalized, vAimForward);
	cmd->sidemove = DotProduct(forwardmove_normalized, vAimRight) + DotProduct(sidemove_normalized, vAimRight) + DotProduct(upmove_normalized, vAimRight);
	cmd->upmove = DotProduct(forwardmove_normalized, vAimUp) + DotProduct(sidemove_normalized, vAimUp) + DotProduct(upmove_normalized, vAimUp);

	Vector vMove(cmd->forwardmove, cmd->sidemove, cmd->upmove);
	float flSpeed = sqrtf(vMove.x * vMove.x + vMove.y * vMove.y), flYaw;
	Vector vecMove, vecRealView(cmd->viewangles);
	VectorAngles(vMove, vecMove);
	flYaw = (cmd->viewangles.y - vecRealView.y + vecMove.y) * static_cast<float>(M_PI) / 180.0f;

	cmd->forwardmove = cosf(flYaw) * flSpeed;

	if (cmd->viewangles.x >= 90.f || cmd->viewangles.x <= -90.f)
		cmd->forwardmove *= -1;

	cmd->sidemove = sinf(flYaw) * flSpeed;
}

void SetAnglesSilent(float *angles, struct usercmd_s *cmd)
{
	FixMoveStart(cmd);

	cmd->viewangles[0] = angles[0];
	cmd->viewangles[1] = angles[1];
	cmd->viewangles[2] = angles[2];

	FixMoveEnd(cmd);
}

//-----------------------------------------------------------------------------

void SetGameSpeed(double dbSpeed)
{
	*s_dbGameSpeed = dbSpeed * 1000.0;
}

void SendPacket(bool bSend)
{
	if (bSend)
		*s_flNextCmdTime = 0.0f;
	else
		*s_flNextCmdTime = FLT_MAX;
}

void EnableTertiaryAttackGlitch()
{
	GaussTertiaryAttack.Patch();
	MinigunTertiaryAttack.Patch();
	HandGrenadeTertiaryAttack.Patch();
	ShockRifleTertiaryAttack.Patch();
}

void DisableTertiaryAttackGlitch()
{
	GaussTertiaryAttack.Unpatch();
	MinigunTertiaryAttack.Unpatch();
	HandGrenadeTertiaryAttack.Unpatch();
	ShockRifleTertiaryAttack.Unpatch();
}

bool IsTertiaryAttackGlitchPatched()
{
	return GaussTertiaryAttack.IsPatched();
}

bool IsTertiaryAttackGlitchInit()
{
	return s_bTertiaryAttackGlitchInitialized;
}

void EnableTertiaryAttackGlitch_Server() // what's the point of loading 'server' library when you create a server????
{
	GaussTertiaryAttack_Server.Patch();
	MinigunTertiaryAttack_Server.Patch();
	HandGrenadeTertiaryAttack_Server.Patch();
	ShockRifleTertiaryAttack_Server.Patch();
	GluonGunTertiaryAttack_Server.Patch();
}

void DisableTertiaryAttackGlitch_Server()
{
	GaussTertiaryAttack_Server.Unpatch();
	MinigunTertiaryAttack_Server.Unpatch();
	HandGrenadeTertiaryAttack_Server.Unpatch();
	ShockRifleTertiaryAttack_Server.Unpatch();
	GluonGunTertiaryAttack_Server.Unpatch();
}

bool IsTertiaryAttackGlitchPatched_Server()
{
	return GaussTertiaryAttack_Server.IsPatched();
}

bool IsTertiaryAttackGlitchInit_Server()
{
	return s_bTertiaryAttackGlitchInitialized_Server;
}

//-----------------------------------------------------------------------------

static bool PatchInterp()
{
	DWORD dwProtection;

	void *pPatchInterpString = LookupForString(L"hw.dll", "cl_updaterate min");

	if (!pPatchInterpString)
	{
		//ThrowError("'Patch Interp' failed initialization\n");
		M_Msg("'Patch Interp' failed initialization\n");
		return false;
	}

	BYTE *pPatchInterp = (BYTE *)FindAddress(L"hw.dll", pPatchInterpString);

	if (!pPatchInterp)
	{
		//ThrowError("'Patch Interp' failed initialization #2\n");
		M_Msg("'Patch Interp' failed initialization #2\n");
		return false;
	}

	// go back to PUSH opcode
	pPatchInterp -= 0x1;

	if (*pPatchInterp != 0x68) // check PUSH opcode
	{
		//ThrowError("'Patch Interp' failed initialization #3\n");
		M_Msg("'Patch Interp' failed initialization #3\n");
		return false;
	}

	if (*(pPatchInterp - 0x1F) != 0x7A) // JP opcode
	{
		//ThrowError("'Patch Interp' failed initialization #4\n");
		M_Msg("'Patch Interp' failed initialization #4\n");
		return false;
	}

	// Patch cl_updaterate min.
	VirtualProtect(pPatchInterp - 0x1F, sizeof(BYTE), PAGE_EXECUTE_READWRITE, &dwProtection);
	*(pPatchInterp - 0x1F) = 0xEB;
	//FlushInstructionCache(GetCurrentProcess(), pPatchInterp - 0x1F, sizeof(BYTE));
	VirtualProtect(pPatchInterp - 0x1F, sizeof(BYTE), dwProtection, &dwProtection);
	
	// Go to PUSH string 'cl_updaterate max...'
	pPatchInterp += 0x3F;

	if (*pPatchInterp != 0x68) // check PUSH opcode
	{
		//ThrowError("'Patch Interp' failed initialization #5\n");
		M_Msg("'Patch Interp' failed initialization #5\n");
		return false;
	}

	if (*(pPatchInterp - 0x1F) != 0x7A) // JP opcode
	{
		//ThrowError("'Patch Interp' failed initialization #6\n");
		M_Msg("'Patch Interp' failed initialization #6\n");
		return false;
	}

	// Patch cl_updaterate max.
	VirtualProtect(pPatchInterp - 0x1F, sizeof(BYTE), PAGE_EXECUTE_READWRITE, &dwProtection);
	*(pPatchInterp - 0x1F) = 0xEB;
	VirtualProtect(pPatchInterp - 0x1F, sizeof(BYTE), dwProtection, &dwProtection);
	
	// Go to PUSH string 'ex_interp forced up...'
	pPatchInterp += 0x62;

	if (*pPatchInterp != 0xB8) // check MOV, EAX ... opcode
	{
		//ThrowError("'Patch Interp' failed initialization #7\n");
		M_Msg("'Patch Interp' failed initialization #7\n");
		return false;
	}

	if (*(pPatchInterp - 0x8) != 0x7D) // JNL opcode
	{
		//ThrowError("'Patch Interp' failed initialization #8\n");
		M_Msg("'Patch Interp' failed initialization #8\n");
		return false;
	}

	// Patch ex_interp force up
	VirtualProtect(pPatchInterp - 0x8, sizeof(BYTE), PAGE_EXECUTE_READWRITE, &dwProtection);
	*(pPatchInterp - 0x8) = 0xEB;
	VirtualProtect(pPatchInterp - 0x8, sizeof(BYTE), dwProtection, &dwProtection);

	if (*(pPatchInterp + 0xD) != 0x7E) // JLE opcode
	{
		//ThrowError("'Patch Interp' failed initialization #9\n");
		M_Msg("'Patch Interp' failed initialization #9\n");
		return false;
	}

	// Patch ex_interp force down
	VirtualProtect(pPatchInterp + 0xD, sizeof(BYTE), PAGE_EXECUTE_READWRITE, &dwProtection);
	*(pPatchInterp + 0xD) = 0xEB;
	VirtualProtect(pPatchInterp + 0xD, sizeof(BYTE), dwProtection, &dwProtection);

	return true;
}

static void InitTertiaryAttackPatch(CPatcher &patcher, void *pfnTertiaryAttack)
{
	INSTRUCTION instruction;

	get_instruction(&instruction, (BYTE *)pfnTertiaryAttack, MODE_32);

	if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_REGISTER && instruction.op2.type == OPERAND_TYPE_MEMORY)
	{
		get_instruction(&instruction, (BYTE *)pfnTertiaryAttack + 0x2, MODE_32);

		if (instruction.type == INSTRUCTION_TYPE_JMP && instruction.op1.type == OPERAND_TYPE_MEMORY)
		{
			patcher.Init(pfnTertiaryAttack, s_TertiaryAttackPatchedBytes, sizeof(s_TertiaryAttackPatchedBytes));
		}
		else
		{
			ThrowError("'InitTertiaryAttackPatch' failed initialization #2\n");
		}
	}
	else
	{
		ThrowError("'InitTertiaryAttackPatch' failed initialization #1\n");
	}
}

static void InitTertiaryAttackGlitch()
{
	INSTRUCTION instruction;
	DWORD *dwVTable[] = { NULL, NULL, NULL, NULL };

	auto clientDLL = GetModuleHandle(L"client.dll");

	if (clientDLL)
	{
		void *weapon_gauss = GetProcAddress(clientDLL, "weapon_gauss"); // vtable <- (byte *)weapon_gauss + 0x63
		void *weapon_minigun = GetProcAddress(clientDLL, "weapon_minigun"); // vtable <- (byte *)weapon_minigun + 0x63
		void *weapon_handgrenade = GetProcAddress(clientDLL, "weapon_handgrenade"); // vtable <- (byte *)weapon_handgrenade + 0x63
		void *weapon_shockrifle = GetProcAddress(clientDLL, "weapon_shockrifle"); // vtable <- (byte *)weapon_shockrifle + 0x67

		if (!weapon_gauss || !weapon_minigun || !weapon_handgrenade || !weapon_shockrifle)
			ThrowError("InitTertiaryAttackGlitch: GetProcAddress failed\n");

		// weapon_gauss
		get_instruction(&instruction, (BYTE *)weapon_gauss + 0x63, MODE_32);

		if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_MEMORY && instruction.op2.type == OPERAND_TYPE_IMMEDIATE)
			dwVTable[0] = reinterpret_cast<DWORD *>(instruction.op2.immediate);
		else
			ThrowError("'InitTertiaryAttackGlitch' failed initialization #1\n");
		
		// weapon_minigun
		get_instruction(&instruction, (BYTE *)weapon_minigun + 0x63, MODE_32);

		if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_MEMORY && instruction.op2.type == OPERAND_TYPE_IMMEDIATE)
			dwVTable[1] = reinterpret_cast<DWORD *>(instruction.op2.immediate);
		else
			ThrowError("'InitTertiaryAttackGlitch' failed initialization #2\n");
		
		// weapon_handgrenade
		get_instruction(&instruction, (BYTE *)weapon_handgrenade + 0x63, MODE_32);

		if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_MEMORY && instruction.op2.type == OPERAND_TYPE_IMMEDIATE)
			dwVTable[2] = reinterpret_cast<DWORD *>(instruction.op2.immediate);
		else
			ThrowError("'InitTertiaryAttackGlitch' failed initialization #3\n");
		
		// weapon_shockrifle
		get_instruction(&instruction, (BYTE *)weapon_shockrifle + 0x67, MODE_32);

		if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_MEMORY && instruction.op2.type == OPERAND_TYPE_IMMEDIATE)
			dwVTable[3] = reinterpret_cast<DWORD *>(instruction.op2.immediate);
		else
			ThrowError("'InitTertiaryAttackGlitch' failed initialization #4\n");

		InitTertiaryAttackPatch(GaussTertiaryAttack, (void *)dwVTable[0][150]); // CBasePlayerWeapon::TertiaryAttack
		InitTertiaryAttackPatch(MinigunTertiaryAttack, (void *)dwVTable[1][150]);
		InitTertiaryAttackPatch(HandGrenadeTertiaryAttack, (void *)dwVTable[2][150]);
		InitTertiaryAttackPatch(ShockRifleTertiaryAttack, (void *)dwVTable[3][150]);

		s_bTertiaryAttackGlitchInitialized = true;
	}
	else
	{
		ThrowError("InitTertiaryAttackGlitch: client module??\n");
	}
}

void InitTertiaryAttackGlitch_Server()
{
	INSTRUCTION instruction;
	DWORD *dwVTable[] = { NULL, NULL, NULL, NULL, NULL };

	auto serverDLL = GetModuleHandle(L"server.dll");

	if (serverDLL)
	{
		void *weapon_gauss = GetProcAddress(serverDLL, "weapon_gauss"); // vtable <- (byte *)weapon_gauss + 0x7B
		void *weapon_minigun = GetProcAddress(serverDLL, "weapon_minigun"); // vtable <- (byte *)weapon_minigun + 0x7B
		void *weapon_handgrenade = GetProcAddress(serverDLL, "weapon_handgrenade"); // vtable <- (byte *)weapon_handgrenade + 0x7B
		void *weapon_shockrifle = GetProcAddress(serverDLL, "weapon_shockrifle"); // vtable <- (byte *)weapon_shockrifle + 0x83
		void *weapon_egon = GetProcAddress(serverDLL, "weapon_egon"); // vtable <- (byte *)weapon_egon + 0x83

		if (!weapon_gauss || !weapon_minigun || !weapon_handgrenade || !weapon_shockrifle || !weapon_egon)
			ThrowError("InitTertiaryAttackGlitch_Server: GetProcAddress failed\n");

		// weapon_gauss
		get_instruction(&instruction, (BYTE *)weapon_gauss + 0x7B, MODE_32);

		if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_MEMORY && instruction.op2.type == OPERAND_TYPE_IMMEDIATE)
			dwVTable[0] = reinterpret_cast<DWORD *>(instruction.op2.immediate);
		else
			ThrowError("'InitTertiaryAttackGlitch_Server' failed initialization #1\n");

		// weapon_minigun
		get_instruction(&instruction, (BYTE *)weapon_minigun + 0x7B, MODE_32);

		if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_MEMORY && instruction.op2.type == OPERAND_TYPE_IMMEDIATE)
			dwVTable[1] = reinterpret_cast<DWORD *>(instruction.op2.immediate);
		else
			ThrowError("'InitTertiaryAttackGlitch_Server' failed initialization #2\n");

		// weapon_handgrenade
		get_instruction(&instruction, (BYTE *)weapon_handgrenade + 0x7B, MODE_32);

		if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_MEMORY && instruction.op2.type == OPERAND_TYPE_IMMEDIATE)
			dwVTable[2] = reinterpret_cast<DWORD *>(instruction.op2.immediate);
		else
			ThrowError("'InitTertiaryAttackGlitch_Server' failed initialization #3\n");

		// weapon_shockrifle
		get_instruction(&instruction, (BYTE *)weapon_shockrifle + 0x83, MODE_32);

		if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_MEMORY && instruction.op2.type == OPERAND_TYPE_IMMEDIATE)
			dwVTable[3] = reinterpret_cast<DWORD *>(instruction.op2.immediate);
		else
			ThrowError("'InitTertiaryAttackGlitch_Server' failed initialization #4\n");
		
		// weapon_egon
		get_instruction(&instruction, (BYTE *)weapon_egon + 0x83, MODE_32);

		if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_MEMORY && instruction.op2.type == OPERAND_TYPE_IMMEDIATE)
			dwVTable[4] = reinterpret_cast<DWORD *>(instruction.op2.immediate);
		else
			ThrowError("'InitTertiaryAttackGlitch_Server' failed initialization #5\n");

		InitTertiaryAttackPatch(GaussTertiaryAttack_Server, (void *)dwVTable[0][151]);
		InitTertiaryAttackPatch(MinigunTertiaryAttack_Server, (void *)dwVTable[1][151]);
		InitTertiaryAttackPatch(HandGrenadeTertiaryAttack_Server, (void *)dwVTable[2][151]);
		InitTertiaryAttackPatch(ShockRifleTertiaryAttack_Server, (void *)dwVTable[3][151]);
		InitTertiaryAttackPatch(GluonGunTertiaryAttack_Server, (void *)dwVTable[4][151]);

		s_bTertiaryAttackGlitchInitialized_Server = true;
	}
	else
	{
		ThrowError("InitTertiaryAttackGlitch: server module??\n");
	}
}

// main func

void InitUtils()
{
	DWORD dwProtection;
	INSTRUCTION instruction;

	if (!PatchInterp())
	{
		M_Msg("PATCH FAILURE\n");
		//return;
	}

	InitTertiaryAttackGlitch();

	void *pNextCmdTime = FIND_PATTERN(L"hw.dll", Patterns::Hardware::flNextCmdTime);

	if (!pNextCmdTime)
	{
		ThrowError("'flNextCmdTime' failed initialization\n");
		return;
	}

	void *pTextureLoadAddress = LookupForString(L"hw.dll", "Texture load: %6.1fms");

	if (!pTextureLoadAddress)
	{
		ThrowError("'Game speed up' failed initialization\n");
		return;
	}

	void *pTextureLoad = FindAddress(L"hw.dll", pTextureLoadAddress);

	if (!pTextureLoad)
	{
		ThrowError("'Game speed up' failed initialization #2\n");
		return;
	}

	// s_dbGameSpeed
	get_instruction(&instruction, (BYTE *)pTextureLoad - 0xA, MODE_32);

	if (instruction.type == INSTRUCTION_TYPE_FMUL && instruction.op1.type == OPERAND_TYPE_MEMORY)
	{
		s_dbGameSpeed = reinterpret_cast<double *>(instruction.op1.displacement);
	}
	else
	{
		ThrowError("'Game speed up' failed initialization #3\n");
		return;
	}

	// s_flNextCmdTime
	get_instruction(&instruction, (BYTE *)pNextCmdTime, MODE_32);

	if (instruction.type == INSTRUCTION_TYPE_FSTP && instruction.op1.type == OPERAND_TYPE_MEMORY)
	{
		s_flNextCmdTime = reinterpret_cast<float *>(instruction.op1.displacement);
	}
	else
	{
		ThrowError("'flNextCmdTime' failed initialization #2\n");
		return;
	}
	
	// dbRealtime
	get_instruction(&instruction, (BYTE *)g_pEngineFuncs->pNetAPI->SendRequest + 0x88, MODE_32);

	if (instruction.type == INSTRUCTION_TYPE_FLD && instruction.op1.type == OPERAND_TYPE_MEMORY)
	{
		dbRealtime = reinterpret_cast<double *>(instruction.op1.displacement);
	}
	else
	{
		ThrowError("'dbRealtime' failed initialization\n");
		return;
	}

	VirtualProtect(s_dbGameSpeed, sizeof(double), PAGE_EXECUTE_READWRITE, &dwProtection);
}