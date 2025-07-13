#include "NBheader.h"
#include "../Aimbot/AimbotGlobal/AimbotGlobal.h"
#include "../Ticks/Ticks.h"
#include "../PacketManip/FakeLag/FakeLag.h"
#include "../Misc/Misc.h"
#include "../Simulation/MovementSimulation/MovementSimulation.h"
#include "../CritHack/CritHack.h"
#include "NavEngine/Controllers/CPController/CPController.h"
#include "NavEngine/Controllers/FlagController/FlagController.h"
#include "NavEngine/Controllers/PLController/PLController.h"
#include "NavEngine/Controllers/Controller.h"
#include "../Players/PlayerUtils.h"
#include "../Misc/NamedPipe/NamedPipe.h"
#include "../../Utils/Optimization/CpuOptimization.h"

bool IsWeaponValidForDT(CTFWeaponBase* pWeapon)
{
	if (!pWeapon)
		return false;

	auto iWepID = pWeapon->GetWeaponID();
	if (iWepID == TF_WEAPON_SNIPERRIFLE || iWepID == TF_WEAPON_SNIPERRIFLE_CLASSIC || iWepID == TF_WEAPON_SNIPERRIFLE_DECAP)
		return false;

	return SDK::WeaponDoesNotUseAmmo(pWeapon, false);
}

void CNavBot::UpdateClassConfig(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	switch (pLocal->m_iClass())
	{
	case TF_CLASS_SCOUT:
	case TF_CLASS_HEAVY:
		m_tSelectedConfig = CONFIG_SHORT_RANGE;
		break;
	case TF_CLASS_ENGINEER:
		m_tSelectedConfig = IsEngieMode(pLocal) ? 
			(pWeapon->m_iItemDefinitionIndex() == Engi_t_TheGunslinger ? CONFIG_GUNSLINGER_ENGINEER : CONFIG_ENGINEER) : 
			CONFIG_SHORT_RANGE;
		break;
	case TF_CLASS_SNIPER:
		m_tSelectedConfig = pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW ? CONFIG_MID_RANGE : CONFIG_LONG_RANGE;
		break;
	default:
		m_tSelectedConfig = CONFIG_MID_RANGE;
	}
}

void CNavBot::HandleMinigunSpinup(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, const ClosestEnemy_t& tClosestEnemy)
{
	if (pWeapon->GetWeaponID() != TF_WEAPON_MINIGUN)
		return;

	static Timer tSpinupTimer{};
	auto pEntity = I::ClientEntityList->GetClientEntity(tClosestEnemy.m_iEntIdx);
	if (pEntity && pEntity->As<CTFPlayer>()->IsAlive() && !pEntity->As<CTFPlayer>()->IsInvulnerable() && pWeapon->HasAmmo())
	{
		if ((G::AimTarget.m_iEntIndex && G::AimTarget.m_iDuration) || tClosestEnemy.m_flDist <= pow(800.f, 2))
			tSpinupTimer.Update();
		if (!tSpinupTimer.Check(3.f)) // 3 seconds until unrev
			pCmd->buttons |= IN_ATTACK2;
	}
}

void CNavBot::HandleDoubletapRecharge(CTFWeaponBase* pWeapon)
{
	static Timer tDoubletapRecharge{};
	
	if (!Vars::NavEng::NavBot::RechargeDT.Value || !IsWeaponValidForDT(pWeapon))
		return;

	if (!F::Ticks.m_bRechargeQueue &&
		(Vars::NavEng::NavBot::RechargeDT.Value != Vars::NavEng::NavBot::RechargeDTEnum::WaitForFL || !Vars::Fakelag::Fakelag.Value || !F::FakeLag.m_iGoal) &&
		G::Attacking != 1 &&
		(F::Ticks.m_iShiftedTicks < F::Ticks.m_iShiftedGoal) && 
		tDoubletapRecharge.Check(Vars::NavEng::NavBot::RechargeDTDelay.Value))
	{
		F::Ticks.m_bRechargeQueue = true;
	}
	else if (F::Ticks.m_iShiftedTicks >= F::Ticks.m_iShiftedGoal)
	{
		tDoubletapRecharge.Update();
	}
}

void CNavBot::UpdateSlot(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, ClosestEnemy_t tClosestEnemy)
{
	if (F::NavEngine.current_priority == engineer || (!m_sEngineerTask.empty() && (m_sEngineerTask.rfind(L"Build", 0) == 0 || m_sEngineerTask.rfind(L"Smack", 0)==0)))
		return;

	static Timer tSlotTimer{};
	if (!tSlotTimer.Run(0.2f))
		return;

	m_iCurrentSlot = pWeapon->GetSlot() + 1;

	// Prioritize reloading
	int iReloadSlot = GetReloadWeaponSlot(pLocal, tClosestEnemy);
	m_eLastReloadSlot = static_cast<slots>(iReloadSlot);

	int iNewSlot = iReloadSlot ? iReloadSlot : (Vars::NavEng::NavBot::WeaponSlot.Value ? GetBestSlot(pLocal, static_cast<slots>(m_iCurrentSlot), tClosestEnemy) : -1);
	if (iNewSlot > -1)
	{
		auto sCommand = "slot" + std::to_string(iNewSlot);
		if (m_iCurrentSlot != iNewSlot)
			I::EngineClient->ClientCmd_Unrestricted(sCommand.c_str());
	}
}

slots CNavBot::GetBestSlot(CTFPlayer* pLocal, slots eActiveSlot, ClosestEnemy_t tClosestEnemy)
{
	if (Vars::NavEng::NavBot::WeaponSlot.Value != Vars::NavEng::NavBot::WeaponSlotEnum::Best)
		return static_cast<slots>(Vars::NavEng::NavBot::WeaponSlot.Value - 1);

	if (pLocal && pLocal->IsAlive())
	{
		auto pClosestEnemy = I::ClientEntityList->GetClientEntity(tClosestEnemy.m_iEntIdx)->As<CTFPlayer>();
		auto pPrimaryWeapon = pLocal->GetWeaponFromSlot(SLOT_PRIMARY);
		auto pSecondaryWeapon = pLocal->GetWeaponFromSlot(SLOT_SECONDARY);
		if (pPrimaryWeapon && pSecondaryWeapon)
		{
			int iPrimaryResAmmo = SDK::WeaponDoesNotUseAmmo(pPrimaryWeapon) ? -1 : pLocal->GetAmmoCount(pPrimaryWeapon->m_iPrimaryAmmoType());
			int iSecondaryResAmmo = SDK::WeaponDoesNotUseAmmo(pSecondaryWeapon) ? -1 : pLocal->GetAmmoCount(pSecondaryWeapon->m_iPrimaryAmmoType());
			switch (pLocal->m_iClass())
			{
			case TF_CLASS_SCOUT:
			{
				if ((!G::AmmoInSlot[SLOT_PRIMARY] && (!G::AmmoInSlot[SLOT_SECONDARY] || (iSecondaryResAmmo != -1 &&
					iSecondaryResAmmo <= SDK::GetWeaponMaxReserveAmmo(G::SavedWepIds[SLOT_SECONDARY], G::SavedDefIndexes[SLOT_SECONDARY]) / 4))) && tClosestEnemy.m_flDist <= pow(200.f, 2))
					return melee;

				if (G::AmmoInSlot[SLOT_PRIMARY] && tClosestEnemy.m_flDist <= pow(800.f, 2))
					return primary;

				else if (G::AmmoInSlot[SLOT_SECONDARY])
					return secondary;

				break;
			}
			case TF_CLASS_HEAVY:
			{
				if (!G::AmmoInSlot[SLOT_PRIMARY] && (!G::AmmoInSlot[SLOT_SECONDARY] && iSecondaryResAmmo == 0) ||
					(G::SavedDefIndexes[SLOT_MELEE] == Heavy_t_TheHolidayPunch && (pClosestEnemy && !pClosestEnemy->IsTaunting() && pClosestEnemy->IsInvulnerable()) && tClosestEnemy.m_flDist < pow(400.f, 2)))
					return melee;

				else if ((!pClosestEnemy || tClosestEnemy.m_flDist <= pow(900.f, 2)) && G::AmmoInSlot[SLOT_PRIMARY])
					return primary;

				else if (G::AmmoInSlot[SLOT_SECONDARY] && G::SavedWepIds[SLOT_SECONDARY] == TF_WEAPON_SHOTGUN_HWG)
					return secondary;

				break;
			}
			case TF_CLASS_MEDIC:
			{
				/*if ( AutoMedic->FoundTarget )
				{
					return secondary;
				}*/

				if (!G::AmmoInSlot[SLOT_PRIMARY] ||
					(tClosestEnemy.m_flDist <= pow(400.f, 2) && pClosestEnemy/* && !pPlayer->IsInvulnerable( )*/))
					return melee;

				return primary;
			}
			case TF_CLASS_SPY:
			{
				if (tClosestEnemy.m_flDist <= pow(250.0f, 2) && pClosestEnemy/* && !pPlayer->IsInvulnerable( )*/)
					return melee;
				else if (G::AmmoInSlot[SLOT_PRIMARY] || iPrimaryResAmmo)
					return primary;

				break;
			}
			case TF_CLASS_SNIPER:
			{
				// We have already skipped the targets which are invulnerable
				// bool bPlayerInvuln = pPlayer && pPlayer->IsInvulnerable( );
				int iPlayerLowHp = pClosestEnemy ? (pClosestEnemy->m_iHealth() < pClosestEnemy->GetMaxHealth() * 0.35f ? 2 : pClosestEnemy->m_iHealth() < pClosestEnemy->GetMaxHealth() * 0.75f) : -1;

				if (!G::AmmoInSlot[SLOT_PRIMARY] && !G::AmmoInSlot[SLOT_SECONDARY] ||
					(tClosestEnemy.m_flDist <= pow(200.0f, 2) && pClosestEnemy/*&& !bPlayerInvuln*/))
					return melee;

				if ((G::AmmoInSlot[SLOT_SECONDARY] || iSecondaryResAmmo) &&
					(tClosestEnemy.m_flDist <= pow(300.0f, 2) && iPlayerLowHp > 1 /*&& !bPlayerInvuln*/))
					return secondary;

				// Keep the smg if the target we previosly tried shooting is running away
				else if (eActiveSlot && eActiveSlot < 3 && G::AmmoInSlot[eActiveSlot - 1] &&
						 (tClosestEnemy.m_flDist <= pow(800.0f, 2) && iPlayerLowHp > 1 /*&& !bPlayerInvuln*/))
					break;

				else if (G::AmmoInSlot[SLOT_PRIMARY])
					return primary;

				break;
			}
			case TF_CLASS_PYRO:
			{
				if (!G::AmmoInSlot[SLOT_PRIMARY] && (!G::AmmoInSlot[SLOT_SECONDARY] && iSecondaryResAmmo != -1 &&
					iSecondaryResAmmo <= SDK::GetWeaponMaxReserveAmmo(G::SavedWepIds[SLOT_SECONDARY], G::SavedDefIndexes[SLOT_SECONDARY]) / 4) &&
					(pClosestEnemy && tClosestEnemy.m_flDist <= pow(300.0f, 2)))
					return melee;

				if (G::AmmoInSlot[SLOT_PRIMARY] && (tClosestEnemy.m_flDist <= pow(550.0f, 2) || !pClosestEnemy))
					return primary;
				else if (G::AmmoInSlot[SLOT_SECONDARY])
					return secondary;

				break;
			}
			case TF_CLASS_SOLDIER:
			{
				auto pEnemyWeapon = pClosestEnemy ? pClosestEnemy->m_hActiveWeapon().Get()->As<CTFWeaponBase>() : nullptr;
				bool bEnemyCanAirblast = pEnemyWeapon && pEnemyWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER && pEnemyWeapon->m_iItemDefinitionIndex() != Pyro_m_ThePhlogistinator;
				bool bEnemyClose = pClosestEnemy && tClosestEnemy.m_flDist <= pow(250.0f, 2);
				if ((eActiveSlot != primary || !G::AmmoInSlot[SLOT_PRIMARY] && iPrimaryResAmmo == 0) && bEnemyClose && (pClosestEnemy->m_iHealth() < 80 ? !G::AmmoInSlot[SLOT_SECONDARY] : pClosestEnemy->m_iHealth() >= 150 || G::AmmoInSlot[SLOT_SECONDARY] < 2))
					return melee;

				if (G::AmmoInSlot[SLOT_SECONDARY] && (bEnemyCanAirblast || (tClosestEnemy.m_flDist <= pow(350.0f, 2) && pClosestEnemy && pClosestEnemy->m_iHealth() <= 125)))
					return secondary;
				else if (G::AmmoInSlot[SLOT_PRIMARY])
					return primary;

				break;
			}
			case TF_CLASS_DEMOMAN:
			{
				if (!G::AmmoInSlot[SLOT_PRIMARY] && !G::AmmoInSlot[SLOT_SECONDARY] && (pClosestEnemy && tClosestEnemy.m_flDist <= pow(200.0f, 2)))
					return melee;

				if (G::AmmoInSlot[SLOT_PRIMARY] && (tClosestEnemy.m_flDist <= pow(800.0f, 2)))
					return primary;
				else if (G::AmmoInSlot[SLOT_SECONDARY] || iSecondaryResAmmo >= SDK::GetWeaponMaxReserveAmmo(G::SavedWepIds[SLOT_SECONDARY], G::SavedDefIndexes[SLOT_SECONDARY]) / 2)
					return secondary;

				break;
			}
			case TF_CLASS_ENGINEER:
			{
				auto pSentry = I::ClientEntityList->GetClientEntity(m_iMySentryIdx);
				auto pDispenser = I::ClientEntityList->GetClientEntity(m_iMyDispenserIdx);
				if (((pSentry && pSentry->GetAbsOrigin().DistToSqr(pLocal->GetAbsOrigin()) <= pow(300.0f, 2)) || (pDispenser && pDispenser->GetAbsOrigin().DistToSqr(pLocal->GetAbsOrigin()) <= pow(500.0f, 2))) || (vCurrentBuildingSpot && vCurrentBuildingSpot->DistToSqr(pLocal->GetAbsOrigin()) <= pow(500.0f, 2)))
				{
					if (eActiveSlot >= melee && F::NavEngine.current_priority != prio_melee)
						return eActiveSlot;
					else
						return melee;
				}
				else if (!G::AmmoInSlot[SLOT_PRIMARY] && !G::AmmoInSlot[SLOT_SECONDARY] && (pClosestEnemy && tClosestEnemy.m_flDist <= pow(200.0f, 2)))
					return melee;
				else if ((G::AmmoInSlot[SLOT_PRIMARY] || iPrimaryResAmmo) && (pClosestEnemy && tClosestEnemy.m_flDist <= pow(500.0f, 2)))
					return primary;
				else if (G::AmmoInSlot[SLOT_SECONDARY] || iSecondaryResAmmo)
					return secondary;

				break;
			}
			default:
				break;
			}
		}
	}
	return eActiveSlot;
} 