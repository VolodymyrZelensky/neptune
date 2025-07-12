#include "ESP.h"

#include "../../Players/PlayerUtils.h"
#include "../../Spectate/Spectate.h"
#include "../../Simulation/MovementSimulation/MovementSimulation.h"
#include "../../PacketManip/AntiAim/AntiAim.h"
#include "../../Simulation/ProjectileSimulation/ProjectileSimulation.h"
#include "../../../Utils/Optimization/CpuOptimization.h"
#include <cmath>

// void CESP::DrawYawArrow(const Vec3& vOrigin, float flYaw, float flArrowLength, float flHeadSize, float flHeightOffset, 
//                       const Color_t& mainColor, const Color_t& outlineColor, const Vec3& vScreenOrigin, 
//                       Vars::ESP::YawArrowsStyleEnum::YawArrowsStyleEnum style, bool isReal)
// {
// 	float flRadians = DEG2RAD(flYaw);
	
// 	Vec3 vArrowEnd;
// 	vArrowEnd.x = vOrigin.x + cos(flRadians) * flArrowLength;
// 	vArrowEnd.y = vOrigin.y + sin(flRadians) * flArrowLength;
// 	vArrowEnd.z = vOrigin.z; // Same height as origin
	
// 	float flHeadAngle1 = flRadians + DEG2RAD(150);
// 	float flHeadAngle2 = flRadians - DEG2RAD(150);
	
// 	Vec3 vHeadPoint1;
// 	vHeadPoint1.x = vArrowEnd.x + cos(flHeadAngle1) * flHeadSize;
// 	vHeadPoint1.y = vArrowEnd.y + sin(flHeadAngle1) * flHeadSize;
// 	vHeadPoint1.z = vArrowEnd.z;
	
// 	Vec3 vHeadPoint2;
// 	vHeadPoint2.x = vArrowEnd.x + cos(flHeadAngle2) * flHeadSize;
// 	vHeadPoint2.y = vArrowEnd.y + sin(flHeadAngle2) * flHeadSize;
// 	vHeadPoint2.z = vArrowEnd.z;
	
// 	Vec3 vScreenEnd, vScreenHead1, vScreenHead2;
// 	if (SDK::W2S(vArrowEnd, vScreenEnd) && 
// 	    SDK::W2S(vHeadPoint1, vScreenHead1) && 
// 	    SDK::W2S(vHeadPoint2, vScreenHead2))
// 	{
// 		switch (style)
// 		{
// 			case Vars::ESP::YawArrowsStyleEnum::Default:
// 			{
// 				Color_t shadowColor = { 0, 0, 0, 160 };
// 				float shadowOffsetX = 2.0f;
// 				float shadowOffsetY = 2.0f;
				
// 				H::Draw.Line(vScreenOrigin.x + shadowOffsetX, vScreenOrigin.y + shadowOffsetY, 
// 				           vScreenEnd.x + shadowOffsetX, vScreenEnd.y + shadowOffsetY, shadowColor);
// 				H::Draw.Line(vScreenEnd.x + shadowOffsetX, vScreenEnd.y + shadowOffsetY, 
// 				           vScreenHead1.x + shadowOffsetX, vScreenHead1.y + shadowOffsetY, shadowColor);
// 				H::Draw.Line(vScreenEnd.x + shadowOffsetX, vScreenEnd.y + shadowOffsetY, 
// 				           vScreenHead2.x + shadowOffsetX, vScreenHead2.y + shadowOffsetY, shadowColor);
				
// 				// Draw outline
// 				H::Draw.Line(vScreenOrigin.x-1, vScreenOrigin.y-1, vScreenEnd.x-1, vScreenEnd.y-1, outlineColor);
// 				H::Draw.Line(vScreenOrigin.x+1, vScreenOrigin.y+1, vScreenEnd.x+1, vScreenEnd.y+1, outlineColor);
// 				H::Draw.Line(vScreenEnd.x-1, vScreenEnd.y-1, vScreenHead1.x-1, vScreenHead1.y-1, outlineColor);
// 				H::Draw.Line(vScreenEnd.x+1, vScreenEnd.y+1, vScreenHead1.x+1, vScreenHead1.y+1, outlineColor);
// 				H::Draw.Line(vScreenEnd.x-1, vScreenEnd.y-1, vScreenHead2.x-1, vScreenHead2.y-1, outlineColor);
// 				H::Draw.Line(vScreenEnd.x+1, vScreenEnd.y+1, vScreenHead2.x+1, vScreenHead2.y+1, outlineColor);

// 				// Draw main lines
// 				H::Draw.Line(vScreenOrigin.x, vScreenOrigin.y, vScreenEnd.x, vScreenEnd.y, mainColor);
// 				H::Draw.Line(vScreenEnd.x, vScreenEnd.y, vScreenHead1.x, vScreenHead1.y, mainColor);
// 				H::Draw.Line(vScreenEnd.x, vScreenEnd.y, vScreenHead2.x, vScreenHead2.y, mainColor);
// 				break;
// 			}
// 			case Vars::ESP::YawArrowsStyleEnum::Modern:
// 			{
// 				Color_t shaftGradientStart = mainColor;
// 				Color_t shaftGradientEnd = mainColor;
// 				shaftGradientStart.a = 230;
// 				shaftGradientEnd.a = 255;
				
// 				Vec3 vMidPoint;
// 				vMidPoint.x = (vScreenOrigin.x + vScreenEnd.x) / 2.0f;
// 				vMidPoint.y = (vScreenOrigin.y + vScreenEnd.y) / 2.0f;
				
// 				H::Draw.Line(vScreenOrigin.x, vScreenOrigin.y, vMidPoint.x, vMidPoint.y, shaftGradientStart);
// 				H::Draw.Line(vMidPoint.x, vMidPoint.y, vScreenEnd.x, vScreenEnd.y, shaftGradientEnd);
				
// 				float perpX = -sin(flRadians) * 1.0f;
// 				float perpY = cos(flRadians) * 1.0f;
// 				H::Draw.Line(vScreenOrigin.x + perpX, vScreenOrigin.y + perpY, 
// 				           vMidPoint.x + perpX, vMidPoint.y + perpY, shaftGradientStart);
// 				H::Draw.Line(vMidPoint.x + perpX, vMidPoint.y + perpY, 
// 				           vScreenEnd.x + perpX, vScreenEnd.y + perpY, shaftGradientEnd);
// 				H::Draw.Line(vScreenOrigin.x - perpX, vScreenOrigin.y - perpY, 
// 				           vMidPoint.x - perpX, vMidPoint.y - perpY, shaftGradientStart);
// 				H::Draw.Line(vMidPoint.x - perpX, vMidPoint.y - perpY, 
// 				           vScreenEnd.x - perpX, vScreenEnd.y - perpY, shaftGradientEnd);
				
// 				Vec3 vModernHead1, vModernHead2;
// 				float largerHeadSize = flHeadSize * 1.3f;
				
// 				vModernHead1.x = vArrowEnd.x + cos(flHeadAngle1) * largerHeadSize;
// 				vModernHead1.y = vArrowEnd.y + sin(flHeadAngle1) * largerHeadSize;
// 				vModernHead1.z = vArrowEnd.z;
				
// 				vModernHead2.x = vArrowEnd.x + cos(flHeadAngle2) * largerHeadSize;
// 				vModernHead2.y = vArrowEnd.y + sin(flHeadAngle2) * largerHeadSize;
// 				vModernHead2.z = vArrowEnd.z;
				
// 				Vec3 vScreenModernHead1, vScreenModernHead2;
// 				if (SDK::W2S(vModernHead1, vScreenModernHead1) && SDK::W2S(vModernHead2, vScreenModernHead2))
// 				{
// 					std::vector<Vertex_t> headPoints;
// 					headPoints.push_back({ vScreenEnd.x, vScreenEnd.y });
// 					headPoints.push_back({ vScreenModernHead1.x, vScreenModernHead1.y });
// 					headPoints.push_back({ vScreenModernHead2.x, vScreenModernHead2.y });
					
// 					H::Draw.FillPolygon(headPoints, mainColor);
					
// 					Color_t highlightColor = mainColor;
// 					highlightColor.r = std::min(255, highlightColor.r + 40);
// 					highlightColor.g = std::min(255, highlightColor.g + 40);
// 					highlightColor.b = std::min(255, highlightColor.b + 40);
					
// 					Vec3 vHighlightPoint;
// 					vHighlightPoint.x = vScreenEnd.x - (cos(flRadians) * (flHeadSize * 0.3f));
// 					vHighlightPoint.y = vScreenEnd.y - (sin(flRadians) * (flHeadSize * 0.3f));
					
// 					std::vector<Vertex_t> highlightPoints;
// 					highlightPoints.push_back({ vHighlightPoint.x, vHighlightPoint.y });
// 					highlightPoints.push_back({ (vScreenModernHead1.x + vScreenEnd.x) / 2.0f, (vScreenModernHead1.y + vScreenEnd.y) / 2.0f });
// 					highlightPoints.push_back({ (vScreenModernHead2.x + vScreenEnd.x) / 2.0f, (vScreenModernHead2.y + vScreenEnd.y) / 2.0f });
					
// 					H::Draw.FillPolygon(highlightPoints, highlightColor);
					
// 					Color_t darkOutline = mainColor;
// 					darkOutline.r = std::max(0, darkOutline.r - 70);
// 					darkOutline.g = std::max(0, darkOutline.g - 70);
// 					darkOutline.b = std::max(0, darkOutline.b - 70);
// 					darkOutline.a = 255;
					
// 					H::Draw.LinePolygon(headPoints, darkOutline);
// 				}
// 				break;
// 			}
// 			default: // Fallback to default
// 			{
// 				H::Draw.Line(vScreenOrigin.x, vScreenOrigin.y, vScreenEnd.x, vScreenEnd.y, mainColor);
// 				H::Draw.Line(vScreenEnd.x, vScreenEnd.y, vScreenHead1.x, vScreenHead1.y, mainColor);
// 				H::Draw.Line(vScreenEnd.x, vScreenEnd.y, vScreenHead2.x, vScreenHead2.y, mainColor);
// 				break;
// 			}
// 		}
// 	}
// }

MAKE_SIGNATURE(CTFPlayerSharedUtils_GetEconItemViewByLoadoutSlot, "client.dll", "48 89 6C 24 ? 56 41 54 41 55 41 56 41 57 48 83 EC", 0x0);
MAKE_SIGNATURE(CEconItemView_GetItemName, "client.dll", "40 53 48 83 EC ? 48 8B D9 C6 81 ? ? ? ? ? E8 ? ? ? ? 48 8B 8B", 0x0);

void CESP::Store(CTFPlayer* pLocal)
{
	CPU_OPT_SKIP_IF_VERY_LOW_PERF(2); // Skip ESP on very low performance
	
	m_mPlayerCache.clear();
	m_mBuildingCache.clear();
	m_mWorldCache.clear();

	if (!Vars::ESP::Draw.Value || !pLocal)
		return;

	// Frame distribution for ESP components
	static int espFrameCounter = 0;
	espFrameCounter = (espFrameCounter + 1) % 3;
	
	// Always store players (most important)
	StorePlayers(pLocal);
	
	// Distribute other expensive operations
	switch (espFrameCounter)
	{
	case 0:
		if (CPU_OPT_EXPENSIVE_FEATURE_CHECK())
			StoreBuildings(pLocal);
		break;
	case 1:
		if (CPU_OPT_EXPENSIVE_FEATURE_CHECK())
		{
			StoreProjectiles(pLocal);
			StoreObjective(pLocal);
		}
		break;
	case 2:
		if (CPU_OPT_EXPENSIVE_FEATURE_CHECK())
			StoreWorld();
		break;
	}
}

void CESP::StorePlayers(CTFPlayer* pLocal)
{
	CPU_OPT_SKIP_IF_LOW_PERF(); // Skip on low performance
	
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Players) || !Vars::ESP::Player.Value)
		return;

	auto pObserverTarget = pLocal->m_hObserverTarget().Get();
	int iObserverMode = pLocal->m_iObserverMode();
	if (F::Spectate.m_iTarget != -1)
	{
		pObserverTarget = F::Spectate.m_pTargetTarget;
		iObserverMode = F::Spectate.m_iTargetMode;
	}

	auto pResource = H::Entities.GetPR();
	
	// Pre-calculate some expensive values
	const int iLocalIndex = I::EngineClient->GetLocalPlayer();
	const int iLocalTeam = pLocal->m_iTeamNum();
	const Vec3 vLocalOrigin = pLocal->m_vecOrigin();
	const bool bThirdPerson = I::Input->CAM_IsThirdPerson();
	
	for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ALL))
	{
		auto pPlayer = pEntity->As<CTFPlayer>();
		int iIndex = pPlayer->entindex();

		bool bLocal = iIndex == iLocalIndex;
		bool bSpectate = iObserverMode == OBS_MODE_FIRSTPERSON || iObserverMode == OBS_MODE_THIRDPERSON;
		bool bTarget = bSpectate && pObserverTarget == pPlayer;

		if (!pPlayer->IsAlive() || pPlayer->IsAGhost())
			continue;

		if (bLocal || bTarget)
		{
			if (!(Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Local) || (!bSpectate ? !bThirdPerson && bLocal : iObserverMode == OBS_MODE_FIRSTPERSON && bTarget))
				continue;
		}
		else
		{
			// Early distance check to avoid expensive operations on far players
			const Vec3 vDelta = pPlayer->m_vecOrigin() - vLocalOrigin;
			const float flDistance = vDelta.Length2D();
			if (flDistance >= Vars::ESP::MaxDist.Value) 
				continue;

			if (pPlayer->IsDormant())
			{
				if (!H::Entities.GetDormancy(iIndex) || !Vars::ESP::DormantAlpha.Value
					|| Vars::ESP::DormantPriority.Value && !F::PlayerUtils.IsPrioritized(iIndex))
					continue;
			}

			// Cache team check
			const bool bIsEnemy = pPlayer->m_iTeamNum() != iLocalTeam;
			if (!(Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Prioritized && F::PlayerUtils.IsPrioritized(iIndex))
				&& !(Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Friends && H::Entities.IsFriend(iIndex))
				&& !(Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Party && H::Entities.InParty(iIndex))
				&& !(bIsEnemy ? Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Enemy : Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Team))
				continue;
		}

		int iClassNum = pPlayer->m_iClass();
		auto pWeapon = pPlayer->m_hActiveWeapon().Get()->As<CTFWeaponBase>();

		// Use pre-calculated distance
		const Vec3 vDelta = pPlayer->m_vecOrigin() - vLocalOrigin;
		const float flDistance = vDelta.Length2D();

		const bool bDormant = pPlayer->IsDormant();
		float flAlpha = bDormant ? Vars::ESP::DormantAlpha.Value : Vars::ESP::ActiveAlpha.Value;
		if (Vars::ESP::Dist2Alpha.Value)
			flAlpha = Math::RemapVal(flDistance, Vars::ESP::MaxDist.Value - 256.f, Vars::ESP::MaxDist.Value, flAlpha / 255.f, 0.f);

		PlayerCache& tCache = m_mPlayerCache[pEntity];
		tCache.m_flAlpha = flAlpha;
		tCache.m_tColor = GetColor(pLocal, pPlayer);
		tCache.m_bBox = Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Box;
		tCache.m_bBones = Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Bones;
		// tCache.m_bYawArrows = Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::YawArrows;

		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Distance && !bLocal)
			tCache.m_vText.emplace_back(ESPTextEnum::Bottom, std::format("[{:.0f}M]", vDelta.Length2D() / 41), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		PlayerInfo_t pi{};
		if (I::EngineClient->GetPlayerInfo(iIndex, &pi))
		{
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Name)
				tCache.m_vText.emplace_back(ESPTextEnum::Top, F::PlayerUtils.GetPlayerName(iIndex, pi.name), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Priority)
			{
				if (auto pTag = F::PlayerUtils.GetSignificantTag(pi.friendsID, 1)) // 50 alpha as white outline tends to be more apparent
					tCache.m_vText.emplace_back(ESPTextEnum::Top, pTag->m_sName, pTag->m_tColor, H::Draw.IsColorDark(pTag->m_tColor) ? Color_t(255, 255, 255, 50) : Color_t(0, 0, 0));
			}

			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Labels)
			{
				std::vector<PriorityLabel_t*> vTags = {};
				for (auto& iID : F::PlayerUtils.m_mPlayerTags[pi.friendsID])
				{
					auto pTag = F::PlayerUtils.GetTag(iID);
					if (pTag && pTag->m_bLabel)
						vTags.push_back(pTag);
				}
				if (H::Entities.IsFriend(pi.friendsID))
				{
					auto pTag = &F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(FRIEND_TAG)];
					if (pTag->m_bLabel)
						vTags.push_back(pTag);
				}
				if (H::Entities.InParty(pi.friendsID))
				{
					auto pTag = &F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(PARTY_TAG)];
					if (pTag->m_bLabel)
						vTags.push_back(pTag);
				}
				if (H::Entities.IsF2P(pi.friendsID))
				{
					auto pTag = &F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(F2P_TAG)];
					if (pTag->m_bLabel)
						vTags.push_back(pTag);
				}

				if (vTags.size())
				{
					std::sort(vTags.begin(), vTags.end(), [&](const auto a, const auto b) -> bool
						{
							// sort by priority if unequal
							if (a->m_iPriority != b->m_iPriority)
								return a->m_iPriority > b->m_iPriority;

							return a->m_sName < b->m_sName;
						});

					for (auto& pTag : vTags) // 50 alpha as white outline tends to be more apparent
						tCache.m_vText.emplace_back(ESPTextEnum::Right, pTag->m_sName, pTag->m_tColor, H::Draw.IsColorDark(pTag->m_tColor) ? Color_t(255, 255, 255, 50) : Color_t(0, 0, 0));
				}
			}
		}

		float flHealth = pPlayer->m_iHealth(), flMaxHealth = pPlayer->GetMaxHealth();
		if (tCache.m_bHealthBar = Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::HealthBar)
		{
			if (flHealth > flMaxHealth)
			{
				float flMaxOverheal = floorf(flMaxHealth / 10.f) * 5;
				tCache.m_flHealth = 1.f + std::clamp((flHealth - flMaxHealth) / flMaxOverheal, 0.f, 1.f);
			}
			else
				tCache.m_flHealth = std::clamp(flHealth / flMaxHealth, 0.f, 1.f);
		}
		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::HealthText)
			tCache.m_vText.emplace_back(ESPTextEnum::Health, std::format("{}", flHealth), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (iClassNum == TF_CLASS_MEDIC)
		{
			auto pMediGun = pPlayer->GetWeaponFromSlot(SLOT_SECONDARY);
			if (pMediGun && pMediGun->GetClassID() == ETFClassID::CWeaponMedigun)
			{
				tCache.m_flUber = std::clamp(pMediGun->As<CWeaponMedigun>()->m_flChargeLevel(), 0.f, 1.f);
				tCache.m_bUberBar = Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::UberBar;
				if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::UberText)
					tCache.m_vText.emplace_back(ESPTextEnum::Uber, std::format("{:.0f}%", tCache.m_flUber * 100.f), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
			}
		}

		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::ClassIcon)
			tCache.m_iClassIcon = iClassNum;
		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::ClassText)
			tCache.m_vText.emplace_back(ESPTextEnum::Right, SDK::GetClassByIndex(iClassNum, false), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::WeaponIcon && pWeapon)
			tCache.m_pWeaponIcon = pWeapon->GetWeaponIcon();
		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::WeaponText && pWeapon)
		{
			auto pAttributeManager = U::Memory.CallVirtual<1, void*>(uintptr_t(pWeapon) + 3096);
			auto pCurItemData = reinterpret_cast<void*>(uintptr_t(pAttributeManager) + 144);
			tCache.m_vText.emplace_back(ESPTextEnum::Bottom, SDK::ConvertWideToUTF8(S::CEconItemView_GetItemName.Call<const wchar_t*>(pCurItemData)), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		if (Vars::Debug::Info.Value && !pPlayer->IsDormant() && !bLocal)
		{
			int iAverage = TIME_TO_TICKS(F::MoveSim.GetPredictedDelta(pPlayer));
			int iCurrent = H::Entities.GetChoke(iIndex);
			tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Lag {}, {}", iAverage, iCurrent), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		{
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::LagCompensation && !pPlayer->IsDormant() && !bLocal)
			{
				if (H::Entities.GetLagCompensation(iIndex))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Lagcomp", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
			}

			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Ping && pResource && !bLocal)
			{
				auto pNetChan = I::EngineClient->GetNetChannelInfo();
				if (pNetChan && !pNetChan->IsLoopback())
				{
					int iPing = pResource->m_iPing(iIndex);
					if (iPing && (iPing >= 200 || iPing <= 5))
						tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("{}MS", iPing), Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				}
			}

			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::KDR && pResource && !bLocal)
			{
				int iKills = pResource->m_iScore(iIndex), iDeaths = pResource->m_iDeaths(iIndex);
				if (iKills >= 20)
				{
					int iKDR = iKills / std::max(iDeaths, 1);
					if (iKDR >= 10)
						tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("High KD [{} / {}]", iKills, iDeaths), Vars::Colors::IndicatorTextMid.Value, Vars::Menu::Theme::Background.Value);
				}
			}

			// Add the Mafia Works feature implementation
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::ThatsHowMafiaWorks && pResource && pPlayer != pLocal)
			{
				int iKills = pResource->m_iScore(iIndex);
				int iDeaths = pResource->m_iDeaths(iIndex);
				int iDamage = pResource->m_iDamage(iIndex);
				
				// Calculate player level based on stats
				int iLevel = 1;
				if (iKills >= 30 && iDamage >= 10000) iLevel = 6;
				else if (iKills >= 20 && iDamage >= 5000) iLevel = 5;
				else if (iKills >= 15 && iDamage >= 3000) iLevel = 4;
				else if (iKills >= 10 && iDamage >= 2000) iLevel = 3;
				else if (iKills >= 5 && iDamage >= 500) iLevel = 2;
				
				// Define title based on level
				std::string sTitle;
				Color_t tTitleColor;
				switch (iLevel) {
					case 1:
						sTitle = "Lv.1 Crook";
						tTitleColor = Color_t(150, 150, 150, 255); // Grey
						break;
					case 2:
						sTitle = "Lv.10 Gangster";
						tTitleColor = Color_t(76, 175, 80, 255); // Green
						break;
					case 3:
						sTitle = "Lv.35 Hitman";
						tTitleColor = Color_t(33, 150, 243, 255); // Blue
						break;
					case 4:
						sTitle = "Lv.50 Boss";
						tTitleColor = Color_t(156, 39, 176, 255); // Purple
						break;
					case 5:
						sTitle = "Lv.80 Godfather";
						tTitleColor = Color_t(211, 47, 47, 255); // Red
						break;
					case 6:
						sTitle = "Lv.100 BOSS OF ALL BOSSES";
						tTitleColor = Color_t(255, 193, 7, 255); // Gold
						break;
				}

				tCache.m_vText.push_back({ ESPTextEnum::Top, sTitle, tTitleColor, Color_t(0, 0, 0, 200) });
			}


			// Buffs
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Buffs)
			{
				if (pPlayer->InCond(TF_COND_INVULNERABLE) ||
					pPlayer->InCond(TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED) ||
					pPlayer->InCond(TF_COND_INVULNERABLE_USER_BUFF) ||
					pPlayer->InCond(TF_COND_INVULNERABLE_CARD_EFFECT))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Uber", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_MEGAHEAL))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Megaheal", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_PHASE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bonk", Vars::Colors::IndicatorTextMid.Value, Vars::Menu::Theme::Background.Value);

				bool bCrits = false, bMiniCrits = false;
				if (pPlayer->IsCritBoosted())
					pWeapon && pWeapon->GetWeaponID() == TF_WEAPON_PARTICLE_CANNON ? bMiniCrits = true : bCrits = true;
				if (pPlayer->IsMiniCritBoosted())
					pWeapon && SDK::AttribHookValue(0, "minicrits_become_crits", pWeapon) == 1 ? bCrits = true : bMiniCrits = true;
				if (pWeapon && SDK::AttribHookValue(0, "crit_while_airborne", pWeapon) && pPlayer->InCond(TF_COND_BLASTJUMPING))
					bCrits = true, bMiniCrits = false;

				if (bCrits)
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Crits", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (bMiniCrits)
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Mini-crits", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);

				/* vaccinator effects */
				if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BULLET_RESIST) || pPlayer->InCond(TF_COND_BULLET_IMMUNE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bullet+", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_MEDIGUN_SMALL_BULLET_RESIST))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bullet", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BLAST_RESIST) || pPlayer->InCond(TF_COND_BLAST_IMMUNE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Blast+", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_MEDIGUN_SMALL_BLAST_RESIST))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Blast", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_FIRE_RESIST) || pPlayer->InCond(TF_COND_FIRE_IMMUNE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Fire+", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_MEDIGUN_SMALL_FIRE_RESIST))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Fire", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_OFFENSEBUFF))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Banner", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_DEFENSEBUFF))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Battalions", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_REGENONDAMAGEBUFF))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Conch", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_RUNE_STRENGTH))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Strength", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_HASTE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Haste", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_REGEN))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Regen", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_RESIST))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Resistance", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_VAMPIRE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Vampire", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_REFLECT))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflect", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_PRECISION))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Precision", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_AGILITY))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Agility", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_KNOCKOUT))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Knockout", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_KING))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "King", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_PLAGUE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Plague", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_SUPERNOVA))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Supernova", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_POWERUPMODE_DOMINANT))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Dominant", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				for (int i = 0; i < MAX_WEAPONS; i++)
				{
					auto pWeapon = pPlayer->GetWeaponFromSlot(i)->As<CTFSpellBook>();
					if (!pWeapon || pWeapon->GetWeaponID() != TF_WEAPON_SPELLBOOK || !pWeapon->m_iSpellCharges())
						continue;

					switch (pWeapon->m_iSelectedSpellIndex())
					{
					case 0:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Fireball", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 1:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bats", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 2:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Heal", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 3:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Pumpkins", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 4:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Jump", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 5:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Stealth", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 6:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Teleport", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 7:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Lightning", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 8:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Minify", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 9:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Meteors", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 10:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Monoculus", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 11:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Skeletons", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 12:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Glove", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 13:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Parachute", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 14:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Heal", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 15:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bomb", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					}
				}

				if (pPlayer->InCond(TF_COND_RADIUSHEAL) ||
					pPlayer->InCond(TF_COND_HEALTH_BUFF) ||
					pPlayer->InCond(TF_COND_RADIUSHEAL_ON_DAMAGE) ||
					pPlayer->InCond(TF_COND_HALLOWEEN_QUICK_HEAL) ||
					pPlayer->InCond(TF_COND_HALLOWEEN_HELL_HEAL) ||
					pPlayer->InCond(TF_COND_KING_BUFFED))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "HP+", Vars::Colors::IndicatorTextGood.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_HEALTH_OVERHEALED))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "HP", Vars::Colors::IndicatorTextGood.Value, Vars::Menu::Theme::Background.Value);

				//if (pPlayer->InCond(TF_COND_BLASTJUMPING))
				//	tCache.m_vText.emplace_back(ESPTextEnum::Right, "Blastjump", Vars::Colors::IndicatorTextMid.Value, Vars::Menu::Theme::Background.Value);
			}

			// Debuffs
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Debuffs)
			{
				if (pPlayer->InCond(TF_COND_MARKEDFORDEATH) || pPlayer->InCond(TF_COND_MARKEDFORDEATH_SILENT))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Marked", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_URINE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Jarate", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_MAD_MILK))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Milk", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_STUNNED))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Stun", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_BURNING))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Burn", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_BLEEDING))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bleed", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
			}

			// Misc
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Misc)
			{
				if (pPlayer->m_bFeignDeathReady())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "DR", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_FEIGN_DEATH))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Feign", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				
				if (pPlayer->m_flInvisibility())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Invis {:.0f}%", pPlayer->m_flInvisibility() * 100), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_DISGUISING) || pPlayer->InCond(TF_COND_DISGUISED))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Disguise", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_AIMING) || pPlayer->InCond(TF_COND_ZOOMED))
				{
					switch (pWeapon ? pWeapon->GetWeaponID() : -1)
					{
					case TF_WEAPON_MINIGUN:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Rev", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case TF_WEAPON_SNIPERRIFLE:
					case TF_WEAPON_SNIPERRIFLE_CLASSIC:
					case TF_WEAPON_SNIPERRIFLE_DECAP:
					{
						if (bLocal)
						{
							tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Charging {:.0f}%", Math::RemapVal(pWeapon->As<CTFSniperRifle>()->m_flChargedDamage(), 0.f, 150.f, 0.f, 100.f)), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
							break;
						}
						else
						{
							auto GetSniperDot = [](CBaseEntity* pEntity) -> CSniperDot*
								{
									for (auto pDot : H::Entities.GetGroup(EGroupType::MISC_DOTS))
									{
										if (pDot->m_hOwnerEntity().Get() == pEntity)
											return pDot->As<CSniperDot>();
									}
									return nullptr;
								};
							if (CSniperDot* pPlayerDot = GetSniperDot(pEntity))
							{
								float flChargeTime = std::max(SDK::AttribHookValue(3.f, "mult_sniper_charge_per_sec", pWeapon), 1.5f);
								tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Charging {:.0f}%", Math::RemapVal(TICKS_TO_TIME(I::ClientState->m_ClockDriftMgr.m_nServerTick) - pPlayerDot->m_flChargeStartTime() - 0.3f, 0.f, flChargeTime, 0.f, 100.f)), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
								break;
							}
						}
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Charging", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					}
					case TF_WEAPON_COMPOUND_BOW:
						if (bLocal)
						{
							tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Charging {:.0f}%", Math::RemapVal(TICKS_TO_TIME(I::ClientState->m_ClockDriftMgr.m_nServerTick) - pWeapon->As<CTFPipebombLauncher>()->m_flChargeBeginTime(), 0.f, 1.f, 0.f, 100.f)), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
							break;
						}
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Charging", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					default:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Charging", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
					}
				}

				if (pPlayer->InCond(TF_COND_SHIELD_CHARGE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Charge", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (Vars::Visuals::Removals::Taunts.Value && pPlayer->InCond(TF_COND_TAUNTING))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Taunt", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
			}
		}
	}
}

void CESP::StoreBuildings(CTFPlayer* pLocal)
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Buildings) || !Vars::ESP::Building.Value)
		return;

	for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_ALL))
	{
		auto pBuilding = pEntity->As<CBaseObject>();
		auto pOwner = pBuilding->m_hBuilder().Get();
		int iIndex = pOwner ? pOwner->entindex() : -1;

		if (pOwner)
		{
			if (iIndex == I::EngineClient->GetLocalPlayer())
			{
				if (!(Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Local))
					continue;
			}
			else
			{
				if (!(Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Prioritized && F::PlayerUtils.IsPrioritized(iIndex))
					&& !(Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Friends && H::Entities.IsFriend(iIndex))
					&& !(Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Party && H::Entities.InParty(iIndex))
					&& !(pOwner->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Enemy : Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Team))
					continue;
			}
		}
		else if (!(pEntity->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Enemy : Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Team))
			continue;

		// distance things
		const Vec3 vDelta = pBuilding->m_vecOrigin() - pLocal->m_vecOrigin();
		const float flDistance = vDelta.Length2D();
		if (flDistance >= Vars::ESP::MaxDist.Value) { continue; }

		float flAlpha = Vars::ESP::ActiveAlpha.Value;
		if (Vars::ESP::Dist2Alpha.Value)
			flAlpha = Math::RemapVal(flDistance, Vars::ESP::MaxDist.Value - 256.f, Vars::ESP::MaxDist.Value, flAlpha / 255.f, 0.f);

		BuildingCache& tCache = m_mBuildingCache[pEntity];
		tCache.m_flAlpha = flAlpha;
		tCache.m_tColor = GetColor(pLocal, pOwner ? pOwner : pEntity);
		tCache.m_bBox = Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Box;

		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Distance)
			tCache.m_vText.emplace_back(ESPTextEnum::Bottom, std::format("[{:.0f}M]", vDelta.Length2D() / 41), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		bool bIsMini = pBuilding->m_bMiniBuilding();
		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Name)
		{
			const char* szName = "Building";
			switch (pEntity->GetClassID())
			{
			case ETFClassID::CObjectSentrygun: szName = bIsMini ? "Mini-Sentry" : "Sentry"; break;
			case ETFClassID::CObjectDispenser: szName = "Dispenser"; break;
			case ETFClassID::CObjectTeleporter: szName = pBuilding->m_iObjectMode() ? "Teleporter Exit" : "Teleporter Entrance";
			}
			tCache.m_vText.emplace_back(ESPTextEnum::Top, szName, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		float flHealth = pBuilding->m_iHealth(), flMaxHealth = pBuilding->m_iMaxHealth();
		if (tCache.m_bHealthBar = Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::HealthBar)
			tCache.m_flHealth = std::clamp(flHealth / flMaxHealth, 0.f, 1.f);
		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::HealthText)
			tCache.m_vText.emplace_back(ESPTextEnum::Health, std::format("{}", flHealth), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Owner && !pBuilding->m_bWasMapPlaced() && pOwner)
		{
			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(iIndex, &pi))
				tCache.m_vText.emplace_back(ESPTextEnum::Top, F::PlayerUtils.GetPlayerName(iIndex, pi.name), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Level && !bIsMini)
			tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("{} / {}", pBuilding->m_iUpgradeLevel(), bIsMini ? 1 : 3), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Flags)
		{
			float flConstructed = pBuilding->m_flPercentageConstructed();
			if (flConstructed < 1.f)
				tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("{:.0f}%", flConstructed * 100.f), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

			if (pBuilding->IsSentrygun() && pBuilding->As<CObjectSentrygun>()->m_bPlayerControlled())
				tCache.m_vText.emplace_back(ESPTextEnum::Right, "Wrangled", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);

			if (pBuilding->m_bHasSapper())
				tCache.m_vText.emplace_back(ESPTextEnum::Right, "Sapped", Vars::Colors::IndicatorTextGood.Value, Vars::Menu::Theme::Background.Value);
			else if (pBuilding->m_bDisabled())
				tCache.m_vText.emplace_back(ESPTextEnum::Right, "Disabled", Vars::Colors::IndicatorTextGood.Value, Vars::Menu::Theme::Background.Value);

			if (pBuilding->IsSentrygun() && !pBuilding->m_bBuilding())
			{
				int iShells, iMaxShells, iRockets, iMaxRockets; pBuilding->As<CObjectSentrygun>()->GetAmmoCount(iShells, iMaxShells, iRockets, iMaxRockets);
				if (!iShells)
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "No ammo", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (!bIsMini && !iRockets)
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "No rockets", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
			}
		}
	}
}

static inline const char* GetProjectileName(CBaseEntity* pProjectile)
{
	const char* sReturn = "Projectile";
	switch (pProjectile->GetClassID())
	{
	case ETFClassID::CTFWeaponBaseMerasmusGrenade: sReturn = "Bomb"; break;
	case ETFClassID::CTFGrenadePipebombProjectile: sReturn = pProjectile->As<CTFGrenadePipebombProjectile>()->HasStickyEffects() ? "Sticky" : "Pipe"; break;
	case ETFClassID::CTFStunBall: sReturn = "Baseball"; break;
	case ETFClassID::CTFBall_Ornament: sReturn = "Bauble"; break;
	case ETFClassID::CTFProjectile_Jar: sReturn = "Jarate"; break;
	case ETFClassID::CTFProjectile_Cleaver: sReturn = "Cleaver"; break;
	case ETFClassID::CTFProjectile_JarGas: sReturn = "Gas"; break;
	case ETFClassID::CTFProjectile_JarMilk:
	case ETFClassID::CTFProjectile_ThrowableBreadMonster: sReturn = "Milk"; break;
	case ETFClassID::CTFProjectile_SpellBats:
	case ETFClassID::CTFProjectile_SpellKartBats: sReturn = "Bats"; break;
	case ETFClassID::CTFProjectile_SpellMeteorShower: sReturn = "Meteor shower"; break;
	case ETFClassID::CTFProjectile_SpellMirv:
	case ETFClassID::CTFProjectile_SpellPumpkin: sReturn = "Pumpkin"; break;
	case ETFClassID::CTFProjectile_SpellSpawnBoss: sReturn = "Monoculus"; break;
	case ETFClassID::CTFProjectile_SpellSpawnHorde:
	case ETFClassID::CTFProjectile_SpellSpawnZombie: sReturn = "Skeleton"; break;
	case ETFClassID::CTFProjectile_SpellTransposeTeleport: sReturn = "Teleport"; break;
	case ETFClassID::CTFProjectile_Arrow: sReturn = pProjectile->As<CTFProjectile_Arrow>()->m_iProjectileType() == TF_PROJECTILE_BUILDING_REPAIR_BOLT ? "Repair" : "Arrow"; break;
	case ETFClassID::CTFProjectile_GrapplingHook: sReturn = "Grapple"; break;
	case ETFClassID::CTFProjectile_HealingBolt: sReturn = "Heal"; break;
	case ETFClassID::CTFProjectile_Rocket:
	case ETFClassID::CTFProjectile_EnergyBall:
	case ETFClassID::CTFProjectile_SentryRocket: sReturn = "Rocket"; break;
	case ETFClassID::CTFProjectile_BallOfFire: sReturn = "Fire"; break;
	case ETFClassID::CTFProjectile_MechanicalArmOrb: sReturn = "Short circuit"; break;
	case ETFClassID::CTFProjectile_SpellFireball: sReturn = "Fireball"; break;
	case ETFClassID::CTFProjectile_SpellLightningOrb: sReturn = "Lightning"; break;
	case ETFClassID::CTFProjectile_SpellKartOrb: sReturn = "Fist"; break;
	case ETFClassID::CTFProjectile_Flare: sReturn = "Flare"; break;
	case ETFClassID::CTFProjectile_EnergyRing: sReturn = "Energy"; break;
	}
	return sReturn;
}
void CESP::StoreProjectiles(CTFPlayer* pLocal)
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Projectiles) || !Vars::ESP::Projectile.Value)
		return;

	for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_PROJECTILES))
	{
		auto pOwner = F::ProjSim.GetEntities(pEntity).second;
		int iIndex = pOwner ? pOwner->entindex() : -1;
		if (pOwner)
		{
			if (iIndex == I::EngineClient->GetLocalPlayer())
			{
				if (!(Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Local))
					continue;
			}
			else
			{
				if (!(Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Prioritized && F::PlayerUtils.IsPrioritized(iIndex))
					&& !(Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Friends && H::Entities.IsFriend(iIndex))
					&& !(Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Party && H::Entities.InParty(iIndex))
					&& !(pOwner->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Enemy : Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Team))
					continue;
			}
		}
		else if (!(pEntity->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Enemy : Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Team))
			continue;

		// distance things
		const Vec3 vDelta = pEntity->m_vecOrigin() - pLocal->m_vecOrigin();
		const float flDistance = vDelta.Length2D();
		if (flDistance >= Vars::ESP::MaxDist.Value) { continue; }

		float flAlpha = Vars::ESP::ActiveAlpha.Value;
		if (Vars::ESP::Dist2Alpha.Value)
			flAlpha = Math::RemapVal(flDistance, Vars::ESP::MaxDist.Value - 256.f, Vars::ESP::MaxDist.Value, flAlpha / 255.f, 0.f);

		WorldCache& tCache = m_mWorldCache[pEntity];
		tCache.m_flAlpha = flAlpha;
		tCache.m_tColor = GetColor(pLocal, pOwner ? pOwner : pEntity);
		tCache.m_bBox = Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Box;

		if (Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Distance)
			tCache.m_vText.emplace_back(ESPTextEnum::Bottom, std::format("[{:.0f}M]", vDelta.Length2D() / 41), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Name)
			tCache.m_vText.emplace_back(ESPTextEnum::Top, GetProjectileName(pEntity), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Owner && pOwner)
		{
			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(iIndex, &pi))
				tCache.m_vText.emplace_back(ESPTextEnum::Top, F::PlayerUtils.GetPlayerName(iIndex, pi.name), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		if (Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Flags)
		{
			switch (pEntity->GetClassID())
			{
			case ETFClassID::CTFWeaponBaseGrenadeProj:
			case ETFClassID::CTFWeaponBaseMerasmusGrenade:
			case ETFClassID::CTFGrenadePipebombProjectile:
			case ETFClassID::CTFStunBall:
			case ETFClassID::CTFBall_Ornament:
			case ETFClassID::CTFProjectile_Jar:
			case ETFClassID::CTFProjectile_Cleaver:
			case ETFClassID::CTFProjectile_JarGas:
			case ETFClassID::CTFProjectile_JarMilk:
			case ETFClassID::CTFProjectile_SpellBats:
			case ETFClassID::CTFProjectile_SpellKartBats:
			case ETFClassID::CTFProjectile_SpellMeteorShower:
			case ETFClassID::CTFProjectile_SpellMirv:
			case ETFClassID::CTFProjectile_SpellPumpkin:
			case ETFClassID::CTFProjectile_SpellSpawnBoss:
			case ETFClassID::CTFProjectile_SpellSpawnHorde:
			case ETFClassID::CTFProjectile_SpellSpawnZombie:
			case ETFClassID::CTFProjectile_SpellTransposeTeleport:
			case ETFClassID::CTFProjectile_Throwable:
			case ETFClassID::CTFProjectile_ThrowableBreadMonster:
			case ETFClassID::CTFProjectile_ThrowableBrick:
			case ETFClassID::CTFProjectile_ThrowableRepel:
				if (pEntity->As<CTFWeaponBaseGrenadeProj>()->m_bCritical())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Crit", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				if (pEntity->As<CTFWeaponBaseGrenadeProj>()->m_iDeflected() && (pEntity->GetClassID() != ETFClassID::CTFGrenadePipebombProjectile || !pEntity->GetAbsVelocity().IsZero()))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflected", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				break;
			case ETFClassID::CTFProjectile_Arrow:
			case ETFClassID::CTFProjectile_GrapplingHook:
			case ETFClassID::CTFProjectile_HealingBolt:
				if (pEntity->As<CTFProjectile_Arrow>()->m_bCritical())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Crit", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				if (pEntity->As<CTFBaseRocket>()->m_iDeflected())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflected", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				if (pEntity->As<CTFProjectile_Arrow>()->m_bArrowAlight())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Alight", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				break;
			case ETFClassID::CTFProjectile_Rocket:
			case ETFClassID::CTFProjectile_BallOfFire:
			case ETFClassID::CTFProjectile_MechanicalArmOrb:
			case ETFClassID::CTFProjectile_SentryRocket:
			case ETFClassID::CTFProjectile_SpellFireball:
			case ETFClassID::CTFProjectile_SpellLightningOrb:
			case ETFClassID::CTFProjectile_SpellKartOrb:
				if (pEntity->As<CTFProjectile_Rocket>()->m_bCritical())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Crit", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				if (pEntity->As<CTFBaseRocket>()->m_iDeflected())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflected", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				break;
			case ETFClassID::CTFProjectile_EnergyBall:
				if (pEntity->As<CTFProjectile_EnergyBall>()->m_bChargedShot())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Charge", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				if (pEntity->As<CTFBaseRocket>()->m_iDeflected())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflected", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				break;
			case ETFClassID::CTFProjectile_Flare:
				if (pEntity->As<CTFProjectile_Flare>()->m_bCritical())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Crit", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				if (pEntity->As<CTFBaseRocket>()->m_iDeflected())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflected", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				break;
			}
		}
	}
}

void CESP::StoreObjective(CTFPlayer* pLocal)
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Objective) || !Vars::ESP::Objective.Value)
		return;

	for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_OBJECTIVE))
	{
		if (!(pEntity->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Enemy : Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Team))
			continue;

		// distance things
		const Vec3 vDelta = pEntity->m_vecOrigin() - pLocal->m_vecOrigin();
		const float flDistance = vDelta.Length2D();
		if (flDistance >= Vars::ESP::MaxDist.Value) { continue; }

		float flAlpha = Vars::ESP::ActiveAlpha.Value;
		if (Vars::ESP::Dist2Alpha.Value)
			flAlpha = Math::RemapVal(flDistance, Vars::ESP::MaxDist.Value - 256.f, Vars::ESP::MaxDist.Value, flAlpha / 255.f, 0.f);

		WorldCache& tCache = m_mWorldCache[pEntity];
		tCache.m_flAlpha = flAlpha;
		tCache.m_tColor = GetColor(pLocal, pEntity);
		tCache.m_bBox = Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Box;

		if (Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Distance)
		{
			Vec3 vDelta = pEntity->m_vecOrigin() - pLocal->m_vecOrigin();
			tCache.m_vText.emplace_back(ESPTextEnum::Bottom, std::format("[{:.0f}M]", vDelta.Length2D() / 41), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		switch (pEntity->GetClassID())
		{
		case ETFClassID::CCaptureFlag:
		{
			auto pIntel = pEntity->As<CCaptureFlag>();

			if (Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Name)
				tCache.m_vText.emplace_back(ESPTextEnum::Top, "Intel", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

			if (Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Flags)
			{
				switch (pIntel->m_nFlagStatus())
				{
				case TF_FLAGINFO_HOME:
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Home", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
					break;
				case TF_FLAGINFO_DROPPED:
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Dropped", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
					break;
				default:
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Stolen", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				}
			}

			if (Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::IntelReturnTime && pIntel->m_nFlagStatus() == TF_FLAGINFO_DROPPED)
			{
				float flReturnTime = std::max(pIntel->m_flResetTime() - TICKS_TO_TIME(I::ClientState->m_ClockDriftMgr.m_nServerTick), 0.f);
				tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Return {:.1f}s", pIntel->m_flResetTime() - TICKS_TO_TIME(I::ClientState->m_ClockDriftMgr.m_nServerTick)).c_str(), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
			}

			break;
		}
		}
	}
}

void CESP::StoreWorld()
{
	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::NPCs)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_NPC))
		{
			WorldCache& tCache = m_mWorldCache[pEntity];

			const char* szName = "NPC";
			switch (pEntity->GetClassID())
			{
			case ETFClassID::CEyeballBoss: szName = "Monoculus"; break;
			case ETFClassID::CHeadlessHatman: szName = "Horseless Headless Horsemann"; break;
			case ETFClassID::CMerasmus: szName = "Merasmus"; break;
			case ETFClassID::CTFBaseBoss: szName = "Boss"; break;
			case ETFClassID::CTFTankBoss: szName = "Tank"; break;
			case ETFClassID::CZombie: szName = "Skeleton"; break;
			}

			tCache.m_vText.emplace_back(ESPTextEnum::Top, szName, Vars::Colors::NPC.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Health)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PICKUPS_HEALTH))
		{
			WorldCache& tCache = m_mWorldCache[pEntity];

			tCache.m_vText.emplace_back(ESPTextEnum::Top, "Health", Vars::Colors::Health.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Ammo)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PICKUPS_AMMO))
		{
			WorldCache& tCache = m_mWorldCache[pEntity];

			tCache.m_vText.emplace_back(ESPTextEnum::Top, "Ammo", Vars::Colors::Ammo.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Money)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PICKUPS_MONEY))
		{
			WorldCache& tCache = m_mWorldCache[pEntity];

			tCache.m_vText.emplace_back(ESPTextEnum::Top, "Money", Vars::Colors::Money.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Powerups)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PICKUPS_POWERUP))
		{
			WorldCache& tCache = m_mWorldCache[pEntity];

			const char* szName = "Powerup";
			switch (FNV1A::Hash32(I::ModelInfoClient->GetModelName(pEntity->GetModel())))
			{
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_agility.mdl"): szName = "Agility"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_crit.mdl"): szName = "Revenge"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_defense.mdl"): szName = "Resistance"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_haste.mdl"): szName = "Haste"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_king.mdl"): szName = "King"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_knockout.mdl"): szName = "Knockout"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_plague.mdl"): szName = "Plague"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_precision.mdl"): szName = "Precision"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_reflect.mdl"): szName = "Reflect"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_regen.mdl"): szName = "Regeneration"; break;
				//case FNV1A::Hash32Const("models/pickups/pickup_powerup_resistance.mdl"): szName = "11"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_strength.mdl"): szName = "Strength"; break;
				//case FNV1A::Hash32Const("models/pickups/pickup_powerup_strength_arm.mdl"): szName = "13"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_supernova.mdl"): szName = "Supernova"; break;
				//case FNV1A::Hash32Const("models/pickups/pickup_powerup_thorns.mdl"): szName = "15"; break;
				//case FNV1A::Hash32Const("models/pickups/pickup_powerup_uber.mdl"): szName = "16"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_vampire.mdl"): szName = "Vampire";
			}
			tCache.m_vText.emplace_back(ESPTextEnum::Top, szName, Vars::Colors::Powerup.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Bombs)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_BOMBS))
		{
			WorldCache& tCache = m_mWorldCache[pEntity];

			tCache.m_vText.emplace_back(ESPTextEnum::Top, pEntity->GetClassID() == ETFClassID::CTFPumpkinBomb ? "Pumpkin Bomb" : "Bomb", Vars::Colors::Halloween.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Spellbook)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PICKUPS_SPELLBOOK))
		{
			WorldCache& tCache = m_mWorldCache[pEntity];

			tCache.m_vText.emplace_back(ESPTextEnum::Top, "Spellbook", Vars::Colors::Halloween.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Gargoyle)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_GARGOYLE))
		{
			WorldCache& tCache = m_mWorldCache[pEntity];

			tCache.m_vText.emplace_back(ESPTextEnum::Top, "Gargoyle", Vars::Colors::Halloween.Value, Vars::Menu::Theme::Background.Value);
		}
	}
}

void CESP::Draw()
{
	if (!Vars::ESP::Draw.Value || !I::EngineClient->IsInGame())
		return;

	DrawWorld();
	DrawBuildings();
	DrawPlayers();
}

void CESP::DrawPlayers()
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Players) || !Vars::ESP::Player.Value)
		return;

	const auto& fFont = H::Fonts.GetFont(FONT_ESP);
	const int nTall = fFont.m_nTall + H::Draw.Scale(2);
	for (auto& [pEntity, tCache] : m_mPlayerCache)
	{
		float x, y, w, h;
		if (!GetDrawBounds(pEntity, x, y, w, h))
			continue;

		int l = x - H::Draw.Scale(6), r = x + w + H::Draw.Scale(6), m = x + w / 2;
		int t = y - H::Draw.Scale(5), b = y + h + H::Draw.Scale(5);
		int lOffset = 0, rOffset = 0, bOffset = 0, tOffset = 0;

		I::MatSystemSurface->DrawSetAlphaMultiplier(tCache.m_flAlpha);
		
		if (tCache.m_bBox)
			H::Draw.LineRectOutline(x, y, w, h, tCache.m_tColor, { 0, 0, 0, 255 });

		if (tCache.m_bBones)
		{
			auto pPlayer = pEntity->As<CTFPlayer>();
			matrix3x4 aBones[MAXSTUDIOBONES];
			if (pPlayer->SetupBones(aBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime))
			{
				switch (H::Entities.GetModel(pPlayer->entindex()))
				{
				case FNV1A::Hash32Const("models/vsh/player/saxton_hale.mdl"):
					DrawBones(pPlayer, aBones, { HITBOX_SAXTON_HEAD, HITBOX_SAXTON_CHEST, HITBOX_SAXTON_PELVIS }, tCache.m_tColor);
					DrawBones(pPlayer, aBones, { HITBOX_SAXTON_CHEST, HITBOX_SAXTON_LEFT_UPPER_ARM, HITBOX_SAXTON_LEFT_FOREARM, HITBOX_SAXTON_LEFT_HAND }, tCache.m_tColor);
					DrawBones(pPlayer, aBones, { HITBOX_SAXTON_CHEST, HITBOX_SAXTON_RIGHT_UPPER_ARM, HITBOX_SAXTON_RIGHT_FOREARM, HITBOX_SAXTON_RIGHT_HAND }, tCache.m_tColor);
					DrawBones(pPlayer, aBones, { HITBOX_SAXTON_PELVIS, HITBOX_SAXTON_LEFT_THIGH, HITBOX_SAXTON_LEFT_CALF, HITBOX_SAXTON_LEFT_FOOT }, tCache.m_tColor);
					DrawBones(pPlayer, aBones, { HITBOX_SAXTON_PELVIS, HITBOX_SAXTON_RIGHT_THIGH, HITBOX_SAXTON_RIGHT_CALF, HITBOX_SAXTON_RIGHT_FOOT }, tCache.m_tColor);
					break;
				default:
					DrawBones(pPlayer, aBones, { HITBOX_HEAD, HITBOX_CHEST, HITBOX_PELVIS }, tCache.m_tColor);
					DrawBones(pPlayer, aBones, { HITBOX_CHEST, HITBOX_LEFT_UPPER_ARM, HITBOX_LEFT_FOREARM, HITBOX_LEFT_HAND }, tCache.m_tColor);
					DrawBones(pPlayer, aBones, { HITBOX_CHEST, HITBOX_RIGHT_UPPER_ARM, HITBOX_RIGHT_FOREARM, HITBOX_RIGHT_HAND }, tCache.m_tColor);
					DrawBones(pPlayer, aBones, { HITBOX_PELVIS, HITBOX_LEFT_THIGH, HITBOX_LEFT_CALF, HITBOX_LEFT_FOOT }, tCache.m_tColor);
					DrawBones(pPlayer, aBones, { HITBOX_PELVIS, HITBOX_RIGHT_THIGH, HITBOX_RIGHT_CALF, HITBOX_RIGHT_FOOT }, tCache.m_tColor);
				}
			}
		}

		if (tCache.m_bHealthBar)
		{
			Color_t bgColor = { 0, 0, 0, 180 };
			H::Draw.FillRect(x - H::Draw.Scale(6), y, H::Draw.Scale(2, Scale_Round), h, bgColor);

			if (tCache.m_flHealth > 1.f)
			{
				Color_t cColor = Vars::Colors::IndicatorGood.Value;
				H::Draw.FillRectPercent(x - H::Draw.Scale(6), y, H::Draw.Scale(2, Scale_Round), h, 1.f, cColor, 
					Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::HealthBarBorder ? Color_t(0, 0, 0, 255) : Color_t(0, 0, 0, 0), ALIGN_BOTTOM, true);

				cColor = Vars::Colors::IndicatorMisc.Value;
				H::Draw.FillRectPercent(x - H::Draw.Scale(6), y, H::Draw.Scale(2, Scale_Round), h, tCache.m_flHealth - 1.f, cColor, 
					Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::HealthBarBorder ? Color_t(0, 0, 0, 255) : Color_t(0, 0, 0, 0), ALIGN_BOTTOM, true);
			}
			else
			{
				Color_t cColor = Vars::Colors::IndicatorBad.Value.Lerp(Vars::Colors::IndicatorGood.Value, tCache.m_flHealth);
				H::Draw.FillRectPercent(x - H::Draw.Scale(6), y, H::Draw.Scale(2, Scale_Round), h, tCache.m_flHealth, cColor, 
					Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::HealthBarBorder ? Color_t(0, 0, 0, 255) : Color_t(0, 0, 0, 0), ALIGN_BOTTOM, true);
			}
			lOffset += H::Draw.Scale(6);
		}

		if (tCache.m_bUberBar)
		{
			H::Draw.FillRectPercent(x, y + h + H::Draw.Scale(4), w, H::Draw.Scale(2, Scale_Round), tCache.m_flUber, Vars::Colors::IndicatorMisc.Value);
			bOffset += H::Draw.Scale(6);
		}

		int iVerticalOffset = H::Draw.Scale(3, Scale_Floor) - 1;
		bool isFirstElement = true; // Flag to track the first element
		for (auto& [iMode, sText, tColor, tOutline] : tCache.m_vText)
		{
			switch (iMode)
			{
			case ESPTextEnum::Top:
				if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::NameBackground && 
				    Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Name && 
				    isFirstElement)
				{
					Color_t backgroundOutline = tOutline;
					backgroundOutline.a = Vars::ESP::BackgroundOpacity.Value;
					
					H::Draw.StringWithBackground(fFont, m, t - tOffset, tColor, backgroundOutline, ALIGN_BOTTOM, sText.c_str());
				}
				else
					H::Draw.StringOutlined(fFont, m, t - tOffset, tColor, tOutline, ALIGN_BOTTOM, sText.c_str());
				tOffset += nTall;
				break;
			case ESPTextEnum::Bottom:
				H::Draw.StringOutlined(fFont, m, b + bOffset, tColor, tOutline, ALIGN_TOP, sText.c_str());
				bOffset += nTall;
				break;
			case ESPTextEnum::Right:
				H::Draw.StringOutlined(fFont, r, t + iVerticalOffset + rOffset, tColor, tOutline, ALIGN_TOPLEFT, sText.c_str());
				rOffset += nTall;
				break;
			case ESPTextEnum::Health:
				H::Draw.StringOutlined(fFont, l - lOffset, t + iVerticalOffset + h - h * std::min(tCache.m_flHealth, 1.f), tColor, tOutline, ALIGN_TOPRIGHT, sText.c_str());
				break;
			case ESPTextEnum::Uber:
				H::Draw.StringOutlined(fFont, r, y + h, tColor, tOutline, ALIGN_TOPLEFT, sText.c_str());
			}
			isFirstElement = false; // Set to false after processing the first element
		}

		if (tCache.m_iClassIcon)
		{
			int size = H::Draw.Scale(18, Scale_Round);
			H::Draw.Texture(m, t - tOffset, size, size, tCache.m_iClassIcon - 1, ALIGN_BOTTOM);
		}

		if (tCache.m_pWeaponIcon)
		{
			float flW = tCache.m_pWeaponIcon->Width(), flH = tCache.m_pWeaponIcon->Height();
			float flScale = H::Draw.Scale(std::min((w + 40) / 2.f, 80.f) / std::max(flW, flH * 2));
			H::Draw.DrawHudTexture(m - flW / 2.f * flScale, b + bOffset, flScale, tCache.m_pWeaponIcon, Vars::Menu::Theme::Active.Value);
		}

		// if (tCache.m_bYawArrows || (pEntity->entindex() == I::EngineClient->GetLocalPlayer() && (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::YawArrows)))
		// {
		// 	auto pPlayer = pEntity->As<CTFPlayer>();
		// 	if (!pPlayer || !pPlayer->IsAlive())
		// 		continue;
				
		// 	const int iEntIndex = pPlayer->entindex();
			
		// 	if (iEntIndex == I::EngineClient->GetLocalPlayer())
		// 	{
		// 		if (Vars::Visuals::Thirdperson::Enabled.Value)
		// 		{
		// 			const float flArrowLength = 40.0f; // Real-world unit length
		// 			const float flArrowThickness = 2.0f; // Thickness of arrow in world units
		// 			const float flHeadSize = 15.0f;     // Arrow head size in world units
		// 			const float flHeightOffset = 5.0f;   // Height from ground

		// 			Vec3 vOrigin = pPlayer->GetAbsOrigin();
		// 			vOrigin.z += flHeightOffset; // Offset from ground

		// 			Vec3 vScreenOrigin;
		// 			if (SDK::W2S(vOrigin, vScreenOrigin))
		// 			{
		// 				Color_t realColor = { 0, 255, 0, 255 }; // Bright green for real yaw
		// 				Color_t fakeColor = { 255, 0, 0, 255 }; // Bright red for fake yaw
		// 				Color_t realOutline = { 0, 0, 0, 255 }; // Black outline for real yaw
		// 				Color_t fakeOutline = { 0, 0, 0, 255 }; // Black outline for fake yaw

		// 				realColor.a = 210; // Slightly transparent to avoid visual clutter
		// 				fakeColor.a = 210;
		// 				realOutline.a = 255;
		// 				fakeOutline.a = 255;
		// 				float flRealYaw = 0.0f;
		// 				float flFakeYaw = 0.0f;

		// 				if (G::AntiAim)
		// 				{
		// 					flRealYaw = F::AntiAim.vRealAngles.y;
		// 					flFakeYaw = F::AntiAim.vFakeAngles.y;
		// 				}
		// 				else
		// 				{
		// 					flRealYaw = I::EngineClient->GetViewAngles().y;
		// 					flFakeYaw = flRealYaw + 120.0f; // Add an offset to demonstrate
		// 				}

		// 				// fuck you (NaN, W2S crashes fix)
		// 				if (!std::isfinite(flRealYaw) || !std::isfinite(flFakeYaw))
		// 					continue;

		// 				int iArrowStyle = Vars::ESP::YawArrowsStyle.Value;
						
		// 				try {
		// 					DrawYawArrow(vOrigin, flRealYaw, flArrowLength, flHeadSize, flHeightOffset, 
		// 								realColor, realOutline, vScreenOrigin, static_cast<Vars::ESP::YawArrowsStyleEnum::YawArrowsStyleEnum>(iArrowStyle), true);
							
		// 					DrawYawArrow(vOrigin, flFakeYaw, flArrowLength, flHeadSize, flHeightOffset, 
		// 								fakeColor, fakeOutline, vScreenOrigin, static_cast<Vars::ESP::YawArrowsStyleEnum::YawArrowsStyleEnum>(iArrowStyle), false);
		// 				} catch (...) {
		// 					// Silently explode if drawing causes an exception
		// 				}
		// 			}
		// 		}
		// 	}
		// }
	}

	I::MatSystemSurface->DrawSetAlphaMultiplier(1.f);
}

void CESP::DrawBuildings()
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Buildings) || !Vars::ESP::Building.Value)
		return;

	const auto& fFont = H::Fonts.GetFont(FONT_ESP);
	const int nTall = fFont.m_nTall + H::Draw.Scale(2);
	for (auto& [pEntity, tCache] : m_mBuildingCache)
	{
		float x, y, w, h;
		if (!GetDrawBounds(pEntity, x, y, w, h))
			continue;

		int l = x - H::Draw.Scale(6), r = x + w + H::Draw.Scale(6), m = x + w / 2;
		int t = y - H::Draw.Scale(5), b = y + h + H::Draw.Scale(5);
		int lOffset = 0, rOffset = 0, bOffset = 0, tOffset = 0;

		I::MatSystemSurface->DrawSetAlphaMultiplier(tCache.m_flAlpha);

		if (tCache.m_bBox)
			H::Draw.LineRectOutline(x, y, w, h, tCache.m_tColor, { 0, 0, 0, 255 });

		if (tCache.m_bHealthBar)
		{
			Color_t cColor = Vars::Colors::IndicatorBad.Value.Lerp(Vars::Colors::IndicatorGood.Value, tCache.m_flHealth);
			H::Draw.FillRectPercent(x - H::Draw.Scale(6), y, H::Draw.Scale(2, Scale_Round), h, tCache.m_flHealth, cColor, { 0, 0, 0, 255 }, ALIGN_BOTTOM, true);
			lOffset += H::Draw.Scale(6);
		}

		int iVerticalOffset = H::Draw.Scale(3, Scale_Floor) - 1;
		for (auto& [iMode, sText, tColor, tOutline] : tCache.m_vText)
		{
			switch (iMode)
			{
			case ESPTextEnum::Top:
				H::Draw.StringOutlined(fFont, m, t - tOffset, tColor, tOutline, ALIGN_BOTTOM, sText.c_str());
				tOffset += nTall;
				break;
			case ESPTextEnum::Bottom:
				H::Draw.StringOutlined(fFont, m, b + bOffset, tColor, tOutline, ALIGN_TOP, sText.c_str());
				bOffset += nTall;
				break;
			case ESPTextEnum::Right:
				H::Draw.StringOutlined(fFont, r, t + iVerticalOffset + rOffset, tColor, tOutline, ALIGN_TOPLEFT, sText.c_str());
				rOffset += nTall;
				break;
			case ESPTextEnum::Health:
				H::Draw.StringOutlined(fFont, l - lOffset, t + iVerticalOffset + h - h * std::min(tCache.m_flHealth, 1.f), tColor, tOutline, ALIGN_TOPRIGHT, sText.c_str());
				break;
			}
		}
	}

	I::MatSystemSurface->DrawSetAlphaMultiplier(1.f);
}

void CESP::DrawWorld()
{
	const auto& fFont = H::Fonts.GetFont(FONT_ESP);
	const int nTall = fFont.m_nTall + H::Draw.Scale(2);
	for (auto& [pEntity, tCache] : m_mWorldCache)
	{
		float x, y, w, h;
		if (!GetDrawBounds(pEntity, x, y, w, h))
			continue;

		int l = x - H::Draw.Scale(6), r = x + w + H::Draw.Scale(6), m = x + w / 2;
		int t = y - H::Draw.Scale(5), b = y + h + H::Draw.Scale(5);
		int lOffset = 0, rOffset = 0, bOffset = 0, tOffset = 0;

		I::MatSystemSurface->DrawSetAlphaMultiplier(tCache.m_flAlpha);

		if (tCache.m_bBox)
			H::Draw.LineRectOutline(x, y, w, h, tCache.m_tColor, { 0, 0, 0, 255 });

		int iVerticalOffset = H::Draw.Scale(3, Scale_Floor) - 1;
		for (auto& [iMode, sText, tColor, tOutline] : tCache.m_vText)
		{
			switch (iMode)
			{
			case ESPTextEnum::Top:
				H::Draw.StringOutlined(fFont, m, t - tOffset, tColor, tOutline, ALIGN_BOTTOM, sText.c_str());
				tOffset += nTall;
				break;
			case ESPTextEnum::Bottom:
				H::Draw.StringOutlined(fFont, m, b + bOffset, tColor, tOutline, ALIGN_TOP, sText.c_str());
				bOffset += nTall;
				break;
			case ESPTextEnum::Right:
				H::Draw.StringOutlined(fFont, r, t + iVerticalOffset + rOffset, tColor, tOutline, ALIGN_TOPLEFT, sText.c_str());
				rOffset += nTall;
			}
		}
	}

	I::MatSystemSurface->DrawSetAlphaMultiplier(1.f);
}

Color_t CESP::GetColor(CTFPlayer* pLocal, CBaseEntity* pEntity)
{
	if (pEntity->entindex() == I::EngineClient->GetLocalPlayer())
		return Vars::Colors::Local.Value;
	if (pEntity->entindex() == G::AimTarget.m_iEntIndex)
		return Vars::Colors::Target.Value;
	return H::Color.GetTeamColor(pLocal->m_iTeamNum(), pEntity->m_iTeamNum(), Vars::Colors::Relative.Value);
}

bool CESP::GetDrawBounds(CBaseEntity* pEntity, float& x, float& y, float& w, float& h)
{
	if (!pEntity || pEntity->IsDormant())
		return false;
	Vec3 vOrigin = pEntity->GetAbsOrigin();
	matrix3x4 mTransform = { { 1, 0, 0, vOrigin.x }, { 0, 1, 0, vOrigin.y }, { 0, 0, 1, vOrigin.z } };
	//if (pEntity->entindex() == I::EngineClient->GetLocalPlayer())
		Math::AngleMatrix({ 0.f, I::EngineClient->GetViewAngles().y, 0.f }, mTransform, false);

	float flLeft, flRight, flTop, flBottom;
	if (!SDK::IsOnScreen(pEntity, mTransform, &flLeft, &flRight, &flTop, &flBottom))
		return false;

	x = flLeft;
	y = flBottom;
	w = flRight - flLeft;
	h = flTop - flBottom;

	switch (pEntity->GetClassID())
	{
	case ETFClassID::CTFPlayer:
	case ETFClassID::CObjectSentrygun:
	case ETFClassID::CObjectDispenser:
	case ETFClassID::CObjectTeleporter:
		x += w * 0.125f;
		w *= 0.75f;
	}

	return !(x > H::Draw.m_nScreenW || x + w < 0 || y > H::Draw.m_nScreenH || y + h < 0);
}

void CESP::DrawBones(CTFPlayer* pPlayer, matrix3x4* aBones, std::vector<int> vecBones, Color_t clr)
{
	for (size_t n = 1; n < vecBones.size(); n++)
	{
		auto vBone1 = pPlayer->GetHitboxCenter(aBones, vecBones[n]);
		auto vBone2 = pPlayer->GetHitboxCenter(aBones, vecBones[n - 1]);

		Vec3 vScreenBone, vScreenParent;
		if (SDK::W2S(vBone1, vScreenBone) && SDK::W2S(vBone2, vScreenParent))
			H::Draw.Line(vScreenBone.x, vScreenBone.y, vScreenParent.x, vScreenParent.y, clr);
	}
}