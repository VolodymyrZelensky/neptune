#pragma once
#include "../../SDK/SDK.h"

class bf_read; // i don't remember why i needed it tho lol

class CMisc
{
	
	void AutoJump(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AutoJumpbug(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AutoStrafe(CTFPlayer* pLocal, CUserCmd* pCmd);
	void MovementLock(CTFPlayer* pLocal, CUserCmd* pCmd);
	void BreakJump(CTFPlayer* pLocal, CUserCmd* pCmd);
	void BreakShootSound(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AntiAFK(CTFPlayer* pLocal, CUserCmd* pCmd);
	void InstantRespawnMVM(CTFPlayer* pLocal);
	void NoiseSpam(CTFPlayer* pLocal);
	void VoiceCommandSpam(CTFPlayer* pLocal);
	void MicSpam(CTFPlayer* pLocal);
	void RandomVotekick(CTFPlayer* pLocal);
	void CallVoteSpam(CTFPlayer* pLocal);
	void ChatSpam(CTFPlayer* pLocal);
	void AchievementSpam(CTFPlayer* pLocal);
	void KillSay(int victim);
	std::string ProcessTextReplacements(std::string text);
	void CheatsBypass();
	void WeaponSway();
	void AutoReport();
	void TauntKartControl(CTFPlayer* pLocal, CUserCmd* pCmd);
	void FastMovement(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AutoPeek(CTFPlayer* pLocal, CUserCmd* pCmd, bool bPost = false);
	void EdgeJump(CTFPlayer* pLocal, CUserCmd* pCmd, bool bPost = false);
	bool m_bPeekPlaced = false;
	Vec3 m_vPeekReturnPos = {};
	std::vector<std::string> m_vChatSpamLines;
	std::vector<std::string> m_vKillSayLines;
	std::vector<std::string> m_vAutoReplyLines;
	Timer m_tChatSpamTimer;
	Timer m_tVoiceCommandTimer;
	Timer m_tCallVoteSpamTimer;
	Timer m_tMicSpamTimer;
	Timer m_tAchievementSpamTimer;
	int m_iTokenBucket = 5;
	float m_flLastTokenTime = 0.0f;
	int m_iCurrentChatSpamIndex = 0;
	int m_iLastKilledPlayer = 0;
	std::string m_sLastKilledPlayerName;

public:
	void RunPre(CTFPlayer* pLocal, CUserCmd* pCmd);
	void RunPost(CTFPlayer* pLocal, CUserCmd* pCmd, bool pSendPacket);
	void Event(IGameEvent* pEvent, uint32_t uNameHash);
	int AntiBackstab(CTFPlayer* pLocal, CUserCmd* pCmd, bool bSendPacket);
	void PingReducer();
	void UnlockAchievements();
	void LockAchievements();
	void VotekickResponse(bf_read& msgData);
	bool SteamRPC();
	void AutoReply(int speaker, const char* text);
	std::unordered_map<std::string, std::vector<std::string>> m_mAutoReplyTriggers;
	std::unordered_map<std::string, std::vector<std::string>> m_mVotekickResponses;
	void LoadVotekickConfig();
	void LoadAutoReplyConfig();
	int m_iWishCmdrate = -1;
	bool m_bAntiAFK = false;
	Timer m_tRandomVotekickTimer;
	std::string m_sVoteInitiatorName; 
	std::string m_sVoteTargetName;
};

private:
	int m_iVoteTarget = -1;
	std::string m_sVoteType;
};

ADD_FEATURE(CMisc, Misc)
