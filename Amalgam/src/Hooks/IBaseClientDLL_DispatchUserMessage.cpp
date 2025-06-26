#include "../SDK/SDK.h"

#include "../Features/Misc/Misc.h"
#include "../Features/Output/Output.h"
#include "../Features/NoSpread/NoSpreadHitscan/NoSpreadHitscan.h"
#include "../Features/Misc/AutoVote/AutoVote.h"
#include "../Features/Aimbot/AutoHeal/AutoHeal.h"
#include "../Features/Players/PlayerUtils.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <algorithm>

//#define DEBUG_VISUALS
#ifdef DEBUG_VISUALS
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#endif

MAKE_HOOK(IBaseClientDLL_DispatchUserMessage, U::Memory.GetVFunc(I::BaseClientDLL, 36), bool,
	void* rcx, UserMessageType type, bf_read& msgData)
{
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::IBaseClientDLL_DispatchUserMessage[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, type, msgData);
#endif

	auto bufData = reinterpret_cast<const char*>(msgData.m_pData);
	msgData.SetAssertOnOverflow(false);
	msgData.Seek(0);

	switch (type)
	{
	case SayText2:
	{
		msgData.Seek(0);
		int speaker = msgData.ReadByte();
		bool bWantsToChat = msgData.ReadByte();

		char formatString[256];
		msgData.ReadString(formatString, sizeof(formatString), true);

		char speakerName[64];
		msgData.ReadString(speakerName, sizeof(speakerName), true);

		char actualMessage[256] = "";
		msgData.ReadString(actualMessage, sizeof(actualMessage), true);

		char locationName[64] = "";
		msgData.ReadString(locationName, sizeof(locationName), true);

		// remove if you don't need debugging
		SDK::Output("ChatDebug", std::format("Format: '{}', Speaker: '{}', Message: '{}', Location: '{}'",
			formatString, speakerName, actualMessage, locationName).c_str(), {}, true, true);

		
		std::string format = formatString;
		if (format == "TF_Chat_All" || format == "TF_Chat_Team" || format == "TF_Chat_AllDead" || format == "TF_Chat_TeamDead")
		{
			std::string message = actualMessage;
			if (!message.empty())
			{
				
				message.erase(0, message.find_first_not_of(" \t\r\n"));
				message.erase(message.find_last_not_of(" \t\r\n") + 1);

				SDK::Output("ChatDebug", std::format("Final message from speaker {}: '{}'", speaker, message).c_str(), {}, true, true);

				if (!message.empty() && speaker != I::EngineClient->GetLocalPlayer())
				{
					SDK::Output("ChatDebug", "Calling AutoReply", {}, true, true);
					F::Misc.AutoReply(speaker, message.c_str());
				}
				else
				{
					SDK::Output("ChatDebug", "Skipping AutoReply - empty message or self-message", {}, true, true);
				}
			}
			else
			{
				SDK::Output("ChatDebug", "Empty actual message", {}, true, true);
			}
		}
		else
		{
			SDK::Output("ChatDebug", std::format("Non-chat format: '{}'", format).c_str(), {}, true, true);
		}

		break;
	}

	case VoteStart:
		F::Output.UserMessage(msgData);
		F::AutoVote.UserMessage(msgData);
		F::Misc.VotekickResponse(msgData);

		break;
	case VoiceSubtitle:
	{
		int iEntityID = msgData.ReadByte();
		int iVoiceMenu = msgData.ReadByte();
		int iCommandID = msgData.ReadByte();
		if (iVoiceMenu == 1 && iCommandID == 6)
			F::AutoHeal.m_mMedicCallers[iEntityID] = true;

		break;
	}

	case TextMsg:
	{
		char rawMsg[256]; msgData.ReadString(rawMsg, sizeof(rawMsg), true);
		msgData.Seek(0);
		std::string sMsg = rawMsg;
		if (!sMsg.empty())
		{
			sMsg.erase(sMsg.begin());

			if (F::NoSpreadHitscan.ParsePlayerPerf(sMsg))
				return true;

#ifdef DEBUG_VISUALS
			if (sMsg.find("[BoxAngles] ") == 0)
			{
				try
				{
					sMsg.replace(0, strlen("[BoxAngles] "), "");
					std::vector<std::string> vValues = {};
					boost::split(vValues, sMsg, boost::is_any_of(" "));
					if (vValues.size() != 17)
						return true;

					Vec3 vOrigin = { std::stof(vValues[0]), std::stof(vValues[1]), std::stof(vValues[2]) };
					Vec3 vMins = { std::stof(vValues[3]), std::stof(vValues[4]), std::stof(vValues[5]) };
					Vec3 vMaxs = { std::stof(vValues[6]), std::stof(vValues[7]), std::stof(vValues[8]) };
					Vec3 vAngles = { std::stof(vValues[9]), std::stof(vValues[10]), std::stof(vValues[11]) };
					Color_t tColor = { byte(std::stoi(vValues[12])), byte(std::stoi(vValues[13])), byte(std::stoi(vValues[14])), byte(255 - std::stoi(vValues[15])) };
					float flDuration = std::stof(vValues[16]);

					G::BoxStorage.emplace_back(vOrigin, vMins, vMaxs, vAngles, I::GlobalVars->curtime + flDuration, tColor, Color_t(0, 0, 0, 0));
				}
				catch (...) {}

				return true;
			}
			if (sMsg.find("[Line] ") == 0)
			{
				try
				{
					sMsg.replace(0, strlen("[Line] "), "");
					std::vector<std::string> vValues = {};
					boost::split(vValues, sMsg, boost::is_any_of(" "));
					if (vValues.size() != 11)
						return true;

					Vec3 vStart = { std::stof(vValues[0]), std::stof(vValues[1]), std::stof(vValues[2]) };
					Vec3 vEnd = { std::stof(vValues[3]), std::stof(vValues[4]), std::stof(vValues[5]) };
					Color_t tColor = { byte(std::stoi(vValues[6])), byte(std::stoi(vValues[7])), byte(std::stoi(vValues[8])), byte(255 - std::stoi(vValues[9])) };
					float flDuration = std::stof(vValues[10]);

					G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vStart, vEnd), I::GlobalVars->curtime + flDuration, tColor);
				}
				catch (...) {}

				return true;
			}
#endif

			if (Vars::Misc::Automation::AntiAutobalance.Value && FNV1A::Hash32(sMsg.c_str()) == FNV1A::Hash32Const("#TF_Autobalance_TeamChangePending"))
				I::EngineClient->ClientCmd_Unrestricted("retry");
		}
		break;
	}
	case VGUIMenu:
		if (Vars::Visuals::Removals::MOTD.Value && bufData
			&& FNV1A::Hash32(bufData) == FNV1A::Hash32Const("info"))
		{
			I::EngineClient->ClientCmd_Unrestricted("closedwelcomemenu");
			return true;
		}
		break;
	case ForcePlayerViewAngles:
		return Vars::Visuals::Removals::AngleForcing.Value ? true : CALL_ORIGINAL(rcx, type, msgData);
	case SpawnFlyingBird:
	case PlayerGodRayEffect:
	case PlayerTauntSoundLoopStart:
	case PlayerTauntSoundLoopEnd:
		return Vars::Visuals::Removals::Taunts.Value ? true : CALL_ORIGINAL(rcx, type, msgData);
	case Shake:
	case Fade:
	case Rumble:
		return Vars::Visuals::Removals::ScreenEffects.Value ? true : CALL_ORIGINAL(rcx, type, msgData);
	}

	msgData.Seek(0);
	return CALL_ORIGINAL(rcx, type, msgData);
}
