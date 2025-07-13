#pragma once
#include "NavEngine/NavEngine.h"
#include "../Players/PlayerUtils.h"

class CFollowBot
{
public:
	void Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd);
	void Reset();
	
	int GetTargetIndex(CTFPlayer* pLocal);
	bool IsValidTarget(CTFPlayer* pLocal, int iEntIndex, bool ignoreIDCheck = false);
	float GetFollowDistance();
	std::optional<Vector> GetFollowPosition(CTFPlayer* pLocal, CTFPlayer* pTarget);
	
	bool m_bIsFollowing = false;
	int m_iTargetIndex = -1;
	float m_flLastTargetTime = 0.0f;
	float m_flLastPathUpdateTime = 0.0f;
	Timer m_StuckTimer{};
	Timer m_StuckDetectionTimer{};
	Vector m_vLastPosition = {};
	int m_iStuckCount = 0;
	bool m_bWasStuck = false;
};

ADD_FEATURE(CFollowBot, FollowBot)
