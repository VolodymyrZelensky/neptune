#include "NBheader.h"
#include "NavEngine/NavEngine.h"
#include <algorithm>

void CNavBot::CleanupExpiredBlacklist()
{
	// run cleanup more frequently for better responsiveness
	if (!m_tBlacklistCleanupTimer.Run(0.5f))
		return;

	auto pBlacklist = F::NavEngine.getFreeBlacklist();
	if (!pBlacklist)
		return;

	std::vector<CNavArea*> vToRemove;
	float flCurrentTime = I::GlobalVars->curtime;

	// First pass: check expired areas based on stored expiry times
	for (auto it = m_mAreaBlacklistExpiry.begin(); it != m_mAreaBlacklistExpiry.end();)
	{
		if (flCurrentTime >= it->second)
		{
			vToRemove.push_back(it->first);
			it = m_mAreaBlacklistExpiry.erase(it);
		}
		else
		{
			++it;
		}
	}

	// Second pass: check if threats that caused blacklisting are still present
	auto pLocal = H::Entities.GetLocal();
	if (pLocal && pLocal->IsAlive())
	{
		for (auto it = pBlacklist->begin(); it != pBlacklist->end();)
		{
			CNavArea* pArea = it->first;
			BlacklistReason_enum reason = it->second.value;
			
			// Only check temporary player-based blacklists
			if (reason == BR_ENEMY_NORMAL || reason == BR_ENEMY_DORMANT)
			{
				bool bThreatStillPresent = false;
				Vector vAreaPos = pArea->m_center;
				vAreaPos.z += PLAYER_JUMP_HEIGHT;
				
				for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
				{
					if (!pEntity->IsPlayer())
						continue;

					auto pPlayer = pEntity->As<CTFPlayer>();
					if (!pPlayer->IsAlive())
						continue;

					auto vPlayerOrigin = F::NavParser.GetDormantOrigin(pPlayer->entindex());
					if (!vPlayerOrigin)
						continue;

					vPlayerOrigin->z += PLAYER_JUMP_HEIGHT;
					float flDist = vAreaPos.DistToSqr(*vPlayerOrigin);
					
					// Check if enemy is still in threat range
					if (flDist < pow(m_tSelectedConfig.m_flMinSlightDanger, 2))
					{
						// Additional check for visibility for non-dormant enemies
						if (!pPlayer->IsDormant())
						{
							if (F::NavParser.IsVectorVisibleNavigation(*vPlayerOrigin, vAreaPos, MASK_SHOT))
							{
								bThreatStillPresent = true;
								break;
							}
						}
						else if (reason == BR_ENEMY_DORMANT)
						{
							// For dormant enemies, keep blacklist shorter time
							bThreatStillPresent = true;
							break;
						}
					}
				}
				
				// If no threat present, mark for removal
				if (!bThreatStillPresent)
				{
					if (std::find(vToRemove.begin(), vToRemove.end(), pArea) == vToRemove.end())
						vToRemove.push_back(pArea);
				}
			}
			++it;
		}
	}

	// Remove all marked areas
	for (auto pArea : vToRemove)
	{
		auto it = pBlacklist->find(pArea);
		if (it != pBlacklist->end())
		{
			BlacklistReason_enum reason = it->second.value;
			if (reason == BR_ENEMY_NORMAL || reason == BR_ENEMY_DORMANT)
			{
				pBlacklist->erase(it);
			}
		}
		
		// Also remove from danger scores and expiry tracking
		m_mAreaDangerScore.erase(pArea);
		m_mAreaBlacklistExpiry.erase(pArea);
	}
}

void CNavBot::FastCleanupInvalidBlacklists(CTFPlayer* pLocal)
{
	static FastTimer cleanupTimer{};
	if (!cleanupTimer.Run(1.0f)) // Reduced frequency for better performance
		return;

	auto pBlacklist = F::NavEngine.getFreeBlacklist();
	if (!pBlacklist || pBlacklist->empty())
		return;

	std::vector<CNavArea*> vToRemove;
	float flCurrentTime = I::GlobalVars->curtime;

	// Quick check for expired blacklists
	for (auto it = m_mAreaBlacklistExpiry.begin(); it != m_mAreaBlacklistExpiry.end();)
	{
		if (flCurrentTime >= it->second)
		{
			vToRemove.push_back(it->first);
			it = m_mAreaBlacklistExpiry.erase(it);
		}
		else
		{
			++it;
		}
	}

	// Remove from actual blacklist
	for (auto pArea : vToRemove)
	{
		pBlacklist->erase(pArea);
		m_mAreaDangerScore.erase(pArea);
	}
}

bool CNavBot::IsAreaInDanger(CNavArea* pArea, CTFPlayer* pLocal, float& flDangerScore)
{
	if (!pArea || !pLocal)
		return false;

	flDangerScore = 0.0f;
	Vector vAreaPos = pArea->m_center;
	vAreaPos.z += PLAYER_JUMP_HEIGHT;

	// Check threats from enemy players
	for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
	{
		if (!pEntity->IsPlayer())
			continue;

		auto pPlayer = pEntity->As<CTFPlayer>();
		if (!pPlayer->IsAlive() || !ShouldTarget(pLocal, pLocal->m_hActiveWeapon().Get()->As<CTFWeaponBase>(), pPlayer->entindex()))
			continue;

		auto vPlayerOrigin = F::NavParser.GetDormantOrigin(pPlayer->entindex());
		if (!vPlayerOrigin)
			continue;

		vPlayerOrigin->z += PLAYER_JUMP_HEIGHT;
		float flDist = vAreaPos.DistToSqr(*vPlayerOrigin);
		
		// Calculate danger based on distance and visibility
		float flThreatRange = m_tSelectedConfig.m_flMinSlightDanger;
		float flHighThreatRange = m_tSelectedConfig.m_flMinFullDanger;
		
		if (flDist < pow(flThreatRange, 2))
		{
			// Check visibility
			if (F::NavParser.IsVectorVisibleNavigation(*vPlayerOrigin, vAreaPos, MASK_SHOT))
			{
				float flThreatLevel = 1.0f;
				
				// Higher threat for closer enemies
				if (flDist < pow(flHighThreatRange, 2))
					flThreatLevel = 3.0f;
				else if (flDist < pow(flHighThreatRange * 1.5f, 2))
					flThreatLevel = 2.0f;
				
				// Modify threat based on player state
				if (pPlayer->IsDormant())
					flThreatLevel *= 0.5f; // Dormant threats are less dangerous
				
				if (pPlayer->IsInvulnerable())
					flThreatLevel *= 1.5f; // Uber players are more dangerous
				
				flDangerScore += flThreatLevel;
			}
		}
	}

	return flDangerScore > 0.5f; // Threshold for considering an area dangerous
}

void CNavBot::UpdateAreaDangerScores(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int iSlot)
{
	if (!pLocal || !pLocal->IsAlive() || !pWeapon)
		return;

	static Timer tUpdateTimer{};
	static int iLastSlot = primary;

	// Update more frequently but be smarter about it
	bool bShouldUpdate = tUpdateTimer.Run(1.0f) || iLastSlot != iSlot;
	if (!bShouldUpdate)
		return;

	iLastSlot = iSlot;

	// Don't be scared in melee mode
	if (iSlot == melee)
		return;

	// Clear old danger scores
	m_mAreaDangerScore.clear();

	// Get local position for optimization
	Vector vLocalPos = pLocal->GetAbsOrigin();
	const float flMaxCheckDistance = 2500.0f; // Only check areas within reasonable distance

	// Check areas around threats more intelligently
	std::vector<Vector> vThreatPositions;
	
	for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ENEMIES))
	{
		if (!pEntity->IsPlayer())
			continue;

		auto pPlayer = pEntity->As<CTFPlayer>();
		if (!pPlayer->IsAlive() || !ShouldTarget(pLocal, pWeapon, pPlayer->entindex()))
			continue;

		auto vPlayerOrigin = F::NavParser.GetDormantOrigin(pPlayer->entindex());
		if (!vPlayerOrigin)
			continue;

		// Skip if too far away
		if (vLocalPos.DistToSqr(*vPlayerOrigin) > pow(flMaxCheckDistance, 2))
			continue;

		vThreatPositions.push_back(*vPlayerOrigin);
	}

	// Now check nav areas around these threats
	for (auto& tArea : F::NavEngine.getNavFile()->m_areas)
	{
		// Skip areas too far from local player
		if (vLocalPos.DistToSqr(tArea.m_center) > pow(flMaxCheckDistance, 2))
			continue;

		float flDangerScore = 0.0f;
		
		// Check if area is near any threats
		bool bNearThreat = false;
		for (const auto& vThreatPos : vThreatPositions)
		{
			if (tArea.m_center.DistToSqr(vThreatPos) < pow(m_tSelectedConfig.m_flMinSlightDanger, 2))
			{
				bNearThreat = true;
				break;
			}
		}

		if (bNearThreat && IsAreaInDanger(&tArea, pLocal, flDangerScore))
		{
			m_mAreaDangerScore[&tArea] = flDangerScore;
			
			// Only blacklist if danger score is high enough
			if (ShouldBlacklistArea(&tArea, flDangerScore))
			{
				BlacklistReason_enum reason = (flDangerScore >= 2.5f) ? BR_ENEMY_NORMAL : BR_ENEMY_DORMANT;
				AddTemporaryBlacklist(&tArea, reason, flDangerScore >= 2.5f ? 5.0f : 8.0f);
			}
		}
	}
}

bool CNavBot::ShouldBlacklistArea(CNavArea* pArea, float flDangerScore, bool bIsTemporary)
{
	if (!pArea)
		return false;

	const float flBlacklistThreshold = 1.5f;
	
	// Don't blacklist if danger score is too low
	if (flDangerScore < flBlacklistThreshold)
		return false;

	// Don't blacklist if we're on an important mission and danger is not too high
	if (F::NavEngine.current_priority == capture || F::NavEngine.current_priority == health)
	{
		if (flDangerScore < 2.5f) // Only blacklist high danger areas when on important missions
			return false;
	}

	// Don't over-blacklist - limit the number of blacklisted areas
	auto pBlacklist = F::NavEngine.getFreeBlacklist();
	if (pBlacklist && pBlacklist->size() > 100) // Reasonable limit
	{
		// Only blacklist very high danger areas if we're at the limit xd
		return flDangerScore >= 3.0f;
	}

	return true;
}

void CNavBot::AddTemporaryBlacklist(CNavArea* pArea, BlacklistReason_enum reason, float flDuration)
{
	if (!pArea)
		return;

	auto pBlacklist = F::NavEngine.getFreeBlacklist();
	if (!pBlacklist)
		return;

	(*pBlacklist)[pArea] = BlacklistReason(reason);
	m_mAreaBlacklistExpiry[pArea] = I::GlobalVars->curtime + flDuration;
}

bool CNavBot::IsFailedBuildingSpot(const Vector& spot) const
{
    for (const auto& v : m_vFailedBuildingSpots)
    {
        if (v.DistToSqr(spot) < pow(BUILD_SPOT_EQUALITY_EPSILON, 2))
            return true;
    }
    return false;
}

void CNavBot::AddFailedBuildingSpot(const Vector& spot)
{
    if (IsFailedBuildingSpot(spot))
        return;

    m_vFailedBuildingSpots.push_back(spot);
    // Prevent uncontrolled growth – keep last 50 spots
    if (m_vFailedBuildingSpots.size() > 50)
    {
        m_vFailedBuildingSpots.erase(m_vFailedBuildingSpots.begin());
    }
}