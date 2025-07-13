#include "Misc.h"
#include "../Backtrack/Backtrack.h"
#include "../Ticks/Ticks.h"
#include "../Players/PlayerUtils.h"
#include "../Aimbot/AutoRocketJump/AutoRocketJump.h"
#include "NamedPipe/NamedPipe.h"
#include "../../Utils/Optimization/CpuOptimization.h"
#include <fstream>
#include <format>
#include <chrono>
#include <ctime>
#include <functional>
#include <algorithm>
#include <unordered_set>
#include <cstdint>

namespace
{
	static std::string EscapeJson(const std::string &input)
	{
		std::string output;
		output.reserve(input.size() * 2);
		for (char c : input)
		{
			switch (c)
			{
			case '"': output += "\\\""; break;
			case '\\': output += "\\\\"; break;
			case '\b': output += "\\b";  break;
			case '\f': output += "\\f";  break;
			case '\n': output += "\\n";  break;
			case '\r': output += "\\r";  break;
			case '\t': output += "\\t";  break;
			default:   output += c;         break;
			}
		}
		return output;
	}

	static std::string FormatTime(const std::chrono::system_clock::time_point &tp, const char *fmt)
	{
		std::time_t tt = std::chrono::system_clock::to_time_t(tp);
		std::tm tmBuf{};
#ifdef _WIN32
		if (localtime_s(&tmBuf, &tt) != 0)
			return "Unknown Time";
#else
		if (!localtime_r(&tt, &tmBuf))
			return "Unknown Time";
#endif
		char buffer[64]{};
		if (!std::strftime(buffer, sizeof(buffer), fmt, &tmBuf))
			return "Unknown Time";
		return buffer;
	}

	static const char *TeamToString(int team)
	{
		switch (team)
		{
		case 2:  return "RED";
		case 3:  return "BLU";
		case 1:  return "SPEC";
		default: return "UNK";
		}
	}
}

void CMisc::RunPre(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	static bool systemInitialized = false;
	if (!systemInitialized)
	{
		CpuOptimization::g_OptimizationManager.Initialize();
		m_iTokenBucket = 5;
		m_tVoiceCommandTimer.Update();
		systemInitialized = true;
	}

	CpuOptimization::g_OptimizationManager.Update();

	static bool frameDistribution = false;
	frameDistribution = !frameDistribution;

	if (CPU_OPT_EXPENSIVE_FEATURE_CHECK())
	{
		CheatsBypass();
		WeaponSway();
	}

	if (frameDistribution)
	{
		NoiseSpam(pLocal);
		VoiceCommandSpam(pLocal);
		ChatSpam(pLocal);
	}
	else if (CPU_OPT_EXPENSIVE_FEATURE_CHECK())
	{
		MicSpam(pLocal);
		BotNetworking();
	}

	static int lowPriorityCounter = 0;
	lowPriorityCounter = (lowPriorityCounter + 1) % 4;

	switch (lowPriorityCounter)
	{
	case 0: AutoReport(); break;
	case 1: GrabSteamIDs(); break;
	case 2: StealIdentity(pLocal); break;
	case 3:
		if (I::EngineClient && I::EngineClient->IsInGame() && I::EngineClient->IsConnected())
		{
			static FastTimer namedPipeTimer{};
			if (namedPipeTimer.Run(1.0f))
				F::NamedPipe::UpdateLocalBotIgnoreStatus();
		}
		break;
	}

	if (!pLocal)
		return;

	// Medium priority player functions
	if (frameDistribution)
	{
		AntiAFK(pLocal, pCmd);
		InstantRespawnMVM(pLocal);
	}
	else if (CPU_OPT_EXPENSIVE_FEATURE_CHECK())
	{
		RandomVotekick(pLocal);
		CallVoteSpam(pLocal);
		AchievementSpam(pLocal);
	}

	if (!pLocal->IsAlive() || pLocal->IsAGhost() || pLocal->m_MoveType() != MOVETYPE_WALK ||
		pLocal->IsSwimming() || pLocal->InCond(TF_COND_SHIELD_CHARGE) || pLocal->InCond(TF_COND_HALLOWEEN_KART))
		return;

	AutoJump(pLocal, pCmd);
	EdgeJump(pLocal, pCmd);
	AutoJumpbug(pLocal, pCmd);
	AutoStrafe(pLocal, pCmd);
	AutoPeek(pLocal, pCmd);
	BreakJump(pLocal, pCmd);

	static FastTimer serverIdTimer{};
	if (serverIdTimer.Run(2.0f))
		m_sServerIdentifier = GetServerIdentifier();
}

void CMisc::RunPost(CTFPlayer* pLocal, CUserCmd* pCmd, bool pSendPacket)
{
	if (!pLocal || !pLocal->IsAlive() || pLocal->IsAGhost() || pLocal->m_MoveType() != MOVETYPE_WALK || pLocal->IsSwimming() || pLocal->InCond(TF_COND_SHIELD_CHARGE))
		return;

	EdgeJump(pLocal, pCmd, true);
	TauntKartControl(pLocal, pCmd);
	FastMovement(pLocal, pCmd);
	AutoPeek(pLocal, pCmd, true);
	BreakShootSound(pLocal, pCmd);
	MovementLock(pLocal, pCmd);
}

void CMisc::AutoJump(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::Misc::Movement::Bunnyhop.Value)
		return;

	if (auto pWeapon = H::Entities.GetWeapon(); pWeapon && pWeapon->GetWeaponID() == TF_WEAPON_GRAPPLINGHOOK && pWeapon->As<CTFGrapplingHook>()->m_hProjectile())
		return;

	static bool bStaticJump = false, bStaticGrounded = false, bLastAttempted = false;
	const bool bLastJump = bStaticJump, bLastGrounded = bStaticGrounded;
	const bool bCurJump = bStaticJump = pCmd->buttons & IN_JUMP, bCurGrounded = bStaticGrounded = pLocal->m_hGroundEntity();

	if (bCurJump && bLastJump && (bCurGrounded ? !pLocal->IsDucking() : true))
	{
		if (!(bCurGrounded && !bLastGrounded))
			pCmd->buttons &= ~IN_JUMP;

		if (!(pCmd->buttons & IN_JUMP) && bCurGrounded && !bLastAttempted)
			pCmd->buttons |= IN_JUMP;
	}

	if (Vars::Misc::Game::AntiCheatCompatibility.Value)
	{	// prevent more than 9 bhops occurring. if a server has this under that threshold they're retarded anyways
		static int iJumps = 0;
		if (bCurGrounded)
		{
			if (!bLastGrounded && pCmd->buttons & IN_JUMP)
				iJumps++;
			else
				iJumps = 0;

			if (iJumps > 9)
				pCmd->buttons &= ~IN_JUMP;
		}
	}
	bLastAttempted = pCmd->buttons & IN_JUMP;
}

void CMisc::AutoJumpbug(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::Misc::Movement::AutoJumpbug.Value || !(pCmd->buttons & IN_DUCK) || pLocal->m_hGroundEntity() || pLocal->m_vecVelocity().z > -650.f)
		return;

	float flUnduckHeight = 20 * pLocal->m_flModelScale();
	float flTraceDistance = flUnduckHeight + 2;

	CGameTrace trace = {};
	CTraceFilterWorldAndPropsOnly filter = {};

	Vec3 vOrigin = pLocal->m_vecOrigin();
	SDK::TraceHull(vOrigin, vOrigin - Vec3(0, 0, flTraceDistance), pLocal->m_vecMins(), pLocal->m_vecMaxs(), pLocal->SolidMask(), &filter, &trace);
	if (!trace.DidHit() || trace.fraction * flTraceDistance < flUnduckHeight) // don't try if we aren't in range to unduck or are too low
		return;

	pCmd->buttons &= ~IN_DUCK;
	pCmd->buttons |= IN_JUMP;
}

void CMisc::AutoStrafe(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::Misc::Movement::AutoStrafe.Value || pLocal->m_hGroundEntity() || !(pLocal->m_afButtonLast() & IN_JUMP) && (pCmd->buttons & IN_JUMP))
		return;

	switch (Vars::Misc::Movement::AutoStrafe.Value)
	{
	case Vars::Misc::Movement::AutoStrafeEnum::Legit:
	{
		static auto cl_sidespeed = U::ConVars.FindVar("cl_sidespeed");
		const float flSideSpeed = cl_sidespeed->GetFloat();

		if (pCmd->mousedx)
		{
			pCmd->forwardmove = 0.f;
			pCmd->sidemove = pCmd->mousedx > 0 ? flSideSpeed : -flSideSpeed;
		}
		break;
	}
	case Vars::Misc::Movement::AutoStrafeEnum::Directional:
	{
		// credits: KGB
		if (!(pCmd->buttons & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)))
			break;

		float flForward = pCmd->forwardmove, flSide = pCmd->sidemove;

		Vec3 vForward, vRight; Math::AngleVectors(pCmd->viewangles, &vForward, &vRight, nullptr);
		vForward.Normalize2D(), vRight.Normalize2D();

		Vec3 vWishDir = Math::VectorAngles({ vForward.x * flForward + vRight.x * flSide, vForward.y * flForward + vRight.y * flSide, 0.f });
		Vec3 vCurDir = Math::VectorAngles(pLocal->m_vecVelocity());
		float flDirDelta = Math::NormalizeAngle(vWishDir.y - vCurDir.y);
		if (fabsf(flDirDelta) > Vars::Misc::Movement::AutoStrafeMaxDelta.Value)
			break;

		float flTurnScale = Math::RemapVal(Vars::Misc::Movement::AutoStrafeTurnScale.Value, 0.f, 1.f, 0.9f, 1.f);
		float flRotation = DEG2RAD((flDirDelta > 0.f ? -90.f : 90.f) + flDirDelta * flTurnScale);
		float flCosRot = cosf(flRotation), flSinRot = sinf(flRotation);

		pCmd->forwardmove = flCosRot * flForward - flSinRot * flSide;
		pCmd->sidemove = flSinRot * flForward + flCosRot * flSide;
	}
	}
}

void CMisc::MovementLock(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	static bool bLock = false;

	if (!Vars::Misc::Movement::MovementLock.Value || pLocal->InCond(TF_COND_HALLOWEEN_KART))
	{
		bLock = false;
		return;
	}

	static Vec3 vMove = {}, vView = {};
	if (!bLock)
	{
		bLock = true;
		vMove = { pCmd->forwardmove, pCmd->sidemove, pCmd->upmove };
		vView = pCmd->viewangles;
	}

	pCmd->forwardmove = vMove.x, pCmd->sidemove = vMove.y, pCmd->upmove = vMove.z;
	SDK::FixMovement(pCmd, vView, pCmd->viewangles);
}

void CMisc::BreakJump(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::Misc::Movement::BreakJump.Value || F::AutoRocketJump.IsRunning())
		return;

	static bool bStaticJump = false;
	const bool bLastJump = bStaticJump;
	const bool bCurrJump = bStaticJump = pCmd->buttons & IN_JUMP;

	static int iTickSinceGrounded = -1;
	if (pLocal->m_hGroundEntity().Get())
		iTickSinceGrounded = -1;
	iTickSinceGrounded++;

	switch (iTickSinceGrounded)
	{
	case 0:
		if (bLastJump || !bCurrJump || pLocal->IsDucking())
			return;
		break;
	case 1:
		break;
	default:
		return;
	}

	pCmd->buttons |= IN_DUCK;
}

void CMisc::BreakShootSound(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	static int iOriginalWeaponSlot = -1;
	auto pWeapon = H::Entities.GetWeapon();
	if (!Vars::Misc::Exploits::BreakShootSound.Value || F::Ticks.m_bDoubletap || pLocal->m_iClass() != TF_CLASS_SOLDIER || !pWeapon)
		return;

	static bool bLastWasInAttack = false;
	static int iLastWeaponSelect = -1;
	const int iCurrSlot = pWeapon->GetSlot();
	if (pCmd->weaponselect && pCmd->weaponselect != iLastWeaponSelect)
		iOriginalWeaponSlot = -1;

	auto pSwap = iCurrSlot == SLOT_SECONDARY ? pLocal->GetWeaponFromSlot(SLOT_PRIMARY) : iCurrSlot == SLOT_PRIMARY ? pLocal->GetWeaponFromSlot(SLOT_SECONDARY) : pLocal->GetWeaponFromSlot(iOriginalWeaponSlot);
	if (pSwap && pSwap->CanBeSelected() && !pCmd->weaponselect)
	{
		if (bLastWasInAttack)
		{
			if (iOriginalWeaponSlot < 0)
			{
				iOriginalWeaponSlot = iCurrSlot;
				pCmd->weaponselect = pSwap->entindex();
				iLastWeaponSelect = pCmd->weaponselect;
			}
		}
		else
		{
			if (iOriginalWeaponSlot == iCurrSlot && G::CanPrimaryAttack)
				iOriginalWeaponSlot = -1;
			else if (iOriginalWeaponSlot >= 0 && iOriginalWeaponSlot != iCurrSlot)
			{
				pCmd->weaponselect = pSwap->entindex();
				iLastWeaponSelect = pCmd->weaponselect;
			}
		}
	}

	bLastWasInAttack = G::Attacking == 1 && G::CanPrimaryAttack;
}

void CMisc::AntiAFK(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	static Timer tTimer = {};
	m_bAntiAFK = false;
	static auto mp_idledealmethod = U::ConVars.FindVar("mp_idledealmethod");
	static auto mp_idlemaxtime = U::ConVars.FindVar("mp_idlemaxtime");
	const int iIdleMethod = mp_idledealmethod->GetInt();
	const float flMaxIdleTime = mp_idlemaxtime->GetFloat();

	if (pCmd->buttons & (IN_MOVELEFT | IN_MOVERIGHT | IN_FORWARD | IN_BACK) || !pLocal->IsAlive())
		tTimer.Update();
	else if (Vars::Misc::Automation::AntiAFK.Value && iIdleMethod && tTimer.Check(flMaxIdleTime * 60.f - 10.f)) // trigger 10 seconds before kick
	{
		pCmd->buttons |= I::GlobalVars->tickcount % 2 ? IN_FORWARD : IN_BACK;
		m_bAntiAFK = true;
	}
}

void CMisc::InstantRespawnMVM(CTFPlayer* pLocal)
{
	if (Vars::Misc::MannVsMachine::InstantRespawn.Value && I::EngineClient->IsInGame() && !pLocal->IsAlive())
	{
		KeyValues* kv = new KeyValues("MVM_Revive_Response");
		kv->SetInt("accepted", 1);
		I::EngineClient->ServerCmdKeyValues(kv);
	}
}

void CMisc::CheatsBypass()
{
	static bool bCheatSet = false;
	static auto sv_cheats = U::ConVars.FindVar("sv_cheats");
	if (Vars::Misc::Exploits::CheatsBypass.Value)
	{
		sv_cheats->m_nValue = 1;
		bCheatSet = true;
	}
	else if (bCheatSet)
	{
		sv_cheats->m_nValue = 0;
		bCheatSet = false;
	}
}

void CMisc::WeaponSway()
{
	static auto cl_wpn_sway_interp = U::ConVars.FindVar("cl_wpn_sway_interp");
	static auto cl_wpn_sway_scale = U::ConVars.FindVar("cl_wpn_sway_scale");

	bool bSway = Vars::Visuals::Viewmodel::SwayInterp.Value || Vars::Visuals::Viewmodel::SwayScale.Value;
	cl_wpn_sway_interp->SetValue(bSway ? Vars::Visuals::Viewmodel::SwayInterp.Value : 0.f);
	cl_wpn_sway_scale->SetValue(bSway ? Vars::Visuals::Viewmodel::SwayScale.Value : 0.f);
}

void CMisc::BotNetworking()
{
#ifdef TEXTMODE
	static bool bNetworkSet = false;
	
	if (Vars::Misc::Game::BotNetworking.Value)
	{
		if (!bNetworkSet)
		{
			// Network settings
			static auto rate = U::ConVars.FindVar("rate");
			static auto cl_cmdrate = U::ConVars.FindVar("cl_cmdrate");
			static auto cl_updaterate = U::ConVars.FindVar("cl_updaterate");
			static auto cl_interp = U::ConVars.FindVar("cl_interp");
			static auto cl_interp_ratio = U::ConVars.FindVar("cl_interp_ratio");
			static auto cl_lagcompensation = U::ConVars.FindVar("cl_lagcompensation");
			static auto cl_smooth = U::ConVars.FindVar("cl_smooth");
			static auto cl_pred_optimize = U::ConVars.FindVar("cl_pred_optimize");
			
			// Voice settings
			static auto voice_loopback = U::ConVars.FindVar("voice_loopback");
			static auto voice_maxgain = U::ConVars.FindVar("voice_maxgain");
			static auto voice_buffer_ms = U::ConVars.FindVar("voice_buffer_ms");
			static auto voice_scale = U::ConVars.FindVar("voice_scale");
			static auto voice_enable = U::ConVars.FindVar("voice_enable");
			static auto voice_modenable = U::ConVars.FindVar("voice_modenable");
			static auto voice_steal = U::ConVars.FindVar("voice_steal");

			// Sound settings
			static auto snd_async_fullyasync = U::ConVars.FindVar("snd_async_fullyasync");
			static auto snd_mixahead = U::ConVars.FindVar("snd_mixahead");
			static auto snd_spatialize_roundrobin = U::ConVars.FindVar("snd_spatialize_roundrobin");
			static auto snd_surround_speakers = U::ConVars.FindVar("snd_surround_speakers");
			static auto snd_cull_duplicates = U::ConVars.FindVar("snd_cull_duplicates");
			static auto snd_pitchquality = U::ConVars.FindVar("snd_pitchquality");
			static auto dsp_slow_cpu = U::ConVars.FindVar("dsp_slow_cpu");

			if (rate) rate->SetValue(80000);
			if (cl_cmdrate) cl_cmdrate->SetValue(66);
			if (cl_updaterate) cl_updaterate->SetValue(66);
			if (cl_interp) cl_interp->SetValue(0.0f);
			if (cl_interp_ratio) cl_interp_ratio->SetValue(1);
			if (cl_lagcompensation) cl_lagcompensation->SetValue(1);
			if (cl_smooth) cl_smooth->SetValue(0);
			if (cl_pred_optimize) cl_pred_optimize->SetValue(2);
			if (voice_loopback) voice_loopback->SetValue(0);
			if (voice_maxgain) voice_maxgain->SetValue(1.0f);
			if (voice_buffer_ms) voice_buffer_ms->SetValue(300);
			if (voice_scale) voice_scale->SetValue(0.0f);
			if (voice_enable) voice_enable->SetValue(1);
			if (voice_modenable) voice_modenable->SetValue(1);
			if (voice_steal) voice_steal->SetValue(2);
			if (snd_async_fullyasync) snd_async_fullyasync->SetValue(1);
			if (snd_mixahead) snd_mixahead->SetValue(0.05f);
			if (snd_spatialize_roundrobin) snd_spatialize_roundrobin->SetValue(0);
			if (snd_surround_speakers) snd_surround_speakers->SetValue(0);
			if (snd_cull_duplicates) snd_cull_duplicates->SetValue(0);
			if (snd_pitchquality) snd_pitchquality->SetValue(1);
			if (dsp_slow_cpu) dsp_slow_cpu->SetValue(1);
			
			bNetworkSet = true;
		}
	}
	else
	{
		bNetworkSet = false;
	}
#endif
}

void CMisc::TauntKartControl(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	// Handle Taunt Slide
	if (Vars::Misc::Automation::TauntControl.Value && pLocal->IsTaunting() && pLocal->m_bAllowMoveDuringTaunt())
	{
		if (pLocal->m_bTauntForceMoveForward())
		{
			if (pCmd->buttons & IN_BACK)
				pCmd->viewangles.x = 91.f;
			else if (!(pCmd->buttons & IN_FORWARD))
				pCmd->viewangles.x = 90.f;
		}
		if (pCmd->buttons & IN_MOVELEFT)
			pCmd->sidemove = pCmd->viewangles.x != 90.f ? -50.f : -450.f;
		else if (pCmd->buttons & IN_MOVERIGHT)
			pCmd->sidemove = pCmd->viewangles.x != 90.f ? 50.f : 450.f;

		Vec3 vAngle = I::EngineClient->GetViewAngles();
		pCmd->viewangles.y = vAngle.y;

		G::SilentAngles = true;
	}
	else if (Vars::Misc::Automation::KartControl.Value && pLocal->InCond(TF_COND_HALLOWEEN_KART))
	{
		const bool bForward = pCmd->buttons & IN_FORWARD;
		const bool bBack = pCmd->buttons & IN_BACK;
		const bool bLeft = pCmd->buttons & IN_MOVELEFT;
		const bool bRight = pCmd->buttons & IN_MOVERIGHT;

		const bool flipVar = I::GlobalVars->tickcount % 2;
		if (bForward && (!bLeft && !bRight || !flipVar))
		{
			pCmd->forwardmove = 450.f;
			pCmd->viewangles.x = 0.f;
		}
		else if (bBack && (!bLeft && !bRight || !flipVar))
		{
			pCmd->forwardmove = 450.f;
			pCmd->viewangles.x = 91.f;
		}
		else if (pCmd->buttons & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT))
		{
			if (flipVar || !F::Ticks.CanChoke())
			{	// you could just do this if you didn't care about viewangles
				Vec3 vMove = { pCmd->forwardmove, pCmd->sidemove, 0.f };
				Vec3 vAngMoveReverse = Math::VectorAngles(vMove * -1.f);
				pCmd->forwardmove = -vMove.Length();
				pCmd->sidemove = 0.f;
				pCmd->viewangles.y = fmodf(pCmd->viewangles.y - vAngMoveReverse.y, 360.f);
				pCmd->viewangles.z = 270.f;
				G::PSilentAngles = true;
			}
		}
		else
			pCmd->viewangles.x = 90.f;

		G::SilentAngles = true;
	}
}

void CMisc::FastMovement(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!pLocal->m_hGroundEntity() || pLocal->InCond(TF_COND_HALLOWEEN_KART))
		return;

	const float flSpeed = pLocal->m_vecVelocity().Length2D();
	const int flMaxSpeed = std::min(pLocal->m_flMaxspeed() * 0.9f, 520.f) - 10.f;
	const int iRun = !pCmd->forwardmove && !pCmd->sidemove ? 0 : flSpeed < flMaxSpeed ? 1 : 2;

	switch (iRun)
	{
	case 0:
	{
		if (!Vars::Misc::Movement::FastStop.Value || !flSpeed)
			return;

		Vec3 vDirection = pLocal->m_vecVelocity().ToAngle();
		vDirection.y = pCmd->viewangles.y - vDirection.y;
		Vec3 vNegatedDirection = vDirection.FromAngle() * -flSpeed;
		pCmd->forwardmove = vNegatedDirection.x;
		pCmd->sidemove = vNegatedDirection.y;

		break;
	}
	case 1:
	{
		if ((pLocal->IsDucking() ? !Vars::Misc::Movement::CrouchSpeed.Value : !Vars::Misc::Movement::FastAccelerate.Value)
			|| Vars::Misc::Game::AntiCheatCompatibility.Value
			|| G::Attacking == 1 || F::Ticks.m_bDoubletap || F::Ticks.m_bSpeedhack || F::Ticks.m_bRecharge || G::AntiAim || I::GlobalVars->tickcount % 2)
			return;

		if (!(pCmd->buttons & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)))
			return;

		Vec3 vMove = { pCmd->forwardmove, pCmd->sidemove, 0.f };
		Vec3 vAngMoveReverse = Math::VectorAngles(vMove * -1.f);
		pCmd->forwardmove = -vMove.Length();
		pCmd->sidemove = 0.f;
		pCmd->viewangles.y = fmodf(pCmd->viewangles.y - vAngMoveReverse.y, 360.f);
		pCmd->viewangles.z = 270.f;
		G::PSilentAngles = true;

		break;
	}
	}
}

void CMisc::AutoPeek(CTFPlayer* pLocal, CUserCmd* pCmd, bool bPost)
{
	static bool bReturning = false;

	if (!bPost)
	{
		if (Vars::AutoPeek::Enabled.Value)
		{
			Vec3 vLocalPos = pLocal->m_vecOrigin();

			if (bReturning)
			{
				if (vLocalPos.DistTo2D(m_vPeekReturnPos) < 8.f)
				{
					bReturning = false;
					return;
				}

				SDK::WalkTo(pCmd, pLocal, m_vPeekReturnPos);
				pCmd->buttons &= ~IN_JUMP;
			}
			else if (!pLocal->m_hGroundEntity())
				m_bPeekPlaced = false;

			if (!m_bPeekPlaced)
			{
				m_vPeekReturnPos = vLocalPos;
				m_bPeekPlaced = true;
			}
			else
			{
				static Timer tTimer = {};
				if (tTimer.Run(0.7f))
					H::Particles.DispatchParticleEffect("ping_circle", m_vPeekReturnPos, {});
			}
		}
		else
			m_bPeekPlaced = bReturning = false;
	}
	else if (G::Attacking && m_bPeekPlaced)
		bReturning = true;
}

void CMisc::EdgeJump(CTFPlayer* pLocal, CUserCmd* pCmd, bool bPost)
{
	if (!Vars::Misc::Movement::EdgeJump.Value)
		return;

	static bool bStaticGround = false;
	if (!bPost)
		bStaticGround = pLocal->m_hGroundEntity();
	else if (bStaticGround && !pLocal->m_hGroundEntity())
		pCmd->buttons |= IN_JUMP;
}

void CMisc::Event(IGameEvent* pEvent, uint32_t uHash)
{
	switch (uHash)
	{
	case FNV1A::Hash32Const("client_disconnect"):
	case FNV1A::Hash32Const("client_beginconnect"):
	case FNV1A::Hash32Const("game_newmap"):
		m_vChatSpamLines.clear();
		m_vKillSayLines.clear();
		m_vAutoReplyLines.clear();
		m_iCurrentChatSpamIndex = 0;
		m_sLastKilledPlayerName = "";
		m_iTokenBucket = 5;
		m_tVoiceCommandTimer.Update();
		[[fallthrough]];
	case FNV1A::Hash32Const("teamplay_round_start"):
		G::LineStorage.clear();
		G::BoxStorage.clear();
		G::PathStorage.clear();
		break;
	case FNV1A::Hash32Const("player_spawn"):
		m_bPeekPlaced = false;
		break;
	case FNV1A::Hash32Const("vote_maps_changed"):
		if (Vars::Misc::Automation::AutoVoteMap.Value)
		{
			I::EngineClient->ClientCmd_Unrestricted(std::format("next_map_vote {}", Vars::Misc::Automation::AutoVoteMapOption.Value).c_str());
		}
		break;
	case FNV1A::Hash32Const("player_death"):
	{
		const int attacker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));
		const int victim = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));

		int localPlayerIdx = I::EngineClient->GetLocalPlayer();
		if (attacker == localPlayerIdx && victim != localPlayerIdx)
		{
			KillSay(victim);
		}
	}

break;
	case FNV1A::Hash32Const("player_say"):
	{
		const int speaker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
		const char* text = pEvent->GetString("text");

		bool isTeamChat = false;
		if (pEvent->GetBool("team", false))
		{
			isTeamChat = true;
		}
		if (text && *text)
		{
			F::Misc.ChatRelay(speaker, text, isTeamChat);
		}

		int localPlayerIdx = I::EngineClient->GetLocalPlayer();
		if (speaker != localPlayerIdx && text)
		{
			AutoReply(speaker, text);
		}
	}
	break;
	}
}


int CMisc::AntiBackstab(CTFPlayer* pLocal, CUserCmd* pCmd, bool bSendPacket)
{
	if (!Vars::Misc::Automation::AntiBackstab.Value || !bSendPacket || G::Attacking == 1 || !pLocal || pLocal->m_MoveType() != MOVETYPE_WALK || pLocal->InCond(TF_COND_HALLOWEEN_KART))
		return 0;

	std::vector<std::pair<Vec3, CBaseEntity*>> vTargets = {};
	for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
	{
		auto pPlayer = pEntity->As<CTFPlayer>();
		if (pPlayer->IsDormant() || !pPlayer->IsAlive() || pPlayer->IsAGhost() || pPlayer->InCond(TF_COND_STEALTHED))
			continue;

		auto pWeapon = pPlayer->m_hActiveWeapon().Get()->As<CTFWeaponBase>();
		if (!pWeapon
			|| pWeapon->GetWeaponID() != TF_WEAPON_KNIFE
			&& !(G::PrimaryWeaponType == EWeaponType::MELEE && SDK::AttribHookValue(0, "crit_from_behind", pWeapon) > 0)
			&& !(pWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER && SDK::AttribHookValue(0, "set_flamethrower_back_crit", pWeapon) == 1)
			|| F::PlayerUtils.IsIgnored(pPlayer->entindex()))
			continue;

		Vec3 vLocalPos = pLocal->GetCenter();
		Vec3 vTargetPos1 = pPlayer->GetCenter();
		Vec3 vTargetPos2 = vTargetPos1 + pPlayer->m_vecVelocity() * F::Backtrack.GetReal();
		float flDistance = std::max(std::max(SDK::MaxSpeed(pPlayer), SDK::MaxSpeed(pLocal)), pPlayer->m_vecVelocity().Length());
		if ((vLocalPos.DistTo(vTargetPos1) > flDistance || !SDK::VisPosWorld(pLocal, pPlayer, vLocalPos, vTargetPos1))
			&& (vLocalPos.DistTo(vTargetPos2) > flDistance || !SDK::VisPosWorld(pLocal, pPlayer, vLocalPos, vTargetPos2)))
			continue;

		vTargets.emplace_back(vTargetPos2, pEntity);
	}
	if (vTargets.empty())
		return 0;

	std::sort(vTargets.begin(), vTargets.end(), [&](const auto& a, const auto& b) -> bool
		{
			return pLocal->GetCenter().DistTo(a.first) < pLocal->GetCenter().DistTo(b.first);
		});

	auto& pTargetPos = vTargets.front();
	switch (Vars::Misc::Automation::AntiBackstab.Value)
	{
	case Vars::Misc::Automation::AntiBackstabEnum::Yaw:
	{
		Vec3 vAngleTo = Math::CalcAngle(pLocal->m_vecOrigin(), pTargetPos.first);
		vAngleTo.x = pCmd->viewangles.x;
		SDK::FixMovement(pCmd, vAngleTo);
		pCmd->viewangles = vAngleTo;

		return 1;
	}
	case Vars::Misc::Automation::AntiBackstabEnum::Pitch:
	case Vars::Misc::Automation::AntiBackstabEnum::Fake:
	{
		bool bCheater = F::PlayerUtils.HasTag(pTargetPos.second->entindex(), F::PlayerUtils.TagToIndex(CHEATER_TAG));
		// if the closest spy is a cheater, assume auto stab is being used, otherwise don't do anything if target is in front
		if (!bCheater)
		{
			auto TargetIsBehind = [&]()
				{
					Vec3 vToTarget = (pLocal->m_vecOrigin() - pTargetPos.first).To2D();
					const float flDist = vToTarget.Normalize();
					if (!flDist)
						return true;

					float flTolerance = 0.0625f;
					float flExtra = 2.f * flTolerance / flDist; // account for origin compression
					float flPosVsTargetViewMinDot = 0.f - 0.0031f - flExtra;

					Vec3 vTargetForward; Math::AngleVectors(pCmd->viewangles, &vTargetForward);
					vTargetForward.Normalize2D();

					return vToTarget.Dot(vTargetForward) > flPosVsTargetViewMinDot;
				};

			if (!TargetIsBehind())
				return 0;
		}

		if (!bCheater || Vars::Misc::Automation::AntiBackstab.Value == Vars::Misc::Automation::AntiBackstabEnum::Pitch)
		{
			pCmd->forwardmove *= -1;
			pCmd->viewangles.x = 269.f;
		}
		else
			pCmd->viewangles.x = 271.f;
		// may slip up some auto backstabs depending on mode, though we are still able to be stabbed

		return 2;
	}
	}

	return 0;
}

void CMisc::PingReducer()
{
	static Timer tTimer = {};
	if (!tTimer.Run(0.1f))
		return;

	auto pNetChan = reinterpret_cast<CNetChannel*>(I::EngineClient->GetNetChannelInfo());
	const auto& pResource = H::Entities.GetPR();
	if (!pNetChan || !pResource)
		return;

	static auto cl_cmdrate = U::ConVars.FindVar("cl_cmdrate");
	const int iCmdRate = cl_cmdrate->GetInt();
	const int Ping = pResource->m_iPing(I::EngineClient->GetLocalPlayer());
	const int iTarget = Vars::Misc::Exploits::PingReducer.Value && (Ping > Vars::Misc::Exploits::PingTarget.Value) ? -1 : iCmdRate;

	NET_SetConVar cmd("cl_cmdrate", std::to_string(m_iWishCmdrate = iTarget).c_str());
	pNetChan->SendNetMsg(cmd);
}

void CMisc::UnlockAchievements()
{
	const auto pAchievementMgr = reinterpret_cast<IAchievementMgr * (*)(void)>(U::Memory.GetVirtual(I::EngineClient, 114))();
	if (pAchievementMgr)
	{
		I::SteamUserStats->RequestCurrentStats();
		for (int i = 0; i < pAchievementMgr->GetAchievementCount(); i++)
			pAchievementMgr->AwardAchievement(pAchievementMgr->GetAchievementByIndex(i)->GetAchievementID());
		I::SteamUserStats->StoreStats();
		I::SteamUserStats->RequestCurrentStats();
	}
}

void CMisc::LockAchievements()
{
	const auto pAchievementMgr = reinterpret_cast<IAchievementMgr * (*)(void)>(U::Memory.GetVirtual(I::EngineClient, 114))();
	if (pAchievementMgr)
	{
		I::SteamUserStats->RequestCurrentStats();
		for (int i = 0; i < pAchievementMgr->GetAchievementCount(); i++)
			I::SteamUserStats->ClearAchievement(pAchievementMgr->GetAchievementByIndex(i)->GetName());
		I::SteamUserStats->StoreStats();
		I::SteamUserStats->RequestCurrentStats();
	}
}

bool CMisc::SteamRPC()
{
	/*
	if (!Vars::Misc::Steam::EnableRPC.Value)
	{
		if (!bSteamCleared) // stupid way to return back to normal rpc
		{
			I::SteamFriends->SetRichPresence("steam_display", ""); // this will only make it say "Team Fortress 2" until the player leaves/joins some server. its bad but its better than making 1000 checks to recreate the original
			bSteamCleared = true;
		}
		return false;
	}

	bSteamCleared = false;
	*/


	if (!Vars::Misc::SteamRPC::Enabled.Value)
		return false;

	I::SteamFriends->SetRichPresence("steam_display", "#TF_RichPresence_Display");
	if (!I::EngineClient->IsInGame() && !Vars::Misc::SteamRPC::OverrideInMenu.Value)
		I::SteamFriends->SetRichPresence("state", "MainMenu");
	else
	{
		I::SteamFriends->SetRichPresence("state", "PlayingMatchGroup");

		switch (Vars::Misc::SteamRPC::MatchGroup.Value)
		{
		case Vars::Misc::SteamRPC::MatchGroupEnum::SpecialEvent: I::SteamFriends->SetRichPresence("matchgrouploc", "SpecialEvent"); break;
		case Vars::Misc::SteamRPC::MatchGroupEnum::MvMMannUp: I::SteamFriends->SetRichPresence("matchgrouploc", "MannUp"); break;
		case Vars::Misc::SteamRPC::MatchGroupEnum::Competitive: I::SteamFriends->SetRichPresence("matchgrouploc", "Competitive6v6"); break;
		case Vars::Misc::SteamRPC::MatchGroupEnum::Casual: I::SteamFriends->SetRichPresence("matchgrouploc", "Casual"); break;
		case Vars::Misc::SteamRPC::MatchGroupEnum::MvMBootCamp: I::SteamFriends->SetRichPresence("matchgrouploc", "BootCamp"); break;
		}
	}
	I::SteamFriends->SetRichPresence("currentmap", Vars::Misc::SteamRPC::MapText.Value.c_str());
	I::SteamFriends->SetRichPresence("steam_player_group_size", std::to_string(Vars::Misc::SteamRPC::GroupSize.Value).c_str());

	return true;
}


void CMisc::NoiseSpam(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::NoiseSpam.Value || !pLocal)
		return;

	if (pLocal->m_bUsingActionSlot())
		return;

	static float flLastSpamTime = 0.0f;
	float flCurrentTime = SDK::PlatFloatTime();
	if (flCurrentTime - flLastSpamTime < 0.2f)
		return;

	flLastSpamTime = flCurrentTime;
	I::EngineClient->ServerCmdKeyValues(new KeyValues("use_action_slot_item_server"));
}

void CMisc::VoiceCommandSpam(CTFPlayer* pLocal)
{
	
	if (!Vars::Misc::Automation::VoiceCommandSpam.Value || !pLocal || !pLocal->IsAlive())
		return;

	bool bUseF2PSystem = Vars::Misc::Automation::VoiceF2PMode.Value;

	// F2Pvoicecommandspam tokensystem
	if (bUseF2PSystem)
	{
		// F2P token bucket system constants
		const int maxTokens = 5;
		const float refillInterval = 6.0f * CPU_OPT_GET_FREQ_MULTIPLIER();

		if (m_tVoiceCommandTimer.Run(refillInterval) && m_iTokenBucket < maxTokens)
		{
			m_iTokenBucket++;
			if (CPU_OPT_EXPENSIVE_FEATURE_CHECK())
				SDK::Output("VoiceCommandSpam", std::format("F2P Mode: Token added, now: {}", m_iTokenBucket).c_str(), {}, true, true);
		}

		if (m_iTokenBucket <= 0)
			return;

		static FastTimer spamTimer{};
		if (!spamTimer.Run(0.2f))
			return;

		m_iTokenBucket--;
	}
	else
	{
		static FastTimer voiceTimer{};
		if (!voiceTimer.Run(4.0f))
			return;
	}

	switch (Vars::Misc::Automation::VoiceCommandSpam.Value)
	{
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Random:
	{
		int menu = SDK::RandomInt(0, 2);
		int command = SDK::RandomInt(0, 8);
		std::string cmd = "voicemenu " + std::to_string(menu) + " " + std::to_string(command);
		I::EngineClient->ClientCmd_Unrestricted(cmd.c_str());
	}
	break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Medic:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 0 0");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Thanks:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 0 1");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::NiceShot:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 2 6");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Cheers:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 2 2");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Jeers:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 2 3");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::GoGoGo:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 0 2");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::MoveUp:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 0 3");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::GoLeft:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 0 4");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::GoRight:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 0 5");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Yes:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 0 6");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::No:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 0 7");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Incoming:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 1 0");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Spy:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 1 1");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Sentry:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 1 2");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::NeedTeleporter:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 1 3");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Pootis:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 1 4");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::NeedSentry:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 1 5");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::ActivateCharge:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 1 6");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Help:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 2 0");
		break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::BattleCry:
		I::EngineClient->ClientCmd_Unrestricted("voicemenu 2 1");
		break;
	}
}

void CMisc::MicSpam(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::MicSpam.Value || !pLocal)
		return;

	if (!m_tMicSpamTimer.Run(2.0f))
		return;

	static std::vector<std::string> micCommands = {
		"+voicerecord",
		"voice_loopback 0",
		"voice_inputfromfile 0",
		"voice_outputtofile 0",
		"voice_threshold 4000",
		"voice_recordtofile 0",
		"voice_overdrive 0",
		"voice_scale 0.8",
		"voice_vox 0",
		"voice_avggain 0.3",
		"voice_maxgain 1.0",
		"sv_voicecodec vaudio_speex",
		"sv_voicequality 1"
	};

	static size_t currentIndex = 0;
	
	I::EngineClient->ClientCmd_Unrestricted(micCommands[currentIndex].c_str());
	
	currentIndex = (currentIndex + 1) % micCommands.size();
}



void CMisc::AchievementSpam(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::AchievementSpam.Value || !pLocal || !pLocal->IsAlive())
	{
		m_eAchievementSpamState = AchievementSpamState::IDLE;
		return;
	}

	const auto pAchievementMgr = reinterpret_cast<IAchievementMgr * (*)(void)>(U::Memory.GetVirtual(I::EngineClient, 114))();
	if (!pAchievementMgr)
	{
		m_eAchievementSpamState = AchievementSpamState::IDLE;
		return;
	}

	switch (m_eAchievementSpamState)
	{
	case AchievementSpamState::IDLE:
	{
		if (!m_tAchievementSpamTimer.Run(20.0f))
			return;

		// Kill Everyone You Meet achievement by default
		// TODO: add a new column to edit achievement timer & number directly in cheat (like you did with autoitem)
		int specificAchievementID = 1105;

		IAchievement* pAchievement = nullptr;
		for (int i = 0; i < pAchievementMgr->GetAchievementCount(); i++)
		{
			IAchievement* pCurrentAchievement = pAchievementMgr->GetAchievementByIndex(i);
			if (pCurrentAchievement && pCurrentAchievement->GetAchievementID() == specificAchievementID)
			{
				pAchievement = pCurrentAchievement;
				break;
			}
		}

		if (!pAchievement || !pAchievement->GetName())
			return;

		m_iAchievementSpamID = specificAchievementID;
		m_sAchievementSpamName = pAchievement->GetName();
		m_eAchievementSpamState = AchievementSpamState::CLEARING;
		break;
	}
	case AchievementSpamState::CLEARING:
	{
		I::SteamUserStats->RequestCurrentStats();
		I::SteamUserStats->ClearAchievement(m_sAchievementSpamName);
		I::SteamUserStats->StoreStats();

		m_tAchievementDelayTimer.Update();
		m_eAchievementSpamState = AchievementSpamState::WAITING;
		break;
	}
	case AchievementSpamState::WAITING:
	{
		if (!m_tAchievementDelayTimer.Run(0.1f))
			return;

		m_eAchievementSpamState = AchievementSpamState::AWARDING;
		break;
	}
	case AchievementSpamState::AWARDING:
	{
		I::SteamUserStats->RequestCurrentStats();
		pAchievementMgr->AwardAchievement(m_iAchievementSpamID);
		I::SteamUserStats->StoreStats();

		m_eAchievementSpamState = AchievementSpamState::IDLE;
		break;
	}
	}
}

void CMisc::RandomVotekick(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::RandomVotekick.Value || !I::EngineClient->IsInGame() || !I::EngineClient->IsConnected())
		return;

	if (!m_tRandomVotekickTimer.Run(1.0f))
		return;

	std::vector<int> vPotentialTargets;

	for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
	{
		if (i == I::EngineClient->GetLocalPlayer())
			continue;

		PlayerInfo_t pi{};
		if (!I::EngineClient->GetPlayerInfo(i, &pi) || pi.fakeplayer)
			continue;

		if (H::Entities.IsFriend(i) ||
			H::Entities.InParty(i) ||
			F::PlayerUtils.IsIgnored(i) ||
			F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
			F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG)))
			continue;

		vPotentialTargets.push_back(i);
	}

	if (vPotentialTargets.empty())
		return;


	int iRandom = SDK::RandomInt(0, vPotentialTargets.size() - 1);
	int iTarget = vPotentialTargets[iRandom];


	PlayerInfo_t pi{};
	if (I::EngineClient->GetPlayerInfo(iTarget, &pi))
	{
		I::ClientState->SendStringCmd(std::format("callvote Kick \"{} other\"", pi.userID).c_str());
	}
}

void CMisc::CallVoteSpam(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::CallVoteSpam.Value || !I::EngineClient->IsInGame() || !I::EngineClient->IsConnected())
		return;

	if (!m_tCallVoteSpamTimer.Run(1.0f))
		return;

	// Create a list of possible vote types
	std::vector<std::string> voteOptions = {
		"callvote changelevel cp_badlands",
		"callvote changelevel cp_granary",
		"callvote changelevel cp_well",
		"callvote changelevel cp_5gorge",
		"callvote changelevel cp_freight_final1",
		"callvote changelevel cp_yukon_final",
		"callvote changelevel cp_gravelpit",
		"callvote changelevel cp_dustbowl",
		"callvote changelevel cp_egypt_final",
		"callvote changelevel cp_junction_final",
		"callvote changelevel cp_steel",
		"callvote changelevel ctf_2fort",
		"callvote changelevel ctf_well",
		"callvote changelevel ctf_sawmill",
		"callvote changelevel ctf_turbine",
		"callvote changelevel ctf_doublecross",
		"callvote changelevel pl_badwater",
		"callvote changelevel pl_goldrush",
		"callvote changelevel pl_dustbowl",
		"callvote changelevel pl_upward",
		"callvote changelevel pl_thundermountain",
		"callvote changelevel koth_harvest_final",
		"callvote changelevel koth_nucleus",
		"callvote changelevel koth_sawmill",
		"callvote changelevel koth_viaduct",
		"callvote changelevel cp_5gorge",
		"callvote changelevel cp_dustbowl",
		"callvote changelevel ctf_2fort",
		"callvote changelevel ctf_doublecross",
		"callvote changelevel ctf_turbine",
		"callvote changelevel koth_brazil",
		"callvote changelevel pl_badwater",
		"callvote changelevel pl_pheonix",
		"callvote changelevel plr_bananabay",
		"callvote changelevel plr_hightower",
		"callvote scrambleteams"
	};

	// Pick a random vote option
	int randomIndex = SDK::RandomInt(0, static_cast<int>(voteOptions.size()) - 1);
	std::string selectedVote = voteOptions[randomIndex];

	// Send the vote command
	I::ClientState->SendStringCmd(selectedVote.c_str());
}


// lmaobox if(crash){(dontcrash)} method down here
void CMisc::ChatSpam(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::ChatSpam::Enable.Value || !pLocal || !I::EngineClient)
	{
		m_tChatSpamTimer.Update();
		return;
	}

	try
	{
		if (!I::EngineClient->IsInGame() || !I::EngineClient->IsConnected())
		{
			m_tChatSpamTimer.Update();
			return;
		}

		if (pLocal->m_iTeamNum() <= 1 || pLocal->m_iClass() == TF_CLASS_UNDEFINED)
		{
			m_tChatSpamTimer.Update();
			return;
		}
	}
	catch (...)
	{
		m_tChatSpamTimer.Update();
		return;
	}

	if (m_vChatSpamLines.empty())
	{
		std::vector<std::string> defaults = {
			"Chatspam is highly customizable",
			"Check misc.cpp for more info",
			"dsc.gg/nptntf",
			"dont call me bro"
		};
		LoadLines("ChatSpam", "cat_chatspam.txt", defaults, m_vChatSpamLines);
	}

	float spamInterval = Vars::Misc::Automation::ChatSpam::Interval.Value;
	if (spamInterval <= 0.2f)
		spamInterval = 0.2f;

	if (!m_tChatSpamTimer.Run(spamInterval))
		return;

	std::string chatLine;
	const size_t lineCount = m_vChatSpamLines.size();

	if (lineCount == 0)
		return;

	try
	{
		if (Vars::Misc::Automation::ChatSpam::Randomize.Value)
		{
			// Safer random index generation
			int max = static_cast<int>(lineCount) - 1;
			if (max < 0) max = 0;
			int randomIndex = SDK::RandomInt(0, max);

			// Double-check bounds for safety
			if (randomIndex >= 0 && randomIndex < static_cast<int>(lineCount))
				chatLine = m_vChatSpamLines[randomIndex];
			else
				chatLine = "[chtspm]";
		}
		else
		{
			if (m_iCurrentChatSpamIndex < 0 || m_iCurrentChatSpamIndex >= static_cast<int>(lineCount))
				m_iCurrentChatSpamIndex = 0;

			if (m_iCurrentChatSpamIndex >= 0 && m_iCurrentChatSpamIndex < static_cast<int>(lineCount))
			{
				chatLine = m_vChatSpamLines[m_iCurrentChatSpamIndex];
				m_iCurrentChatSpamIndex = (m_iCurrentChatSpamIndex + 1) % static_cast<int>(lineCount);
			}
			else
			{
				chatLine = "[ChatSpam]";
				m_iCurrentChatSpamIndex = 0;
			}
		}

		// Limit message length to prevent buffer issues
		if (chatLine.length() > 150)
		{
			chatLine = chatLine.substr(0, 150);
		}

		// Process text replacements
		chatLine = ProcessTextReplacements(chatLine);

		std::string chatCommand;
		if (Vars::Misc::Automation::ChatSpam::TeamChat.Value)
			chatCommand = "say_team \"" + chatLine + "\"";
		else
			chatCommand = "say \"" + chatLine + "\"";

		I::EngineClient->ClientCmd_Unrestricted(chatCommand.c_str());
	}
	catch (...)
	{
		// Silently fail if we encounter any exception during command execution
		return;
	}
}

void CMisc::AutoReport()
{
	if (!Vars::Misc::Automation::AutoReport.Value)
		return;

	static Timer reportTimer{};
	if (!reportTimer.Run(5.0f))
		return;

	static auto reportFunc = reinterpret_cast<void(*)(uint64_t, int)>(U::Memory.FindSignature("client.dll", "48 89 5C 24 ? 57 48 83 EC 60 48 8B D9 8B FA"));
	if (!reportFunc)
		return;

	if (!I::EngineClient || !I::EngineClient->IsInGame() || !I::EngineClient->IsConnected())
		return;

	CTFPlayer* pLocal = H::Entities.GetLocal();
	if (!pLocal)
		return;

	for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
	{
		if (i == I::EngineClient->GetLocalPlayer())
			continue;

		CTFPlayer* pPlayer = reinterpret_cast<CTFPlayer*>(I::ClientEntityList->GetClientEntity(i));
		if (!pPlayer || !pPlayer->IsAlive())
			continue;

		PlayerInfo_t playerInfo{};
		if (!I::EngineClient->GetPlayerInfo(i, &playerInfo))
			continue;

		if (playerInfo.fakeplayer)
			continue;


		if (H::Entities.IsFriend(i) ||
			H::Entities.InParty(i) ||
			F::PlayerUtils.IsIgnored(i) ||
			F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
			F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG)))
			continue;

		uint64_t steamID64 = ((uint64_t)1 << 56) | ((uint64_t)1 << 52) | ((uint64_t)1 << 32) | playerInfo.friendsID;
		reportFunc(steamID64, 1);
	}
}


std::string CMisc::ProcessTextReplacements(std::string text)
{
	if (!Vars::Misc::Automation::ChatSpam::TextReplace.Value)
		return text;

	auto pLocal = H::Entities.GetLocal();
	if (!pLocal)
		return text;

	std::vector<int> allPlayers;
	std::vector<int> enemyPlayers;
	std::vector<int> friendlyPlayers;

	auto pResource = H::Entities.GetPR();
	if (!pResource)
		return text;

	int localTeam = pLocal->m_iTeamNum();

	for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
	{
		if (!pResource->m_bValid(i) || !pResource->m_bConnected(i))
			continue;

		if (i == I::EngineClient->GetLocalPlayer())
			continue;

		PlayerInfo_t pi{};
		if (!I::EngineClient->GetPlayerInfo(i, &pi) || pi.fakeplayer)
			continue;

		allPlayers.push_back(i);

		int playerTeam = pResource->m_iTeam(i);
		if (playerTeam == localTeam)
			friendlyPlayers.push_back(i);
		else if (playerTeam > 1) // Not spectator
			enemyPlayers.push_back(i);
	}

	PlayerInfo_t localPi{};
	std::string localName = "Player";
	if (I::EngineClient->GetPlayerInfo(I::EngineClient->GetLocalPlayer(), &localPi))
		localName = localPi.name;

	std::string enemyTeamName = "Unknown";
	std::string friendlyTeamName = "Unknown";
	std::string enemyTeamColor = "";
	std::string friendlyTeamColor = "";

	switch (localTeam)
	{
	case 2: // Case if local player is RED
		friendlyTeamName = "RED";
		enemyTeamName = "BLU";
		friendlyTeamColor = "\x07\x01XXXXXFF4040"; // We pick red for our team (%fteamc%)
		enemyTeamColor = "\x07\x01XXXXX99CCFF";   // We pick blue for enemy team (%eteamc%)
		break;
	case 3: // Case if local player is BLU
		friendlyTeamName = "BLU";
		enemyTeamName = "RED";
		friendlyTeamColor = "\x07\x01XXXXX99CCFF"; // We pick blue for our team (%fteamc%)
		enemyTeamColor = "\x07\x01XXXXXFF4040";   // We pick red for enemy team (%eteamc%)
		break;
	}

	if (text.find("%randomplayer%") != std::string::npos && !allPlayers.empty())
	{
		std::vector<int> validPlayers;
		for (int playerIdx : allPlayers)
		{
			uint32_t uFriendsID = F::PlayerUtils.GetFriendsID(playerIdx);


			bool isProtected = false;
			if (uFriendsID > 0)
			{
				isProtected = F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(IGNORED_TAG)) ||
					F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(FRIEND_TAG)) ||
					F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
					F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG)) ||
					H::Entities.IsFriend(uFriendsID) ||
					H::Entities.InParty(uFriendsID);
			}
			else
			{

				isProtected = F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(IGNORED_TAG)) ||
					F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(FRIEND_TAG)) ||
					F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
					F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG));
			}

			if (!isProtected)
			{
				validPlayers.push_back(playerIdx);
			}
		}

		if (!validPlayers.empty())
		{
			int idx = SDK::RandomInt(0, validPlayers.size() - 1);
			int playerIdx = validPlayers[idx];
			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(playerIdx, &pi))
			{
				std::string playerName = F::PlayerUtils.GetPlayerName(playerIdx, pi.name);
				size_t pos;
				while ((pos = text.find("%randomplayer%")) != std::string::npos)
					text.replace(pos, 14, playerName);
			}
		}
	}

	if (text.find("%randomenemy%") != std::string::npos && !enemyPlayers.empty())
	{
		std::vector<int> validEnemies;
		for (int playerIdx : enemyPlayers)
		{
			uint32_t uFriendsID = F::PlayerUtils.GetFriendsID(playerIdx);


			bool isProtected = false;
			if (uFriendsID > 0)
			{
				isProtected = F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(IGNORED_TAG)) ||
					F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(FRIEND_TAG)) ||
					F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
					F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG)) ||
					H::Entities.IsFriend(uFriendsID) ||
					H::Entities.InParty(uFriendsID);
			}
			else
			{
				isProtected = F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(IGNORED_TAG)) ||
					F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(FRIEND_TAG)) ||
					F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
					F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG));
			}

			if (!isProtected)
			{
				validEnemies.push_back(playerIdx);
			}
		}

		if (!validEnemies.empty())
		{
			int idx = SDK::RandomInt(0, validEnemies.size() - 1);
			int playerIdx = validEnemies[idx];
			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(playerIdx, &pi))
			{
				std::string playerName = F::PlayerUtils.GetPlayerName(playerIdx, pi.name);
				size_t pos;
				while ((pos = text.find("%randomenemy%")) != std::string::npos)
					text.replace(pos, 13, playerName);
			}
		}
	}

	if (text.find("%randomfriendly%") != std::string::npos && !friendlyPlayers.empty())
	{
		std::vector<int> validFriendlies;
		for (int playerIdx : friendlyPlayers)
		{
			uint32_t uFriendsID = F::PlayerUtils.GetFriendsID(playerIdx);


			bool isProtected = false;
			if (uFriendsID > 0)
			{
				isProtected = F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(IGNORED_TAG)) ||
					F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(FRIEND_TAG)) ||
					F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
					F::PlayerUtils.HasTag(uFriendsID, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG)) ||
					H::Entities.IsFriend(uFriendsID) ||
					H::Entities.InParty(uFriendsID);
			}
			else
			{
				isProtected = F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(IGNORED_TAG)) ||
					F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(FRIEND_TAG)) ||
					F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
					F::PlayerUtils.HasTag(playerIdx, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG));
			}

			if (!isProtected)
			{
				validFriendlies.push_back(playerIdx);
			}
		}

		if (!validFriendlies.empty())
		{
			int idx = SDK::RandomInt(0, validFriendlies.size() - 1);
			int playerIdx = validFriendlies[idx];
			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(playerIdx, &pi))
			{
				std::string playerName = F::PlayerUtils.GetPlayerName(playerIdx, pi.name);
				size_t pos;
				while ((pos = text.find("%randomfriendly%")) != std::string::npos)
					text.replace(pos, 16, playerName);
			}
		}
	}

	if (text.find("%lastkilled%") != std::string::npos && !m_sLastKilledPlayerName.empty())
	{
		size_t pos;
		while ((pos = text.find("%lastkilled%")) != std::string::npos)
			text.replace(pos, 12, m_sLastKilledPlayerName);
	}

	if (text.find("%urname%") != std::string::npos)
	{
		size_t pos;
		while ((pos = text.find("%urname%")) != std::string::npos)
			text.replace(pos, 8, localName);
	}

	if (text.find("%team%") != std::string::npos)
	{
		std::string teamName = "Unknown";
		switch (localTeam)
		{
		case 2: teamName = "RED"; break;
		case 3: teamName = "BLU"; break;
		}

		size_t pos;
		while ((pos = text.find("%team%")) != std::string::npos)
			text.replace(pos, 6, teamName);
	}

	// Replance team names
	if (text.find("%enemyteam%") != std::string::npos)
	{
		size_t pos;
		while ((pos = text.find("%enemyteam%")) != std::string::npos)
			text.replace(pos, 11, enemyTeamName);
	}

	if (text.find("%friendlyteam%") != std::string::npos)
	{
		size_t pos;
		while ((pos = text.find("%friendlyteam%")) != std::string::npos)
			text.replace(pos, 14, friendlyTeamName);
	}

	// Replace team colors
	if (text.find("%eteamc%") != std::string::npos)
	{
		size_t pos;
		while ((pos = text.find("%eteamc%")) != std::string::npos)
			text.replace(pos, 8, enemyTeamColor);
	}

	if (text.find("%fteamc%") != std::string::npos)
	{
		size_t pos;
		while ((pos = text.find("%fteamc%")) != std::string::npos)
			text.replace(pos, 8, friendlyTeamColor);
	}
	if (text.find("%initiator%") != std::string::npos || text.find("%target%") != std::string::npos)
	{
		SDK::Output("TextReplace", std::format("Processing vote-related replacements. Initiator: '{}', Target: '{}'",
			m_sVoteInitiatorName, m_sVoteTargetName).c_str(), {}, true, true);
	}

	if (text.find("%initiator%") != std::string::npos && !m_sVoteInitiatorName.empty())
	{
		size_t pos;
		while ((pos = text.find("%initiator%")) != std::string::npos)
		{
			text.replace(pos, 11, m_sVoteInitiatorName);
			SDK::Output("TextReplace", std::format("Replaced %initiator% with '{}'", m_sVoteInitiatorName).c_str(), {}, true, true);
		}
	}

	if (text.find("%target%") != std::string::npos && !m_sVoteTargetName.empty())
	{
		size_t pos;
		while ((pos = text.find("%target%")) != std::string::npos)
		{
			text.replace(pos, 8, m_sVoteTargetName);
			SDK::Output("TextReplace", std::format("Replaced %target% with '{}'", m_sVoteTargetName).c_str(), {}, true, true);
		}
	}
	return text;
}

void CMisc::KillSay(int victim)
{
	if (!Vars::Misc::Automation::ChatSpam::KillSay.Value || !I::EngineClient)
		return;

	auto pLocal = H::Entities.GetLocal();
	if (!pLocal || !pLocal->IsAlive())
		return;

	PlayerInfo_t victimInfo{};
	if (I::EngineClient->GetPlayerInfo(victim, &victimInfo))
	{
		m_iLastKilledPlayer = victim;
		m_sLastKilledPlayerName = F::PlayerUtils.GetPlayerName(victim, victimInfo.name);
	}

	if (m_vKillSayLines.empty())
	{
		std::vector<std::string> defaults = {
			"Get owned %lastkilled%",
			"Nice try %lastkilled%, better luck next time",
			"%lastkilled% just got destroyed by %urname%",
			"%team% > all",
			"Skill issue, %lastkilled%"
		};
		LoadLines("KillSay", "killsay.txt", defaults, m_vKillSayLines);
	}

	if (m_vKillSayLines.empty())
		return;

	std::string killsayLine;
	const size_t lineCount = m_vKillSayLines.size();

	int randomIndex = SDK::RandomInt(0, lineCount - 1);
	if (randomIndex >= 0 && randomIndex < static_cast<int>(lineCount))
		killsayLine = m_vKillSayLines[randomIndex];
	else
		killsayLine = "Get owned %lastkilled%";

	killsayLine = ProcessTextReplacements(killsayLine);

	if (killsayLine.length() > 150)
		killsayLine = killsayLine.substr(0, 150);

	std::string chatCommand;
	if (Vars::Misc::Automation::ChatSpam::TeamChat.Value)
		chatCommand = "say_team \"" + killsayLine + "\"";
	else
		chatCommand = "say \"" + killsayLine + "\"";

	if (I::EngineClient)
	{
		SDK::Output("KillSay", std::format("Sending: {}", chatCommand).c_str(), {}, true, true);
		I::EngineClient->ClientCmd_Unrestricted(chatCommand.c_str());
	}
}

void CMisc::AutoReply(int speaker, const char* text)
{
	if (!Vars::Misc::Automation::ChatSpam::AutoReply.Value || !I::EngineClient || !text)
		return;

	auto pLocal = H::Entities.GetLocal();
	if (!pLocal || !pLocal->IsAlive())
		return;

	
	if (H::Entities.IsFriend(speaker) ||
		H::Entities.InParty(speaker) ||
		F::PlayerUtils.IsIgnored(speaker) ||
		F::PlayerUtils.HasTag(speaker, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
		F::PlayerUtils.HasTag(speaker, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG)))
	{
		return;
	}

	
	std::string message = text;
	std::transform(message.begin(), message.end(), message.begin(), ::tolower);

	
	std::string cleanMessage = message;
	
	for (char& c : cleanMessage) {
		if (!std::isalnum(c) && c != ' ') {
			c = ' ';
		}
	}

	PlayerInfo_t speakerInfo{};
	std::string speakerName;
	if (I::EngineClient->GetPlayerInfo(speaker, &speakerInfo))
		speakerName = F::PlayerUtils.GetPlayerName(speaker, speakerInfo.name);
	else
		speakerName = "Player";

	if (m_mAutoReplyTriggers.empty())
	{
		LoadAutoReplyConfig();
	}

	if (m_mAutoReplyTriggers.empty())
		return;

	std::string foundTrigger;
	std::vector<std::string> possibleResponses;

	
	for (const auto& pair : m_mAutoReplyTriggers)
	{
		const std::string& trigger = pair.first;

		
		if (message.find(trigger) != std::string::npos)
		{
			foundTrigger = trigger;
			possibleResponses = pair.second;

			
			SDK::Output("AutoReply", std::format("Found trigger '{}' in message '{}'", trigger, message).c_str(), {}, true, true);
			break;
		}

		// The second method: a specific word search.
		// You can use this one if you want.
		/*
		size_t pos = 0;
		while ((pos = cleanMessage.find(trigger, pos)) != std::string::npos)
		{
			bool isWordStart = (pos == 0) || std::isspace(cleanMessage[pos - 1]);
			bool isWordEnd = (pos + trigger.length() == cleanMessage.length()) ||
							 std::isspace(cleanMessage[pos + trigger.length()]);

			if (isWordStart && isWordEnd)
			{
				foundTrigger = trigger;
				possibleResponses = pair.second;
				SDK::Output("AutoReply", std::format("Found word trigger '{}' in message '{}'", trigger, message).c_str(), {}, true, true);
				break;
			}
			pos++;
		}

		if (!foundTrigger.empty())
			break;
		*/
	}

	if (foundTrigger.empty() || possibleResponses.empty())
	{
		
		SDK::Output("AutoReply", std::format("No trigger found in message: '{}'", message).c_str(), {}, true, true);
		return;
	}

	
	std::string response;
	if (possibleResponses.size() == 1)
	{
		response = possibleResponses[0];
	}
	else
	{
		int randomIndex = SDK::RandomInt(0, static_cast<int>(possibleResponses.size()) - 1);
		response = possibleResponses[randomIndex];
	}

	response = ProcessTextReplacements(response);

	
	size_t pos = 0;
	while ((pos = response.find("%triggername%", pos)) != std::string::npos)
	{
		response.replace(pos, 13, speakerName);
		pos += speakerName.length();
	}

	
	if (response.find("%randomenemy%") != std::string::npos || response.find("%randomfriendly%") != std::string::npos)
	{
		std::vector<int> enemyPlayers, friendlyPlayers;
		auto pResource = H::Entities.GetPR();
		if (pResource)
		{
			int localPlayer = I::EngineClient->GetLocalPlayer();
			int localTeam = pResource->m_iTeam(localPlayer);

			for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
			{
				if (!pResource->m_bValid(i) || !pResource->m_bConnected(i) || i == localPlayer)
					continue;

				
				if (H::Entities.IsFriend(i) ||
					H::Entities.InParty(i) ||
					F::PlayerUtils.IsIgnored(i) ||
					F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
					F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG)))
					continue;

				int playerTeam = pResource->m_iTeam(i);
				if (playerTeam != localTeam)
					enemyPlayers.push_back(i);
				else
					friendlyPlayers.push_back(i);
			}
		}

		if (response.find("%randomenemy%") != std::string::npos && !enemyPlayers.empty())
		{
			int idx = SDK::RandomInt(0, enemyPlayers.size() - 1);
			int playerIdx = enemyPlayers[idx];
			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(playerIdx, &pi))
			{
				std::string playerName = F::PlayerUtils.GetPlayerName(playerIdx, pi.name);
				pos = 0;
				while ((pos = response.find("%randomenemy%", pos)) != std::string::npos)
				{
					response.replace(pos, 13, playerName);
					pos += playerName.length();
				}
			}
		}

		if (response.find("%randomfriendly%") != std::string::npos && !friendlyPlayers.empty())
		{
			int idx = SDK::RandomInt(0, friendlyPlayers.size() - 1);
			int playerIdx = friendlyPlayers[idx];
			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(playerIdx, &pi))
			{
				std::string playerName = F::PlayerUtils.GetPlayerName(playerIdx, pi.name);
				pos = 0;
				while ((pos = response.find("%randomfriendly%", pos)) != std::string::npos)
				{
					response.replace(pos, 16, playerName);
					pos += playerName.length();
				}
			}
		}
	}

	if (response.length() > 150)
		response = response.substr(0, 150);

	std::string chatCommand;
	if (Vars::Misc::Automation::ChatSpam::TeamChat.Value)
		chatCommand = "say_team \"" + response + "\"";
	else
		chatCommand = "say \"" + response + "\"";

	if (I::EngineClient)
	{
		SDK::Output("AutoReply", std::format("Sending: {}", chatCommand).c_str(), {}, true, true);
		I::EngineClient->ClientCmd_Unrestricted(chatCommand.c_str());
	}
}

void CMisc::LoadAutoReplyConfig()
{
	m_mAutoReplyTriggers.clear();

	try
	{
		char gamePath[MAX_PATH];
		GetModuleFileNameA(GetModuleHandleA("tf_win64.exe"), gamePath, MAX_PATH);
		std::string gameDir = gamePath;
		size_t lastSlash = gameDir.find_last_of("\\/");
		if (lastSlash != std::string::npos)
			gameDir = gameDir.substr(0, lastSlash);

		std::vector<std::string> pathsToTry = {
			"autoreply.txt",
			gameDir + "\\amalgam\\autoreply.txt"
		};

		bool fileLoaded = false;

		for (const auto& path : pathsToTry)
		{
			try
			{
				std::ifstream file(path);
				if (file.is_open())
				{
					std::string line;
					while (std::getline(file, line))
					{
						if (!line.empty() && line.find('=') != std::string::npos)
						{
							size_t equalPos = line.find('=');
							std::string trigger = line.substr(0, equalPos);
							std::string response = line.substr(equalPos + 1);

							trigger.erase(0, trigger.find_first_not_of(" \t"));
							trigger.erase(trigger.find_last_not_of(" \t") + 1);
							response.erase(0, response.find_first_not_of(" \t"));
							response.erase(response.find_last_not_of(" \t") + 1);

							if (!trigger.empty() && !response.empty())
							{
								std::string lowerTrigger = trigger;
								std::transform(lowerTrigger.begin(), lowerTrigger.end(), lowerTrigger.begin(), ::tolower);
								m_mAutoReplyTriggers[lowerTrigger].push_back(response);
							}
						}
					}
					file.close();

					int totalResponses = 0;
					for (const auto& pair : m_mAutoReplyTriggers)
						totalResponses += pair.second.size();

					SDK::Output("AutoReply", std::format("Loaded {} triggers with {} total responses from {}",
						m_mAutoReplyTriggers.size(), totalResponses, path).c_str(), {}, true, true);
					fileLoaded = true;
					break;
				}
			}
			catch (...)
			{
				continue;
			}
		}

		if (!fileLoaded)
		{
			try
			{
				std::string defaultPath = gameDir + "\\amalgam\\autoreply.txt";
				std::ofstream newFile(defaultPath);

				if (newFile.is_open())
				{
					
					newFile << "bot = I'm not a bot, %triggername%!\n";
					newFile << "bots = Bots? You wish, %triggername%!\n";
					newFile << "cheater = Cheater? Skill issue, %triggername%!\n";
					newFile << "hacker = Hacker? Mad cuz bad, %triggername%!\n";
					newFile << "aimbot = Aimbot? Get good, %triggername%!\n";
					newFile.close();

					LoadAutoReplyConfig();

					SDK::Output("AutoReply", std::format("Created default file at {}", defaultPath).c_str(), {}, true, true);
					fileLoaded = true;
				}
			}
			catch (...)
			{
			}
		}

		if (!fileLoaded || m_mAutoReplyTriggers.empty())
		{
			SDK::Output("AutoReply", "Failed to load autoreply file, using built-in triggers", {}, true, true);
			m_mAutoReplyTriggers["bot"].push_back("I'm not a bot, you're a bot!");
			m_mAutoReplyTriggers["bots"].push_back("Bots? You wish!");
			m_mAutoReplyTriggers["cheater"].push_back("Cheater? Skill issue!");
			m_mAutoReplyTriggers["hacker"].push_back("Hacker? Mad cuz bad!");
		}
	}
	catch (...)
	{
		m_mAutoReplyTriggers["bot"].push_back("I'm not a bot, you're a bot!");
		m_mAutoReplyTriggers["bots"].push_back("Bots? You wish!");
		m_mAutoReplyTriggers["cheater"].push_back("Cheater? Skill issue!");
	}
}

void CMisc::VotekickResponse(bf_read& msgData)
{
	if (!Vars::Misc::Automation::ChatSpam::VotekickResponse.Value || !I::EngineClient)
		return;

	auto pLocal = H::Entities.GetLocal();
	if (!pLocal)
		return;

	
	if (m_mVotekickResponses.empty())
	{
		LoadVotekickConfig();
	}

	
	int totalBits = msgData.GetNumBitsLeft();
	int totalBytes = (totalBits + 7) / 8;
	SDK::Output("VotekickDebug", std::format("Message size: {} bytes ({} bits)", totalBytes, totalBits).c_str(), {}, true, true);

	
	std::string hexDump;
	msgData.Seek(0);
	for (int i = 0; i < totalBytes && !msgData.IsOverflowed(); i++)
	{
		unsigned char byte = msgData.ReadByte();
		hexDump += std::format("{:02X} ", byte);
	}
	SDK::Output("VotekickDebug", std::format("Raw data: {}", hexDump).c_str(), {}, true, true);

	
	msgData.Seek(0);
	int iTeam = msgData.ReadByte();
	/*int iVoteID =*/ msgData.ReadLong();
	int iCaller = msgData.ReadByte();
	char sReason[256] = { 0 };
	msgData.ReadString(sReason, sizeof(sReason), true);
	char sTarget[256] = { 0 };
	msgData.ReadString(sTarget, sizeof(sTarget), true);
	int iTarget = msgData.ReadByte() >> 1;

	
	SDK::Output("VotekickDebug", std::format("Parsed: Team: {}, Caller: {}, Reason: '{}', TargetName: '{}', Target: {}",
		iTeam, iCaller, sReason, sTarget, iTarget).c_str(), {}, true, true);

	
	if (iCaller < 1 || iCaller > 32 || iTarget < 1 || iTarget > 32)
	{
		SDK::Output("VotekickDebug", "Invalid caller or target index, skipping", {}, true, true);
		return;
	}

	
	PlayerInfo_t piCaller{}, piTarget{};
	std::string callerName = "Unknown", targetName = "Unknown";
	if (I::EngineClient->GetPlayerInfo(iCaller, &piCaller))
		callerName = F::PlayerUtils.GetPlayerName(iCaller, piCaller.name);
	if (I::EngineClient->GetPlayerInfo(iTarget, &piTarget))
		targetName = F::PlayerUtils.GetPlayerName(iTarget, piTarget.name);

	SDK::Output("VotekickDebug", std::format("Vote by {} ({}) on {} ({})",
		callerName, iCaller, targetName, iTarget).c_str(), {}, true, true);

	
	int localPlayer = I::EngineClient->GetLocalPlayer();
	bool isLocalTarget = (iTarget == localPlayer);
	bool isFriendTarget = H::Entities.IsFriend(iTarget);
	bool isPartyTarget = H::Entities.InParty(iTarget);
	bool isIgnoredTarget = F::PlayerUtils.IsIgnored(iTarget);
	bool isBotTarget = F::PlayerUtils.HasTag(iTarget, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG));
	bool isBotCaller = F::PlayerUtils.HasTag(iCaller, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG));

	std::string responseType;
	if (isLocalTarget || isFriendTarget || isPartyTarget || isIgnoredTarget || isBotTarget)
		responseType = "protest"; 
	else if (isBotCaller)
		responseType = "support"; 
	else
		responseType = "support"; 

	
	m_sVoteInitiatorName = callerName;
	m_sVoteTargetName = targetName;

	
	std::string response;

	
	if (m_mVotekickResponses.find(responseType) != m_mVotekickResponses.end() &&
		!m_mVotekickResponses[responseType].empty())
	{
		const auto& responses = m_mVotekickResponses[responseType];

		
		if (responses.size() == 1)
		{
			response = responses[0];
		}
		else
		{
			int randomIndex = SDK::RandomInt(0, static_cast<int>(responses.size()) - 1);
			response = responses[randomIndex];
		}

		SDK::Output("VotekickDebug", std::format("Selected response '{}' from {} available for type '{}'",
			response, responses.size(), responseType).c_str(), {}, true, true);
	}
	else
	{
		
		if (responseType == "protest")
			response = "f2 they're legit";
		else
			response = "f1 cheater";

		SDK::Output("VotekickDebug", std::format("Using fallback response: '{}'", response).c_str(), {}, true, true);
	}

	
	response = ProcessTextReplacements(response);

	
	std::string chatCommand;
	if (Vars::Misc::Automation::ChatSpam::TeamChat.Value)
		chatCommand = "say_team \"" + response + "\"";
	else
		chatCommand = "say \"" + response + "\"";

	SDK::Output("VotekickResponse", std::format("Auto-responding to vote by {} on {}: {}",
		callerName, targetName, chatCommand).c_str(), {}, true, true);
	I::EngineClient->ClientCmd_Unrestricted(chatCommand.c_str());
}

void CMisc::LoadVotekickConfig()
{
	m_mVotekickResponses.clear();

	try
	{
		char gamePath[MAX_PATH] = { 0 };
		HMODULE hModule = GetModuleHandleA("tf_win64.exe");
		if (!hModule)
		{
			SDK::Output("VotekickDebug", "Failed to get tf_win64.exe module handle", {}, true, true);
			hModule = GetModuleHandleA(nullptr);
		}

		if (!GetModuleFileNameA(hModule, gamePath, MAX_PATH))
		{
			SDK::Output("VotekickDebug", "Failed to get module filename", {}, true, true);
			
			m_mVotekickResponses["support"].push_back("f1 cheater");
			m_mVotekickResponses["protest"].push_back("f2 they're legit");
			return;
		}

		std::string gameDir = gamePath;
		size_t lastSlash = gameDir.find_last_of("\\/");
		if (lastSlash != std::string::npos)
			gameDir = gameDir.substr(0, lastSlash);

		SDK::Output("VotekickDebug", std::format("Game directory: {}", gameDir).c_str(), {}, true, true);

		std::vector<std::string> pathsToTry = {
			"votekick.txt",
			gameDir + "\\Amalgam\\votekick.txt",
			gameDir + "\\amalgam\\votekick.txt"
		};

		bool fileLoaded = false;
		std::string loadedPath;

		
		for (const auto& path : pathsToTry)
		{
			SDK::Output("VotekickDebug", std::format("Trying to load: {}", path).c_str(), {}, true, true);

			try
			{
				std::ifstream file(path);
				if (file.is_open() && file.good())
				{
					std::string line;
					int linesProcessed = 0;

					while (std::getline(file, line))
					{
						
						if (line.empty() || line[0] == '#' || line[0] == ';')
							continue;

						size_t equalPos = line.find('=');
						if (equalPos != std::string::npos)
						{
							std::string trigger = line.substr(0, equalPos);
							std::string response = line.substr(equalPos + 1);

							
							trigger.erase(0, trigger.find_first_not_of(" \t\r\n"));
							trigger.erase(trigger.find_last_not_of(" \t\r\n") + 1);
							response.erase(0, response.find_first_not_of(" \t\r\n"));
							response.erase(response.find_last_not_of(" \t\r\n") + 1);

							if (!trigger.empty() && !response.empty())
							{
								
								std::string lowerTrigger = trigger;
								std::transform(lowerTrigger.begin(), lowerTrigger.end(), lowerTrigger.begin(), ::tolower);
								m_mVotekickResponses[lowerTrigger].push_back(response);
								linesProcessed++;

								SDK::Output("VotekickDebug", std::format("Added: '{}' -> '{}'", lowerTrigger, response).c_str(), {}, true, true);
							}
						}
					}
					file.close();

					if (linesProcessed > 0)
					{
						int totalResponses = 0;
						for (const auto& pair : m_mVotekickResponses)
							totalResponses += static_cast<int>(pair.second.size());

						SDK::Output("VotekickResponse", std::format("Successfully loaded {} triggers with {} total responses from '{}'",
							m_mVotekickResponses.size(), totalResponses, path).c_str(), {}, true, true);

						fileLoaded = true;
						loadedPath = path;
						break;
					}
					else
					{
						SDK::Output("VotekickDebug", std::format("File '{}' exists but no valid entries found", path).c_str(), {}, true, true);
					}
				}
				else
				{
					SDK::Output("VotekickDebug", std::format("Cannot open file: {}", path).c_str(), {}, true, true);
				}
			}
			catch (const std::exception& e)
			{
				SDK::Output("VotekickDebug", std::format("Exception loading '{}': {}", path, e.what()).c_str(), {}, true, true);
				continue;
			}
		}

		
		if (!fileLoaded)
		{
			SDK::Output("VotekickDebug", "No existing config found, creating default file", {}, true, true);

			try
			{
				std::string defaultPath = gameDir + "\\Amalgam\\votekick.txt";

				
				std::string dirPath = gameDir + "\\Amalgam";
				CreateDirectoryA(dirPath.c_str(), nullptr);

				SDK::Output("VotekickDebug", std::format("Creating default config at: {}", defaultPath).c_str(), {}, true, true);

				std::ofstream newFile(defaultPath);
				if (newFile.is_open())
				{
					newFile << "# Votekick auto-response configuration\n";
					newFile << "# Format: trigger = response\n";
					newFile << "# Available placeholders: %initiator%, %target%\n";
					newFile << "\n";
					newFile << "support = f1 lads\n";
					newFile << "support = f1 on %initiator%'s vote against %target%\n";
					newFile << "support = f1, %target% is sus\n";
					newFile << "support = yeah %initiator%, %target% needs to go\n";
					newFile << "support = f1 cheater detected\n";
					newFile << "\n";
					newFile << "protest = f2\n";
					newFile << "protest = f2, %target% is legit\n";
					newFile << "protest = %initiator%, %target% is not cheating\n";
					newFile << "protest = f2, %initiator%, why %target%?\n";
					newFile << "protest = f2 they're clean\n";
					newFile.close();

					SDK::Output("VotekickResponse", std::format("Created default config file at '{}'", defaultPath).c_str(), {}, true, true);

					
					try
					{
						std::ifstream testFile(defaultPath);
						if (testFile.is_open())
						{
							std::string line;
							int linesProcessed = 0;

							while (std::getline(testFile, line))
							{
								if (line.empty() || line[0] == '#' || line[0] == ';')
									continue;

								size_t equalPos = line.find('=');
								if (equalPos != std::string::npos)
								{
									std::string trigger = line.substr(0, equalPos);
									std::string response = line.substr(equalPos + 1);

									trigger.erase(0, trigger.find_first_not_of(" \t\r\n"));
									trigger.erase(trigger.find_last_not_of(" \t\r\n") + 1);
									response.erase(0, response.find_first_not_of(" \t\r\n"));
									response.erase(response.find_last_not_of(" \t\r\n") + 1);

									if (!trigger.empty() && !response.empty())
									{
										std::string lowerTrigger = trigger;
										std::transform(lowerTrigger.begin(), lowerTrigger.end(), lowerTrigger.begin(), ::tolower);
										m_mVotekickResponses[lowerTrigger].push_back(response);
										linesProcessed++;
									}
								}
							}
							testFile.close();

							if (linesProcessed > 0)
							{
								fileLoaded = true;
								loadedPath = defaultPath;

								int totalResponses = 0;
								for (const auto& pair : m_mVotekickResponses)
									totalResponses += static_cast<int>(pair.second.size());

								SDK::Output("VotekickResponse", std::format("Loaded {} triggers with {} total responses from created file",
									m_mVotekickResponses.size(), totalResponses).c_str(), {}, true, true);
							}
						}
					}
					catch (const std::exception& e)
					{
						SDK::Output("VotekickDebug", std::format("Exception reading created file: {}", e.what()).c_str(), {}, true, true);
					}
				}
				else
				{
					SDK::Output("VotekickDebug", std::format("Failed to create file at: {}", defaultPath).c_str(), {}, true, true);
				}
			}
			catch (const std::exception& e)
			{
				SDK::Output("VotekickDebug", std::format("Exception creating default file: {}", e.what()).c_str(), {}, true, true);
			}
		}

		
		if (!fileLoaded || m_mVotekickResponses.empty())
		{
			SDK::Output("VotekickResponse", "All loading attempts failed, using built-in fallback responses", {}, true, true);
			m_mVotekickResponses.clear();
			m_mVotekickResponses["support"].push_back("f1 cheater");
			m_mVotekickResponses["support"].push_back("f1 lads");
			m_mVotekickResponses["protest"].push_back("f2 they're legit");
			m_mVotekickResponses["protest"].push_back("f2");
		}
		else
		{
			
			int totalResponses = 0;
			for (const auto& pair : m_mVotekickResponses)
			{
				totalResponses += static_cast<int>(pair.second.size());
				SDK::Output("VotekickDebug", std::format("Trigger '{}': {} responses", pair.first, pair.second.size()).c_str(), {}, true, true);
			}
			SDK::Output("VotekickResponse", std::format("Config loaded successfully: {} triggers, {} total responses from '{}'",
				m_mVotekickResponses.size(), totalResponses, loadedPath).c_str(), {}, true, true);
		}
	}
	catch (const std::exception& e)
	{
		SDK::Output("VotekickResponse", std::format("Critical exception in LoadVotekickConfig: {}", e.what()).c_str(), {}, true, true);
		m_mVotekickResponses.clear();
		m_mVotekickResponses["support"].push_back("f1 cheater");
		m_mVotekickResponses["protest"].push_back("f2 they're legit");
	}
	catch (...)
	{
		SDK::Output("VotekickResponse", "Unknown exception in LoadVotekickConfig, using fallback responses", {}, true, true);
		m_mVotekickResponses.clear();
		m_mVotekickResponses["support"].push_back("f1 cheater");
		m_mVotekickResponses["protest"].push_back("f2 they're legit");
	}
}

void CMisc::ChatRelay(int speaker, const char* text, bool teamChat)
{
	if (!Vars::Misc::Automation::ChatRelay::Enable.Value)
		return;

	if (!m_bChatRelayInitialized)
	{
		InitializeChatRelay();
		if (!m_bChatRelayInitialized)
			return;
	}

	std::string messageText = text ? text : "";
	std::string playerName = "Unknown";
	
	PlayerInfo_t playerInfo{};
	if (I::EngineClient->GetPlayerInfo(speaker, &playerInfo))
	{
		playerName = F::PlayerUtils.GetPlayerName(speaker, playerInfo.name);
	}

	uint32_t steamId32 = F::PlayerUtils.GetFriendsID(speaker);
	std::string serverIP = "offline";
	if (auto pNetInfo = I::EngineClient->GetNetChannelInfo(); pNetInfo)
	{
		serverIP = pNetInfo->GetAddress();
	}

	// Create hash from message content + player name + timestamp (rounded to nearest second)
	time_t currentTime = time(nullptr);
	std::string hashInput = messageText + playerName + std::to_string(currentTime);
	
	// Simple hash function
	std::hash<std::string> hasher;
	std::string messageHash = std::to_string(hasher(hashInput));

	// Check if we should log this message
	if (!ShouldLogMessage(messageHash))
		return;

	try
	{
		const std::string timestamp = FormatTime(std::chrono::system_clock::now(), "%Y-%m-%d %H:%M:%S");

		std::string teamName = "UNK";
		if (auto pResource = H::Entities.GetPR(); pResource && pResource->m_bValid(speaker))
			teamName = TeamToString(pResource->m_iTeam(speaker));

		const std::string escapedMessage     = EscapeJson(messageText);
		const std::string escapedPlayerName  = EscapeJson(playerName);
		const std::string escapedServerName  = EscapeJson(m_sServerIdentifier);
		const std::string escapedServerIP   = EscapeJson(serverIP);

		// Create JSON log entry
		std::string logEntry = std::format(
			"{{\"timestamp\":\"{}\",\"server\":\"{}\",\"serverip\":\"{}\",\"player\":\"{}\",\"steamid32\":{},\"team\":\"{}\",\"message\":\"{}\",\"teamchat\":{}}}\n",
			timestamp, escapedServerName, escapedServerIP, escapedPlayerName, steamId32, teamName, escapedMessage, teamChat ? "true" : "false"
		);

		// Write to file
		std::ofstream logFile(m_sChatRelayPath, std::ios::app);
		if (logFile.is_open())
		{
			logFile << logEntry;
			logFile.close();
		}

		// Add to recent messages
		m_setRecentMessageHashes.insert(messageHash);

		// Cleanup old messages periodically
		if (m_tCleanupTimer.Run(30.0f))
		{
			CleanupOldMessages();
		}
	}
	catch (...)
	{
		// Silently fail if logging fails
	}
}

void CMisc::InitializeChatRelay()
{
	if (m_bChatRelayInitialized)
		return;

	try
	{
		// Get AppData path
		std::string appDataPath = GetAppDataPath();
		if (appDataPath.empty())
			return;

		// Create Amalgam directory in AppData
		std::string amalgamDir = appDataPath + "\\Amalgam";
		CreateDirectoryA(amalgamDir.c_str(), nullptr);

		// Set up chat relay path
		m_sChatRelayPath = GetChatRelayPath();
		if (m_sChatRelayPath.empty())
			return;

		// Get server identifier
		m_sServerIdentifier = GetServerIdentifier();

		// Initialize cleanup timer
		m_tCleanupTimer.Update();

		m_bChatRelayInitialized = true;
		
		SDK::Output("ChatRelay", std::format("Initialized chat relay: {}", m_sChatRelayPath).c_str(), {}, true, true);
	}
	catch (...)
	{
		m_bChatRelayInitialized = false;
	}
}

std::string CMisc::GetAppDataPath()
{
	char* appDataPath = nullptr;
	size_t len = 0;
	
	if (_dupenv_s(&appDataPath, &len, "APPDATA") == 0 && appDataPath != nullptr)
	{
		std::string result = appDataPath;
		free(appDataPath);
		return result;
	}
	
	return "";
}

std::string CMisc::GetChatRelayPath()
{
	std::string appDataPath = GetAppDataPath();
	if (appDataPath.empty())
		return "";

	const std::string dateStr = FormatTime(std::chrono::system_clock::now(), "%Y%m%d");
	const std::string filename = std::format("chat_relay_{}.log", dateStr);
	return appDataPath + "\\Amalgam\\" + filename;
}

bool CMisc::IsPrimaryBot()
{
	if (!I::EngineClient || !I::EngineClient->IsInGame())
		return false;

	// Get local player index
	int localPlayerIndex = I::EngineClient->GetLocalPlayer();
	if (localPlayerIndex <= 0)
		return false;

	// For multiple bot instances, use the lowest entity index as primary
	// This is a simple but effective way to prevent duplicates when running multiple bots
	std::vector<int> validPlayerIndices;
	
	for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
	{
		PlayerInfo_t playerInfo{};
		if (I::EngineClient->GetPlayerInfo(i, &playerInfo))
		{
			// Consider all players (not just bots) to ensure we have one primary logger per server
			// This will work regardless of whether players are marked as bots or not
			validPlayerIndices.push_back(i);
		}
	}

	// If no valid players, assume we're primary
	if (validPlayerIndices.empty())
		return true;

	// We're the primary if we have the lowest valid index
	// This ensures only one instance logs per server
	int lowestIndex = *std::min_element(validPlayerIndices.begin(), validPlayerIndices.end());
	return localPlayerIndex == lowestIndex;
}

std::string CMisc::GetServerIdentifier()
{
	if (!I::EngineClient || !I::EngineClient->IsInGame())
		return "offline";

	const char* levelName = I::EngineClient->GetLevelName();
	if (levelName)
	{
		std::string serverStr = levelName;
		size_t lastSlash = serverStr.find_last_of("/\\");
		if (lastSlash != std::string::npos)
			serverStr = serverStr.substr(lastSlash + 1);
		
		size_t lastDot = serverStr.find_last_of('.');
		if (lastDot != std::string::npos)
			serverStr = serverStr.substr(0, lastDot);
		
		for (char& c : serverStr)
		{
			if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
				c = '_';
		}
		return serverStr.empty() ? "unknown_map" : serverStr;
	}

	return "unknown_server";
}

bool CMisc::ShouldLogMessage(const std::string& messageHash)
{
	// Check if we've already logged this message recently
	return m_setRecentMessageHashes.find(messageHash) == m_setRecentMessageHashes.end();
}

void CMisc::CleanupOldMessages()
{
	// Clear old message hashes to prevent memory buildup
	// Keep only the last 100 messages in memory
	if (m_setRecentMessageHashes.size() > 100)
	{
		m_setRecentMessageHashes.clear();
	}
}

std::string CMisc::GetGameDirectory()
{
	static std::string sGameDir;
	if (!sGameDir.empty())
		return sGameDir;

	char gamePath[MAX_PATH] = { 0 };
	if (!GetModuleFileNameA(GetModuleHandleA("tf_win64.exe"), gamePath, MAX_PATH))
	{
		return sGameDir; // empty
	}

	std::string gameDir = gamePath;
	size_t lastSlash = gameDir.find_last_of("\\/");
	if (lastSlash != std::string::npos)
		gameDir = gameDir.substr(0, lastSlash);

	sGameDir = gameDir;
	return sGameDir;
}

// Helper: load text lines from fileName (optionally create with defaults).
bool CMisc::LoadLines(const std::string& category, const std::string& fileName,
	const std::vector<std::string>& defaultLines, std::vector<std::string>& outLines)
{
	outLines.clear();

	const std::string gameDir = GetGameDirectory();
	std::vector<std::string> pathsToTry = {
		fileName,
		gameDir + "\\amalgam\\" + fileName
	};

	bool fileLoaded = false;

	for (const auto& path : pathsToTry)
	{
		try
		{
			std::ifstream file(path);
			if (file.is_open())
			{
				std::string line;
				while (std::getline(file, line))
				{
					if (!line.empty())
						outLines.push_back(line);
				}
				file.close();

				SDK::Output(category.c_str(), std::format("Loaded {} lines from {}", outLines.size(), path).c_str(), {}, true, true);
				fileLoaded = true;
				break;
			}
		}
		catch (...)
		{
			continue;
		}
	}

	if (!fileLoaded)
	{
		try
		{
			const std::string dirPath = gameDir + "\\amalgam";
			CreateDirectoryA(dirPath.c_str(), nullptr);

			const std::string defaultPath = dirPath + "\\" + fileName;
			std::ofstream newFile(defaultPath);
			if (newFile.is_open())
			{
				for (const auto& l : defaultLines)
					newFile << l << '\n';
				newFile.close();

				std::ifstream checkFile(defaultPath);
				if (checkFile.is_open())
				{
					std::string line;
					while (std::getline(checkFile, line))
					{
						if (!line.empty())
							outLines.push_back(line);
					}
					checkFile.close();
					SDK::Output(category.c_str(), std::format("Created and loaded default file at {}", defaultPath).c_str(), {}, true, true);
					fileLoaded = true;
				}
			}
		}
		catch (...)
		{
			// swallow the cum
		}
	}

	if (!fileLoaded)
	{
		outLines = defaultLines;
		SDK::Output(category.c_str(), std::format("Failed to load or create {}, using built-in messages", fileName).c_str(), {}, true, true);
	}

	return !outLines.empty();
}

void CMisc::GrabSteamIDs()
{
	CPU_OPT_SKIP_IF_VERY_LOW_PERF(2); // Skip on very low performance
	
	if (!Vars::Misc::Automation::GrabSteamIDs.Value || !I::EngineClient || 
		!I::EngineClient->IsInGame() || !I::EngineClient->IsConnected())
		return;

	static FastTimer tTimer{};
	float grabInterval = 5.0f * CPU_OPT_GET_FREQ_MULTIPLIER();
	if (!tTimer.Run(grabInterval))
		return;

	auto pResource = H::Entities.GetPR();
	if (!pResource)
		return;

	static std::unordered_set<uint32_t> s_SavedIDs{};
	static std::vector<uint32_t> s_PendingIDs; // Batch writes
	
	// Collect new IDs first
	for (int i = 1; i <= I::EngineClient->GetMaxClients(); ++i)
	{
		if (!pResource->m_bValid(i) || !pResource->m_bConnected(i))
			continue;

		PlayerInfo_t pi{};
		if (!I::EngineClient->GetPlayerInfo(i, &pi) || pi.fakeplayer)
			continue;

		uint32_t steam32 = pi.friendsID;
		if (!steam32 || s_SavedIDs.contains(steam32))
			continue;

		s_SavedIDs.insert(steam32);
		s_PendingIDs.push_back(steam32);
	}

	// Batch write pending IDs to reduce I/O operations
	if (!s_PendingIDs.empty() && CPU_OPT_EXPENSIVE_FEATURE_CHECK())
	{
		std::string appDataPath = GetAppDataPath();
		if (!appDataPath.empty())
		{
			std::string dirPath = appDataPath + "\\Amalgam";
			CreateDirectoryA(dirPath.c_str(), nullptr);
			std::string filePath = dirPath + "\\ids.txt";

			std::ofstream file(filePath, std::ios::app);
			if (file.is_open())
			{
				for (uint32_t steamID : s_PendingIDs)
					file << steamID << "\n";
			}
		}
		s_PendingIDs.clear();
	}
}

void CMisc::StealIdentity(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::StealIdentity.Value)
		return;
	if (!I::EngineClient || !I::SteamFriends || !I::SteamUser || !I::EngineClient->IsInGame() || !I::EngineClient->IsConnected())
		return;
	const float flInterval = static_cast<float>(Vars::Misc::Automation::StealIdentityInterval.Value) * 60.0f;
	if (!m_tStealIdentityTimer.Run(flInterval))
		return;
	auto pResource = H::Entities.GetPR();
	if (!pResource)
		return;
	std::vector<int> vCandidates;
	int iLocalIdx = I::EngineClient->GetLocalPlayer();
	for (int i = 1; i <= I::EngineClient->GetMaxClients(); ++i)
	{
		if (i == iLocalIdx)
			continue;
		if (!pResource->m_bValid(i) || !pResource->m_bConnected(i))
			continue;
		PlayerInfo_t pi{};
		if (!I::EngineClient->GetPlayerInfo(i, &pi) || pi.fakeplayer)
			continue;
		
		
		if (H::Entities.IsFriend(i) ||
			H::Entities.InParty(i) ||
			F::PlayerUtils.IsIgnored(i) ||
			F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(FRIEND_IGNORE_TAG)) ||
			F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(BOT_IGNORE_TAG)))
			continue;
		
		vCandidates.push_back(i);
	}
	if (vCandidates.empty())
		return;
	int iTarget = vCandidates[SDK::RandomInt(0, static_cast<int>(vCandidates.size()) - 1)];
	PlayerInfo_t piTarget{};
	if (!I::EngineClient->GetPlayerInfo(iTarget, &piTarget))
		return;
	I::SteamFriends->SetPersonaName(piTarget.name);
	CSteamID steamIDTarget;
	steamIDTarget.SetFromUint64((0x0110000100000000ULL) | static_cast<uint64_t>(piTarget.friendsID));
	m_nStolenAvatar = I::SteamFriends->GetLargeFriendAvatar(steamIDTarget);
}
