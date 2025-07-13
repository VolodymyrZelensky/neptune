#include "NBheader.h"
#include "../Misc/NamedPipe/NamedPipe.h"
#include "../../Utils/Timer/Timer.h"
#include "../../Utils/Math/Math.h"
#include <optional>
#include <vector>
#include <algorithm>

void CNavBot::UpdateLocalBotPositions(CTFPlayer* pLocal)
{
	if (!m_tUpdateFormationTimer.Run(0.5f))
		return;

	m_vLocalBotPositions.clear();
	if (!I::EngineClient->IsInGame() || !pLocal)
		return;

	// Get our own info first
	PlayerInfo_t localInfo{};
	if (!I::EngineClient->GetPlayerInfo(I::EngineClient->GetLocalPlayer(), &localInfo))
		return;

	uint32_t localFriendsID = localInfo.friendsID;
	int iLocalTeam = pLocal->m_iTeamNum();

	// Then check each player
	for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
	{
		if (i == I::EngineClient->GetLocalPlayer())
			continue;

		PlayerInfo_t pi{};
		if (!I::EngineClient->GetPlayerInfo(i, &pi))
			continue;

		// Is this a local bot????
		if (!F::NamedPipe::IsLocalBot(pi.friendsID))
			continue;

		// Get the player entity
		auto pEntity = I::ClientEntityList->GetClientEntity(i);
		if (!pEntity || !pEntity->IsPlayer())
			continue;

		auto pPlayer = pEntity->As<CTFPlayer>();
		if (!pPlayer->IsAlive() || pPlayer->IsDormant())
			continue;

		if (pPlayer->m_iTeamNum() != iLocalTeam)
			continue;

		// Add to our list
		m_vLocalBotPositions.push_back({ pi.friendsID, pPlayer->GetAbsOrigin() });
	}

	// Sort by friendsID to ensure consistent ordering across all bots
	std::sort(m_vLocalBotPositions.begin(), m_vLocalBotPositions.end(), 
		[](const std::pair<uint32_t, Vector>& a, const std::pair<uint32_t, Vector>& b) {
			return a.first < b.first;
		});

	// Determine our position in the formatin
	m_iPositionInFormation = -1;
	
	// Add ourselves to the list for calculation purposes
	std::vector<uint32_t> allBotsInOrder;
	allBotsInOrder.push_back(localFriendsID);
	
	for (const auto& bot : m_vLocalBotPositions)
		allBotsInOrder.push_back(bot.first);
	
	// Sort all bots (including us)
	std::sort(allBotsInOrder.begin(), allBotsInOrder.end());
	
	// Find our pofition
	for (size_t i = 0; i < allBotsInOrder.size(); i++)
	{
		if (allBotsInOrder[i] == localFriendsID)
		{
			m_iPositionInFormation = static_cast<int>(i);
			break;
		}
	}
}

std::optional<Vector> CNavBot::GetFormationOffset(CTFPlayer* pLocal, int positionIndex)
{
	if (positionIndex <= 0)
		return std::nullopt; // Leader has no offset
	
	// Calculate our desired position relative to the leader
	Vector vOffset(0, 0, 0);
	
	// Calculate the movement direction of the leader
	Vector vLeaderOrigin(0, 0, 0);
	Vector vLeaderVelocity(0, 0, 0);
	
	if (!m_vLocalBotPositions.empty())
	{
		auto pLeader = I::ClientEntityList->GetClientEntity(1); // Leader is always at index 1
		for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
		{
			PlayerInfo_t pi{};
			if (!I::EngineClient->GetPlayerInfo(i, &pi))
				continue;
				
			if (pi.friendsID == m_vLocalBotPositions[0].first)
			{
				pLeader = I::ClientEntityList->GetClientEntity(i);
				break;
			}
		}
		
		if (pLeader && pLeader->IsPlayer())
		{
			auto pLeaderPlayer = pLeader->As<CTFPlayer>();
			vLeaderOrigin = pLeaderPlayer->GetAbsOrigin();
			vLeaderVelocity = pLeaderPlayer->m_vecVelocity();
		}
		else
		{
			vLeaderOrigin = m_vLocalBotPositions[0].second;
			vLeaderVelocity = Vector(0, 0, 0);
		}
	}
	else
	{
		// No leader found, use our own direction
		vLeaderOrigin = pLocal->GetAbsOrigin();
		vLeaderVelocity = pLocal->m_vecVelocity();
	}
	
	// Normalize leader velocity for direction
	Vector vDirection = vLeaderVelocity;
	if (vDirection.Length() < 10.0f) // If leader is barely moving, use view direction
	{
		QAngle viewAngles;
		I::EngineClient->GetViewAngles(viewAngles);
		Math::AngleVectors(viewAngles, &vDirection);
	}
	
	vDirection.z = 0; // Ignore vertical component
	float length = vDirection.Length();
	if (length > 0.001f) {
		vDirection.x /= length;
		vDirection.y /= length;
		vDirection.z /= length;
	}
	
	// Calculate cross product for perpendicular direction (for side-by-side formations)
	Vector vRight = vDirection.Cross(Vector(0, 0, 1));
	// Normalize right vector
	length = vRight.Length();
	if (length > 0.001f) {
		vRight.x /= length;
		vRight.y /= length;
		vRight.z /= length;
	}
	
	// Different formation styles:
	// 1. Line formation (bots following one after another)
	vOffset = (vDirection * -m_flFormationDistance * positionIndex);
	
	return vOffset;
}

bool CNavBot::MoveInFormation(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	if (!(Vars::NavEng::NavBot::Preferences.Value & Vars::NavEng::NavBot::PreferencesEnum::GroupWithOthers))
		return false;
		
	// UpdateLocalBotPositions is called from Run(), so we don't need to call it here
	// If we haven't found a position in formation, we can't move in formation
	if (m_iPositionInFormation < 0 || m_vLocalBotPositions.empty())
		return false;
	
	// If we're the leader, don't move in formation
	if (m_iPositionInFormation == 0)
		return false;
	
	static Timer tFormationNavTimer{};
	if (!tFormationNavTimer.Run(1.0f))
	{
		return F::NavEngine.current_priority == patrol;
	}
	
	// Get our offset in the formation
	auto vOffsetOpt = GetFormationOffset(pLocal, m_iPositionInFormation);
	if (!vOffsetOpt)
		return false;
	
	// Find the leader
	uint32_t leaderID = 0;
	Vector vLeaderPos;
	CTFPlayer* pLeaderPlayer = nullptr;
	
	if (!m_vLocalBotPositions.empty())
	{
		// Find the actual leader in-game (by friendsID)
		for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
		{
			PlayerInfo_t pi{};
			if (!I::EngineClient->GetPlayerInfo(i, &pi))
				continue;
			if (pi.friendsID == m_vLocalBotPositions[0].first)
			{
				auto pLeader = I::ClientEntityList->GetClientEntity(i);
				if (pLeader && pLeader->IsPlayer())
				{
					pLeaderPlayer = pLeader->As<CTFPlayer>();
					leaderID = pi.friendsID;
					vLeaderPos = pLeaderPlayer->GetAbsOrigin();
				}
				break;
			}
		}
	}
	if (leaderID == 0 || !pLeaderPlayer)
		return false;
	
	if (pLeaderPlayer->m_iTeamNum() != pLocal->m_iTeamNum())
		return false;
	
	Vector vTargetPos = vLeaderPos + *vOffsetOpt;
	
	// If we're already close enough to our position, don't bother moving
	float distToTarget = pLocal->GetAbsOrigin().DistToSqr(vTargetPos);
	if (distToTarget <= pow(30.0f, 2))
	{
		F::NavEngine.cancelPath();
		return true;
	}
	
	// Only try to move to the position if we're not already pathing to something important
	if (F::NavEngine.current_priority > patrol)
		return false;
	
	// Try to navigate to our position in formation – smoother: only path when really far away
	if (distToTarget > pow(120.0f, 2))
	{
		if (F::NavEngine.navTo(vTargetPos, patrol, true, !F::NavEngine.isPathing()))
			return true;
	}
	return false;
}




