#include "NBheader.h"
#include "../../SDK/SDK.h"
#include "NavEngine/Controllers/FlagController/FlagController.h"
#include "NavEngine/NavEngine.h"
#include <algorithm>
#include <format>

bool CNavBot::IsEngieMode(CTFPlayer* pLocal)
{
	return Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::AutoEngie && (Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value || Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value) && pLocal && pLocal->IsAlive() && pLocal->m_iClass() == TF_CLASS_ENGINEER;
}

bool CNavBot::BlacklistedFromBuilding(CNavArea* pArea)
{
	// FIXME: Better way of doing this ?
	if (auto pBlackList = F::NavEngine.getFreeBlacklist())
	{
		for (auto tBlackListedArea : *pBlackList)
		{
			if (tBlackListedArea.first == pArea && tBlackListedArea.second.value == BlacklistReason_enum::BR_BAD_BUILDING_SPOT)
				return true;
		}
	}
	return false;
}

void CNavBot::RefreshBuildingSpots(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, ClosestEnemy_t tClosestEnemy, bool bForce)
{
	static Timer tRefreshBuildingSpotsTimer;
	if (!IsEngieMode(pLocal) || !pWeapon)
		return;

	bool bHasGunslinger = pWeapon->m_iItemDefinitionIndex() == Engi_t_TheGunslinger;
	if (bForce || tRefreshBuildingSpotsTimer.Run(bHasGunslinger ? 1.f : 5.f))
	{
		m_vBuildingSpots.clear();
		auto vTarget = F::FlagController.GetSpawnPosition(pLocal->m_iTeamNum());;
		if (!vTarget)
		{
			if (tClosestEnemy.m_iEntIdx)
				vTarget = F::NavParser.GetDormantOrigin(tClosestEnemy.m_iEntIdx);
			if (!vTarget)
				vTarget = pLocal->GetAbsOrigin();
		}
		if (vTarget)
		{
			// Search all nav areas for valid spots
			for (auto& tArea : F::NavEngine.getNavFile()->m_areas)
			{
				if (BlacklistedFromBuilding(&tArea))
					continue;

				if (tArea.m_TFattributeFlags & (TF_NAV_SPAWN_ROOM_RED | TF_NAV_SPAWN_ROOM_BLUE | TF_NAV_SPAWN_ROOM_EXIT))
					continue;

				if (tArea.m_TFattributeFlags & TF_NAV_SENTRY_SPOT)
					m_vBuildingSpots.emplace_back(tArea.m_center);
				else
				{
					for (auto tHidingSpot : tArea.m_hidingSpots)
					{
						if (tHidingSpot.HasGoodCover())
							m_vBuildingSpots.emplace_back(tHidingSpot.m_pos);
					}
				}
			}
			// Sort by distance to nearest, lower is better
			// TODO: This isn't really optimal, need a dif way to where it is a good distance from enemies but also bots dont build in the same spot
			std::sort(m_vBuildingSpots.begin(), m_vBuildingSpots.end(),
					  [&](Vector a, Vector b) -> bool
					  {
						  if (bHasGunslinger)
						  {
							  auto a_flDist = a.DistTo(*vTarget);
							  auto b_flDist = b.DistTo(*vTarget);

							  // Penalty for being in danger ranges
							  if (a_flDist + 100.0f < 300.0f)
								  a_flDist += 4000.0f;
							  if (b_flDist + 100.0f < 300.0f)
								  b_flDist += 4000.0f;

							  if (a_flDist + 1000.0f < 500.0f)
								  a_flDist += 1500.0f;
							  if (b_flDist + 1000.0f < 500.0f)
								  b_flDist += 1500.0f;

							  return a_flDist < b_flDist;
						  }
						  else
							  return a.DistTo(*vTarget) < b.DistTo(*vTarget);
					  });

            m_vBuildingSpots.erase(std::remove_if(m_vBuildingSpots.begin(), m_vBuildingSpots.end(), [&](const Vector& v){ return IsFailedBuildingSpot(v); }), m_vBuildingSpots.end());
		}
	}
}

void CNavBot::RefreshLocalBuildings(CTFPlayer* pLocal)
{
	if (IsEngieMode(pLocal))
	{
		m_iMySentryIdx = -1;
		m_iMyDispenserIdx = -1;

		for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_TEAMMATES))
		{
			auto iClassID = pEntity->GetClassID();
			if (iClassID != ETFClassID::CObjectSentrygun && iClassID != ETFClassID::CObjectDispenser)
				continue;

			auto pBuilding = pEntity->As<CBaseObject>();
			auto pBuilder = pBuilding->m_hBuilder().Get();
			if (!pBuilder)
				continue;

			if (pBuilding->m_bPlacing())
				continue;

			if (pBuilder->entindex() != pLocal->entindex())
				continue;

			if (iClassID == ETFClassID::CObjectSentrygun)
				m_iMySentryIdx = pBuilding->entindex();
			else if (iClassID == ETFClassID::CObjectDispenser)
				m_iMyDispenserIdx = pBuilding->entindex();
		}
	}
}

bool CNavBot::NavToSentrySpot()
{
	static Timer tWaitUntilPathSentryTimer;

	// Wait a bit before pathing again
	if (!tWaitUntilPathSentryTimer.Run(0.3f))
		return false;

    // Remove failed spots from consideration
    m_vBuildingSpots.erase(std::remove_if(m_vBuildingSpots.begin(), m_vBuildingSpots.end(), [&](const Vector& v){ return IsFailedBuildingSpot(v); }), m_vBuildingSpots.end());

	// Try to nav to our existing sentry spot
	if (auto pSentry = I::ClientEntityList->GetClientEntity(m_iMySentryIdx))
	{
		// Don't overwrite current nav
		if (F::NavEngine.current_priority == engineer)
			return true;

		auto vSentryOrigin = F::NavParser.GetDormantOrigin(pSentry->entindex());
		if (!vSentryOrigin)
			return false;

		if (F::NavEngine.navTo(*vSentryOrigin, engineer))
			return true;
	}
	else
		m_iMySentryIdx = -1;

	if (m_vBuildingSpots.empty())
		return false;

	// Don't overwrite current nav
	if (F::NavEngine.current_priority == engineer)
		return false;

	// Max 10 attempts
	for (int iAttempts = 0; iAttempts < 10 && iAttempts < m_vBuildingSpots.size(); ++iAttempts)
	{
		// Get a semi-random building spot to still keep distance preferrance
		auto iRandomOffset = SDK::RandomInt(0, std::min(3, (int)m_vBuildingSpots.size()-1));

		Vector vRandom;
		// Wrap around
		if (iAttempts - iRandomOffset < 0)
			vRandom = m_vBuildingSpots[m_vBuildingSpots.size() + (iAttempts - iRandomOffset)];
		else
			vRandom = m_vBuildingSpots[iAttempts - iRandomOffset];

        // Skip if this spot previously failed (could have slipped through earlier removal due to epsilon)
        if (IsFailedBuildingSpot(vRandom))
            continue;

		// Try to nav there
		if (F::NavEngine.navTo(vRandom, engineer))
		{
			vCurrentBuildingSpot = vRandom;
			return true;
		}
        else
        {
            // Mark as failed so we skip it next times
            AddFailedBuildingSpot(vRandom);
        }
	}
	return false;
}

// --- Build / Smack helpers ---


static CBaseObject* GetCarriedBuilding(CTFPlayer* pLocal)
{
	if (auto pEntity = pLocal->m_hCarriedObject().Get())
		return pEntity->As<CBaseObject>();
	return nullptr;
}

bool CNavBot::BuildBuilding(CUserCmd* pCmd, CTFPlayer* pLocal, ClosestEnemy_t tClosestEnemy, int building)
{
	auto pMeleeWeapon = pLocal->GetWeaponFromSlot(SLOT_MELEE);
	if (!pMeleeWeapon)
		return false;

	// Blacklist this spot and refresh the building spots
	if (m_iBuildAttempts >= 15)
	{
		(*F::NavEngine.getFreeBlacklist())[F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin())] = BlacklistReason_enum::BR_BAD_BUILDING_SPOT;
        if (vCurrentBuildingSpot) AddFailedBuildingSpot(*vCurrentBuildingSpot);
		RefreshBuildingSpots(pLocal, pMeleeWeapon, tClosestEnemy, true);
		vCurrentBuildingSpot = std::nullopt;
		m_iBuildAttempts = 0;
		m_sEngineerTask = std::format(L"Build {}", building == dispenser ? L"dispenser" : L"sentry");
		return NavToSentrySpot();
	}

	// Make sure we have right amount of metal
	int iRequiredMetal = (pMeleeWeapon->m_iItemDefinitionIndex() == Engi_t_TheGunslinger || building == dispenser) ? 100 : 130;
	if (pLocal->m_iMetalCount() < iRequiredMetal)
	{
		m_bWaitingForMetal = true;
		return GetAmmo(pCmd, pLocal, true);
	}
	m_bWaitingForMetal = false;

	m_sEngineerTask = std::format(L"Build {}", building == dispenser ? L"dispenser" : L"sentry");
	static float flPrevYaw = 0.0f;
	// Try to build! we are close enough
	if (vCurrentBuildingSpot && vCurrentBuildingSpot->DistToSqr(pLocal->GetAbsOrigin()) <= pow(building == dispenser ? 600.0f : 250.0f, 2))
	{
		static float flPitch = 30.0f;
		pCmd->viewangles.x = flPitch;
		pCmd->viewangles.y = flPrevYaw += 5.0f;
		flPitch += 5.0f;
		if (flPitch > 80.0f)
			flPitch = 30.0f;

		static Timer tAttemptTimer;
		if (tAttemptTimer.Run(0.25f))
		{
			m_iBuildAttempts++;
			if (m_iBuildAttempts > 90)
			{
				(*F::NavEngine.getFreeBlacklist())[F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin())] = BlacklistReason_enum::BR_BAD_BUILDING_SPOT;
                if (vCurrentBuildingSpot) AddFailedBuildingSpot(*vCurrentBuildingSpot);
				RefreshBuildingSpots(pLocal, pMeleeWeapon, tClosestEnemy, true);
				vCurrentBuildingSpot = std::nullopt;
				m_iBuildAttempts = 0;
				// Search for another spot right away
				return NavToSentrySpot();
			}
		}

		//auto pCarriedBuilding = GetCarriedBuilding( pLocal );
		if (!pLocal->m_bCarryingObject())
		{
			static Timer command_timer;
			static Timer attack_timer;
			if (!pLocal->m_bCarryingObject())
			{
				if (command_timer.Run(0.9f))
				{
					I::EngineClient->ClientCmd_Unrestricted(std::format("build {}", building).c_str());
					attack_timer.Update(); 
				}
			}
			else
			{
				if (attack_timer.Run(0.05f))
					pCmd->buttons |= IN_ATTACK;
			}
		}
		//else if (pCarriedBuilding->m_bServerOverridePlacement()) // Can place
		pCmd->buttons |= IN_ATTACK;
		return true;
	}
	else
	{
		flPrevYaw = 0.0f;
		return NavToSentrySpot();
	}

	return false;
}

bool CNavBot::BuildingNeedsToBeSmacked(CBaseObject* pBuilding)
{
	if (!pBuilding || pBuilding->IsDormant())
		return false;

	if (pBuilding->m_iUpgradeLevel() < 3)
		return true;

	// Only repair if health below 95 %
	if (static_cast<float>(pBuilding->m_iHealth()) / static_cast<float>(pBuilding->m_iMaxHealth()) < 0.95f)
		return true;

	if (pBuilding->GetClassID() == ETFClassID::CObjectSentrygun)
	{
		int iMaxAmmo = 0;
		switch (pBuilding->m_iUpgradeLevel())
		{
		case 1: iMaxAmmo = 150; break;
		case 2:
		case 3: iMaxAmmo = 200; break;
		}

		if (iMaxAmmo > 0)
		{
			float flAmmoRatio = static_cast<float>(pBuilding->As<CObjectSentrygun>()->m_iAmmoShells()) / static_cast<float>(iMaxAmmo);
			// Only smack if ammo really low (< 40 %)
			if (flAmmoRatio < 0.40f)
				return true;
		}
	}
	return false;
}

bool CNavBot::SmackBuilding(CUserCmd* pCmd, CTFPlayer* pLocal, CBaseObject* pBuilding)
{
	if (!pBuilding || pBuilding->IsDormant())
		return false;

	// If we have no metal, gather some first.
	if (!pLocal->m_iMetalCount())
	{
		m_bWaitingForMetal = true;
		m_sEngineerTask   = L"Gather metal";
		return GetAmmo(pCmd, pLocal, true);
	}

	// Ensure we are holding the wrench (slot3) while performing smack tasks.
	CTFWeaponBase* pCurWeapon = H::Entities.GetWeapon();
	if (pCurWeapon && pCurWeapon->GetSlot() != SLOT_MELEE)
	{
		static Timer tSwitchTimer{};
		// Do not spam the command every tick – send at most once every 0.25s.
		if (tSwitchTimer.Run(0.25f))
		{
			I::EngineClient->ClientCmd_Unrestricted("slot3");
		}
	}

	m_sEngineerTask = std::format(L"Smack {}", pBuilding->GetClassID() == ETFClassID::CObjectDispenser ? L"dispenser" : L"sentry");

	if (pBuilding->GetAbsOrigin().DistToSqr(pLocal->GetAbsOrigin()) <= pow(100.0f, 2) && pCurWeapon->GetSlot() == SLOT_MELEE)
	{
		pCmd->buttons |= IN_ATTACK;

		auto vAngTo = Math::CalcAngle(pLocal->GetEyePosition(), pBuilding->GetCenter());
		G::Attacking = SDK::IsAttacking(pLocal, pCurWeapon, pCmd, true);
		if (G::Attacking == 1)
			pCmd->viewangles = vAngTo;
	}
	else if (F::NavEngine.current_priority != engineer)
		return F::NavEngine.navTo(pBuilding->GetAbsOrigin(), engineer);

	return true;
}

bool CNavBot::EngineerLogic(CUserCmd* pCmd, CTFPlayer* pLocal, ClosestEnemy_t tClosestEnemy)
{
	if (!IsEngieMode(pLocal))
	{
		m_sEngineerTask.clear();
		return false;
	}

	auto pMeleeWeapon = pLocal->GetWeaponFromSlot(SLOT_MELEE);
	if (!pMeleeWeapon)
		return false;

	auto pSentry = I::ClientEntityList->GetClientEntity(m_iMySentryIdx);
	// Already have a sentry
	if (pSentry && pSentry->GetClassID() == ETFClassID::CObjectSentrygun)
	{
		if (pMeleeWeapon->m_iItemDefinitionIndex() == Engi_t_TheGunslinger)
		{
			auto vSentryOrigin = F::NavParser.GetDormantOrigin(m_iMySentryIdx);
			// Too far away, destroy it
			// BUG Ahead, building isnt destroyed lol
			if (!vSentryOrigin || vSentryOrigin->DistToSqr(pLocal->GetAbsOrigin()) >= pow(1800.0f, 2))
			{
				// If we have a valid building
				I::EngineClient->ClientCmd_Unrestricted("destroy 2");
			}
			// Return false so we run another task
			return false;
		}
		else
		{
			if (!pLocal->m_iMetalCount())
			{
				m_sEngineerTask = L"Gather metal";
				return GetAmmo(pCmd, pLocal, true);
			}

			if (BuildingNeedsToBeSmacked(pSentry->As<CBaseObject>()))
				return SmackBuilding(pCmd, pLocal, pSentry->As<CBaseObject>());
			else
			{
				auto pDispenser = I::ClientEntityList->GetClientEntity(m_iMyDispenserIdx);
				// We put dispenser by sentry
				if (!pDispenser)
					return BuildBuilding(pCmd, pLocal, tClosestEnemy, dispenser);
				else
				{
					// We already have a dispenser, see if it needs to be smacked
					if (!pLocal->m_iMetalCount())
					{
						m_sEngineerTask = L"Gather metal";
						return GetAmmo(pCmd, pLocal, true);
					}

					if (BuildingNeedsToBeSmacked(pDispenser->As<CBaseObject>()))
						return SmackBuilding(pCmd, pLocal, pDispenser->As<CBaseObject>());

					// Return false so we run another task
					return false;
				}
			}

		}
	}
	// Try to build a sentry
	return BuildBuilding(pCmd, pLocal, tClosestEnemy, sentry);
}