#include "Commands.h"
#include "../NavBot/NBheader.h"

#include "../../Core/Core.h"
#include "../ImGui/Menu/Menu.h"
#include "../NavBot/NavEngine/NavEngine.h"
#include "../Configs/Configs.h"
#include "../Players/PlayerUtils.h"
#include "../Misc/AutoItem/AutoItem.h"
#include "../Misc/Misc.h"
#include <utility>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>

bool CCommands::Run(const std::string& sCmd, std::deque<std::string>& vArgs)
{
	auto uHash = FNV1A::Hash32(sCmd.c_str());
	if (!m_mCommands.contains(uHash))
		return false;

	m_mCommands[uHash](vArgs);
	return true;
}

void CCommands::Register(const std::string& sName, CommandCallback fCallback)
{
	m_mCommands[FNV1A::Hash32(sName.c_str())] = std::move(fCallback);
}

void CCommands::Initialize()
{
	Register("cat_queue", [](const std::deque<std::string>& vArgs)
		{
			if (!I::TFPartyClient)
				return;
			if (I::TFPartyClient->BInQueueForMatchGroup(k_eTFMatchGroup_Casual_Default))
				return;
			if (I::EngineClient->IsDrawingLoadingImage())
				return;

			static bool bHasLoaded = false;
			if (!bHasLoaded)
			{
				I::TFPartyClient->LoadSavedCasualCriteria();
				bHasLoaded = true;
			}
			I::TFPartyClient->RequestQueueForMatch(k_eTFMatchGroup_Casual_Default);
		});

	Register("cat_mannup_queue", [](const std::deque<std::string>& vArgs)
		{
			if (!I::TFPartyClient)
				return;
			if (I::TFPartyClient->BInQueueForMatchGroup(k_eTFMatchGroup_MvM_MannUp))
				return;
			if (I::EngineClient->IsDrawingLoadingImage())
				return;

			I::TFPartyClient->RequestQueueForMatch(k_eTFMatchGroup_MvM_MannUp);

		});

	Register("cat_abandon_match", [](const std::deque<std::string>&)
			{
				if (!I::TFGCClientSystem)
					return;
				I::TFGCClientSystem->AbandonCurrentMatch();
			});

	Register("cat_cc_reload", [](const std::deque<std::string>&)
			{
				if (!I::TFPartyClient)
					return;
				I::TFPartyClient->LoadSavedCasualCriteria();
			});

	Register("cat_load", [](const std::deque<std::string>& args)
		{
			if (args.empty())
			{
				SDK::Output("Usage:\n\tcat_load <config_name>");
				return;
			}

			F::Configs.LoadConfig(args[0], true);
		});

	Register("cat_setcvar", [](const std::deque<std::string>& vArgs)
		{
			if (vArgs.size() < 2)
			{
				SDK::Output("Usage:\n\tcat_setcvar <cvar> <value>");
				return;
			}

			std::string sCVar = vArgs[0];
			auto pCVar = I::CVar->FindVar(sCVar.c_str());
			if (!pCVar)
			{
				SDK::Output(std::format("Could not find {}", sCVar).c_str());
				return;
			}

			auto vArgs2 = vArgs; vArgs2.pop_front();
			std::string sValue = boost::algorithm::join(vArgs2, " ");
			boost::replace_all(sValue, "\"", "");
			pCVar->SetValue(sValue.c_str());
			SDK::Output(std::format("Set {} to {}", sCVar, sValue).c_str());
		});

	Register("cat_getcvar", [](const std::deque<std::string>& vArgs)
		{
			if (vArgs.size() != 1)
			{
				SDK::Output("Usage:\n\tcat_getcvar <cvar>");
				return;
			}

			std::string sCVar = vArgs[0];
			auto pCVar = I::CVar->FindVar(sCVar.c_str());
			if (!pCVar)
			{
				SDK::Output(std::format("Could not find {}", sCVar).c_str());
				return;
			}

			SDK::Output(std::format("Value of {} is {}", sCVar, pCVar->GetString()).c_str());
		});

	Register("cat_clearchat", [](const std::deque<std::string>& vArgs)
		{
			I::ClientModeShared->m_pChatElement->SetText("");
		});

	Register("cat_menu", [](const std::deque<std::string>& vArgs)
		{
			I::MatSystemSurface->SetCursorAlwaysVisible(F::Menu.m_bIsOpen = !F::Menu.m_bIsOpen);
		});

	Register("cat_path_to", [](std::deque<std::string> vArgs)
		{
			// Check if the user provided at least 3 args
			if (vArgs.size() < 3)
			{
				I::CVar->ConsoleColorPrintf({ 255, 255, 255, 255 }, "Usage: cat_path_to <x> <y> <z>\n");
				return;
			}

			// Get the Vec3
			const auto Vec = Vec3( atoi( vArgs[ 0 ].c_str( ) ), atoi( vArgs[ 1 ].c_str( ) ), atoi( vArgs[ 2 ].c_str( ) ) );

			F::NavEngine.navTo( Vec );
		});

	Register("cat_nav_search_spawnrooms", [](std::deque<std::string> vArgs)
		{
			if ( F::NavEngine.map && F::NavEngine.map->state == CNavParser::NavState::Active )
				F::NavEngine.map->UpdateRespawnRooms( );
		});

	Register("cat_nav_add_mvm_spawn", [](const std::deque<std::string>& vArgs)
		{
			auto pLocal = H::Entities.GetLocal();
			if (!pLocal)
			{
				SDK::Output("NavBot", "Cannot add spawn spot: no local player", {255,50,50}, true, true);
				return;
			}
			F::NavBot.AddMvMSpawnSpot(pLocal->GetAbsOrigin());
		});
	Register("cat_nav_clear_mvm_spawns", [](const std::deque<std::string>& vArgs)
		{
			F::NavBot.ClearMvMSpawnSpots();
		});

	Register("cat_save_nav_mesh", [](std::deque<std::string> vArgs)
		{
			if ( auto pNavFile = F::NavEngine.getNavFile( ) )
				pNavFile->Write( );
		});

	Register("cat_detach", [](const std::deque<std::string>& vArgs)
		{
			if (F::Menu.m_bIsOpen)
				I::MatSystemSurface->SetCursorAlwaysVisible(F::Menu.m_bIsOpen = false);
			U::Core.m_bUnload = true;
		});

	Register("cat_ignore", [](const std::deque<std::string>& vArgs)
		{
			if (vArgs.empty())
			{
				SDK::Output("Usage:\n\tcat_ignore <id32>");
				return;
			}

			uint32_t uFriendsID = 0;
			try
			{
				uFriendsID = std::stoul(vArgs[0]);
			}
			catch (...)
			{
				SDK::Output("Invalid ID32 format");
				return;
			}

			if (!uFriendsID)
			{
				SDK::Output("Invalid ID32");
				return;
			}

			int iTagID = F::PlayerUtils.TagToIndex(IGNORED_TAG);
			
			if (F::PlayerUtils.HasTag(uFriendsID, iTagID))
			{
				return;
			}
			else
			{
				F::PlayerUtils.AddTag(uFriendsID, iTagID, true);
				SDK::Output(std::format("Added player {} to ignore list", uFriendsID).c_str());
			}
		});

	Register("cat_crash", [](const std::deque<std::string>& vArgs) // if you want to time out of a server and rejoin
		{
			switch (vArgs.empty() ? 0 : FNV1A::Hash32(vArgs.front().c_str()))
			{
			case FNV1A::Hash32Const("true"):
			case FNV1A::Hash32Const("t"):
			case FNV1A::Hash32Const("1"):
				break;
			default:
				Vars::Debug::CrashLogging.Value = false; // we are voluntarily crashing, don't give out log if we don't want one
			}
			reinterpret_cast<void(*)()>(0)();
		});

	Register("cat_rent_item", [](const std::deque<std::string>& vArgs)
		{
			if (vArgs.size() != 1)
			{
				SDK::Output("Usage:\n\tcat_rent_item <item_def_index>");
				return;
			}

			item_definition_index_t iDefIdx;
			try
			{
				iDefIdx = atoi(vArgs[0].c_str());
			}
			catch (const std::invalid_argument&)
			{
				SDK::Output("Invalid item_def_index");
				return;
			}

			F::AutoItem.Rent(iDefIdx);
		});

	Register("cat_achievement_unlock", [](const std::deque<std::string>& vArgs)
		{
			F::Misc.UnlockAchievements();
		});
}