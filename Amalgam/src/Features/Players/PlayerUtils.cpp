#include "PlayerUtils.h"

#include "../../SDK/Definitions/Types.h"
#include "../Output/Output.h"

uint32_t CPlayerlistUtils::GetFriendsID(int iIndex)
{
	PlayerInfo_t pi{};
	if (I::EngineClient->GetPlayerInfo(iIndex, &pi) && !pi.fakeplayer)
		return pi.friendsID;
	return 0;
}

PriorityLabel_t* CPlayerlistUtils::GetTag(int iID)
{
	if (iID > -1 && iID < m_vTags.size())
		return &m_vTags[iID];
	return nullptr;
}

int CPlayerlistUtils::GetTag(std::string sTag)
{
	auto uHash = FNV1A::Hash32(sTag.c_str());

	int iID = -1;
	for (auto& tTag : m_vTags)
	{
		iID++;
		if (uHash == FNV1A::Hash32(tTag.m_sName.c_str()))
			return iID;
	}

	return -1;
}


// warnin.... coded by ai :broken_heart:
// also, i know thats dumb bcuz everyone can add these chars to their name, but legits are too dumb lol
bool CPlayerlistUtils::ContainsSpecialChars(const std::string& name)
{
	if (name.empty())
		return false;

	// UTF-8 sequences for Thai characters are 3 bytes each
	// Check for these sequences in the input string
	for (size_t i = 0; i < name.size(); )
	{
		// Check if this position could start a Thai character (first byte of sequence)
		if ((unsigned char)name[i] == 0xE0 && i + 2 < name.size())
		{
			// Verify if we have a Thai character
			for (size_t j = 0; j < m_vSpecialChars.size(); j += 3) 
			{
				if ((unsigned char)name[i] == m_vSpecialChars[j] &&
					(unsigned char)name[i + 1] == m_vSpecialChars[j + 1] &&
					(unsigned char)name[i + 2] == m_vSpecialChars[j + 2])
					return true;
			}
		}
		
		// Move to next character (UTF-8 aware)
		if ((name[i] & 0x80) == 0)
			i += 1;  // ASCII character
		else if ((name[i] & 0xE0) == 0xC0)
			i += 2;  // 2-byte UTF-8 sequence
		else if ((name[i] & 0xF0) == 0xE0)
			i += 3;  // 3-byte UTF-8 sequence (Thai characters are here)
		else if ((name[i] & 0xF8) == 0xF0)
			i += 4;  // 4-byte UTF-8 sequence
		else
			i += 1;  // Invalid UTF-8, skip
	}
	
	return false;
}

void CPlayerlistUtils::ProcessSpecialCharsInName(uint32_t friendsID, const std::string& name)
{
	if (!friendsID || name.empty())
		return;

	if (Vars::CheaterDetection::AutoIgnoreThai.Value && ContainsSpecialChars(name) && !HasTags(friendsID))
	{
		AddTag(friendsID, TagToIndex(IGNORED_TAG), true, name);
		SDK::Output("Amalgam", std::format("Auto-ignored player with special characters: {}", name).c_str(), { 255, 100, 100, 255 });
		m_bSave = true;
	}
}


void CPlayerlistUtils::AddTag(uint32_t uFriendsID, int iID, bool bSave, std::string sName, std::unordered_map<uint32_t, std::vector<int>>& mPlayerTags)
{
	if (!uFriendsID)
		return;

	if (!HasTag(uFriendsID, iID))
	{
		mPlayerTags[uFriendsID].push_back(iID);
		m_bSave = bSave;
		if (sName.length())
		{
			if (PriorityLabel_t* pTag = GetTag(iID))
				F::Output.TagsChanged(sName, "Added", pTag->m_tColor.ToHexA(), pTag->m_sName);
		}
	}
}
void CPlayerlistUtils::AddTag(int iIndex, int iID, bool bSave, std::string sName, std::unordered_map<uint32_t, std::vector<int>>& mPlayerTags)
{
	if (const uint32_t uFriendsID = GetFriendsID(iIndex))
		AddTag(uFriendsID, iID, bSave, sName, mPlayerTags);
}
void CPlayerlistUtils::AddTag(uint32_t uFriendsID, int iID, bool bSave, std::string sName)
{
	AddTag(uFriendsID, iID, bSave, sName, m_mPlayerTags);
}
void CPlayerlistUtils::AddTag(int iIndex, int iID, bool bSave, std::string sName)
{
	AddTag(iIndex, iID, bSave, sName, m_mPlayerTags);
}

void CPlayerlistUtils::RemoveTag(uint32_t uFriendsID, int iID, bool bSave, std::string sName, std::unordered_map<uint32_t, std::vector<int>>& mPlayerTags)
{
	if (!uFriendsID)
		return;

	auto& _vTags = mPlayerTags[uFriendsID];
	for (auto it = _vTags.begin(); it != _vTags.end(); it++)
	{
		if (iID == *it)
		{
			_vTags.erase(it);
			m_bSave = bSave;
			if (sName.length())
			{
				if (auto pTag = GetTag(iID))
					F::Output.TagsChanged(sName, "Removed", pTag->m_tColor.ToHexA(), pTag->m_sName);
			}
			break;
		}
	}
}
void CPlayerlistUtils::RemoveTag(int iIndex, int iID, bool bSave, std::string sName, std::unordered_map<uint32_t, std::vector<int>>& mPlayerTags)
{
	if (const uint32_t uFriendsID = GetFriendsID(iIndex))
		RemoveTag(uFriendsID, iID, bSave, sName, mPlayerTags);
}
void CPlayerlistUtils::RemoveTag(uint32_t uFriendsID, int iID, bool bSave, std::string sName)
{
	RemoveTag(uFriendsID, iID, bSave, sName, m_mPlayerTags);
}
void CPlayerlistUtils::RemoveTag(int iIndex, int iID, bool bSave, std::string sName)
{
	RemoveTag(iIndex, iID, bSave, sName, m_mPlayerTags);
}

bool CPlayerlistUtils::HasTags(uint32_t uFriendsID, std::unordered_map<uint32_t, std::vector<int>>& mPlayerTags)
{
	if (!uFriendsID)
		return false;

	return mPlayerTags[uFriendsID].size();
}
bool CPlayerlistUtils::HasTags(int iIndex, std::unordered_map<uint32_t, std::vector<int>>& mPlayerTags)
{
	if (const uint32_t uFriendsID = GetFriendsID(iIndex))
		return HasTags(uFriendsID, mPlayerTags);
	return false;
}

bool CPlayerlistUtils::HasTags(uint32_t uFriendsID)
{
	return HasTags(uFriendsID, m_mPlayerTags);
}

bool CPlayerlistUtils::HasTags(int iIndex)
{
	if (const uint32_t uFriendsID = GetFriendsID(iIndex))
		return HasTags(uFriendsID);
	return false;
}

bool CPlayerlistUtils::HasTag(uint32_t uFriendsID, int iID, std::unordered_map<uint32_t, std::vector<int>>& mPlayerTags)
{
	if (!uFriendsID)
		return false;

	auto it = std::ranges::find_if(mPlayerTags[uFriendsID], [iID](const auto& _iID) { return iID == _iID; });
	return it != mPlayerTags[uFriendsID].end();
}
bool CPlayerlistUtils::HasTag(int iIndex, int iID, std::unordered_map<uint32_t, std::vector<int>>& mPlayerTags)
{
	if (const uint32_t uFriendsID = GetFriendsID(iIndex))
		return HasTag(uFriendsID, iID, mPlayerTags);
	return false;
}
bool CPlayerlistUtils::HasTag(uint32_t uFriendsID, int iID)
{
	return HasTag(uFriendsID, iID, m_mPlayerTags);
}
bool CPlayerlistUtils::HasTag(int iIndex, int iID)
{
	return HasTag(iIndex, iID, m_mPlayerTags);
}



int CPlayerlistUtils::GetPriority(uint32_t uFriendsID, bool bCache)
{
	if (bCache)
		return H::Entities.GetPriority(uFriendsID);

	const int iDefault = m_vTags[TagToIndex(DEFAULT_TAG)].m_iPriority;
	if (!uFriendsID)
		return iDefault;

	if (HasTag(uFriendsID, TagToIndex(IGNORED_TAG)))
		return m_vTags[TagToIndex(IGNORED_TAG)].m_iPriority;

	std::vector<int> vPriorities;
	for (auto& iID : m_mPlayerTags[uFriendsID])
	{
		auto pTag = GetTag(iID);
		if (pTag && !pTag->m_bLabel)
			vPriorities.push_back(pTag->m_iPriority);
	}
	if (H::Entities.IsFriend(uFriendsID))
	{
		auto& tTag = m_vTags[TagToIndex(FRIEND_TAG)];
		if (!tTag.m_bLabel)
			vPriorities.push_back(tTag.m_iPriority);
	}
	if (H::Entities.InParty(uFriendsID))
	{
		auto& tTag = m_vTags[TagToIndex(PARTY_TAG)];
		if (!tTag.m_bLabel)
			vPriorities.push_back(tTag.m_iPriority);
	}
	if (H::Entities.IsF2P(uFriendsID))
	{
		auto& tTag = m_vTags[TagToIndex(F2P_TAG)];
		if (!tTag.m_bLabel)
			vPriorities.push_back(tTag.m_iPriority);
	}
	if (vPriorities.empty())
		return iDefault;

	std::sort(vPriorities.begin(), vPriorities.end(), std::greater<int>());
	return vPriorities.front();
}
int CPlayerlistUtils::GetPriority(int iIndex, bool bCache)
{
	if (bCache)
		return H::Entities.GetPriority(iIndex);

	if (const uint32_t uFriendsID = GetFriendsID(iIndex))
		return GetPriority(uFriendsID);
	return m_vTags[TagToIndex(DEFAULT_TAG)].m_iPriority;
}

PriorityLabel_t* CPlayerlistUtils::GetSignificantTag(uint32_t uFriendsID, int iMode)
{
	if (!uFriendsID)
		return nullptr;

	std::vector<PriorityLabel_t*> vTags;
	if (!iMode || iMode == 1)
	{
		if (HasTag(uFriendsID, TagToIndex(IGNORED_TAG)))
			return &m_vTags[TagToIndex(IGNORED_TAG)];

		for (auto& iID : m_mPlayerTags[uFriendsID])
		{
			PriorityLabel_t* _pTag = GetTag(iID);
			if (_pTag && !_pTag->m_bLabel)
				vTags.push_back(_pTag);
		}
		if (H::Entities.IsFriend(uFriendsID))
		{
			auto _pTag = &m_vTags[TagToIndex(FRIEND_TAG)];
			if (!_pTag->m_bLabel)
				vTags.push_back(_pTag);
		}
		if (H::Entities.InParty(uFriendsID))
		{
			auto _pTag = &m_vTags[TagToIndex(PARTY_TAG)];
			if (!_pTag->m_bLabel)
				vTags.push_back(_pTag);
		}
		if (H::Entities.IsF2P(uFriendsID))
		{
			auto _pTag = &m_vTags[TagToIndex(F2P_TAG)];
			if (!_pTag->m_bLabel)
				vTags.push_back(_pTag);
		}
	}
	if ((!iMode || iMode == 2) && !vTags.size())
	{
		for (auto& iID : m_mPlayerTags[uFriendsID])
		{
			PriorityLabel_t* _pTag = GetTag(iID);
			if (_pTag && _pTag->m_bLabel)
				vTags.push_back(_pTag);
		}
		if (H::Entities.IsFriend(uFriendsID))
		{
			auto _pTag = &m_vTags[TagToIndex(FRIEND_TAG)];
			if (_pTag->m_bLabel)
				vTags.push_back(_pTag);
		}
		if (H::Entities.InParty(uFriendsID))
		{
			auto _pTag = &m_vTags[TagToIndex(PARTY_TAG)];
			if (_pTag->m_bLabel)
				vTags.push_back(_pTag);
		}
		if (H::Entities.IsF2P(uFriendsID))
		{
			auto _pTag = &m_vTags[TagToIndex(F2P_TAG)];
			if (_pTag->m_bLabel)
				vTags.push_back(_pTag);
		}
	}
	if (vTags.empty())
		return nullptr;

	std::sort(vTags.begin(), vTags.end(), [&](const PriorityLabel_t* a, const PriorityLabel_t* b) -> bool
		{
			// sort by priority if unequal
			if (a->m_iPriority != b->m_iPriority)
				return a->m_iPriority > b->m_iPriority;

			return a->m_sName < b->m_sName;
		});
	return vTags.front();
}
PriorityLabel_t* CPlayerlistUtils::GetSignificantTag(int iIndex, int iMode)
{
	if (const uint32_t uFriendsID = GetFriendsID(iIndex))
		return GetSignificantTag(uFriendsID, iMode);
	return nullptr;
}

bool CPlayerlistUtils::IsIgnored(uint32_t uFriendsID)
{
	if (!uFriendsID)
		return false;

	if (HasTag(uFriendsID, TagToIndex(FRIEND_IGNORE_TAG)))
		return true;

	if (HasTag(uFriendsID, TagToIndex(BOT_IGNORE_TAG)))
	{
		auto& botData = m_mBotIgnoreData[uFriendsID];
		if (!botData.m_bIsIgnored)
			return false;
			
		if (botData.m_iKillCount >= 2)
		{
			// nigga u killed me twice, now youll feel my rough.
			RemoveTag(uFriendsID, TagToIndex(BOT_IGNORE_TAG), true);
			botData.m_iKillCount = 0;
			botData.m_bIsIgnored = false;
			m_bSave = true;
			return false;
		}
		return true;
	}

	const int iPriority = GetPriority(uFriendsID);
	const int iIgnored = m_vTags[TagToIndex(IGNORED_TAG)].m_iPriority;
	return iPriority <= iIgnored;
}
bool CPlayerlistUtils::IsIgnored(int iIndex)
{
	if (const uint32_t uFriendsID = GetFriendsID(iIndex))
		return IsIgnored(uFriendsID);
	return false;
}

bool CPlayerlistUtils::IsPrioritized(uint32_t uFriendsID)
{
	if (!uFriendsID)
		return false;

	const int iPriority = GetPriority(uFriendsID);
	const int iDefault = m_vTags[TagToIndex(DEFAULT_TAG)].m_iPriority;
	return iPriority > iDefault;
}
bool CPlayerlistUtils::IsPrioritized(int iIndex)
{
	if (const uint32_t uFriendsID = GetFriendsID(iIndex))
		return IsPrioritized(uFriendsID);
	return false;
}

const char* CPlayerlistUtils::GetPlayerName(int iIndex, const char* sDefault, int* pType)
{
	if (Vars::Visuals::UI::StreamerMode.Value)
	{
		if (iIndex == I::EngineClient->GetLocalPlayer())
		{
			if (Vars::Visuals::UI::StreamerMode.Value >= Vars::Visuals::UI::StreamerModeEnum::Local)
			{
				if (pType) *pType = 1;
				return "Local";
			}
		}
		else if (H::Entities.IsFriend(iIndex))
		{
			if (Vars::Visuals::UI::StreamerMode.Value >= Vars::Visuals::UI::StreamerModeEnum::Friends)
			{
				if (pType) *pType = 1;
				return "Friend";
			}
		}
		else if (H::Entities.InParty(iIndex))
		{
			if (Vars::Visuals::UI::StreamerMode.Value >= Vars::Visuals::UI::StreamerModeEnum::Party)
			{
				if (pType) *pType = 1;
				return "Party";
			}
		}
		else if (Vars::Visuals::UI::StreamerMode.Value >= Vars::Visuals::UI::StreamerModeEnum::All)
		{
			if (auto pTag = GetSignificantTag(iIndex, 0))
			{
				if (pType) *pType = 1;
				return pTag->m_sName.c_str();
			}
			else
			{
				if (pType) *pType = 1;
				auto pResource = H::Entities.GetPR();
				return !pResource || pResource->m_iTeam(I::EngineClient->GetLocalPlayer()) != pResource->m_iTeam(iIndex) ? "Enemy" : "Teammate";
			}
		}
	}
	if (const uint32_t uFriendsID = GetFriendsID(iIndex))
	{
		if (m_mPlayerAliases.contains(uFriendsID))
		{
			if (pType) *pType = 2;
			return m_mPlayerAliases[uFriendsID].c_str();
		}
	}
	return sDefault;
}



void CPlayerlistUtils::UpdatePlayers( )
{
	static Timer tTimer = {};
	if (!tTimer.Run(1.f))
		return;

	std::lock_guard lock(m_mutex);
	m_vPlayerCache.clear();

	auto pResource = H::Entities.GetPR();
	if (!pResource)
		return;

	for (int n = 1; n <= I::EngineClient->GetMaxClients(); n++)
	{
		if (!pResource->m_bValid(n) || !pResource->m_bConnected(n))
			continue;

		PlayerInfo_t pi{};
		uint32_t uFriendsID = pResource->m_iAccountID(n);
		const char* sName = pResource->m_pszPlayerName(n);
		
		// Process special characters in player names
		if (sName && uFriendsID)
			ProcessSpecialCharsInName(uFriendsID, sName);

		m_vPlayerCache.emplace_back(
			sName ? sName : "",
			uFriendsID,
			pResource->m_iUserID(n),
			pResource->m_iTeam(n),
			pResource->m_bAlive(n),
			n == I::EngineClient->GetLocalPlayer(),
			!I::EngineClient->GetPlayerInfo(n, &pi) || pi.fakeplayer,
			H::Entities.IsFriend(uFriendsID),
			H::Entities.InParty(uFriendsID),
			H::Entities.IsF2P(uFriendsID),
			H::Entities.GetLevel(uFriendsID),
			H::Entities.GetParty(uFriendsID)
		);
	}
}

void CPlayerlistUtils::IncrementBotIgnoreKillCount(uint32_t uFriendsID)
{
	if (!uFriendsID)
		return;

	if (HasTag(uFriendsID, TagToIndex(BOT_IGNORE_TAG)))
	{
		auto& botData = m_mBotIgnoreData[uFriendsID];
		botData.m_iKillCount++;
		m_bSave = true;
	}
}