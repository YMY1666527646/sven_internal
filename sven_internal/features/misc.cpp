// Misc

#include "misc.h"

#include <algorithm>
#include <cmath>

#include "../sdk.h"
#include "../interfaces.h"

#include "../modules/client.h"

#include "../game/utils.h"
#include "../game/console.h"
#include "../game/ammo.h"
#include "../config.h"

#include "../utils/trampoline_hook.h"

//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------

typedef BOOL (WINAPI *QueryPerformanceCounterFn)(LARGE_INTEGER *);

//-----------------------------------------------------------------------------
// Imports
//-----------------------------------------------------------------------------

extern playermove_s *g_pPlayerMove;
extern bool bSendPacket;

//-----------------------------------------------------------------------------
// Init Hooks
//-----------------------------------------------------------------------------

TRAMPOLINE_HOOK(QueryPerformanceCounter_Hook);

//-----------------------------------------------------------------------------
// Vars
//-----------------------------------------------------------------------------

CMisc g_Misc;

QueryPerformanceCounterFn QueryPerformanceCounter_Original = NULL;

int g_iWeaponID = -1;
int g_iChokeCommands = 0;

cvar_s *ex_interp = NULL;

static int s_nSinkState = 0;
static bool s_bFreeze = false;

static float s_flTopColorDelay = 0.0f;
static float s_flBottomColorDelay = 0.0f;

static int s_iTopColorOffset = 0;
static int s_iBottomColorOffset = 0;

static float s_flWeaponOffset[32] =
{
	0.0f, // 0
	0.0f, // 1
	-4.0f, // 2
	0.0f, // 3
	-2.5f, // 4
	0.0f, // 5
	-4.0f, // 6
	-5.0f, // 7
	-9.2f, // 8
	-3.0f, // 9
	0.0f, // 10
	-8.0f, // 11
	-4.0f, // 12
	0.0f, // 13
	0.0f, // 14
	0.0f, // 15
	0.0f, // 16
	-4.15f, // 17
	0.0f, // 18
	0.0f, // 19
	0.0f, // 20
	0.0f, // 21
	0.0f, // 22
	-7.3f, // 23
	-4.25f, // 24
	0.0f, // 25
	-2.0f, // 26
	-2.7f, // 27
	0.0f, // 28
	-7.5f, // 29
	0.0f, // 30
	0.0f, // 31
};

//-----------------------------------------------------------------------------
// Hooks
//-----------------------------------------------------------------------------

BOOL WINAPI QueryPerformanceCounter_Hooked(LARGE_INTEGER *lpPerformanceCount)
{
	static LONGLONG oldfakevalue = 0;
	static LONGLONG oldrealvalue = 0;

	LONGLONG newvalue;

	if (oldfakevalue == 0 || oldrealvalue == 0)
	{
		oldfakevalue = lpPerformanceCount->QuadPart;
		oldrealvalue = lpPerformanceCount->QuadPart;
	}

	BOOL result = QueryPerformanceCounter_Original(lpPerformanceCount);

	newvalue = lpPerformanceCount->QuadPart;
	newvalue = oldfakevalue + (LONGLONG)((newvalue - oldrealvalue) * static_cast<double>(g_Config.cvars.app_speed));

	oldrealvalue = lpPerformanceCount->QuadPart;
	oldfakevalue = newvalue;

	lpPerformanceCount->QuadPart = newvalue;

	return result;
}

//-----------------------------------------------------------------------------
// Common Functions
//-----------------------------------------------------------------------------

static float GetWeaponOffset(cl_entity_s *pViewModel, int iWeaponID)
{
	// ToDo: use class_table

	if (iWeaponID == 0)
	{
		const char *pszModelName = pViewModel->model->name;

		if (pszModelName && *pszModelName)
		{
			const char *pszModelNameEnd = pszModelName + strlen(pszModelName);

			while (pszModelNameEnd > pszModelName)
			{
				if (*(pszModelNameEnd - 1) == '/')
				{
					pszModelName = pszModelNameEnd;
					break;
				}

				--pszModelNameEnd;
			}

			if (!strcmp(pszModelName, "v_crowbar.mdl"))
			{
				return -1.5f;
			}
			else if (!strcmp(pszModelName, "v_pipe_wrench.mdl"))
			{
				return -2.0f;
			}
			else if (!strcmp(pszModelName, "v_medkit.mdl"))
			{
				return -0.7f;
			}
			else if (!strcmp(pszModelName, "v_bgrap.mdl"))
			{
				return -9.0f;
			}
			else if (!strcmp(pszModelName, "v_357.mdl"))
			{
				return -6.2f;
			}
			else if (!strcmp(pszModelName, "v_egon.mdl"))
			{
				return -5.0f;
			}
			else if (!strcmp(pszModelName, "v_squeak.mdl"))
			{
				return -3.0f;
			}
		}
	}
	else
	{
		return s_flWeaponOffset[iWeaponID];
	}

	return 0.0f;
}

static void ConCommand_DropEmptyWeapons_Iterator(WEAPON *pWeapon, bool bHasAmmo, int nAmmo, int nAmmo2)
{
#ifdef _DEBUG
	g_pEngineFuncs->Con_Printf("===============================\n");
	g_pEngineFuncs->Con_Printf("Name: %s\n", pWeapon->szName);
	g_pEngineFuncs->Con_Printf("Has Ammo: %d\n", bHasAmmo);
	g_pEngineFuncs->Con_Printf("iAmmoType (Ammo: %d): %d\n", nAmmo, pWeapon->iAmmoType);
	g_pEngineFuncs->Con_Printf("iAmmo2Type (Ammo: %d): %d\n", nAmmo2, pWeapon->iAmmo2Type);
	g_pEngineFuncs->Con_Printf("iMax1: %d\n", pWeapon->iMax1);
	g_pEngineFuncs->Con_Printf("iMax2: %d\n", pWeapon->iMax2);
	g_pEngineFuncs->Con_Printf("iSlot: %d\n", pWeapon->iSlot);
	g_pEngineFuncs->Con_Printf("iSlotPos: %d\n", pWeapon->iSlotPos);
	g_pEngineFuncs->Con_Printf("iFlags: %d\n", pWeapon->iFlags);
	g_pEngineFuncs->Con_Printf("iId: %d\n", pWeapon->iId);
	g_pEngineFuncs->Con_Printf("iClip: %d\n", pWeapon->iClip);
	g_pEngineFuncs->Con_Printf("iCount: %d\n", pWeapon->iCount);
	g_pEngineFuncs->Con_Printf("===============================\n\n");
#endif

	static char command_buffer[140] = { 0 };

	if (!bHasAmmo)
	{
		sprintf_s(command_buffer, sizeof(command_buffer), "drop %s", pWeapon->szName);
		g_pEngineFuncs->pfnClientCmd(command_buffer);
	}
}

//-----------------------------------------------------------------------------
// Commands, CVars..
//-----------------------------------------------------------------------------

CON_COMMAND_FUNC(sc_autojump, ConCommand_AutoJump, "sc_autojump - Toggle autojump")
{
	g_pEngineFuncs->Con_Printf(g_Config.cvars.autojump ? "Auto Jump disabled\n" : "Auto Jump enabled\n");
	g_Config.cvars.autojump = !g_Config.cvars.autojump;
}

CON_COMMAND_FUNC(sc_doubleduck, ConCommand_DoubleDuck, "sc_doubleduck - Toggle doubleduck")
{
	g_pEngineFuncs->Con_Printf(g_Config.cvars.doubleduck ? "Double Duck disabled\n" : "Double Duck enabled\n");
	g_Config.cvars.doubleduck = !g_Config.cvars.doubleduck;
}

CON_COMMAND_FUNC(sc_jumpbug, ConCommand_JumpBug, "sc_jumpbug - Toggle jumpbug")
{
	g_pEngineFuncs->Con_Printf(g_Config.cvars.jumpbug ? "Jump Bug disabled\n" : "Jump Bug enabled\n");
	g_Config.cvars.jumpbug = !g_Config.cvars.jumpbug;
}

CON_COMMAND_FUNC(sc_fakelag, ConCommand_FakeLag, "sc_fakelag - Toggle fake lag")
{
	g_pEngineFuncs->Con_Printf(g_Config.cvars.fakelag ? "Fake Lag disabled\n" : "Fake Lag enabled\n");
	g_Config.cvars.fakelag = !g_Config.cvars.fakelag;
}

CON_COMMAND_FUNC(sc_selfsink, ConCommand_AutoSelfSink, "sc_selfsink - Perform self sink")
{
	if (g_pPlayerMove->view_ofs.z == 12.0f)
	{
		g_pEngineFuncs->SetViewAngles(Vector(-0.1f, -90.0f, 0.0f));
		g_pEngineFuncs->pfnClientCmd("+jump");

		s_nSinkState = 1;
	}
	else
	{
		g_pEngineFuncs->pfnClientCmd("+duck");

		s_nSinkState = 2;
	}
}

CON_COMMAND_FUNC(sc_speedhack, ConCommand_SpeedHack, "sc_speedhack [value] - Set speedhack value")
{
	if (CMD_ARGC() >= 2)
	{
		float flSpeed = strtof(CMD_ARGV(1), NULL);

		g_Config.cvars.speedhack = flSpeed;
	}
}

CON_COMMAND_FUNC(sc_reset_colors, ConCommand_ResetColors, "sc_reset_colors - Reset colors in Color Pulsator")
{
	s_iBottomColorOffset = s_iTopColorOffset;
}

CON_COMMAND_FUNC(sc_sync_colors, ConCommand_SyncColors, "sc_sync_colors - Sync. change time for top and bottom colors in Color Pulsator")
{
	if (s_flTopColorDelay > s_flBottomColorDelay)
		s_flBottomColorDelay = s_flTopColorDelay;

	if (s_flBottomColorDelay > s_flTopColorDelay)
		s_flTopColorDelay = s_flBottomColorDelay;
}

CON_COMMAND_FUNC(drop_empty_weapons, ConCommand_DropEmptyWeapons, "drop_empty_weapons - Drop all empty weapons from your inventory")
{
	g_pWR->IterateWeapons(ConCommand_DropEmptyWeapons_Iterator);
}

CON_COMMAND_FUNC(freeze, ConCommand_Freeze, "freeze - Block connection with a server")
{
	g_pEngineFuncs->Con_Printf(s_bFreeze ? "Connection restored\n" : "Connection blocked\n");
	s_bFreeze = !s_bFreeze;
}

//-----------------------------------------------------------------------------
// Callbacks
//-----------------------------------------------------------------------------

void CMisc::CreateMove(float frametime, struct usercmd_s *cmd, int active)
{
	SetGameSpeed(static_cast<double>(g_Config.cvars.speedhack));
	*dbRealtime += static_cast<double>(g_Config.cvars.ltfxspeed) * frametime;

	FakeLag(frametime);
	ColorPulsator();
	TertiaryAttackGlitch();

	if (g_pPlayerMove->iuser1 < 1)
	{
		AutoSelfSink();
		AutoJump(cmd);
		DoubleDuck(cmd);
		Helicopter(cmd);
		FastRun(cmd);
		JumpBug(frametime, cmd);
	}

	if (s_bFreeze)
		bSendPacket = false;
}

void CMisc::V_CalcRefdef(struct ref_params_s *pparams)
{
	QuakeGuns_V_CalcRefdef();
}

void CMisc::HUD_PostRunCmd(struct local_state_s *from, struct local_state_s *to, struct usercmd_s *cmd, int runfuncs, double time, unsigned int random_seed)
{
	QuakeGuns_HUD_PostRunCmd(to);
	NoWeaponAnim_HUD_PostRunCmd(to);
}

//-----------------------------------------------------------------------------
// Auto Jump
//-----------------------------------------------------------------------------

void CMisc::AutoJump(struct usercmd_s *cmd)
{
	static bool bAllowJump = false;

	if (g_Config.cvars.autojump && cmd->buttons & IN_JUMP)
	{
		if (bAllowJump && GetAsyncKeyState(VK_SPACE))
		{
			cmd->buttons &= ~IN_JUMP;
			bAllowJump = false;
		}
		else
		{
			bAllowJump = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Auto Jumpbug
//-----------------------------------------------------------------------------

void CMisc::JumpBug(float frametime, struct usercmd_s *cmd)
{
	static int nJumpBugState = 0;

	if (g_Config.cvars.jumpbug && g_Local.flGroundNormalAngle <= acosf(0.7f) * 180.0f / static_cast<float>(M_PI) && g_pPlayerMove->flFallVelocity >= 500.0f)
	{
		float flPlayerHeight = g_Local.flHeight; // = 0.0f
		float flFrameZDist = fabsf((g_pPlayerMove->flFallVelocity + (800.0f * frametime)) * frametime);

		//if (g_Local.flGroundNormalAngle > 1.0f)
		//{
			//bool bDuck = g_pPlayerMove->flags & FL_DUCKING;
			//Vector vBottomOrigin = g_pPlayerMove->origin; vBottomOrigin.z -= 8192.0f;

			//pmtrace_t *pTrace = g_pEngineFuncs->PM_TraceLine(g_pPlayerMove->origin, vBottomOrigin, PM_NORMAL, bDuck ? 1 : 0 /* g_pPlayerMove->usehull */, -1);
			//flPlayerHeight = fabsf(g_pPlayerMove->origin.z - pTrace->endpos.z + (bDuck ? 18.0f : 36.0f));
		//}
		//else
		//{
			//flPlayerHeight = g_Local.flHeight;
		//}

		cmd->buttons |= IN_DUCK;
		cmd->buttons &= ~IN_JUMP;

		switch (nJumpBugState)
		{
		case 1:
			cmd->buttons &= ~IN_DUCK;
			cmd->buttons |= IN_JUMP;

			nJumpBugState = 2;
			break;

		case 2:
			nJumpBugState = 0;
			break;

		default:
			if (fabsf(flPlayerHeight - flFrameZDist * 1.5f) <= (20.0f) && flFrameZDist > 0.0f)
			{
				float flNeedSpeed = fabsf(flPlayerHeight - 19.f);
				float flScale = fabsf(flNeedSpeed / flFrameZDist);

				SetGameSpeed(flScale);

				nJumpBugState = 1;
			}
			break;
		}
	}
	else
	{
		nJumpBugState = 0;
	}
}

//-----------------------------------------------------------------------------
// Auto Doubleduck
//-----------------------------------------------------------------------------

void CMisc::DoubleDuck(struct usercmd_s *cmd)
{
	if (g_Config.cvars.doubleduck && GetAsyncKeyState(VK_LCONTROL))
	{
		static bool bForceUnduck = false;

		if (bForceUnduck)
		{
			cmd->buttons &= ~IN_DUCK;

			bForceUnduck = false;
		}
		else if (g_pPlayerMove->onground != -1)
		{
			cmd->buttons |= IN_DUCK;

			bForceUnduck = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Fast Run
//-----------------------------------------------------------------------------

void CMisc::FastRun(struct usercmd_s *cmd)
{
	if (g_Config.cvars.fastrun && g_pPlayerMove->onground != -1)
	{
		static bool bFastRun = false;
		float flMaxSpeed = g_pPlayerMove->clientmaxspeed;

		if ((cmd->buttons & IN_FORWARD && cmd->buttons & IN_MOVELEFT) || (cmd->buttons & IN_BACK && cmd->buttons & IN_MOVERIGHT))
		{
			if (bFastRun)
			{
				cmd->sidemove -= flMaxSpeed; // sqrtf(2.0f) * flMaxSpeed   vvv
				cmd->forwardmove -= flMaxSpeed;

				bFastRun = false;
			}
			else
			{
				cmd->sidemove += flMaxSpeed;
				cmd->forwardmove += flMaxSpeed;

				bFastRun = true;
			}
		}
		else if ((cmd->buttons & IN_FORWARD && cmd->buttons & IN_MOVERIGHT) || (cmd->buttons & IN_BACK && cmd->buttons & IN_MOVELEFT))
		{
			if (bFastRun)
			{
				cmd->sidemove -= flMaxSpeed;
				cmd->forwardmove += flMaxSpeed;

				bFastRun = false;
			}
			else
			{
				cmd->sidemove += flMaxSpeed;
				cmd->forwardmove -= flMaxSpeed; // sqrtf(2.0f) * flMaxSpeed  ^^^

				bFastRun = true;
			}
		}
		else if (cmd->buttons & IN_FORWARD || cmd->buttons & IN_BACK)
		{
			if (bFastRun)
			{
				cmd->sidemove -= flMaxSpeed;

				bFastRun = false;
			}
			else
			{
				cmd->sidemove += flMaxSpeed;
				bFastRun = true;
			}
		}
		else if (cmd->buttons & IN_MOVELEFT || cmd->buttons & IN_MOVERIGHT)
		{
			if (bFastRun)
			{
				cmd->forwardmove -= flMaxSpeed;
				bFastRun = false;
			}
			else
			{
				cmd->forwardmove += flMaxSpeed;
				bFastRun = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Helicopter
//-----------------------------------------------------------------------------

void CMisc::Helicopter(struct usercmd_s *cmd)
{
	static Vector vHeliAngles(0.f, 0.f, 0.f);

	if (g_Config.cvars.helicopter)
	{
		vHeliAngles.x = g_Config.cvars.helicopter_pitch_angle; // not seen for client
		vHeliAngles.y += g_Config.cvars.helicopter_rotation_angle;

		if (vHeliAngles.y > 180.f)
			vHeliAngles.y -= 360.f;
		
		if (vHeliAngles.y < -180.f)
			vHeliAngles.y += 360.f;

		SetAnglesSilent(vHeliAngles, cmd);
	}
}

//-----------------------------------------------------------------------------
// Fake Lag
//-----------------------------------------------------------------------------

void CMisc::FakeLag(float frametime)
{
	static bool bSetInterpOnce = false;

	if (g_Config.cvars.fakelag_adaptive_ex_interp)
	{
		if (ex_interp->value != 0.01f)
			ex_interp->value = 0.01f;

		bSetInterpOnce = true;
	}
	else if (bSetInterpOnce)
	{
		if (ex_interp->value == 0.01f)
			ex_interp->value = 0.1f;

		bSetInterpOnce = false;
	}

	if (g_Config.cvars.fakelag)
	{
		bool bFakeLag = true;

		if (g_Config.cvars.fakelag_move != 0)
		{
			if (g_Config.cvars.fakelag_move == 1) // On land
			{
				if (g_Local.flVelocity > 0.f)
					bFakeLag = false;
			}
			else if (g_Config.cvars.fakelag_move == 2) // On move
			{
				if (g_Local.flVelocity == 0.f)
					bFakeLag = false;
			}
			else if (g_Config.cvars.fakelag_move == 3) // In air
			{
				if (g_Local.flHeight <= 0.f)
					bFakeLag = false;
			}
		}

		if (bFakeLag)
		{
			static int choked = 0;
			static int good = 0;

			if (g_Config.cvars.fakelag_type == 0) // Dynamic
			{
				if (choked < g_Config.cvars.fakelag_limit)
				{
					bSendPacket = false;

					choked++;

					good = 0;
				}
				else
				{
					float one = g_Config.cvars.fakelag_limit / 100.f;
					float tmp = one * g_Config.cvars.fakelag_variance;

					good++;

					if (good > tmp)
					{
						choked = 0;
					}
				}
			}
			else if (g_Config.cvars.fakelag_type == 1) // Maximum
			{
				choked++;

				if (choked > 0)
					bSendPacket = false;

				if (choked > g_Config.cvars.fakelag_limit)
					choked = -1; // 1 tick valid
			}
			else if (g_Config.cvars.fakelag_type == 2) // Flucture
			{
				static bool jitter = false;

				if (jitter)
					bSendPacket = false;

				jitter = !jitter;
			}
			else if (g_Config.cvars.fakelag_type == 3) // Break lag compensation
			{
				Vector velocity = g_pPlayerMove->velocity;
				velocity.z = 0;
				float len = velocity.Length() * frametime;

				int choke = std::min<int>(static_cast<int>(std::ceilf(64.0f / len)), 14);
				if (choke > 14) return;

				static int choked = 0;
				if (choked > choke)
				{
					bSendPacket = true;
					choked = 0;
				}
				else
				{
					bSendPacket = false;
					choked++;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Auto Selfsink
//-----------------------------------------------------------------------------

void CMisc::AutoSelfSink()
{
	if (s_nSinkState == 1)
	{
		g_pEngineFuncs->pfnClientCmd("kill;-jump;-duck");

		s_nSinkState = 0;
	}
	else if (s_nSinkState == 2 && g_pPlayerMove->view_ofs.z == 12.0f)
	{
		ConCommand_AutoSelfSink();
	}
}

//-----------------------------------------------------------------------------
// Tertiary Attack Glitch
//-----------------------------------------------------------------------------

void CMisc::TertiaryAttackGlitch()
{
	if (g_Config.cvars.tertiary_attack_glitch)
	{
		if (!IsTertiaryAttackGlitchPatched())
		{
			EnableTertiaryAttackGlitch();
		}
	}
	else if (IsTertiaryAttackGlitchPatched())
	{
		DisableTertiaryAttackGlitch();
	}

	if (IsTertiaryAttackGlitchInit_Server())
	{
		if (g_Config.cvars.tertiary_attack_glitch)
		{
			if (!IsTertiaryAttackGlitchPatched_Server())
			{
				EnableTertiaryAttackGlitch_Server();
			}
		}
		else if (IsTertiaryAttackGlitchPatched_Server())
		{
			DisableTertiaryAttackGlitch_Server();
		}
	}
}

//-----------------------------------------------------------------------------
// Color Pulsator
//-----------------------------------------------------------------------------

void CMisc::ColorPulsator()
{
	static char command_buffer[32];

	if (g_Config.cvars.color_pulsator)
	{
		if (g_Config.cvars.color_pulsator_top && g_pEngineFuncs->Sys_FloatTime() - s_flTopColorDelay >= g_Config.cvars.color_pulsator_delay)
		{
			if (s_iTopColorOffset > 12)
				s_iTopColorOffset = 0;
			
			s_flTopColorDelay = g_pEngineFuncs->Sys_FloatTime() + g_Config.cvars.color_pulsator_delay;

			sprintf_s(command_buffer, sizeof(command_buffer), "topcolor %d", s_iTopColorOffset * 20);
			g_pEngineFuncs->pfnClientCmd(command_buffer);

			++s_iTopColorOffset;
		}

		if (g_Config.cvars.color_pulsator_bottom && g_pEngineFuncs->Sys_FloatTime() - s_flBottomColorDelay >= g_Config.cvars.color_pulsator_delay)
		{
			if (s_iBottomColorOffset > 12)
				s_iBottomColorOffset = 0;

			s_flBottomColorDelay = g_pEngineFuncs->Sys_FloatTime() + g_Config.cvars.color_pulsator_delay;

			sprintf_s(command_buffer, sizeof(command_buffer), "bottomcolor %d", s_iBottomColorOffset * 20);
			g_pEngineFuncs->pfnClientCmd(command_buffer);

			++s_iBottomColorOffset;
		}
	}
}

//-----------------------------------------------------------------------------
// No Weapon Animations
//-----------------------------------------------------------------------------

void CMisc::NoWeaponAnim_HUD_PostRunCmd(struct local_state_s *to)
{
	cl_entity_s *pViewModel = g_pEngineFuncs->GetViewModel();

	if (!pViewModel)
		return;

	static int s_iWeaponID = -1;
	static int s_iWaitTicks = 0;
	static char *s_pszWeaponName = NULL;

	int nWeaponID = to->client.m_iId;

	if (g_Config.cvars.no_weapon_anim == 2)
	{
		if (s_iWeaponID != nWeaponID || s_iWeaponID == 0 && (s_pszWeaponName && *s_pszWeaponName && pViewModel->model->name && *pViewModel->model->name && strcmp(pViewModel->model->name, s_pszWeaponName)))
		{
			s_pszWeaponName = pViewModel->model->name;
			s_iWeaponID = nWeaponID;

			s_iWaitTicks = 5;
		}

		if (s_iWaitTicks > 0)
		{
			g_pEngineFuncs->pfnWeaponAnim(0, 0);

			--s_iWaitTicks;
		}
	}
	else if (g_Config.cvars.no_weapon_anim == 1)
	{
		g_pEngineFuncs->pfnWeaponAnim(0, 0);

		s_iWeaponID = nWeaponID;
		s_iWaitTicks = 0;
	}
}

//-----------------------------------------------------------------------------
// Quake Guns
//-----------------------------------------------------------------------------

void CMisc::QuakeGuns_V_CalcRefdef()
{
	if (g_Config.cvars.quake_guns)
	{
		cl_entity_s *pViewModel = g_pEngineFuncs->GetViewModel();

		if (!pViewModel)
			return;

		if (g_iWeaponID == -1)
			return;

		float offset = GetWeaponOffset(pViewModel, g_iWeaponID);

		Vector va, right;

		float *org = pViewModel->origin;
		float *ang = pViewModel->angles;

		g_pEngineFuncs->GetViewAngles(va);
		g_pEngineFuncs->pfnAngleVectors(va, NULL, right, NULL);

		org[0] += right[0] * offset;
		org[1] += right[1] * offset;
		org[2] += right[2] * offset;
	}
}

void CMisc::QuakeGuns_HUD_PostRunCmd(struct local_state_s *to)
{
	g_iWeaponID = to->client.m_iId;
}

//-----------------------------------------------------------------------------

void CMisc::Init()
{
	HOOK_FUNCTION(QueryPerformanceCounter_Hook, QueryPerformanceCounter, QueryPerformanceCounter_Hooked, QueryPerformanceCounter_Original, QueryPerformanceCounterFn);

	ex_interp = g_pEngineFuncs->pfnGetCvarPointer("ex_interp");
}