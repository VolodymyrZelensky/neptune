#include "Aimbot.h"

#include "AimbotSafety.h"
#include "TextmodeConfig.h"
#include "AimbotHitscan/AimbotHitscan.h"
#ifndef TEXTMODE
#include "AimbotProjectile/AimbotProjectile.h"
#endif
#include "AimbotMelee/AimbotMelee.h"
#include "AutoDetonate/AutoDetonate.h"
#include "AutoAirblast/AutoAirblast.h"
#include "AutoHeal/AutoHeal.h"
#include "AutoRocketJump/AutoRocketJump.h"
#include "../Misc/Misc.h"
#include "../Visuals/Visuals.h"
#include "../NavBot/NavEngine/NavEngine.h"

bool CAimbot::ShouldRun(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	if (!pLocal || !pWeapon || !pLocal->CanAttack()
		|| !SDK::AttribHookValue(1, "mult_dmg", pWeapon)
		/*|| I::EngineVGui->IsGameUIVisible()*/)
		return false;

	return true;
}

void CAimbot::RunAimbot(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, bool bSecondaryType)
{
	m_bRunningSecondary = bSecondaryType;
	EWeaponType eWeaponType = !m_bRunningSecondary ? G::PrimaryWeaponType : G::SecondaryWeaponType;

	bool bOriginal;
	if (m_bRunningSecondary)
		bOriginal = G::CanPrimaryAttack, G::CanPrimaryAttack = G::CanSecondaryAttack;

	switch (eWeaponType)
	{
	case EWeaponType::HITSCAN: F::AimbotHitscan.Run(pLocal, pWeapon, pCmd); break;
#ifndef TEXTMODE
	case EWeaponType::PROJECTILE: F::AimbotProjectile.Run(pLocal, pWeapon, pCmd); break;
#endif
	case EWeaponType::MELEE: F::AimbotMelee.Run(pLocal, pWeapon, pCmd); break;
	}

	if (m_bRunningSecondary)
		G::CanPrimaryAttack = bOriginal;
}

void CAimbot::RunMain(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
#ifndef TEXTMODE
	if (F::AimbotProjectile.m_iLastTickCancel)
	{
		pCmd->weaponselect = F::AimbotProjectile.m_iLastTickCancel;
		F::AimbotProjectile.m_iLastTickCancel = 0;
	}
#endif

	m_bRan = false;
	if (abs(G::AimTarget.m_iTickCount - I::GlobalVars->tickcount) > G::AimTarget.m_iDuration)
		G::AimTarget = {};
	if (abs(G::AimPoint.m_iTickCount - I::GlobalVars->tickcount) > G::AimPoint.m_iDuration)
		G::AimPoint = {};

	if (pCmd->weaponselect)
		return;

	F::AutoRocketJump.Run(pLocal, pWeapon, pCmd);
	if (!ShouldRun(pLocal, pWeapon))
		return;

	F::AutoDetonate.Run(pLocal, pWeapon, pCmd);
	F::AutoAirblast.Run(pLocal, pWeapon, pCmd);
	F::AutoHeal.Run(pLocal, pWeapon, pCmd);

	RunAimbot(pLocal, pWeapon, pCmd);
	RunAimbot(pLocal, pWeapon, pCmd, true);
}

void CAimbot::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	// Critical safety checks to prevent crashes during map loading/transitions
	if (!pLocal || !pWeapon || !pCmd || !I::EngineClient || !I::EngineClient->IsInGame())
		return;
	
	// Wrap main execution in try-catch for stability in textmode
	try {
		RunMain(pLocal, pWeapon, pCmd);
		G::Attacking = SDK::IsAttacking(pLocal, pWeapon, pCmd, true);
	} catch (...) {
		// Silently handle exceptions to prevent crashes
		G::Attacking = 0;
	}
}

void CAimbot::Draw(CTFPlayer* pLocal)
{
	if (!Vars::Aimbot::General::FOVCircle.Value || !Vars::Colors::FOVCircle.Value.a || !pLocal->CanAttack(false))
		return;

	auto pWeapon = H::Entities.GetWeapon();
	if (pWeapon && !SDK::AttribHookValue(1, "mult_dmg", pWeapon))
		return;

		if (Vars::Aimbot::General::AimFOV.Value >= 90.f)
			return;

	float flW = H::Draw.m_nScreenW, flH = H::Draw.m_nScreenH;
	float flRadius = tanf(DEG2RAD(Vars::Aimbot::General::AimFOV.Value)) / tanf(DEG2RAD(pLocal->m_iFOV()) / 2) * flW * (4.f / 6.f) / (16.f / 9.f);
	H::Draw.LineCircle(H::Draw.m_nScreenW / 2, H::Draw.m_nScreenH / 2, flRadius, 68, Vars::Colors::FOVCircle.Value);
}

void CAimbot::Store(CBaseEntity* pEntity, size_t iSize)
{
	if (!Vars::Visuals::Simulation::RealPath.Value)
		return;

	if (pEntity)
	{
		auto pResource = H::Entities.GetPR();
		if (pEntity->IsPlayer() && pResource)
		{
			m_tPath = { { pEntity->m_vecOrigin() }, I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::RealPath.Value, Vars::Visuals::Simulation::RealPath.Value };
			m_iSize = iSize;
			m_iPlayer = pResource->m_iUserID(pEntity->entindex());
		}
		return;
	}

	int iLag;
	{
		static int iStaticTickcout = I::GlobalVars->tickcount;
		iLag = I::GlobalVars->tickcount - iStaticTickcout;
		iStaticTickcout = I::GlobalVars->tickcount;
	}

	if (!m_tPath.m_flTime)
		return;
	else if (m_tPath.m_vPath.size() >= m_iSize || m_tPath.m_flTime < I::GlobalVars->curtime)
	{
		G::PathStorage.push_back(m_tPath);
		m_tPath = {};
		return;
	}

	auto pPlayer = I::ClientEntityList->GetClientEntity(I::EngineClient->GetPlayerForUserID(m_iPlayer))->As<CTFPlayer>();
	if (!pPlayer)
		return;

	for (int i = 0; i < iLag; i++)
		m_tPath.m_vPath.push_back(pPlayer->m_vecOrigin());
}