#include "../SDK/SDK.h"

#include "../Features/Aimbot/Aimbot.h"
#include "../Features/Backtrack/Backtrack.h"
#include "../Features/CritHack/CritHack.h"
#include "../Features/EnginePrediction/EnginePrediction.h"
#include "../Features/Misc/Misc.h"
#include "../Features/NoSpread/NoSpread.h"
#include "../Features/NoSpread/NoSpreadHitscan/NoSpreadHitscan.h"
#include "../Features/PacketManip/PacketManip.h"
#include "../Features/Resolver/Resolver.h"
#include "../Features/Ticks/Ticks.h"
#include "../Features/Visuals/Visuals.h"
#include "../Features/Visuals/FakeAngle/FakeAngle.h"
#include "../Features/Spectate/Spectate.h"
#include "../Features/NavBot/NavEngine/Controllers/Controller.h"
#include "../Features/NavBot/NavEngine/NavEngine.h"
#include "../Features/NavBot/NBheader.h"
#include "../Features/NavBot/Fbot.h"
#include "../Features/AutoJoin/AutoJoin.h"
#include "../Features/Misc/AutoItem/AutoItem.h"

#define MATH_EPSILON (1.f / 16)
#define PSILENT_EPSILON (1.f - MATH_EPSILON)
#define REAL_EPSILON (0.1f + MATH_EPSILON)
#define SNAP_SIZE_EPSILON (10.f - MATH_EPSILON)
#define SNAP_NOISE_EPSILON (0.5f + MATH_EPSILON)

#define SAFE_CALL(expr)                \
    do                                \
    {                                 \
        try                           \
        {                             \
            expr;                     \
        }                             \
        catch (...)                   \
        {                             \
        }                             \
    } while (false)

struct CmdHistory_t
{
	Vec3 m_vAngle;
	bool m_bAttack1;
	bool m_bAttack2;
	bool m_bSendingPacket;
};


MAKE_SIGNATURE(IHasGenericMeter_GetMeterMultiplier, "client.dll", "F3 0F 10 81 ? ? ? ? C3 CC CC CC CC CC CC CC 48 85 D2", 0x0);

MAKE_HOOK(CClientModeShared_CreateMove, U::Memory.GetVirtual(I::ClientModeShared, 21), bool,
	CClientModeShared* rcx, float flInputSampleTime, CUserCmd* pCmd)
{
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::CClientModeShared_CreateMove[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, flInputSampleTime, pCmd);
#endif

	if (pCmd)
	{
		G::OriginalMove.m_vMove = { pCmd->forwardmove, pCmd->sidemove, pCmd->upmove };
		G::OriginalMove.m_vView = pCmd->viewangles;
		G::OriginalMove.m_iButtons = pCmd->buttons;
	}

	const bool bReturn = CALL_ORIGINAL(rcx, flInputSampleTime, pCmd);
	if (!pCmd || !pCmd->command_number || G::Unload)
		return bReturn;

	bool* pSendPacket = reinterpret_cast<bool*>(uintptr_t(_AddressOfReturnAddress()) + 0x128);

	G::PSilentAngles = G::SilentAngles = G::Attacking = G::Throwing = false;
	G::LastUserCmd = G::CurrentUserCmd ? G::CurrentUserCmd : pCmd;
	G::CurrentUserCmd = pCmd;

	I::Prediction->Update(I::ClientState->m_nDeltaTick, I::ClientState->m_nDeltaTick > 0, I::ClientState->last_command_ack, I::ClientState->lastoutgoingcommand + I::ClientState->chokedcommands);

	// correct tick_count for fakeinterp / nointerp
	// 	pCmd->tick_count += TICKS_TO_TIME(F::Backtrack.GetFakeInterp()) - (Vars::Visuals::Removals::Interpolation.Value ? 0 : TICKS_TO_TIME(G::Lerp));
	if (G::OriginalMove.m_iButtons & IN_DUCK) // lol
		pCmd->buttons |= IN_DUCK;

	auto pLocal = H::Entities.GetLocal();
	auto pWeapon = H::Entities.GetWeapon();
	if (pLocal && pWeapon)
	{	// update weapon info
		G::CanPrimaryAttack = G::CanSecondaryAttack = G::Reloading = false;

		bool bCanAttack = pLocal->CanAttack();
		{
			static int iStaticItemDefinitionIndex = 0;
			int iOldItemDefinitionIndex = iStaticItemDefinitionIndex;
			int iNewItemDefinitionIndex = iStaticItemDefinitionIndex = pWeapon->m_iItemDefinitionIndex();

			if (iNewItemDefinitionIndex != iOldItemDefinitionIndex || !bCanAttack || !pWeapon->m_iClip1())
				F::Ticks.m_iWait = 1;
		}

		for (int i = 0; i <= SLOT_MELEE; i++)
		{
			auto pWeaponInSlot = pLocal->GetWeaponFromSlot(i);
			if (pWeaponInSlot)
			{
				G::SavedDefIndexes[i] = pWeaponInSlot->m_iItemDefinitionIndex();
				G::SavedWepIds[i] = pWeaponInSlot->GetWeaponID();
				if (i != SLOT_MELEE)
					G::AmmoInSlot[i] = pWeaponInSlot->m_iClip1();
			}
		}

		if (bCanAttack)
		{
			G::CanPrimaryAttack = pWeapon->CanPrimaryAttack();
			G::CanSecondaryAttack = pWeapon->CanSecondaryAttack();

			switch (pWeapon->GetWeaponID())
			{
			case TF_WEAPON_FLAME_BALL:
				if (G::CanPrimaryAttack)
				{
					// do this, otherwise it will be a tick behind
					float flFrametime = TICK_INTERVAL * 100;
					float flMeterMult = S::IHasGenericMeter_GetMeterMultiplier.Call<float>(pWeapon->m_pMeter());
					float flRate = SDK::AttribHookValue(1.f, "item_meter_charge_rate", pWeapon) - 1;
					float flMult = SDK::AttribHookValue(1.f, "mult_item_meter_charge_rate", pWeapon);
					float flTankPressure = pLocal->m_flTankPressure() + flFrametime * flMeterMult / (flRate * flMult);

					if (G::CanPrimaryAttack && flTankPressure < 100.f)
						G::CanPrimaryAttack = G::CanSecondaryAttack = false;
				}
				break;
			case TF_WEAPON_MINIGUN:
			{
				int iState = pWeapon->As<CTFMinigun>()->m_iWeaponState();
				if (iState != AC_STATE_FIRING && iState != AC_STATE_SPINNING || !pWeapon->HasPrimaryAmmoForShot())
					G::CanPrimaryAttack = false;
				break;
			}
			case TF_WEAPON_FLAREGUN_REVENGE:
				if (pCmd->buttons & IN_ATTACK2)
					G::CanPrimaryAttack = false;
				break;
			case TF_WEAPON_BAT_WOOD:
			case TF_WEAPON_BAT_GIFTWRAP:
				if (!pWeapon->HasPrimaryAmmoForShot())
					G::CanSecondaryAttack = false;
				break;
			case TF_WEAPON_MEDIGUN:
			case TF_WEAPON_BUILDER:
			case TF_WEAPON_LASER_POINTER:
				break;
			case TF_WEAPON_PARTICLE_CANNON:
			{
				float flChargeBeginTime = pWeapon->As<CTFParticleCannon>()->m_flChargeBeginTime();
				if (flChargeBeginTime > 0)
				{
					float flTotalChargeTime = TICKS_TO_TIME(pLocal->m_nTickBase()) - flChargeBeginTime;
					if (flTotalChargeTime < TF_PARTICLE_MAX_CHARGE_TIME)
					{
						G::CanPrimaryAttack = G::CanSecondaryAttack = false;
						break;
					}
				}
				[[fallthrough]];
			}
			default:
				if (pWeapon->GetSlot() != SLOT_MELEE)
				{
					bool bAmmo = pWeapon->HasPrimaryAmmoForShot();
					bool bReload = pWeapon->IsInReload();
					if (!bAmmo && pWeapon->m_iItemDefinitionIndex() != Soldier_m_TheBeggarsBazooka)
						G::CanPrimaryAttack = G::CanSecondaryAttack = false;
					if (bReload && bAmmo && !G::CanPrimaryAttack)
						G::Reloading = true;
				}
			}
			if (G::CanPrimaryAttack)
			{
				switch (pWeapon->GetWeaponID())
				{
				case TF_WEAPON_FLAMETHROWER:
				case TF_WEAPON_FLAME_BALL:
				case TF_WEAPON_FLAREGUN:
				case TF_WEAPON_FLAREGUN_REVENGE:
					if (pLocal->IsUnderwater())
						G::CanPrimaryAttack = G::CanSecondaryAttack = false;
				}
			}
		}

		G::Attacking = SDK::IsAttacking(pLocal, pWeapon, pCmd);
		G::PrimaryWeaponType = SDK::GetWeaponType(pWeapon, &G::SecondaryWeaponType);
		G::CanHeadshot = pWeapon->CanHeadshot() || pWeapon->AmbassadorCanHeadshot(TICKS_TO_TIME(pLocal->m_nTickBase()));
	}

	// run features
#ifndef TEXTMODE
	SAFE_CALL(F::Spectate.CreateMove(pCmd));
#endif
	SAFE_CALL(F::Misc.RunPre(pLocal, pCmd));
	SAFE_CALL(F::AutoJoin.Run(pLocal));
	SAFE_CALL(F::AutoItem.Run(pLocal));
	SAFE_CALL(F::EnginePrediction.Start(pLocal, pCmd));
	SAFE_CALL(F::Aimbot.Run(pLocal, pWeapon, pCmd));
	if (F::NavEngine.isReady()) {
		SAFE_CALL(F::NavBot.Run(pLocal, pWeapon, pCmd));
		SAFE_CALL(F::FollowBot.Run(pLocal, pWeapon, pCmd));
		SAFE_CALL(F::GameObjectiveController.Update());
	}
	SAFE_CALL(F::NavEngine.Run(pCmd));
	SAFE_CALL(F::EnginePrediction.End(pLocal, pCmd));

	SAFE_CALL(F::CritHack.Run(pLocal, pWeapon, pCmd));
	SAFE_CALL(F::Misc.RunPost(pLocal, pCmd, *pSendPacket));
	SAFE_CALL(F::PacketManip.Run(pLocal, pWeapon, pCmd, pSendPacket));
	SAFE_CALL(F::Resolver.CreateMove(pLocal));

#ifndef TEXTMODE
	SAFE_CALL(F::Visuals.CreateMove(pLocal, pWeapon));
	SAFE_CALL(F::Backtrack.BacktrackToCrosshair(pCmd));
	SAFE_CALL(F::NoSpread.Run(pLocal, pWeapon, pCmd));
#endif

	{
		static bool bWasSet = false;
		const bool bCanChoke = F::Ticks.CanChoke(); // failsafe
		if (G::PSilentAngles && bCanChoke)
			*pSendPacket = false, bWasSet = true;
		else if (bWasSet || !bCanChoke)
			*pSendPacket = true, bWasSet = false;
	}
	try {
		F::Ticks.CreateMove(pLocal, pCmd, pSendPacket);
		F::AntiAim.Run(pLocal, pWeapon, pCmd, *pSendPacket);
	} catch (...) {}

#ifndef TEXTMODE
	if (pLocal)
	{
		static std::vector<Vec3> vAngles = {};
		vAngles.push_back(pCmd->viewangles);
		auto pAnimState = pLocal->m_PlayerAnimState();
		if (*pSendPacket && pAnimState)
		{
			float flOldFrametime = I::GlobalVars->frametime;
			float flOldCurtime = I::GlobalVars->curtime;
			I::GlobalVars->frametime = TICK_INTERVAL;
			I::GlobalVars->curtime = TICKS_TO_TIME(pLocal->m_nTickBase());
			for (auto& vAngle : vAngles)
			{
				if (pLocal->IsTaunting() && pLocal->m_bAllowMoveDuringTaunt())
					pLocal->m_flTauntYaw() = vAngle.y;
				pAnimState->Update(pAnimState->m_flEyeYaw = vAngle.y, vAngle.x);
				pLocal->FrameAdvance(TICK_INTERVAL);
			}
			I::GlobalVars->frametime = flOldFrametime;
			I::GlobalVars->curtime = flOldCurtime;
			vAngles.clear();

			F::FakeAngle.Run(pLocal);
		}
	}
#endif
try {
	G::Choking = !*pSendPacket;
	G::LastUserCmd = pCmd;
	F::NoSpreadHitscan.AskForPlayerPerf();

	if (Vars::Misc::Game::AntiCheatCompatibility.Value)
	{
		Math::ClampAngles(pCmd->viewangles); // shouldn't happen, but failsafe

		static std::deque<CmdHistory_t> vHistory;
		vHistory.emplace_front(pCmd->viewangles, pCmd->buttons & IN_ATTACK, pCmd->buttons & IN_ATTACK2, *pSendPacket);
		if (vHistory.size() > 5)
			vHistory.pop_back();

		if (vHistory.size() < 3)
			return false;

		// prevent trigger checks, though this shouldn't happen ordinarily
		if (!vHistory[0].m_bAttack1 && vHistory[1].m_bAttack1 && !vHistory[2].m_bAttack1)
			pCmd->buttons |= IN_ATTACK;
		if (!vHistory[0].m_bAttack2 && vHistory[1].m_bAttack2 && !vHistory[2].m_bAttack2)
			pCmd->buttons |= IN_ATTACK2;

		// don't care if we are actually attacking or not, a miss is less important than a detection
		if (vHistory[0].m_bAttack1 || vHistory[1].m_bAttack1 || vHistory[2].m_bAttack1)
		{
			// prevent silent aim checks
			if (Math::CalcFov(vHistory[0].m_vAngle, vHistory[1].m_vAngle) > PSILENT_EPSILON
				&& Math::CalcFov(vHistory[0].m_vAngle, vHistory[2].m_vAngle) < REAL_EPSILON)
			{
				pCmd->viewangles = vHistory[1].m_vAngle.LerpAngle(vHistory[0].m_vAngle, 0.5f);
				if (Math::CalcFov(pCmd->viewangles, vHistory[2].m_vAngle) < REAL_EPSILON)
					pCmd->viewangles = vHistory[0].m_vAngle + Vec3(0.f, REAL_EPSILON * 2);
				vHistory[0].m_vAngle = pCmd->viewangles;
				vHistory[0].m_bSendingPacket = *pSendPacket = vHistory[1].m_bSendingPacket;
				G::Choking = !*pSendPacket;
			}

			// prevent aim snap checks
			if (vHistory.size() == 5)
			{	// not my fault the original code is so atrocious
				float flDelta01 = Math::CalcFov(vHistory[0].m_vAngle, vHistory[1].m_vAngle);
				float flDelta12 = Math::CalcFov(vHistory[1].m_vAngle, vHistory[2].m_vAngle);
				float flDelta23 = Math::CalcFov(vHistory[2].m_vAngle, vHistory[3].m_vAngle);
				float flDelta34 = Math::CalcFov(vHistory[3].m_vAngle, vHistory[4].m_vAngle);

				if ((
						flDelta12 > SNAP_SIZE_EPSILON && flDelta23 < SNAP_NOISE_EPSILON && vHistory[2].m_vAngle != vHistory[3].m_vAngle
					 || flDelta23 > SNAP_SIZE_EPSILON && flDelta12 < SNAP_NOISE_EPSILON && vHistory[1].m_vAngle != vHistory[2].m_vAngle
					)
					&& flDelta01 < SNAP_NOISE_EPSILON && vHistory[0].m_vAngle != vHistory[1].m_vAngle
					&& flDelta34 < SNAP_NOISE_EPSILON && vHistory[3].m_vAngle != vHistory[4].m_vAngle)
				{
					pCmd->viewangles.y += SNAP_NOISE_EPSILON * 2;
					vHistory[0].m_vAngle = pCmd->viewangles;
					vHistory[0].m_bSendingPacket = *pSendPacket = vHistory[1].m_bSendingPacket;
					G::Choking = !*pSendPacket;
				}
			}
		}
	}
	} catch (...) {}
	return false;
}