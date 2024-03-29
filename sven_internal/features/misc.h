// Misc

#pragma once

class CMisc
{
public:
	void Init();

	void CreateMove(float frametime, struct usercmd_s *cmd, int active);
	void V_CalcRefdef(struct ref_params_s *pparams);
	void HUD_PostRunCmd(struct local_state_s *from, struct local_state_s *to, struct usercmd_s *cmd, int runfuncs, double time, unsigned int random_seed);

private:
	void AutoJump(struct usercmd_s *cmd);
	void JumpBug(float frametime, struct usercmd_s *cmd);
	void DoubleDuck(struct usercmd_s *cmd);
	void FastRun(struct usercmd_s *cmd);
	void Helicopter(struct usercmd_s *cmd);
	
	void FakeLag(float frametime);
	void AutoSelfSink();
	void TertiaryAttackGlitch();

	void ColorPulsator();

	void QuakeGuns_V_CalcRefdef();
	void QuakeGuns_HUD_PostRunCmd(struct local_state_s *to);

	void NoWeaponAnim_HUD_PostRunCmd(struct local_state_s *to);
};

extern CMisc g_Misc;