#include "Fbot.h"
#include <cmath>
#include "../Ticks/Ticks.h"
#include "../Misc/Misc.h"
#include <direct.h>

int CFollowBot::GetTargetIndex(CTFPlayer* pLocal)
{
    bool bCurrentTargetValid = false;
    if (m_iTargetIndex != -1)
    {
        auto pEntity = I::ClientEntityList->GetClientEntity(m_iTargetIndex);
        if (pEntity && pEntity->IsPlayer())
        {
            auto pPlayer = pEntity->As<CTFPlayer>();
            if (pPlayer->IsAlive() && IsValidTarget(pLocal, m_iTargetIndex, true))
            {
                bCurrentTargetValid = true;
                
                if (Vars::NavEng::FollowBot::StickToTarget.Value)
                {
                    m_flLastTargetTime = I::GlobalVars->curtime;
                    return m_iTargetIndex;
                }
            }
        }
    }
    
    if (!bCurrentTargetValid || I::GlobalVars->curtime - m_flLastTargetTime > 3.0f) 
    {
        m_flLastTargetTime = I::GlobalVars->curtime;
        int iBestTarget = -1;
        float flBestScore = -FLT_MAX; 
        
        std::string followID = Vars::NavEng::FollowBot::FollowID.Value;
        if (!followID.empty())
        {
            for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
            {
                if (!IsValidTarget(pLocal, i, true))
                    continue;

                auto pEntity = I::ClientEntityList->GetClientEntity(i);
                if (!pEntity || !pEntity->IsPlayer())
                    continue;

                auto pPlayer = pEntity->As<CTFPlayer>();
                if (!pPlayer->IsAlive() || pPlayer == pLocal)
                    continue;
                    
                PlayerInfo_t info;
                if (I::EngineClient->GetPlayerInfo(i, &info))
                {
                    std::string playerID = std::to_string(info.friendsID);
                    if (playerID == followID)
                    {
                        iBestTarget = i;
                        break;
                    }
                }
            }
            
            if (iBestTarget != -1)
            {
                if (iBestTarget != m_iTargetIndex)
                {
                    m_bIsFollowing = true;
                    m_iTargetIndex = iBestTarget;
                }
                return m_iTargetIndex;
            }
            
            return -1;
        }

        for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
        {
            if (!IsValidTarget(pLocal, i, false))
                continue;

            auto pEntity = I::ClientEntityList->GetClientEntity(i);
            if (!pEntity || !pEntity->IsPlayer())
                continue;

            auto pPlayer = pEntity->As<CTFPlayer>();
            if (!pPlayer->IsAlive() || pPlayer == pLocal)
                continue;

            float flDistance = pLocal->GetAbsOrigin().DistTo(pPlayer->GetAbsOrigin());
            float flScore = -flDistance;
            
            if (Vars::NavEng::FollowBot::SmartSelection.Value)
            {
                if (H::Entities.IsFriend(i))
                    flScore += 5000.0f;
                
                if (H::Entities.InParty(i))
                    flScore += 10000.0f;
                
                flScore += pPlayer->m_iHealth() * 5.0f;
                
                if (pPlayer->m_iClass() == pLocal->m_iClass())
                    flScore += 2000.0f;
                
                if (flDistance > 1500.0f)
                    flScore -= 10000.0f;
                
                if (pPlayer->GetAbsVelocity().Length() > 50.0f)
                    flScore += 1000.0f;
            }
            
            if (flScore > flBestScore)
            {
                flBestScore = flScore;
                iBestTarget = i;
            }
        }

        if (iBestTarget != m_iTargetIndex)
        {
            if (iBestTarget != -1)
            {
                m_bIsFollowing = true;
            }
            else
            {
                m_bIsFollowing = false;
            }
            m_iTargetIndex = iBestTarget;
        }
    }
    
    return m_iTargetIndex;
}

bool CFollowBot::IsValidTarget(CTFPlayer* pLocal, int iEntIndex, bool ignoreIDCheck)
{
    if (iEntIndex == pLocal->entindex())
        return false;

    auto pEntity = I::ClientEntityList->GetClientEntity(iEntIndex);
    if (!pEntity || !pEntity->IsPlayer())
        return false;

    auto pPlayer = pEntity->As<CTFPlayer>();
    if (!pPlayer->IsAlive())
        return false;

    if (pPlayer->m_iTeamNum() != pLocal->m_iTeamNum() && 
        !(Vars::NavEng::FollowBot::FollowEnemies.Value))
        return false;
    
    // If we have a specific ID to follow and we're not ignoring that check
    if (!ignoreIDCheck && !Vars::NavEng::FollowBot::FollowID.Value.empty())
    {
        PlayerInfo_t info;
        if (I::EngineClient->GetPlayerInfo(iEntIndex, &info))
        {
            std::string playerID = std::to_string(info.friendsID);
            if (playerID != Vars::NavEng::FollowBot::FollowID.Value)
                return false;  // Not the player we want to follow
        }
        else
        {
            return false;  // Couldn't get player info
        }
    }

    // Only follow friends if that option is enabled
    if (Vars::NavEng::FollowBot::OnlyFriends.Value)
    {
        return H::Entities.IsFriend(iEntIndex) || H::Entities.InParty(iEntIndex);
    }
    
    // Only follow party members if that option is enabled
    if (Vars::NavEng::FollowBot::OnlyParty.Value)
    {
        return H::Entities.InParty(iEntIndex);
    }

    return true;
}

float CFollowBot::GetFollowDistance()
{

    return static_cast<float>(Vars::NavEng::FollowBot::Distance.Value);
}

std::optional<Vector> CFollowBot::GetFollowPosition(CTFPlayer* pLocal, CTFPlayer* pTarget)
{
    if (!pTarget)
        return std::nullopt;


    Vector vTargetPos = pTarget->GetAbsOrigin();
    Vector vTargetAng = pTarget->GetAbsAngles();
    

    float flDistance = GetFollowDistance();
    
    if (Vars::NavEng::FollowBot::PositionMode.Value == 0) // Behind
    {

        Vector vForward;
        Math::AngleVectors(vTargetAng, &vForward);
        vForward *= -flDistance;
        return vTargetPos + vForward;
    }
    else if (Vars::NavEng::FollowBot::PositionMode.Value == 1) // Side
    {

        Vector vRight;
        Math::AngleVectors(vTargetAng, nullptr, &vRight, nullptr);
        vRight *= flDistance * (std::fmod(I::GlobalVars->curtime, 10.0f) < 5.0f ? 1.0f : -1.0f);
        return vTargetPos + vRight;
    }
    else // Adaptive
    {

        Vector vForward, vRight;
        Math::AngleVectors(vTargetAng, &vForward, &vRight, nullptr);
        

        Vector vBehindPos = vTargetPos - (vForward * flDistance);
        if (!F::NavEngine.map)
            return vTargetPos;
            
        CNavArea* pBehindArea = F::NavEngine.map->findClosestNavSquare(vBehindPos);
        if (pBehindArea && F::NavParser.IsVectorVisibleNavigation(pLocal->GetAbsOrigin(), vBehindPos))
            return vBehindPos;
        

        Vector vRightPos = vTargetPos + (vRight * flDistance);
        CNavArea* pRightArea = F::NavEngine.map->findClosestNavSquare(vRightPos);
        if (pRightArea && F::NavParser.IsVectorVisibleNavigation(pLocal->GetAbsOrigin(), vRightPos))
            return vRightPos;
        

        Vector vLeftPos = vTargetPos - (vRight * flDistance);
        CNavArea* pLeftArea = F::NavEngine.map->findClosestNavSquare(vLeftPos);
        if (pLeftArea && F::NavParser.IsVectorVisibleNavigation(pLocal->GetAbsOrigin(), vLeftPos))
            return vLeftPos;
            
        return vTargetPos;
    }
}

void CFollowBot::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
    if (!pLocal || !pCmd)
        return;
        
    if (!Vars::NavEng::FollowBot::Enabled.Value)
    {
        if (m_bIsFollowing)
        {
            Reset();
        }
        return;
    }

    if (!pLocal->IsAlive())
        return;
    
    if (!F::NavEngine.map)
    {
        if (m_bIsFollowing)
        {
            Reset();
        }
        return;
    }
    
    static std::string last_map = "";
    std::string current_map = I::EngineClient->GetLevelName() ? I::EngineClient->GetLevelName() : "";
    if (current_map != last_map)
    {
        last_map = current_map;
        Reset();
        return;
    }
    
    if (!F::NavEngine.isReady(true))
        return;

    int iTargetIndex = GetTargetIndex(pLocal);
    if (iTargetIndex == -1)
    {
        if (m_bIsFollowing)
        {
            F::NavEngine.cancelPath();
            m_bIsFollowing = false;
        }
        return;
    }

    auto pTarget = I::ClientEntityList->GetClientEntity(iTargetIndex);
    if (!pTarget || !pTarget->IsPlayer())
        return;

    auto pPlayerTarget = pTarget->As<CTFPlayer>();
    if (!pPlayerTarget->IsAlive())
        return;

    auto vFollowPos = GetFollowPosition(pLocal, pPlayerTarget);
    if (!vFollowPos)
        return;

    if (vFollowPos->DistTo(pLocal->GetAbsOrigin()) < 50.0f)
        return;
    
    Vector vCurrentPos = pLocal->GetAbsOrigin();
    
    if (m_vLastPosition.IsZero())
    {
        m_vLastPosition = vCurrentPos;
        m_StuckTimer.Update();
        m_StuckDetectionTimer.Update();
    }
    
    float flDistanceMoved = vCurrentPos.DistTo(m_vLastPosition);
    
    const float MIN_MOVEMENT = 20.0f;
    
    if (flDistanceMoved < MIN_MOVEMENT && F::NavEngine.isPathing())
    {
        if (m_StuckTimer.Check(Vars::NavEng::NavEngine::StuckTime.Value / 2))
        {
            m_iStuckCount++;
            
            if (m_iStuckCount > TIME_TO_TICKS(Vars::NavEng::NavEngine::StuckDetectTime.Value))
            {
                m_bWasStuck = true;
                
                SDK::Output("CFollowBot", "Stuck detected, replanning path", { 255, 131, 131 }, 
                           Vars::Debug::Logging.Value, Vars::Debug::Logging.Value);
                
                F::NavEngine.cancelPath();
                
                m_iStuckCount = 0;
                m_StuckTimer.Update();
                m_StuckDetectionTimer.Update();
            }
        }
    }
    else
    {
        m_iStuckCount = 0;
        m_StuckTimer.Update();
        m_bWasStuck = false;
    }
    
    m_vLastPosition = vCurrentPos;
    
    bool bShouldUpdatePath = (I::GlobalVars->curtime - m_flLastPathUpdateTime > 0.5f) || m_bWasStuck;
    if (bShouldUpdatePath && F::NavEngine.map)
    {
        F::NavEngine.navTo(*vFollowPos, 5);
        m_flLastPathUpdateTime = I::GlobalVars->curtime;
        m_bWasStuck = false;
    }
}

void CFollowBot::Reset()
{
    F::NavEngine.cancelPath();
    m_bIsFollowing = false;
    m_iTargetIndex = -1;
    
    m_StuckTimer.Update();
    m_StuckDetectionTimer.Update();
    m_vLastPosition = Vector();
    m_iStuckCount = 0;
    m_bWasStuck = false;
    m_flLastTargetTime = 0.0f;
    m_flLastPathUpdateTime = 0.0f;
}
