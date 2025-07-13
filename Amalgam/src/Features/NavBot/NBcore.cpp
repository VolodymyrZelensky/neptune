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
#include <unordered_set>
#include <format>
#include <optional>
#include <functional>
#include <algorithm>
#include <cmath>

bool CNavBot::ShouldSearchHealth(CTFPlayer* pLocal, bool bLowPrio) const
{
	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::SearchHealth))
		return false;

	// Priority too high
	if (F::NavEngine.current_priority > health)
		return false;

	if (SDK::IsSaxton(pLocal))
		return false;

	// Check if being gradually healed in any way
	if (pLocal->m_nPlayerCond() & (1 << 21)/*TFCond_Healing*/)
		return false;

	const float flHealthPercent = static_cast<float>(pLocal->m_iHealth()) / pLocal->GetMaxHealth();
	// Get health when below threshold, or below higher threshold when low priority
	return flHealthPercent < HEALTH_SEARCH_THRESHOLD_LOW || 
		   (bLowPrio && (F::NavEngine.current_priority <= patrol || F::NavEngine.current_priority == lowprio_health) && 
		    flHealthPercent <= HEALTH_SEARCH_THRESHOLD_HIGH);
}

bool CNavBot::ShouldSearchAmmo(CTFPlayer* pLocal) const
{
	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::SearchAmmo))
		return false;

	// Priority too high
	if (F::NavEngine.current_priority > ammo)
		return false;

	if (SDK::IsSaxton(pLocal))
		return false;

	for (int i = 0; i < 2; i++)
	{
		auto pWeapon = pLocal->GetWeaponFromSlot(i);
		if (!pWeapon || SDK::WeaponDoesNotUseAmmo(pWeapon))
			continue;

		const int iWepID = pWeapon->GetWeaponID();
		const int iMaxClip = pWeapon->GetWeaponInfo() ? pWeapon->GetWeaponInfo()->iMaxClip1 : 0;
		const int iCurClip = pWeapon->m_iClip1();
		
		// Special case for sniper rifles - very low ammo threshold
		if ((iWepID == TF_WEAPON_SNIPERRIFLE ||
			iWepID == TF_WEAPON_SNIPERRIFLE_CLASSIC ||
			iWepID == TF_WEAPON_SNIPERRIFLE_DECAP) &&
			pLocal->GetAmmoCount(pWeapon->m_iPrimaryAmmoType()) <= 5)
			return true;

		if (!pWeapon->HasAmmo())
			return true;

		const int iMaxAmmo = SDK::GetWeaponMaxReserveAmmo(iWepID, pWeapon->m_iItemDefinitionIndex());
		if (!iMaxAmmo)
			continue;

		const int iResAmmo = pLocal->GetAmmoCount(pWeapon->m_iPrimaryAmmoType());
		
		// If clip and reserve are both very low, definitely get ammo
		if (iMaxClip > 0 && iCurClip <= iMaxClip * 0.25f && iResAmmo <= iMaxAmmo * 0.25f)
			return true;
			
		// Don't search for ammo if we have more than threshold of max reserve
		if (iResAmmo >= iMaxAmmo * AMMO_SEARCH_THRESHOLD)
			continue;
			
		// Search for ammo if we're below critical threshold
		if (iResAmmo <= iMaxAmmo * AMMO_CRITICAL_THRESHOLD)
			return true;
	}

	return false;
}

int CNavBot::ShouldTarget(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int iPlayerIdx) const
{
	auto pEntity = I::ClientEntityList->GetClientEntity(iPlayerIdx);
	if (!pEntity || !pEntity->IsPlayer())
		return -1;

	auto pPlayer = pEntity->As<CTFPlayer>();
	if (!pPlayer->IsAlive() || pPlayer == pLocal)
		return -1;

	// pipe local playa
	PlayerInfo_t pi{};
	if (I::EngineClient->GetPlayerInfo(iPlayerIdx, &pi) && F::NamedPipe::IsLocalBot(pi.friendsID))
		return 0;

	if (F::PlayerUtils.IsIgnored(iPlayerIdx))
		return 0;

	if (Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Friends && H::Entities.IsFriend(iPlayerIdx)
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Party && H::Entities.InParty(iPlayerIdx)
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Invulnerable && pPlayer->IsInvulnerable() && G::SavedDefIndexes[SLOT_MELEE] != Heavy_t_TheHolidayPunch
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Cloaked && pPlayer->m_flInvisibility() && pPlayer->m_flInvisibility() >= Vars::Aimbot::General::IgnoreCloak.Value / 100.f
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::DeadRinger && pPlayer->m_bFeignDeathReady()
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Taunting && pPlayer->IsTaunting()
		|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Disguised && pPlayer->InCond(TF_COND_DISGUISED))
		return 0;

	if (pPlayer->m_iTeamNum() == pLocal->m_iTeamNum())
		return 0;

	if (Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Vaccinator)
	{
		switch (SDK::GetWeaponType(pWeapon))
		{
		case EWeaponType::HITSCAN:
			if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BULLET_RESIST) && SDK::AttribHookValue(0, "mod_pierce_resists_absorbs", pWeapon) != 0)
				return 0;
			break;
		case EWeaponType::PROJECTILE:
			if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_FIRE_RESIST) && (G::SavedWepIds[SLOT_PRIMARY] == TF_WEAPON_FLAMETHROWER && G::SavedWepIds[SLOT_SECONDARY] == TF_WEAPON_FLAREGUN))
				return 0;
			else if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BULLET_RESIST) && G::SavedWepIds[SLOT_PRIMARY] == TF_WEAPON_COMPOUND_BOW)
				return 0;
			else if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BLAST_RESIST))
				return 0;
		}
	}

	return 1;
}

int CNavBot::ShouldTargetBuilding(CTFPlayer* pLocal, int iEntIdx) const
{
	auto pEntity = I::ClientEntityList->GetClientEntity(iEntIdx);
	if (!pEntity || !pEntity->IsBuilding())
		return -1;

	auto pBuilding = pEntity->As<CBaseObject>();
	if (!(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Sentry) && pBuilding->IsSentrygun()
		|| !(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Dispenser) && pBuilding->IsDispenser()
		|| !(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Teleporter) && pBuilding->IsTeleporter())
		return 1;

	if (pLocal->m_iTeamNum() == pBuilding->m_iTeamNum())
		return 1;

	auto pOwner = pBuilding->m_hBuilder().Get();
	if (pOwner)
	{
		if (F::PlayerUtils.IsIgnored(pOwner->entindex()))
			return 0;

		if (Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Friends && H::Entities.IsFriend(pOwner->entindex())
			|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Party && H::Entities.InParty(pOwner->entindex()))
			return 0;
	}

	return 1;
}

bool CNavBot::ShouldAssist(CTFPlayer* pLocal, int iTargetIdx) const
{
	auto pEntity = I::ClientEntityList->GetClientEntity(iTargetIdx);
	if (!pEntity || pEntity->As<CBaseEntity>()->m_iTeamNum() != pLocal->m_iTeamNum())
		return false;

	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::HelpFriendlyCaptureObjectives))
		return true;

	if (F::PlayerUtils.IsIgnored(iTargetIdx)
		|| H::Entities.InParty(iTargetIdx)
		|| H::Entities.IsFriend(iTargetIdx))
		return true;

	return false;
}

std::vector<CObjectDispenser*> CNavBot::GetDispensers(CTFPlayer* pLocal) const
{
	std::vector<CObjectDispenser*> vDispensers;
	for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_TEAMMATES))
	{
		if (pEntity->GetClassID() != ETFClassID::CObjectDispenser)
			continue;

		auto pDispenser = pEntity->As<CObjectDispenser>();
		if (pDispenser->m_bCarryDeploy() || pDispenser->m_bHasSapper() || pDispenser->m_bBuilding())
			continue;

		auto vOrigin = F::NavParser.GetDormantOrigin(pDispenser->entindex());
		if (!vOrigin)
			continue;

		Vec2 vOrigin2D = Vec2(vOrigin->x, vOrigin->y);
		auto pLocalNav = F::NavEngine.findClosestNavSquare(*vOrigin);

		// This fixes the fact that players can just place dispensers in unreachable locations
		if (pLocalNav->getNearestPoint(vOrigin2D).DistToSqr(*vOrigin) > pow(300.0f, 2) ||
			pLocalNav->getNearestPoint(vOrigin2D).z - vOrigin->z > PLAYER_JUMP_HEIGHT)
			continue;

		vDispensers.push_back(pDispenser);
	}

	// Sort by distance, closer is better
	auto vLocalOrigin = pLocal->GetAbsOrigin();
	std::sort(vDispensers.begin(), vDispensers.end(), [&](CBaseEntity* a, CBaseEntity* b) -> bool
			  {
				  return F::NavParser.GetDormantOrigin(a->entindex())->DistToSqr(vLocalOrigin) < F::NavParser.GetDormantOrigin(b->entindex())->DistToSqr(vLocalOrigin);
			  });

	return vDispensers;
}

std::vector<CBaseEntity*> CNavBot::GetEntities(CTFPlayer* pLocal, bool bHealth) const
{
	EGroupType eGroupType = bHealth ? EGroupType::PICKUPS_HEALTH : EGroupType::PICKUPS_AMMO;

	std::vector<CBaseEntity*> vEntities;
	for (auto pEntity : H::Entities.GetGroup(eGroupType))
	{
		if (!pEntity->IsDormant())
			vEntities.push_back(pEntity);
	}

	// Sort by distance, closer is better
	auto vLocalOrigin = pLocal->GetAbsOrigin();
	std::sort(vEntities.begin(), vEntities.end(), [&](CBaseEntity* a, CBaseEntity* b) -> bool
			  {
				  return a->GetAbsOrigin().DistToSqr(vLocalOrigin) < b->GetAbsOrigin().DistToSqr(vLocalOrigin);
			  });

	return vEntities;
}

bool CNavBot::GetHealth(CUserCmd* pCmd, CTFPlayer* pLocal, bool bLowPrio)
{
	if (SDK::IsSaxton(pLocal))
		return false;

	const Priority_list ePriority = bLowPrio ? lowprio_health : health;
	static Timer tHealthCooldown{};
	static Timer tRepathTimer;
	if (!tHealthCooldown.Check(1.f))
		return F::NavEngine.current_priority == ePriority;

	// This should also check if pLocal is valid
	if (ShouldSearchHealth(pLocal, bLowPrio))
	{
		// Already pathing, only try to repath every 2s
		if (F::NavEngine.current_priority == ePriority && !tRepathTimer.Run(2.f))
			return true;

		auto vHealthpacks = GetEntities(pLocal, true);
		auto vDispensers = GetDispensers(pLocal);
		auto vTotalEnts = vHealthpacks;

		// Add dispensers and sort list again
		const auto vLocalOrigin = pLocal->GetAbsOrigin();
		if (!vDispensers.empty())
		{
			vTotalEnts.reserve(vHealthpacks.size() + vDispensers.size());
			vTotalEnts.insert(vTotalEnts.end(), vDispensers.begin(), vDispensers.end());
			std::sort(vTotalEnts.begin(), vTotalEnts.end(), [&](CBaseEntity* a, CBaseEntity* b) -> bool
					  {
						  return a->GetAbsOrigin().DistToSqr(vLocalOrigin) < b->GetAbsOrigin().DistToSqr(vLocalOrigin);
					  });
		}

		CBaseEntity* pBestEnt = nullptr;
		if (!vTotalEnts.empty())
			pBestEnt = vTotalEnts.front();

		if (vTotalEnts.size() > 1)
		{
			F::NavEngine.navTo(pBestEnt->GetAbsOrigin(), ePriority, true, pBestEnt->GetAbsOrigin().DistToSqr(vLocalOrigin) > pow(200.0f, 2));
			auto iFirstTargetPoints = F::NavEngine.crumbs.size();
			F::NavEngine.cancelPath();

			F::NavEngine.navTo(vTotalEnts.at(1)->GetAbsOrigin(), ePriority, true, vTotalEnts.at(1)->GetAbsOrigin().DistToSqr(vLocalOrigin) > pow(200.0f, 2));
			auto iSecondTargetPoints = F::NavEngine.crumbs.size();
			F::NavEngine.cancelPath();

			pBestEnt = iSecondTargetPoints < iFirstTargetPoints ? vTotalEnts.at(1) : pBestEnt;
		}

		if (pBestEnt)
		{
			if (F::NavEngine.navTo(pBestEnt->GetAbsOrigin(), ePriority, true, pBestEnt->GetAbsOrigin().DistToSqr(vLocalOrigin) > pow(200.0f, 2)))
			{
				// Check if we are close enough to the health pack to pick it up (unless its not a health pack)
				if (pBestEnt->GetAbsOrigin().DistToSqr(pLocal->GetAbsOrigin()) < pow(75.0f, 2) && !pBestEnt->IsDispenser())
				{
					// Try to touch (:3) the health pack
					auto pLocalNav = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());
					if (pLocalNav)
					{
						Vector2D vTo = { pBestEnt->GetAbsOrigin().x, pBestEnt->GetAbsOrigin().y };
						Vector vPathPoint = pLocalNav->getNearestPoint(vTo);
						vPathPoint.z = pBestEnt->GetAbsOrigin().z;

						// Walk towards the health pack
						SDK::WalkTo(pCmd, pLocal, vPathPoint);
					}
				}
				return true;
			}
		}

		tHealthCooldown.Update();
	}
	else if (F::NavEngine.current_priority == ePriority)
		F::NavEngine.cancelPath();

	return false;
}

bool CNavBot::GetAmmo(CUserCmd* pCmd, CTFPlayer* pLocal, bool bForce)
{
	if (SDK::IsSaxton(pLocal))
	return false;

	static Timer tRepathTimer;
	static Timer tAmmoCooldown{};
	static bool bWasForce = false;

	if (!bForce && !tAmmoCooldown.Check(1.f))
		return F::NavEngine.current_priority == ammo;

	if (bForce || ShouldSearchAmmo(pLocal))
	{
		// Already pathing, only try to repath every 2s
		if (F::NavEngine.current_priority == ammo && !tRepathTimer.Run(2.f))
			return true;
		else
			bWasForce = false;

		auto vAmmopacks = GetEntities(pLocal);
		auto vDispensers = GetDispensers(pLocal);
		auto vTotalEnts = vAmmopacks;

		// Add dispensers and sort list again
		const auto vLocalOrigin = pLocal->GetAbsOrigin();
		if (!vDispensers.empty())
		{
			vTotalEnts.reserve(vAmmopacks.size() + vDispensers.size());
			vTotalEnts.insert(vTotalEnts.end(), vDispensers.begin(), vDispensers.end());
			std::sort(vTotalEnts.begin(), vTotalEnts.end(), [&](CBaseEntity* a, CBaseEntity* b) -> bool
					  {
						  return a->GetAbsOrigin().DistToSqr(vLocalOrigin) < b->GetAbsOrigin().DistToSqr(vLocalOrigin);
					  });
		}

		CBaseEntity* pBestEnt = nullptr;
		if (!vTotalEnts.empty())
			pBestEnt = vTotalEnts.front();

		if (vTotalEnts.size() > 1)
		{
			F::NavEngine.navTo(pBestEnt->GetAbsOrigin(), ammo, true, pBestEnt->GetAbsOrigin().DistToSqr(vLocalOrigin) > pow(200.0f, 2));
			const auto iFirstTargetPoints = F::NavEngine.crumbs.size();
			F::NavEngine.cancelPath();

			F::NavEngine.navTo(vTotalEnts.at(1)->GetAbsOrigin(), ammo, true, vTotalEnts.at(1)->GetAbsOrigin().DistToSqr(vLocalOrigin) > pow(200.0f, 2));
			const auto iSecondTargetPoints = F::NavEngine.crumbs.size();
			F::NavEngine.cancelPath();
			pBestEnt = iSecondTargetPoints < iFirstTargetPoints ? vTotalEnts.at(1) : pBestEnt;
		}

		if (pBestEnt)
		{
			if (F::NavEngine.navTo(pBestEnt->GetAbsOrigin(), ammo, true, pBestEnt->GetAbsOrigin().DistToSqr(vLocalOrigin) > pow(200.0f, 2)))
			{
				// Check if we are close enough to the ammo pack to pick it up (unless its not an ammo pack)
				if (pBestEnt->GetAbsOrigin().DistToSqr(pLocal->GetAbsOrigin()) < pow(75.0f, 2) && !pBestEnt->IsDispenser())
				{
					// Try to touch (:3) the ammo pack
					auto pLocalNav = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());
					if (pLocalNav)
					{
						Vector2D vTo = { pBestEnt->GetAbsOrigin().x, pBestEnt->GetAbsOrigin().y };
						Vector vPathPoint = pLocalNav->getNearestPoint(vTo);
						vPathPoint.z = pBestEnt->GetAbsOrigin().z;

						// Walk towards the ammo pack
						SDK::WalkTo(pCmd, pLocal, vPathPoint);
					}
				}
				bWasForce = bForce;
				return true;
			}
		}

		tAmmoCooldown.Update();
	}
	else if (F::NavEngine.current_priority == ammo && !bWasForce)
		F::NavEngine.cancelPath();

	return false;
}

static Timer tRefreshSniperspotsTimer{};
void CNavBot::RefreshSniperSpots()
{
	if (!tRefreshSniperspotsTimer.Run(60.f))
		return;

	m_vSniperSpots.clear();

	// Vector of exposed spots to nav to in case we find no sniper spots
	std::vector<Vector> vExposedSpots;
	// Search all nav areas for valid sniper spots
	for (auto tArea : F::NavEngine.getNavFile()->m_areas)
	{
		// Dont use spawn as a snipe spot
		if (tArea.m_TFattributeFlags & (TF_NAV_SPAWN_ROOM_BLUE | TF_NAV_SPAWN_ROOM_RED | TF_NAV_SPAWN_ROOM_EXIT))
			continue;

		for (auto tHidingSpot : tArea.m_hidingSpots)
		{
			// Spots actually marked for sniping
			if (tHidingSpot.IsGoodSniperSpot() || tHidingSpot.IsIdealSniperSpot() || tHidingSpot.HasGoodCover())
			{
				m_vSniperSpots.emplace_back(tHidingSpot.m_pos);
				continue;
			}

			if (tHidingSpot.IsExposed())
				vExposedSpots.emplace_back(tHidingSpot.m_pos);
		}
	}

	// If we have no sniper spots, just use nav areas marked as exposed. They're good enough for sniping.
	if (m_vSniperSpots.empty() && !vExposedSpots.empty())
		m_vSniperSpots = vExposedSpots;
}

ClosestEnemy_t CNavBot::GetNearestPlayerDistance(CTFPlayer* pLocal, CTFWeaponBase* pWeapon) const
{
	float flMinDist = FLT_MAX;
	int iBest = -1;

	if (pLocal && pWeapon)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
		{
			auto pPlayer = pEntity->As<CTFPlayer>();
			if (!ShouldTarget(pLocal, pWeapon, pPlayer->entindex()))
				continue;

			auto vOrigin = F::NavParser.GetDormantOrigin(pPlayer->entindex());
			if (!vOrigin)
				continue;

			auto flDist = pLocal->GetAbsOrigin().DistToSqr(*vOrigin);
			if (flDist >= flMinDist)
				continue;

			flMinDist = flDist;
			iBest = pPlayer->entindex();
		}
	}
	return ClosestEnemy_t{ iBest, flMinDist };
}


void CNavBot::UpdateEnemyBlacklist(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int iSlot)
{
	if (!pLocal || !pLocal->IsAlive() || !pWeapon || !(Vars::NavEng::NavBot::Blacklist.Value & Vars::NavEng::NavBot::BlacklistEnum::Players))
		return;

	// Clean up expired blacklists periodically
	CleanupExpiredBlacklist();

	// Update area danger scores more frequently and intelligently
	UpdateAreaDangerScores(pLocal, pWeapon, iSlot);
}

bool CNavBot::IsAreaValidForStayNear(Vector vEntOrigin, CNavArea* pArea, bool bFixLocalZ)
{
	if (bFixLocalZ)
		vEntOrigin.z += PLAYER_JUMP_HEIGHT;
	auto vAreaOrigin = pArea->m_center;
	vAreaOrigin.z += PLAYER_JUMP_HEIGHT;

	float flDist = vEntOrigin.DistToSqr(vAreaOrigin);
	// Too close
	if (flDist < pow(m_tSelectedConfig.m_flMinFullDanger, 2))
		return false;

	// Blacklisted
	if (F::NavEngine.getFreeBlacklist()->find(pArea) != F::NavEngine.getFreeBlacklist()->end())
		return false;

	// Too far away
	if (flDist > pow(m_tSelectedConfig.m_flMax, 2))
		return false;

	CGameTrace trace = {};
	CTraceFilterWorldAndPropsOnly filter = {};

	// Attempt to vischeck
	SDK::Trace(vEntOrigin, vAreaOrigin, MASK_SHOT | CONTENTS_GRATE, &filter, &trace);
	return trace.fraction == 1.f;
}

bool CNavBot::StayNearTarget(int iEntIndex)
{
	auto pEntity = I::ClientEntityList->GetClientEntity(iEntIndex);
	if (!pEntity)
		return false;

	auto vOrigin = F::NavParser.GetDormantOrigin(iEntIndex);
	// No origin recorded, don't bother
	if (!vOrigin)
		return false;

	// Add the vischeck height
	vOrigin->z += PLAYER_JUMP_HEIGHT;

	// Use std::pair to avoid using the distance functions more than once
	std::vector<std::pair<CNavArea*, float>> vGoodAreas{};

	for (auto& tArea : F::NavEngine.getNavFile()->m_areas)
	{
		auto vAreaOrigin = tArea.m_center;

		// Is this area valid for stay near purposes?
		if (!IsAreaValidForStayNear(*vOrigin, &tArea, false))
			continue;

		// Good area found
		vGoodAreas.emplace_back(&tArea, (*vOrigin).DistToSqr(vAreaOrigin));
	}
	// Sort based on distance
	if (m_tSelectedConfig.m_bPreferFar)
		std::sort(vGoodAreas.begin(), vGoodAreas.end(), [](std::pair<CNavArea*, float> a, std::pair<CNavArea*, float> b) { return a.second > b.second; });
	else
		std::sort(vGoodAreas.begin(), vGoodAreas.end(), [](std::pair<CNavArea*, float> a, std::pair<CNavArea*, float> b) { return a.second < b.second; });

	// Try to path to all the good areas, based on distance
	for (const auto& pair : vGoodAreas)
	{
		if (F::NavEngine.navTo(pair.first->m_center, staynear, true, !F::NavEngine.isPathing()))
		{
			m_iStayNearTargetIdx = pEntity->entindex();
			if (auto pPlayerResource = H::Entities.GetPR())
				m_sFollowTargetName = SDK::ConvertUtf8ToWide(pPlayerResource->m_pszPlayerName(pEntity->entindex()));
			return true;
		}
	}
	


	return false;
}

int CNavBot::IsStayNearTargetValid(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int iEntIndex)
{
	int iShouldTarget = ShouldTarget(pLocal, pWeapon, iEntIndex);
	int iResult = iEntIndex ? iShouldTarget : 0;
	return iResult;
}

std::optional<std::pair<CNavArea*, int>> CNavBot::FindClosestHidingSpot(CNavArea* pArea, std::optional<Vector> vVischeckPoint, int iRecursionCount, int iRecursionIndex)
{
	static std::vector<CNavArea*> vAlreadyRecursed;
	if (iRecursionIndex == 0)
		vAlreadyRecursed.clear();

	Vector vAreaOrigin = pArea->m_center;
	vAreaOrigin.z += PLAYER_JUMP_HEIGHT;

	// Increment recursion index
	iRecursionIndex++;

	// If the area works, return it
	if (vVischeckPoint && !F::NavParser.IsVectorVisibleNavigation(vAreaOrigin, *vVischeckPoint))
		return std::make_optional(std::pair<CNavArea*, int>{ pArea, iRecursionIndex - 1 });
	// Termination condition not hit yet
	else if (iRecursionIndex < iRecursionCount)
	{
		// Store the nearest area
		std::optional<std::pair<CNavArea*, int>> vBestArea;
		for (auto& tConnection : pArea->m_connections)
		{
			if (std::find(vAlreadyRecursed.begin(), vAlreadyRecursed.end(), tConnection.area) != vAlreadyRecursed.end())
				continue;

			vAlreadyRecursed.push_back(tConnection.area);

			auto pNestedArea = FindClosestHidingSpot(tConnection.area, vVischeckPoint, iRecursionCount, iRecursionIndex);
			if (pNestedArea && (!vBestArea || pNestedArea->second < vBestArea->second))
				vBestArea = std::make_optional(std::pair<CNavArea*, int>{ pNestedArea->first, pNestedArea->second });
		}
		return vBestArea;
	}
	return std::nullopt;
}

bool CNavBot::RunReload(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	static Timer tReloadrunCooldown{};

	// Not reloading, do not run
	if (!G::Reloading && pWeapon->m_iClip1())
		return false;

	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::StalkEnemies))
		return false;

	// Too high priority, so don't try
	if (F::NavEngine.current_priority > run_reload)
		return false;

	// Re-calc only every once in a while
	if (!tReloadrunCooldown.Run(1.f))
		return F::NavEngine.current_priority == run_reload;

	// Get our area and start recursing the neighbours
	auto pLocalNav = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());
	if (!pLocalNav)
		return false;

	// Get closest enemy to vicheck
	CBaseEntity* pClosestEnemy = nullptr;
	float flBestDist = FLT_MAX;
	for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
	{
		if (!ShouldTarget(pLocal, pWeapon, pEntity->entindex()))
			continue;

		float flDist = pEntity->GetAbsOrigin().DistToSqr(pLocal->GetAbsOrigin());
		if (flDist > pow(flBestDist, 2))
			continue;

		flBestDist = flDist;
		pClosestEnemy = pEntity;
	}

	if (!pClosestEnemy)
		return false;

	Vector vVischeckPoint = pClosestEnemy->GetAbsOrigin();
	vVischeckPoint.z += PLAYER_JUMP_HEIGHT;

	// Get the best non visible area
	auto vBestArea = FindClosestHidingSpot(pLocalNav, vVischeckPoint, 5);
	if (!vBestArea)
		return false;

	// If we can, path
	if (F::NavEngine.navTo((*vBestArea).first->m_center, run_reload, true, !F::NavEngine.isPathing()))
		return true;
	return false;
}

slots CNavBot::GetReloadWeaponSlot(CTFPlayer* pLocal, ClosestEnemy_t tClosestEnemy)
{
	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::ReloadWeapons))
		return slots();

	// Priority too high
	if (F::NavEngine.current_priority > capture)
		return slots();

	// Dont try to reload in combat
	if (F::NavEngine.current_priority == staynear && tClosestEnemy.m_flDist <= pow(500, 2)
		|| tClosestEnemy.m_flDist <= pow(250, 2))
		return slots();

	auto pPrimaryWeapon = pLocal->GetWeaponFromSlot(SLOT_PRIMARY);
	auto pSecondaryWeapon = pLocal->GetWeaponFromSlot(SLOT_SECONDARY);
	bool bCheckPrimary = !SDK::WeaponDoesNotUseAmmo(G::SavedWepIds[SLOT_PRIMARY], G::SavedDefIndexes[SLOT_PRIMARY], false);
	bool bCheckSecondary = !SDK::WeaponDoesNotUseAmmo(G::SavedWepIds[SLOT_SECONDARY], G::SavedDefIndexes[SLOT_SECONDARY], false);

	float flDivider = F::NavEngine.current_priority < staynear && tClosestEnemy.m_flDist > pow(500, 2) ? 1.f : 3.f;

	CTFWeaponInfo* pWeaponInfo = nullptr;
	bool bWeaponCantReload = false;
	if (bCheckPrimary && pPrimaryWeapon)
	{
		pWeaponInfo = pPrimaryWeapon->GetWeaponInfo();
		bWeaponCantReload = (!pWeaponInfo || pWeaponInfo->iMaxClip1 < 0 || !pLocal->GetAmmoCount(pPrimaryWeapon->m_iPrimaryAmmoType())) && G::SavedWepIds[SLOT_PRIMARY] != TF_WEAPON_PARTICLE_CANNON && G::SavedWepIds[SLOT_PRIMARY] != TF_WEAPON_DRG_POMSON;
		if (pWeaponInfo && !bWeaponCantReload && G::AmmoInSlot[SLOT_PRIMARY] < (pWeaponInfo->iMaxClip1 / flDivider))
			return primary;
	}

	bool bFoundPrimaryWepInfo = pWeaponInfo;
	if (bCheckSecondary && pSecondaryWeapon && (bFoundPrimaryWepInfo || !bCheckPrimary))
	{
		pWeaponInfo = pSecondaryWeapon->GetWeaponInfo();
		bWeaponCantReload = (!pWeaponInfo || pWeaponInfo->iMaxClip1 < 0 || !pLocal->GetAmmoCount(pSecondaryWeapon->m_iPrimaryAmmoType())) && G::SavedWepIds[SLOT_SECONDARY] != TF_WEAPON_RAYGUN;
		if (pWeaponInfo && !bWeaponCantReload && G::AmmoInSlot[SLOT_SECONDARY] < (pWeaponInfo->iMaxClip1 / flDivider))
			return secondary;
	}

	// We succeeded in finding a reload slot previously
	// return our current slot to avoid getting stuck switching between our best weapon slot and reload weapon slot
	/*if ( m_eLastReloadSlot && ( ( bCheckPrimary && ( !pPrimaryWeapon || !bFoundPrimaryWepInfo ) )
		|| ( bCheckSecondary && ( !pSecondaryWeapon || !pWeaponInfo ) ) ) )
	{
		return m_eLastReloadSlot;
	}*/

	return slots();
}

bool CNavBot::RunSafeReload(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	static Timer tReloadrunCooldown{};
	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::ReloadWeapons) || !m_eLastReloadSlot && !G::Reloading)
	{
		if (F::NavEngine.current_priority == run_safe_reload)
			F::NavEngine.cancelPath();
		return false;
	}

	// Re-calc only every once in a while
	if (!tReloadrunCooldown.Run(1.f))
		return F::NavEngine.current_priority == run_safe_reload;

	// Get our area and start recursing the neighbours
	auto pLocalNav = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());
	if (!pLocalNav)
		return false;

	// If pathing try to avoid going to our current destination until we fully reload
	std::optional<Vector> vCurrentDestination;
	auto pCrumbs = F::NavEngine.getCrumbs();
	if (F::NavEngine.current_priority != run_safe_reload && pCrumbs->size() > 4)
		vCurrentDestination = pCrumbs->at(4).vec;

	if (vCurrentDestination)
		vCurrentDestination->z += PLAYER_JUMP_HEIGHT;
	else
	{
		// Get closest enemy to vicheck
		CBaseEntity* pClosestEnemy = nullptr;
		float flBestDist = FLT_MAX;
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
		{
			if (pEntity->IsDormant())
				continue;

			if (!ShouldTarget(pLocal, pWeapon, pEntity->entindex()))
				continue;

			float flDist = pEntity->GetAbsOrigin().DistToSqr(pLocal->GetAbsOrigin());
			if (flDist > pow(flBestDist, 2))
				continue;

			flBestDist = flDist;
			pClosestEnemy = pEntity;
		}

		if (pClosestEnemy)
		{
			vCurrentDestination = pClosestEnemy->GetAbsOrigin();
			vCurrentDestination->z += PLAYER_JUMP_HEIGHT;
		}
	}
	// Get the best non visible area
	auto vBestArea = FindClosestHidingSpot(pLocalNav, vCurrentDestination, 5);
	if (vBestArea)
	{
		// If we can, path
		if (F::NavEngine.navTo((*vBestArea).first->m_center, run_safe_reload, true, !F::NavEngine.isPathing()))
			return true;
	}

	return false;
}

bool CNavBot::StayNear(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	static Timer tStaynearCooldown{};
	static Timer tInvalidTargetTimer{};
	static int iStayNearTargetIdx = -1;

	// Stay near is expensive so we have to cache. We achieve this by only checking a pre-determined amount of players every
	// CreateMove
	constexpr int MAX_STAYNEAR_CHECKS_RANGE = 3;
	constexpr int MAX_STAYNEAR_CHECKS_CLOSE = 2;

	// Stay near is off
	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::StalkEnemies))
	{
		iStayNearTargetIdx = -1;
		return false;
	}

	// Don't constantly path, it's slow.
	// Far range classes do not need to repath nearly as often as close range ones.
	if (!tStaynearCooldown.Run(m_tSelectedConfig.m_bPreferFar ? 2.f : 0.5f))
		return F::NavEngine.current_priority == staynear;

	// Too high priority, so don't try
	if (F::NavEngine.current_priority > staynear)
	{
		iStayNearTargetIdx = -1;
		return false;
	}

	int iPreviousTargetValid = IsStayNearTargetValid(pLocal, pWeapon, iStayNearTargetIdx);
	// Check and use our previous target if available
	if (iPreviousTargetValid)
	{
		tInvalidTargetTimer.Update();

		// Check if target is RAGE status - if so, always keep targeting them
		int iPriority = H::Entities.GetPriority(iStayNearTargetIdx);
		if (iPriority > F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(DEFAULT_TAG)].m_iPriority)
		{
			if (StayNearTarget(iStayNearTargetIdx))
				return true;
		}

		auto vOrigin = F::NavParser.GetDormantOrigin(iStayNearTargetIdx);
		if (vOrigin)
		{
			// Check if current target area is valid
			if (F::NavEngine.isPathing())
			{
				auto pCrumbs = F::NavEngine.getCrumbs();
				// We cannot just use the last crumb, as it is always nullptr
				if (pCrumbs->size() > 2)
				{
					auto tLastCrumb = (*pCrumbs)[pCrumbs->size() - 2];
					// Area is still valid, stay on it
					if (IsAreaValidForStayNear(*vOrigin, tLastCrumb.navarea))
						return true;
				}
			}
			// Else Check our origin for validity (Only for ranged classes)
			else if (m_tSelectedConfig.m_bPreferFar && IsAreaValidForStayNear(*vOrigin, F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin())))
				return true;
		}
		// Else we try to path again
		if (StayNearTarget(iStayNearTargetIdx))
			return true;

	}
	// Our previous target wasn't properly checked, try again unless
	else if (iPreviousTargetValid == -1 && !tInvalidTargetTimer.Check(0.1f))
		return F::NavEngine.current_priority == staynear;

	// Failed, invalidate previous target and try others
	iStayNearTargetIdx = -1;
	tInvalidTargetTimer.Update();

	// Cancel path so that we dont follow old target
	if (F::NavEngine.current_priority == staynear)
		F::NavEngine.cancelPath();

	std::vector<std::pair<int, int>> vPriorityPlayers{};
	std::unordered_set<int> sHasPriority{};
	for (const auto& pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
	{
		int iPriority = H::Entities.GetPriority(pEntity->entindex());
		if (iPriority > F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(DEFAULT_TAG)].m_iPriority)
		{
			vPriorityPlayers.push_back({ pEntity->entindex(), iPriority });
			sHasPriority.insert(pEntity->entindex());
		}
	}
	std::sort(vPriorityPlayers.begin(), vPriorityPlayers.end(), [](std::pair<int, int> a, std::pair<int, int> b) { return a.second > b.second; });

	// First check for RAGE players - they get highest priority
	for (auto [iPlayerIdx, _] : vPriorityPlayers)
	{
		if (!IsStayNearTargetValid(pLocal, pWeapon, iPlayerIdx))
			continue;

		if (StayNearTarget(iPlayerIdx))
		{
			iStayNearTargetIdx = iPlayerIdx;
			return true;
		}
	}

	// Then check other players
	int iCalls = 0;
	auto iAdvanceCount = m_tSelectedConfig.m_bPreferFar ? MAX_STAYNEAR_CHECKS_RANGE : MAX_STAYNEAR_CHECKS_CLOSE;
	std::vector<std::pair<int, float>> vSortedPlayers{};
	for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
	{
		if (iCalls >= iAdvanceCount)
			break;
		iCalls++;

		// Skip RAGE players as we already checked them
		if (sHasPriority.contains(pEntity->entindex()))
			continue;

		auto iPlayerIdx = pEntity->entindex();
		if (!IsStayNearTargetValid(pLocal, pWeapon, iPlayerIdx))
		{
			iCalls--;
			continue;
		}

		auto vOrigin = F::NavParser.GetDormantOrigin(iPlayerIdx);
		if (!vOrigin)
			continue;

		vSortedPlayers.push_back({ iPlayerIdx, vOrigin->DistToSqr(pLocal->GetAbsOrigin()) });
	}
	if (!vSortedPlayers.empty())
	{
		std::sort(vSortedPlayers.begin(), vSortedPlayers.end(), [](std::pair<int, float> a, std::pair<int, float> b) { return a.second < b.second; });

		for (auto [iIdx, _] : vSortedPlayers)
		{
			// Succeeded pathing
			if (StayNearTarget(iIdx))
			{
				iStayNearTargetIdx = iIdx;
				return true;
			}
		}
	}

	// Stay near failed to find any good targets, add extra delay
	tStaynearCooldown += 3.f;
	return false;
}

bool CNavBot::MeleeAttack(CUserCmd* pCmd, CTFPlayer* pLocal, int iSlot, ClosestEnemy_t tClosestEnemy)
{
	static bool bIsVisible = false;
	auto pEntity = I::ClientEntityList->GetClientEntity(tClosestEnemy.m_iEntIdx);
	if (!pEntity || pEntity->IsDormant())
		return F::NavEngine.current_priority == prio_melee;

	if (iSlot != melee || m_eLastReloadSlot)
	{
		if (F::NavEngine.current_priority == prio_melee)
			F::NavEngine.cancelPath();
		return false;
	}

	auto pPlayer = pEntity->As<CTFPlayer>();
	if (pPlayer->IsInvulnerable() && G::SavedDefIndexes[SLOT_MELEE] != Heavy_t_TheHolidayPunch)
		return false;

	// Too high priority, so don't try
	if (F::NavEngine.current_priority > prio_melee)
		return false;

	Vector vTargetOrigin = pPlayer->GetAbsOrigin();
	Vector vLocalOrigin = pLocal->GetAbsOrigin();
	
	float flHeightDiff = vTargetOrigin.z - vLocalOrigin.z;
	const float flMaxMeleeHeight = 120.0f;
	
	if (flHeightDiff > flMaxMeleeHeight)
	{
		auto pLocalNav = F::NavEngine.findClosestNavSquare(vLocalOrigin);
		auto pTargetNav = F::NavEngine.findClosestNavSquare(vTargetOrigin);
		
		if (!pLocalNav || !pTargetNav)
			return false;
		
		float flNavHeightDiff = pTargetNav->m_center.z - pLocalNav->m_center.z;
		if (flNavHeightDiff > flMaxMeleeHeight * 1.2f)
			return false;
		
		if (F::NavEngine.current_priority == prio_melee && F::NavEngine.isPathing())
		{
			auto pCrumbs = F::NavEngine.getCrumbs();
			if (pCrumbs && pCrumbs->size() <= 2)
				return false;
		}
		
		static Timer tPathTestCooldown{};
		static bool bLastPathTestResult = false;
		static int iLastTestedEnemy = -1;
		
		if (tPathTestCooldown.Run(2.0f) || iLastTestedEnemy != tClosestEnemy.m_iEntIdx)
		{
			bool bCanPath = F::NavEngine.navTo(vTargetOrigin, prio_melee, true, true);
			if (bCanPath)
			{
				auto pTestCrumbs = F::NavEngine.getCrumbs();
				bLastPathTestResult = pTestCrumbs && pTestCrumbs->size() > 3;
			}
			else
			{
				bLastPathTestResult = false;
			}
			
			F::NavEngine.cancelPath();
			iLastTestedEnemy = tClosestEnemy.m_iEntIdx;
		}
		
		if (!bLastPathTestResult)
			return false;
	}
	
	static Timer tVischeckCooldown{};
	if (tVischeckCooldown.Run(0.2f))
	{
		trace_t trace;
		CTraceFilterHitscan filter{}; filter.pSkip = pLocal;
		SDK::TraceHull(pLocal->GetShootPos(), pPlayer->GetAbsOrigin(), pLocal->m_vecMins() * 0.3f, pLocal->m_vecMaxs() * 0.3f, MASK_PLAYERSOLID, &filter, &trace);
		bIsVisible = trace.DidHit() ? trace.m_pEnt && trace.m_pEnt == pPlayer : true;
	}

	// If we are close enough, don't even bother with using the navparser to get there
	if (tClosestEnemy.m_flDist < pow(100.0f, 2) && bIsVisible)
	{
		// Crouch if we are standing on someone
		if (pLocal->m_hGroundEntity().Get() && pLocal->m_hGroundEntity().Get()->IsPlayer())
			pCmd->buttons |= IN_DUCK;

		SDK::WalkTo(pCmd, pLocal, vTargetOrigin);
		F::NavEngine.cancelPath();
		F::NavEngine.current_priority = prio_melee;
		return true;
	}
	
	// Don't constantly path, it's slow.
	// The closer we are, the more we should try to path
	static Timer tMeleeCooldown{};
	if (!tMeleeCooldown.Run(tClosestEnemy.m_flDist < pow(100.0f, 2) ? 0.2f : tClosestEnemy.m_flDist < pow(1000.0f, 2) ? 0.5f : 2.f) && F::NavEngine.isPathing())
		return F::NavEngine.current_priority == prio_melee;

	// Just walk at the enemy l0l
	if (F::NavEngine.navTo(vTargetOrigin, prio_melee, true, !F::NavEngine.isPathing()))
		return true;
	return false;
}

bool CNavBot::IsAreaValidForSnipe(Vector vEntOrigin, Vector vAreaOrigin, bool bFixSentryZ)
{
	if (bFixSentryZ)
		vEntOrigin.z += 40.0f;
	vAreaOrigin.z += PLAYER_JUMP_HEIGHT;

	// Too close to be valid
	if (vEntOrigin.DistToSqr(vAreaOrigin) <= pow(1100.0f + HALF_PLAYER_WIDTH, 2))
		return false;

	// Fails vischeck, bad
	if (!F::NavParser.IsVectorVisibleNavigation(vAreaOrigin, vEntOrigin))
		return false;
	return true;
}

bool CNavBot::TryToSnipe(CBaseObject* pBulding)
{
	auto vOrigin = F::NavParser.GetDormantOrigin(pBulding->entindex());
	if (!vOrigin)
		return false;

	// Add some z to dormant sentries as it only returns origin
	//if (ent->IsDormant())
	vOrigin->z += 40.0f;

	std::vector<std::pair<CNavArea*, float>> vGoodAreas;
	for (auto& area : F::NavEngine.getNavFile()->m_areas)
	{
		// Not usable
		if (!IsAreaValidForSnipe(*vOrigin, area.m_center, false))
			continue;
		vGoodAreas.push_back(std::pair<CNavArea*, float>(&area, area.m_center.DistToSqr(*vOrigin)));
	}

	// Sort based on distance
	if (m_tSelectedConfig.m_bPreferFar)
		std::sort(vGoodAreas.begin(), vGoodAreas.end(), [](std::pair<CNavArea*, float> a, std::pair<CNavArea*, float> b) { return a.second > b.second; });
	else
		std::sort(vGoodAreas.begin(), vGoodAreas.end(), [](std::pair<CNavArea*, float> a, std::pair<CNavArea*, float> b) { return a.second < b.second; });

	if (std::ranges::any_of(vGoodAreas, [](std::pair<CNavArea*, float> pair) { return F::NavEngine.navTo(pair.first->m_center, snipe_sentry); }))
		return true;
	return false;
}

int CNavBot::IsSnipeTargetValid(CTFPlayer* pLocal, int iBuildingIdx)
{
	if (!(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Sentry))
		return 0;

	int iShouldTarget = ShouldTargetBuilding(pLocal, iBuildingIdx);
	int iResult = iBuildingIdx ? iShouldTarget : 0;
	return iResult;
}

bool CNavBot::SnipeSentries(CTFPlayer* pLocal)
{
	static Timer tSentrySnipeCooldown;
	static Timer tInvalidTargetTimer{};
	static int iPreviousTargetIdx = -1;

	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::TargetSentries))
		return false;

	// Make sure we don't try to do it on shortrange classes unless specified
	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::TargetSentriesLowRange)
		&& (pLocal->m_iClass() == TF_CLASS_SCOUT || pLocal->m_iClass() == TF_CLASS_PYRO))
		return false;

	// Sentries don't move often, so we can use a slightly longer timer
	if (!tSentrySnipeCooldown.Run(2.f))
		return F::NavEngine.current_priority == snipe_sentry;

	int iPreviousTargetValid = IsSnipeTargetValid(pLocal, iPreviousTargetIdx);
	if (iPreviousTargetValid)
	{
		tInvalidTargetTimer.Update();

		auto pCrumbs = F::NavEngine.getCrumbs();
		if (auto pPrevTarget = I::ClientEntityList->GetClientEntity(iPreviousTargetIdx)->As<CBaseObject>())
		{
			auto vOrigin = F::NavParser.GetDormantOrigin(iPreviousTargetIdx);
			if (vOrigin)
			{
				// We cannot just use the last crumb, as it is always nullptr
				if (pCrumbs->size() > 2)
				{
					auto tLastCrumb = (*pCrumbs)[pCrumbs->size() - 2];

					// Area is still valid, stay on it
					if (IsAreaValidForSnipe(*vOrigin, tLastCrumb.navarea->m_center))
						return true;
				}
				if (TryToSnipe(pPrevTarget))
					return true;
			}
		}
	}
	// Our previous target wasn't properly checked
	else if (iPreviousTargetValid == -1 && !tInvalidTargetTimer.Check(0.1f))
		return F::NavEngine.current_priority == snipe_sentry;

	tInvalidTargetTimer.Update();

	for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_ENEMIES))
	{
		// Invalid sentry
		if (IsSnipeTargetValid(pLocal, pEntity->entindex()))
			continue;

		// Succeeded in trying to snipe it
		if (TryToSnipe(pEntity->As<CBaseObject>()))
		{
			iPreviousTargetIdx = pEntity->entindex();
			return true;
		}
	}

	iPreviousTargetIdx = -1;
	return false;
}

std::optional<Vector> CNavBot::GetCtfGoal(CTFPlayer* pLocal, int iOurTeam, int iEnemyTeam)
{
	// Get Flag related information
	auto iStatus = F::FlagController.GetStatus(iEnemyTeam);
	auto vPosition = F::FlagController.GetPosition(iEnemyTeam);
	auto iCarrierIdx = F::FlagController.GetCarrier(iEnemyTeam);

	// CTF is the current capture type
	if (iStatus == TF_FLAGINFO_STOLEN)
	{
		if (iCarrierIdx == pLocal->entindex())
		{
			// Return our capture point location
			return F::FlagController.GetSpawnPosition(iOurTeam);
		}
		// Assist with capturing
		else if (Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::HelpCaptureObjectives)
		{
			if (ShouldAssist(pLocal, iCarrierIdx))
			{
				// Stay slightly behind and to the side to avoid blocking
				if (vPosition)
				{
					Vector vOffset(40.0f, 40.0f, 0.0f);
					*vPosition -= vOffset;
				}
				return vPosition;
			}
		}
		return std::nullopt;
	}

	// Get the flag if not taken by us already
	return vPosition;
}

std::optional<Vector> CNavBot::GetPayloadGoal(const Vector vLocalOrigin, int iOurTeam)
{
	auto vPosition = F::PLController.GetClosestPayload(vLocalOrigin, iOurTeam);
	if (!vPosition)
		return std::nullopt;

	Vector vAdjusted_pos = *vPosition;

	// Adjust position, so it's not floating high up, provided the local player is close.
	if (vLocalOrigin.DistToSqr(vAdjusted_pos) <= pow(75.0f, 2))
		vAdjusted_pos.z = vLocalOrigin.z;

	// If close enough, don't move (mostly due to lifts)
	if (vAdjusted_pos.DistToSqr(vLocalOrigin) <= pow(10.0f, 2))
	{
		m_bOverwriteCapture = true;
		return std::nullopt;
	}

	return vAdjusted_pos;
}

std::optional<Vector> CNavBot::GetControlPointGoal(const Vector vLocalOrigin, int iOurTeam)
{
	static Vector vPreviousPosition;
	static Vector vRandomizedPosition;

	auto vPosition = F::CPController.GetClosestControlPoint(vLocalOrigin, iOurTeam);
	if (!vPosition)
		return std::nullopt;

	// Get number of teammates on point
	int iTeammatesOnPoint = 0;
	constexpr float flCapRadius = 100.0f; // Approximate capture radius

	for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_TEAMMATES))
	{
		if (pEntity->IsDormant() || pEntity->entindex() == I::EngineClient->GetLocalPlayer())
			continue;

		auto pTeammate = pEntity->As<CTFPlayer>();
		if (!pTeammate->IsAlive())
			continue;

		if (pTeammate->GetAbsOrigin().DistToSqr(*vPosition) <= pow(flCapRadius, 2))
			iTeammatesOnPoint++;
	}

	// Check for enemies near point
	bool bEnemiesNear = false;
	constexpr float flThreatRadius = 800.0f; // Distance to check for enemies

	for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
	{
		if (pEntity->IsDormant())
			continue;

		auto pEnemy = pEntity->As<CTFPlayer>();
		if (!pEnemy->IsAlive())
			continue;

		if (pEnemy->GetAbsOrigin().DistToSqr(*vPosition) <= pow(flThreatRadius, 2))
		{
			bEnemiesNear = true;
			break;
		}
	}

	Vector vAdjustedPos = *vPosition;

	// If enemies are near, take defensive positions
	if (bEnemiesNear)
	{
		// Find nearby cover points
		for (auto tArea : F::NavEngine.getNavFile()->m_areas)
		{
			for (auto& tHidingSpot : tArea.m_hidingSpots)
			{
				if (tHidingSpot.HasGoodCover() && tHidingSpot.m_pos.DistTo(*vPosition) <= flCapRadius)
				{
					vAdjustedPos = tHidingSpot.m_pos;
					break;
				}
			}
		}
	}
	else
	{
		// Only update position when needed
		if (vPreviousPosition != *vPosition || !F::NavEngine.isPathing())
		{
			vPreviousPosition = *vPosition;

			if (!Vars::NavEng::NavBot::NoRandomizeCPSpot.Value)
			{
				constexpr float flBaseRadius = 120.0f;

				iTeammatesOnPoint++;

				float flAngle = PI * 2 * (float)(I::EngineClient->GetLocalPlayer() % iTeammatesOnPoint) / iTeammatesOnPoint;
				float flRadius = flBaseRadius + SDK::RandomFloat(-10.0f, 10.0f);
				Vector vOffset(cos(flAngle) * flRadius, sin(flAngle) * flRadius, 0.0f);

				vAdjustedPos += vOffset;
			}
		}
	}
	// If close enough, don't move
	if ((vAdjustedPos.DistToSqr(vLocalOrigin) <= pow(50.0f, 2)))
	{
		m_bOverwriteCapture = true;
		return std::nullopt;
	}

	return vAdjustedPos;
}

std::optional<Vector> CNavBot::GetDoomsdayGoal(CTFPlayer* pLocal, int iOurTeam, int iEnemyTeam)
{
	int iTeam = TEAM_UNASSIGNED;
	while (iTeam != -1)
	{
		auto tFlag = F::FlagController.GetFlag(iTeam);
		if (tFlag.m_pFlag)
			break;

		iTeam = iTeam != iOurTeam ? iOurTeam : -1;
	}

	// No australium found
	if (iTeam == -1)
		return std::nullopt;

	// Get Australium related information
	auto iStatus = F::FlagController.GetStatus(iTeam);
	auto vPosition = F::FlagController.GetPosition(iTeam);
	auto iCarrierIdx = F::FlagController.GetCarrier(iTeam);

	if (iStatus == TF_FLAGINFO_STOLEN)
	{
		// We have the australium
		if (iCarrierIdx == pLocal->entindex())
		{
			// Get rocket position - in Doomsday it's marked as a cap point
			auto vRocket = F::CPController.GetClosestControlPoint(pLocal->GetAbsOrigin(), iOurTeam);
			if (vRocket)
			{
				// If close enough, don't move
				if (vRocket->DistToSqr(pLocal->GetAbsOrigin()) <= pow(50.0f, 2))
				{
					m_bOverwriteCapture = true;
					return std::nullopt;
				}

				// Check for enemies near the capture point that might intercept
				bool bEnemiesNearRocket = false;
				constexpr float flThreatRadius = 500.0f; // Distance to check for enemies
				
				for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
				{
					if (pEntity->IsDormant())
						continue;

					auto pEnemy = pEntity->As<CTFPlayer>();
					if (!pEnemy->IsAlive())
						continue;

					if (pEnemy->GetAbsOrigin().DistToSqr(*vRocket) <= pow(flThreatRadius, 2))
					{
						bEnemiesNearRocket = true;
						break;
					}
				}

				// If enemies are near the rocket, stay back a bit until teammates can help
				if (bEnemiesNearRocket && (Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::DefendObjectives))
				{
					// Find a safer approach path or wait for teammates
					auto vPathToRocket = *vRocket - pLocal->GetAbsOrigin();
					float pathLen = vPathToRocket.Length();
					if (pathLen > 0.001f) {
						vPathToRocket.x /= pathLen;
						vPathToRocket.y /= pathLen;
						vPathToRocket.z /= pathLen;
					}
					
					// Back up a bit from the rocket
					Vector vSaferPosition = *vRocket - (vPathToRocket * 300.0f);
					return vSaferPosition;
				}

				return vRocket;
			}
		}
		// Help friendly carrier
		else if (Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::HelpCaptureObjectives)
		{
			if (ShouldAssist(pLocal, iCarrierIdx))
			{
				// Check if carrier is navigating to the rocket
				auto pCarrier = I::ClientEntityList->GetClientEntity(iCarrierIdx);
				if (pCarrier && !pCarrier->IsDormant())
				{
					// Stay slightly behind and to the side to avoid blocking
					if (vPosition)
					{
						// Try to position strategically to protect the carrier
						auto vRocket = F::CPController.GetClosestControlPoint(pCarrier->GetAbsOrigin(), iOurTeam);
						if (vRocket)
						{
							Vector vCarrierToRocket = *vRocket - pCarrier->GetAbsOrigin();
							float len = vCarrierToRocket.Length();
							if (len > 0.001f) {
								vCarrierToRocket.x /= len;
								vCarrierToRocket.y /= len;
								vCarrierToRocket.z /= len;
							}
							
							// Position to the side and slightly behind the carrier in the direction of the rocket
							Vector vCrossProduct = vCarrierToRocket.Cross(Vector(0, 0, 1));
							float crossLen = vCrossProduct.Length();
							if (crossLen > 0.001f) {
								vCrossProduct.x /= crossLen;
								vCrossProduct.y /= crossLen;
								vCrossProduct.z /= crossLen;
							}
							
							// Position offset from carrier toward rocket but slightly to the side
							Vector vOffset = (vCarrierToRocket * -80.0f) + (vCrossProduct * 60.0f);
							return pCarrier->GetAbsOrigin() + vOffset;
						}
						
						// Default offset if rocket position not found
						Vector vOffset(40.0f, 40.0f, 0.0f);
						*vPosition -= vOffset;
					}
					return vPosition;
				}
			}
		}
	}

	// If nobody has the australium, look for it
	if (vPosition)
	{
		// Check if enemies are near the australium
		bool bEnemiesNearAustralium = false;
		constexpr float flThreatRadius = 600.0f;
		
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
		{
			if (pEntity->IsDormant())
				continue;

			auto pEnemy = pEntity->As<CTFPlayer>();
			if (!pEnemy->IsAlive())
				continue;

			if (pEnemy->GetAbsOrigin().DistToSqr(*vPosition) <= pow(flThreatRadius, 2))
			{
				bEnemiesNearAustralium = true;
				break;
			}
		}
		
		// If enemies are near and we're not close, approach carefully
		if (bEnemiesNearAustralium && pLocal->GetAbsOrigin().DistToSqr(*vPosition) > pow(300.0f, 2))
		{
			// Try to find a safer approach path
			auto pClosestNav = F::NavEngine.findClosestNavSquare(*vPosition);
			if (pClosestNav)
			{
				std::optional<Vector> vVischeckPoint = *vPosition;
				if (auto vHidingSpot = FindClosestHidingSpot(pClosestNav, vVischeckPoint, 5))
				{
					return (*vHidingSpot).first->m_center;
				}
			}
		}
	}

	// Get the australium if not taken
	return vPosition;
}

bool CNavBot::CaptureObjectives(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	static Timer tCaptureTimer;
	static Vector vPreviousTarget;
	static int iPreviousStatus = -1;
	static int iPreviousCarrier = -1;

	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::CaptureObjectives))
		return false;

	if (const auto& pGameRules = I::TFGameRules())
	{
		if (!((pGameRules->m_iRoundState() == GR_STATE_RND_RUNNING || pGameRules->m_iRoundState() == GR_STATE_STALEMATE) && !pGameRules->m_bInWaitingForPlayers())
			|| pGameRules->m_iRoundState() == GR_STATE_TEAM_WIN
			|| pGameRules->m_bPlayingSpecialDeliveryMode())
			return false;
	}

	if (F::NavEngine.current_priority == capture)
	{
		bool bObjectiveStatusChanged = false;
		int iOurTeam = pLocal->m_iTeamNum();
		int iEnemyTeam = iOurTeam == TF_TEAM_BLUE ? TF_TEAM_RED : TF_TEAM_BLUE;

		switch (F::GameObjectiveController.m_eGameMode)
		{
		case TF_GAMETYPE_CTF:
		{
			auto iCurrentStatus = F::FlagController.GetStatus(iEnemyTeam);
			auto iCurrentCarrier = F::FlagController.GetCarrier(iEnemyTeam);
			
			// Check if flag status or carrier changed
			if ((iPreviousStatus != -1 && iPreviousStatus != iCurrentStatus) ||
				(iPreviousCarrier != -1 && iPreviousCarrier != iCurrentCarrier))
			{
				bObjectiveStatusChanged = true;
			}
			
			// If we were carrying the flag and successfully captured it
			if (iPreviousCarrier == pLocal->entindex() && iCurrentStatus != TF_FLAGINFO_STOLEN)
			{
				bObjectiveStatusChanged = true;
			}
			
			iPreviousStatus = iCurrentStatus;
			iPreviousCarrier = iCurrentCarrier;
			break;
		}
		case TF_GAMETYPE_CP:
		{
			// Check if control point we were targeting got captured
			auto vCurrentTarget = GetControlPointGoal(pLocal->GetAbsOrigin(), iOurTeam);
			if (!vCurrentTarget || (vPreviousTarget != Vector(0,0,0) && vCurrentTarget->DistToSqr(vPreviousTarget) > pow(100.0f, 2)))
			{
				bObjectiveStatusChanged = true;
			}
			break;
		}
		case TF_GAMETYPE_ESCORT:
		{
			auto vCurrentTarget = GetPayloadGoal(pLocal->GetAbsOrigin(), iOurTeam);
			if (!vCurrentTarget || (vPreviousTarget != Vector(0,0,0) && vCurrentTarget->DistToSqr(vPreviousTarget) > pow(75.0f, 2)))
			{
				bObjectiveStatusChanged = true;
			}
			break;
		}
		default:
		{
			if (F::GameObjectiveController.m_bDoomsday)
			{
				auto iCurrentStatus = F::FlagController.GetStatus(iOurTeam);
				auto iCurrentCarrier = F::FlagController.GetCarrier(iOurTeam);
				
				if ((iPreviousStatus != -1 && iPreviousStatus != iCurrentStatus) ||
					(iPreviousCarrier != -1 && iPreviousCarrier != iCurrentCarrier))
				{
					bObjectiveStatusChanged = true;
				}
				
				iPreviousStatus = iCurrentStatus;
				iPreviousCarrier = iCurrentCarrier;
			}
			break;
		}
		}

		// If objective status changed, cancel current path and force immediate repath
		if (bObjectiveStatusChanged)
		{
			F::NavEngine.cancelPath();
			tCaptureTimer.Update();
			vPreviousTarget = Vector(0, 0, 0);
		}
	}

	float flUpdateInterval = (F::GameObjectiveController.m_eGameMode == TF_GAMETYPE_ESCORT) ? 0.5f : 2.f;
	if (!tCaptureTimer.Check(flUpdateInterval))
		return F::NavEngine.current_priority == capture;

	// Priority too high, don't try
	if (F::NavEngine.current_priority > capture)
		return false;

	// Where we want to go
	std::optional<Vector> vTarget;

	int iOurTeam = pLocal->m_iTeamNum();
	int iEnemyTeam = iOurTeam == TF_TEAM_BLUE ? TF_TEAM_RED : TF_TEAM_BLUE;

	m_bOverwriteCapture = false;

	const auto vLocalOrigin = pLocal->GetAbsOrigin();

	// Run logic
	switch (F::GameObjectiveController.m_eGameMode)
	{
	case TF_GAMETYPE_CTF:
		vTarget = GetCtfGoal(pLocal, iOurTeam, iEnemyTeam);
		break;
	case TF_GAMETYPE_CP:
		vTarget = GetControlPointGoal(vLocalOrigin, iOurTeam);
		break;
	case TF_GAMETYPE_ESCORT:
		vTarget = GetPayloadGoal(vLocalOrigin, iOurTeam);
		break;
	default:
		if (F::GameObjectiveController.m_bDoomsday)
		{
			vTarget = GetDoomsdayGoal(pLocal, iOurTeam, iEnemyTeam);
		}
		break;
	}

	// Overwritten, for example because we are currently on the payload, cancel any sort of pathing and return true
	if (m_bOverwriteCapture)
	{
		F::NavEngine.cancelPath();
		return true;
	}
	// No target, bail and set on cooldown
	else if (!vTarget)
	{
		tCaptureTimer.Update();
		return F::NavEngine.current_priority == capture;
	}
	// If priority is not capturing, or we have a new target, try to path there
	else if (F::NavEngine.current_priority != capture || *vTarget != vPreviousTarget)
	{
		if (F::NavEngine.navTo(*vTarget, capture, true, !F::NavEngine.isPathing()))
		{
			vPreviousTarget = *vTarget;
			return true;
		}
		tCaptureTimer.Update();
	}
	return false;
}

bool CNavBot::Roam(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	static Timer tRoamTimer;
	static std::vector<CNavArea*> vVisitedAreas;
	static Timer tVisitedAreasClearTimer;
	static CNavArea* pCurrentTargetArea = nullptr;
	static int iConsecutiveFails = 0;

	// Clear visited areas every 60 seconds to allow revisiting
	if (tVisitedAreasClearTimer.Run(60.f))
	{
		vVisitedAreas.clear();
		iConsecutiveFails = 0;
	}

	// Don't path constantly
	if (!tRoamTimer.Run(2.f))
		return false;

	if (F::NavEngine.current_priority > patrol)
		return false;

	// Defend our objective if possible
	if (Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::DefendObjectives)
	{
		int iEnemyTeam = pLocal->m_iTeamNum() == TF_TEAM_BLUE ? TF_TEAM_RED : TF_TEAM_BLUE;

		std::optional<Vector> vTarget;
		const auto vLocalOrigin = pLocal->GetAbsOrigin();

		switch (F::GameObjectiveController.m_eGameMode)
		{
		case TF_GAMETYPE_CP:
			vTarget = GetControlPointGoal(vLocalOrigin, iEnemyTeam);
			break;
		case TF_GAMETYPE_ESCORT:
			vTarget = GetPayloadGoal(vLocalOrigin, iEnemyTeam);
			break;
		default:
			break;
		}
		if (vTarget)
		{
			if (auto pClosestNav = F::NavEngine.findClosestNavSquare(*vTarget))
			{
				// Get closest enemy to vicheck
				CBaseEntity* pClosestEnemy = nullptr;
				float flBestDist = FLT_MAX;
				for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
				{
					if (!ShouldTarget(pLocal, pWeapon, pEntity->entindex()))
						continue;

					float flDist = pEntity->GetAbsOrigin().DistToSqr(pClosestNav->m_center);
					if (flDist > pow(flBestDist, 2))
						continue;

					flBestDist = flDist;
					pClosestEnemy = pEntity;
				}

				std::optional<Vector> vVischeckPoint;
				if (pClosestEnemy && flBestDist <= 1000.f)
				{
					vVischeckPoint = pClosestEnemy->GetAbsOrigin();
					vVischeckPoint->z += PLAYER_JUMP_HEIGHT;
				}

				if (auto vClosestSpot = FindClosestHidingSpot(pClosestNav, vVischeckPoint, 5))
				{
					if ((*vClosestSpot).first->m_center.DistToSqr(vLocalOrigin) <= pow(250.0f, 2))
					{
						F::NavEngine.cancelPath();
						m_bDefending = true;
						return true;
					}
					if (F::NavEngine.navTo((*vClosestSpot).first->m_center, patrol, true, !F::NavEngine.isPathing()))
					{
						m_bDefending = true;
						return true;
					}
				}
			}
		}
	}
	m_bDefending = false;

	// If we have a current target and are pathing, continue
	if (pCurrentTargetArea && F::NavEngine.current_priority == patrol)
		return true;

	// Reset current target
	pCurrentTargetArea = nullptr;

	std::vector<CNavArea*> vValidAreas;

	// Get all nav areas
	for (auto& tArea : F::NavEngine.getNavFile()->m_areas)
	{
		// Skip if area is blacklisted
		if (F::NavEngine.getFreeBlacklist()->find(&tArea) != F::NavEngine.getFreeBlacklist()->end())
			continue;

		// Dont run in spawn bitch
		if (tArea.m_TFattributeFlags & (TF_NAV_SPAWN_ROOM_BLUE | TF_NAV_SPAWN_ROOM_RED | TF_NAV_SPAWN_ROOM_EXIT))
			continue;

		// Skip if we recently visited this area
		if (std::find(vVisitedAreas.begin(), vVisitedAreas.end(), &tArea) != vVisitedAreas.end())
			continue;

		// Skip areas that are too close
		float flDist = tArea.m_center.DistToSqr(pLocal->GetAbsOrigin());
		if (flDist < pow(500.0f, 2))
			continue;

		vValidAreas.push_back(&tArea);
	}

	// No valid areas found
	if (vValidAreas.empty())
	{
		// If we failed too many times in a row, clear visited areas
		if (++iConsecutiveFails >= 3)
		{
			vVisitedAreas.clear();
			iConsecutiveFails = 0;
		}
		return false;
	}

	// Reset fail counter since we found valid areas
	iConsecutiveFails = 0;

	// Different strategies for area selection
	std::vector<CNavArea*> vPotentialTargets;

	// Strategy 1: Try to find areas that are far from current position
	for (auto pArea : vValidAreas)
	{
		float flDist = pArea->m_center.DistToSqr(pLocal->GetAbsOrigin());
		if (flDist > pow(2000.0f, 2))
			vPotentialTargets.push_back(pArea);
	}

	// Strategy 2: If no far areas found, try areas that are at medium distance
	if (vPotentialTargets.empty())
	{
		for (auto pArea : vValidAreas)
		{
			float flDist = pArea->m_center.DistToSqr(pLocal->GetAbsOrigin());
			if (flDist > pow(1000.0f, 2) && flDist <= pow(2000.0f, 2))
				vPotentialTargets.push_back(pArea);
		}
	}

	// Strategy 3: If still no areas found, use any valid area
	if (vPotentialTargets.empty())
		vPotentialTargets = vValidAreas;

	// Shuffle the potential targets to add randomness
	for (size_t i = vPotentialTargets.size() - 1; i > 0; i--)
	{
		int j = rand() % (i + 1);
		std::swap(vPotentialTargets[i], vPotentialTargets[j]);
	}

	// Try to path to potential targets
	for (auto pArea : vPotentialTargets)
	{
		if (F::NavEngine.navTo(pArea->m_center, patrol))
		{
			pCurrentTargetArea = pArea;
			vVisitedAreas.push_back(pArea);
			return true;
		}
	}

	return false;
}

// Check if a position is safe from stickies and projectiles
static bool IsPositionSafe(Vector vPos, int iLocalTeam)
{
	if (!(Vars::NavEng::NavBot::Blacklist.Value & Vars::NavEng::NavBot::BlacklistEnum::Stickies) &&
		!(Vars::NavEng::NavBot::Blacklist.Value & Vars::NavEng::NavBot::BlacklistEnum::Projectiles))
		return true;

	for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_PROJECTILES))
	{
		if (pEntity->m_iTeamNum() == iLocalTeam)
			continue;

		auto iClassId = pEntity->GetClassID();
		// Check for stickies
		if (Vars::NavEng::NavBot::Blacklist.Value & Vars::NavEng::NavBot::BlacklistEnum::Stickies && iClassId == ETFClassID::CTFGrenadePipebombProjectile)
		{
			// Skip non-sticky projectiles
			if (pEntity->As<CTFGrenadePipebombProjectile>()->m_iType() != TF_GL_MODE_REMOTE_DETONATE)
				continue;

			float flDist = pEntity->m_vecOrigin().DistToSqr(vPos);
			if (flDist < pow(Vars::NavEng::NavBot::StickyDangerRange.Value, 2))
				return false;
		}

		// Check for rockets and pipes
		if (Vars::NavEng::NavBot::Blacklist.Value & Vars::NavEng::NavBot::BlacklistEnum::Projectiles)
		{
			if (iClassId == ETFClassID::CTFProjectile_Rocket ||
				(iClassId == ETFClassID::CTFGrenadePipebombProjectile && pEntity->As<CTFGrenadePipebombProjectile>()->m_iType() == TF_GL_MODE_REGULAR))
			{
				float flDist = pEntity->m_vecOrigin().DistToSqr(vPos);
				if (flDist < pow(Vars::NavEng::NavBot::ProjectileDangerRange.Value, 2))
					return false;
			}
		}
	}
	return true;
}

bool CNavBot::EscapeProjectiles(CTFPlayer* pLocal)
{
	if (!(Vars::NavEng::NavBot::Blacklist.Value & Vars::NavEng::NavBot::BlacklistEnum::Stickies) &&
		!(Vars::NavEng::NavBot::Blacklist.Value & Vars::NavEng::NavBot::BlacklistEnum::Projectiles))
		return false;

	// Don't interrupt higher priority tasks
	if (F::NavEngine.current_priority > danger)
		return false;

	// Check if current position is unsafe
	if (IsPositionSafe(pLocal->GetAbsOrigin(), pLocal->m_iTeamNum()))
	{
		if (F::NavEngine.current_priority == danger)
			F::NavEngine.cancelPath();
		return false;
	}

	auto pLocalNav = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());
	if (!pLocalNav)
		return false;

	// Find safe nav areas sorted by distance
	std::vector<std::pair<CNavArea*, float>> vSafeAreas;
	for (auto& tArea : F::NavEngine.getNavFile()->m_areas)
	{
		// Skip current area
		if (&tArea == pLocalNav)
			continue;

		// Skip if area is blacklisted
		if (F::NavEngine.getFreeBlacklist()->find(&tArea) != F::NavEngine.getFreeBlacklist()->end())
			continue;

		if (IsPositionSafe(tArea.m_center, pLocal->m_iTeamNum()))
		{
			float flDist = tArea.m_center.DistToSqr(pLocal->GetAbsOrigin());
			vSafeAreas.push_back({ &tArea, flDist });
		}
	}

	// Sort by distance
	std::sort(vSafeAreas.begin(), vSafeAreas.end(),
			  [](const std::pair<CNavArea*, float>& a, const std::pair<CNavArea*, float>& b)
			  {
				  return a.second < b.second;
			  });

	// Try to path to closest safe area
	for (auto& pArea : vSafeAreas)
	{
		if (F::NavEngine.navTo(pArea.first->m_center, danger))
			return true;
	}

	return false;
}

bool CNavBot::EscapeDanger(CTFPlayer* pLocal)
{
	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::EscapeDanger))
		return false;

	// Don't escape while we have the intel
	if (Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::DontEscapeDangerIntel && F::GameObjectiveController.m_eGameMode == TF_GAMETYPE_CTF)
	{
		auto iFlagCarrierIdx = F::FlagController.GetCarrier(pLocal->m_iTeamNum());
		if (iFlagCarrierIdx == pLocal->entindex())
			return false;
	}

	// Priority too high
	if (F::NavEngine.current_priority > danger || F::NavEngine.current_priority == prio_melee || F::NavEngine.current_priority == run_safe_reload)
		return false;

	auto pLocalNav = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());

	// Check if we're in spawn - if so, ignore danger and focus on getting out
	if (pLocalNav && (pLocalNav->m_TFattributeFlags & TF_NAV_SPAWN_ROOM_RED || pLocalNav->m_TFattributeFlags & TF_NAV_SPAWN_ROOM_BLUE))
		return false;

	auto pBlacklist = F::NavEngine.getFreeBlacklist();
	
	// Check if we're in any danger
	bool bInHighDanger = false;
	bool bInMediumDanger = false;
	bool bInLowDanger = false;
	
	if (pBlacklist && pBlacklist->contains(pLocalNav))
	{
		// Check building spot - don't run away from that
		if ((*pBlacklist)[pLocalNav].value == BR_BAD_BUILDING_SPOT)
			return false;
			
		// Determine danger level
		switch ((*pBlacklist)[pLocalNav].value)
		{
		case BR_SENTRY:
		case BR_STICKY:
		case BR_ENEMY_INVULN:
			bInHighDanger = true;
			break;
		case BR_SENTRY_MEDIUM:
		case BR_ENEMY_NORMAL:
			bInMediumDanger = true;
			break;
		case BR_SENTRY_LOW:
		case BR_ENEMY_DORMANT:
			bInLowDanger = true;
			break;
		}
		
		// More intelligent escape logic
		float flHealthRatio = (float)pLocal->m_iHealth() / (float)pLocal->GetMaxHealth();
		
		// Calculate escape priority based on danger level and context
		bool bShouldEscape = false;
		
		if (bInHighDanger)
		{
			bShouldEscape = true; // Always escape high danger
		}
		else if (bInMediumDanger)
		{
			// Escape medium danger if health is low or we're not on a critical mission
			bShouldEscape = (flHealthRatio < 0.6f) || 
			               (F::NavEngine.current_priority != capture && F::NavEngine.current_priority != engineer);
		}
		else if (bInLowDanger)
		{
			// Only escape low danger if health is very low and we're not on an important mission
			bShouldEscape = (flHealthRatio < 0.3f) && 
			               (F::NavEngine.current_priority == patrol || F::NavEngine.current_priority == 0);
		}
		
		if (!bShouldEscape)
			return false;

		static CNavArea* pTargetArea = nullptr;
		// Already running and our target is still valid
		if (F::NavEngine.current_priority == danger && !pBlacklist->contains(pTargetArea))
			return true;

		// Determine the reference position to stay close to
		Vector vReferencePosition;
		bool bHasTarget = false;

		// If we were pursuing a specific objective or following a target, try to stay close to it
		if (F::NavEngine.current_priority != 0 && F::NavEngine.current_priority != danger && !F::NavEngine.crumbs.empty())
		{
			// Use the last crumb in our path as the reference position
			vReferencePosition = F::NavEngine.crumbs.back().vec;
			bHasTarget = true;
		}
		else
		{
			// Use current position if we don't have a target
			vReferencePosition = pLocal->GetAbsOrigin();
		}

		std::vector<std::pair<CNavArea*, float>> vSafeAreas;
		// Copy areas and calculate distances
		for (auto& tArea : F::NavEngine.getNavFile()->m_areas)
		{
			// Skip if area is blacklisted with high danger
			auto it = pBlacklist->find(&tArea);
			if (it != pBlacklist->end())
			{
				// Check danger level - allow pathing through medium or low danger if we have a target
				BlacklistReason_enum danger = it->second.value;
				
				// Skip high danger areas
				if (danger == BR_SENTRY || danger == BR_STICKY || danger == BR_ENEMY_INVULN)
					continue;
					
				// Skip medium danger areas if we don't have a target or have low health
				if ((danger == BR_SENTRY_MEDIUM || danger == BR_ENEMY_NORMAL) && 
				    (!bHasTarget || pLocal->m_iHealth() < pLocal->GetMaxHealth() * 0.5f))
					continue;
			}

			float flDistToReference = tArea.m_center.DistToSqr(vReferencePosition);
			float flDistToCurrent = tArea.m_center.DistToSqr(pLocal->GetAbsOrigin());
			
			// Only consider areas that are not too far away and reachable
			if (flDistToCurrent < pow(2000.0f, 2))
			{
				// If we have a target, prioritize staying near it
				float flScore = bHasTarget ? flDistToReference : flDistToCurrent;
				vSafeAreas.push_back({ &tArea, flScore });
			}
		}

		// Sort by score (closer to reference position is better)
		std::sort(vSafeAreas.begin(), vSafeAreas.end(), [](const std::pair<CNavArea*, float>& a, const std::pair<CNavArea*, float>& b) -> bool
		{
			return a.second < b.second;
		});

		int iCalls = 0;
		// Try to path to safe areas
		for (auto& pArea : vSafeAreas)
		{
			// Try the 10 closest areas (increased from 5 to give more options)
			iCalls++;
			if (iCalls > 10)
				break;

			// Check if this area is safe (not near enemy)
			bool bIsSafe = true;
			for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
			{
				if (!ShouldTarget(pLocal, pLocal->m_hActiveWeapon().Get()->As<CTFWeaponBase>(), pEntity->entindex()))
					continue;

				// If enemy is too close to this area, mark it as unsafe
				float flDist = pEntity->GetAbsOrigin().DistToSqr(pArea.first->m_center);
				if (flDist < pow(m_tSelectedConfig.m_flMinFullDanger * 1.2f, 2))
				{
					bIsSafe = false;
					break;
				}
			}

			// Skip unsafe areas
			if (!bIsSafe)
		    {
				std::vector<std::pair<CNavArea*, BlacklistReason_enum>> vBlacklistedAreas;
				for (auto& [area, blEntry] : *pBlacklist)
				{
				if (blEntry.value == BR_BAD_BUILDING_SPOT)
					continue;
				vBlacklistedAreas.emplace_back(area, blEntry.value);
			}
			if (!vBlacklistedAreas.empty())
			{
				auto getSeverity = [&](BlacklistReason_enum reason) {
					switch (reason)
					{
					case BR_SENTRY_LOW:
					case BR_ENEMY_DORMANT:
						return 0;
					case BR_SENTRY_MEDIUM:
					case BR_ENEMY_NORMAL:
						return 1;
					case BR_SENTRY:
					case BR_STICKY:
					case BR_ENEMY_INVULN:
						return 2;
					default:
						return 1;
					}
				};
				std::sort(vBlacklistedAreas.begin(), vBlacklistedAreas.end(),
					[&](const auto& a, const auto& b) {
						int sa = getSeverity(a.second), sb = getSeverity(b.second);
						if (sa != sb) return sa < sb;
						return a.first->m_center.DistToSqr(pLocal->GetAbsOrigin()) < b.first->m_center.DistToSqr(pLocal->GetAbsOrigin());
					});
				for (auto& [area, reason] : vBlacklistedAreas)
				{
					if (F::NavEngine.navTo(area->m_center, danger, true, false))
					{
						pTargetArea = area;
						return true;
					}
				}
			}
		};

			if (F::NavEngine.navTo(pArea.first->m_center, danger, true, false))
			{
				pTargetArea = pArea.first;
				return true;
			}
		}

		// If we couldn't find a safe area close to the target, fall back to any safe area
		if (iCalls <= 0 || (bInHighDanger && iCalls < 10))
		{
			std::vector<CNavArea*> vAreaPointers;
			// Get all areas
			for (auto& tArea : F::NavEngine.getNavFile()->m_areas)
				vAreaPointers.push_back(&tArea);

			// Sort by distance to player
			std::sort(vAreaPointers.begin(), vAreaPointers.end(), [&](CNavArea* a, CNavArea* b) -> bool
			{
				return a->m_center.DistToSqr(pLocal->GetAbsOrigin()) < b->m_center.DistToSqr(pLocal->GetAbsOrigin());
			});

			// Try to path to any non-blacklisted area
			for (auto& pArea : vAreaPointers)
			{
				auto it = pBlacklist->find(pArea);
				if (it == pBlacklist->end() || 
				   (bInHighDanger && (it->second.value == BR_SENTRY_LOW || it->second.value == BR_ENEMY_DORMANT)))
				{
					iCalls++;
					if (iCalls > 5)
						break;
					if (F::NavEngine.navTo(pArea->m_center, danger, true, false))
					{
						pTargetArea = pArea;
						return true;
					}
				}
			}
		}
		{
			std::vector<std::pair<CNavArea*, BlacklistReason_enum>> vBlacklistedAreas;
			for (auto& [area, blEntry] : *pBlacklist)
			{
				if (blEntry.value == BR_BAD_BUILDING_SPOT)
					continue;
				vBlacklistedAreas.emplace_back(area, blEntry.value);
			}
			if (!vBlacklistedAreas.empty())
			{
				auto getSeverity = [&](BlacklistReason_enum reason) {
					switch (reason)
					{
					case BR_SENTRY_LOW:
					case BR_ENEMY_DORMANT:
						return 0;
					case BR_SENTRY_MEDIUM:
					case BR_ENEMY_NORMAL:
						return 1;
					case BR_SENTRY:
					case BR_STICKY:
					case BR_ENEMY_INVULN:
						return 2;
					default:
						return 1;
					}
				};
				std::sort(vBlacklistedAreas.begin(), vBlacklistedAreas.end(),
					[&](const auto& a, const auto& b) {
						int sa = getSeverity(a.second), sb = getSeverity(b.second);
						if (sa != sb) return sa < sb;
						return a.first->m_center.DistToSqr(pLocal->GetAbsOrigin()) < b.first->m_center.DistToSqr(pLocal->GetAbsOrigin());
					});
				for (auto& [area, reason] : vBlacklistedAreas)
				{
					if (F::NavEngine.navTo(area->m_center, danger, true, false))
					{
						pTargetArea = area;
						return true;
					}
				}
			}
		}
	}
	// No longer in danger
	else if (F::NavEngine.current_priority == danger)
		F::NavEngine.cancelPath();

	if (F::NavEngine.current_priority == danger && F::NavEngine.isPathing())
	{
		auto pCrumbs = F::NavEngine.getCrumbs();
		if (pCrumbs && pCrumbs->size() <= 2)
		{
			F::NavEngine.cancelPath();
		}
	}

	return false;
}

bool CNavBot::EscapeSpawn(CTFPlayer* pLocal)
{
	static Timer tSpawnEscapeCooldown{};

	// Don't try too often
	if (!tSpawnEscapeCooldown.Run(2.f))
		return F::NavEngine.current_priority == escape_spawn;

	auto pLocalNav = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin());
	if (!pLocalNav)
		return false;

	// Cancel if we're not in spawn and this is running
	if (!(pLocalNav->m_TFattributeFlags & (TF_NAV_SPAWN_ROOM_RED | TF_NAV_SPAWN_ROOM_BLUE | TF_NAV_SPAWN_ROOM_EXIT)))
	{
		if (F::NavEngine.current_priority == escape_spawn)
			F::NavEngine.cancelPath();
		return false;
	}

	// Try to find an exit
	for (auto tArea : F::NavEngine.getNavFile()->m_areas)
	{
		// Skip spawn areas
		if (tArea.m_TFattributeFlags & (TF_NAV_SPAWN_ROOM_RED | TF_NAV_SPAWN_ROOM_BLUE | TF_NAV_SPAWN_ROOM_EXIT))
			continue;

		// Try to get there
		if (F::NavEngine.navTo(tArea.m_center, escape_spawn))
			return true;
	}

	return false;
}

void CNavBot::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!Vars::NavEng::NavBot::Enabled.Value || !Vars::NavEng::NavEngine::Enabled.Value || !F::NavEngine.isReady())
	{
		m_iStayNearTargetIdx = -1;
		return;
	}

	if (F::NavEngine.current_priority != staynear)
		m_iStayNearTargetIdx = -1;

	if (F::Ticks.m_bWarp || F::Ticks.m_bDoubletap)
		return;

	if (!pLocal || !pLocal->IsAlive() || !pWeapon || !pCmd)
		return;

	if (pCmd->buttons & (IN_FORWARD | IN_BACK | IN_MOVERIGHT | IN_MOVELEFT) && !F::Misc.m_bAntiAFK)
		return;

	// Frame distribution for expensive operations
	static int frameDistributionCounter = 0;
	frameDistributionCounter = (frameDistributionCounter + 1) % 4;
		
	// Distribute expensive operations across frames
	switch (frameDistributionCounter)
	{
	case 0:
		UpdateLocalBotPositions(pLocal);
		break;
	case 1:
		RefreshSniperSpots();
		RefreshLocalBuildings(pLocal);
		break;
	case 2:
		UpdateClassConfig(pLocal, pWeapon);
		break;
	case 3:
		UpdateEnemyBlacklist(pLocal, pWeapon, m_iCurrentSlot);
		break;
	}

	HandleDoubletapRecharge(pWeapon);
	const auto tClosestEnemy = GetNearestPlayerDistance(pLocal, pWeapon);
	
	// Only refresh building spots every few frames for better performance
	static FastTimer buildingSpotsTimer{};
	if (buildingSpotsTimer.Run(1.0f))
		RefreshBuildingSpots(pLocal, pLocal->GetWeaponFromSlot(SLOT_MELEE), tClosestEnemy);

	HandleMinigunSpinup(pLocal, pWeapon, pCmd, tClosestEnemy);
	UpdateSlot(pLocal, pWeapon, tClosestEnemy);
	
	// Reduce frequency of blacklist cleanup
	static FastTimer blacklistTimer{};
	if (blacklistTimer.Run(0.5f))
		FastCleanupInvalidBlacklists(pLocal);
	
	// --- Behavior scheduler ---
	{
		using TaskFn = std::function<bool()>;
		// Pre-allocate static vector to avoid reallocations
		static std::vector<std::pair<int, TaskFn>> vTasks;
		vTasks.clear();
		vTasks.reserve(12);
		
		// Only add tasks that are likely to be needed to reduce lambda overhead
		vTasks.emplace_back(danger, [&]() { return EscapeProjectiles(pLocal) || EscapeDanger(pLocal); });
		vTasks.emplace_back(escape_spawn, [&]() { return EscapeSpawn(pLocal); });
		vTasks.emplace_back(prio_melee, [&]() { return MeleeAttack(pCmd, pLocal, m_iCurrentSlot, tClosestEnemy); });
		
		// Resource gathering tasks
		vTasks.emplace_back(health, [&]() { return GetHealth(pCmd, pLocal); });
		vTasks.emplace_back(ammo, [&]() { return GetAmmo(pCmd, pLocal); });
		
		vTasks.emplace_back(run_safe_reload, [&]() { return RunSafeReload(pLocal, pWeapon); });
		vTasks.emplace_back(capture, [&]() { return CaptureObjectives(pLocal, pWeapon); });
		
		// Engineer logic only for engineers
		if (pLocal->m_iClass() == TF_CLASS_ENGINEER)
			vTasks.emplace_back(engineer, [&]() { return EngineerLogic(pCmd, pLocal, tClosestEnemy); });
		
		// Add remaining tasks based on class
		if (pLocal->m_iClass() == TF_CLASS_SNIPER)
			vTasks.emplace_back(snipe_sentry, [&]() { return SnipeSentries(pLocal); });
		
		vTasks.emplace_back(staynear, [&]() { return StayNear(pLocal, pWeapon); });
		vTasks.emplace_back(run_reload, [&]() { return MoveInFormation(pLocal, pWeapon); });
		vTasks.emplace_back(patrol, [&]() { return Roam(pLocal, pWeapon); });

		// Pre-sorted by priority (manual sorting to avoid runtime overhead)
		bool bDidSomething = false;
		for (auto& [prio, fn] : vTasks)
		{
			if (fn())
			{
				bDidSomething = true;
				break;
			}
		}

		if (!bDidSomething)
		{
			bDidSomething = EscapeDanger(pLocal) || Roam(pLocal, pWeapon);
		}

		// Optimize crit hack logic
		if (bDidSomething)
		{
			CTFPlayer* pPlayer = nullptr;
			switch (F::NavEngine.current_priority)
			{
			case staynear:
				if (m_iStayNearTargetIdx > 0)
				{
					pPlayer = I::ClientEntityList->GetClientEntity(m_iStayNearTargetIdx)->As<CTFPlayer>();
					if (pPlayer)
						F::CritHack.m_bForce = !pPlayer->IsDormant() && pPlayer->m_iHealth() >= pWeapon->GetDamage();
				}
				break;
			case prio_melee:
			case health:
			case danger:
				if (tClosestEnemy.m_iEntIdx > 0)
				{
					pPlayer = I::ClientEntityList->GetClientEntity(tClosestEnemy.m_iEntIdx)->As<CTFPlayer>();
					F::CritHack.m_bForce = pPlayer && !pPlayer->IsDormant() && pPlayer->m_iHealth() >= pWeapon->GetDamage();
				}
				break;
			default:
				F::CritHack.m_bForce = false;
				break;
			}
		}

		// Stuck detection with reduced frequency
		static Vector vPrevPos{};
		static float flStuckTime = 0.0f;
		static FastTimer tPosTimer{};

		if (tPosTimer.Run(0.3f)) // Reduced frequency from 0.2f to 0.3f
		{
			if (pLocal->GetAbsOrigin().DistToSqr(vPrevPos) < pow(10.0f, 2))
			{
				flStuckTime += 0.3f;
			}
			else
			{
				flStuckTime = 0.0f;
				vPrevPos = pLocal->GetAbsOrigin();
			}

			if (flStuckTime >= 3.0f && F::NavEngine.current_priority != 0)
			{
				if (auto pArea = F::NavEngine.findClosestNavSquare(pLocal->GetAbsOrigin()))
				{
					(*F::NavEngine.getFreeBlacklist())[pArea] = BlacklistReason(BR_BAD_BUILDING_SPOT);
				}

				F::NavEngine.cancelPath();
				flStuckTime = 0.0f;
				vPrevPos = pLocal->GetAbsOrigin();

				Roam(pLocal, pWeapon);
			}
		}
	}
}

void CNavBot::Reset( )
{
	// Make it run asap
	tRefreshSniperspotsTimer -= 60;
	m_iStayNearTargetIdx = -1;
	m_iMySentryIdx = -1;
	m_iMyDispenserIdx = -1;
	m_vSniperSpots.clear( );
	
	// Clear new blacklist management data
	m_mAreaDangerScore.clear();
	m_mAreaBlacklistExpiry.clear();
}
