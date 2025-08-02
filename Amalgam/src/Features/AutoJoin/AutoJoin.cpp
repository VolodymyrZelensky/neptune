#include "AutoJoin.h"
#include <random>

int CAutoJoin::GetRandomClass()
{
	static std::random_device rd;
	static std::mt19937 rng(rd());

	std::vector<int> availableClasses;
	for (int i = 1; i <= 9; i++)
	{
		bool excluded = false;
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 0)) && i == 3) excluded = true; // Soldier
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 1)) && i == 4) excluded = true; // Demoman
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 2)) && i == 7) excluded = true; // Pyro
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 3)) && i == 5) excluded = true; // Medic
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 4)) && i == 1) excluded = true; // Scout
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 5)) && i == 6) excluded = true; // Heavy
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 6)) && i == 9) excluded = true; // Engineer
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 7)) && i == 2) excluded = true; // Sniper
		if ((Vars::Misc::Automation::ExcludeProjectileClasses.Value & (1 << 8)) && i == 8) excluded = true; // Spy

		if (!excluded)
			availableClasses.push_back(i);
	}

	if (!availableClasses.empty())
	{
		std::uniform_int_distribution<int> dist(0, availableClasses.size() - 1);
		int randomIndex = dist(rng);
		return availableClasses[randomIndex];
	}
	return 1;
}

// Doesnt work with custom huds!1!!
void CAutoJoin::Run(CTFPlayer* pLocal)
{
	static Timer tJoinTimer{};
	static Timer tRandomClassTimer{};

	// MOTD Bot
	if (Vars::Misc::Automation::MotdBot.Value && pLocal)
	{
		if (tJoinTimer.Run(1.f))
		{
			if (pLocal->IsAlive() && pLocal->m_iClass() != TF_CLASS_UNDEFINED)
			{
				I::EngineClient->ClientCmd_Unrestricted("kill");
				I::EngineClient->ClientCmd_Unrestricted("menuopen");
			}
			else
			{
				I::EngineClient->ClientCmd_Unrestricted("team_ui_setup");
				I::EngineClient->ClientCmd_Unrestricted("menuopen");
			}
		}
		return;
	}

	if ((Vars::Misc::Automation::ForceClass.Value || Vars::Misc::Automation::RandomClassSwitch.Value) && pLocal)
	{
		if (tJoinTimer.Run(1.f))
		{
			if (Vars::Misc::Automation::RandomClassSwitch.Value)
			{
				static int iCurrentRandomClass = GetRandomClass();

				const float flSwitchInterval = Vars::Misc::Automation::RandomClassInterval.Value * 60.f;

				if (iCurrentRandomClass < 1 || iCurrentRandomClass > 9 || tRandomClassTimer.Run(flSwitchInterval))
				{
					iCurrentRandomClass = GetRandomClass();
				}

				if (pLocal->m_iTeamNum() == TF_TEAM_RED || pLocal->m_iTeamNum() == TF_TEAM_BLUE)
				{
					I::EngineClient->ClientCmd_Unrestricted(std::format("joinclass {}", m_aClassNames[iCurrentRandomClass - 1]).c_str());
					I::EngineClient->ClientCmd_Unrestricted("menuclosed");
				}
				else
				{
					I::EngineClient->ClientCmd_Unrestricted("team_ui_setup");
					I::EngineClient->ClientCmd_Unrestricted("menuopen");
					I::EngineClient->ClientCmd_Unrestricted("autoteam");
					I::EngineClient->ClientCmd_Unrestricted("menuclosed");
				}
			}
			else if (Vars::Misc::Automation::ForceClass.Value)
			{
				if (pLocal->m_iTeamNum() == TF_TEAM_RED || pLocal->m_iTeamNum() == TF_TEAM_BLUE)
				{
					I::EngineClient->ClientCmd_Unrestricted(std::format("joinclass {}", m_aClassNames[Vars::Misc::Automation::ForceClass.Value - 1]).c_str());
					I::EngineClient->ClientCmd_Unrestricted("menuclosed");
				}
				else
				{
					I::EngineClient->ClientCmd_Unrestricted("team_ui_setup");
					I::EngineClient->ClientCmd_Unrestricted("menuopen");
					I::EngineClient->ClientCmd_Unrestricted("autoteam");
					I::EngineClient->ClientCmd_Unrestricted("menuclosed");
				}
			}
		}
	}
}