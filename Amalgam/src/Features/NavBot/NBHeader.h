#pragma once
#include "NavEngine/NavEngine.h"
#include <optional>
#include <vector>
#include <unordered_map>
#include <format>
#include "NavEngine/NavCommon.h"

enum slots
{
	primary = 1,
	secondary,
	melee,
	pda1,
	pda2
};

enum building
{
    dispenser = 0,
    sentry    = 2
};

struct ClosestEnemy_t
{	
	int m_iEntIdx;
	float m_flDist;
};

class CNavBot
{
public:
	std::vector<Vector> m_vSlightDangerDrawlistNormal;
	std::vector<Vector> m_vSlightDangerDrawlistDormant;
	bool m_bDefending = false;
	std::wstring m_sFollowTargetName{};
	std::wstring m_sEngineerTask{};
	int m_iStayNearTargetIdx = -1;
	bool m_bWaitingForMetal = false;
	
	ClosestEnemy_t GetNearestPlayerDistance(CTFPlayer* pLocal, CTFWeaponBase* pWeapon) const;

private:
	static constexpr float HEALTH_SEARCH_THRESHOLD_LOW = 0.64f;
	static constexpr float HEALTH_SEARCH_THRESHOLD_HIGH = 0.80f;
	static constexpr float AMMO_SEARCH_THRESHOLD = 0.6f;
	static constexpr float AMMO_CRITICAL_THRESHOLD = 0.33f;
	static constexpr float FORMATION_DISTANCE_DEFAULT = 120.0f;
	static constexpr float FORMATION_DISTANCE_MAX = 300.0f;
	static constexpr float SENTRY_SNIPE_DISTANCE = 1100.0f;
	static constexpr float MELEE_ATTACK_DISTANCE = 100.0f;
	static constexpr float BUILDING_PROXIMITY_DISTANCE = 200.0f;
	static constexpr int MAX_BUILD_ATTEMPTS = 15;
	static constexpr int MAX_STAYNEAR_CHECKS_RANGE = 3;
	static constexpr int MAX_STAYNEAR_CHECKS_CLOSE = 2;
	static constexpr float BUILD_SPOT_EQUALITY_EPSILON = 200.0f;

	// Controls the bot parameters like distance from enemy
	struct bot_class_config
	{
		float m_flMinFullDanger;
		float m_flMinSlightDanger;
		float m_flMax;
		bool m_bPreferFar;
	};

	const bot_class_config CONFIG_SHORT_RANGE = { 70.0f, 200.0f, 600.0f, false };
	const bot_class_config CONFIG_MID_RANGE = { 100.0f, 250.0f, 3000.0f, true };
	const bot_class_config CONFIG_LONG_RANGE = { 150.0f, 250.0f, 4000.0f, true };
	const bot_class_config CONFIG_ENGINEER = { 100.0f, 250.0f, 3000.0f, false };
	const bot_class_config CONFIG_GUNSLINGER_ENGINEER = { 25.0f, 150.0f, 2000.0f, false };
	bot_class_config m_tSelectedConfig;

	std::vector<Vector> m_vSniperSpots;
	std::vector<Vector> m_vBuildingSpots;
	std::optional<Vector> vCurrentBuildingSpot;
	int m_iMySentryIdx = -1;
	int m_iMyDispenserIdx = -1;
	int m_iBuildAttempts = 0;
	int m_iCurrentSlot = primary;
	slots m_eLastReloadSlot = slots();

	std::vector<std::pair<uint32_t, Vector>> m_vLocalBotPositions;
	int m_iPositionInFormation = -1;
	float m_flFormationDistance = FORMATION_DISTANCE_DEFAULT; 
	Timer m_tUpdateFormationTimer;

	// Overwrite to return true for payload carts as an example
	bool m_bOverwriteCapture = false;

	// Blacklist management
	Timer m_tBlacklistCleanupTimer;
	std::unordered_map<CNavArea*, float> m_mAreaDangerScore;
	std::unordered_map<CNavArea*, float> m_mAreaBlacklistExpiry;

	// List of building spots that have already failed (placement/pathing) so we don't retry them immediately.
	std::vector<Vector> m_vFailedBuildingSpots;

private:
	bool ShouldAssist(CTFPlayer* pLocal, int iTargetIdx) const;
	int ShouldTarget(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int iPlayerIdx) const;
	int ShouldTargetBuilding(CTFPlayer* pLocal, int iEntIdx) const;

	std::vector<CBaseEntity*> GetEntities(CTFPlayer* pLocal, bool bHealth = false) const;
	std::vector<CObjectDispenser*> GetDispensers(CTFPlayer* pLocal) const;

	bool ShouldSearchHealth(CTFPlayer* pLocal, bool bLowPrio = false) const;
	bool ShouldSearchAmmo(CTFPlayer* pLocal) const;
	bool GetHealth(CUserCmd* pCmd, CTFPlayer* pLocal, bool bLowPrio = false);
	bool GetAmmo(CUserCmd* pCmd, CTFPlayer* pLocal, bool bForce = false);

	void UpdateEnemyBlacklist(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int iSlot);
	void RefreshSniperSpots();
	void RefreshBuildingSpots(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, ClosestEnemy_t tClosestEnemy, bool bForce = false);
	void RefreshLocalBuildings(CTFPlayer* pLocal);
	
	void CleanupExpiredBlacklist();
	void FastCleanupInvalidBlacklists(CTFPlayer* pLocal);
	bool IsAreaInDanger(CNavArea* pArea, CTFPlayer* pLocal, float& flDangerScore);
	void UpdateAreaDangerScores(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int iSlot);
	bool ShouldBlacklistArea(CNavArea* pArea, float flDangerScore, bool bIsTemporary = true);
	void AddTemporaryBlacklist(CNavArea* pArea, BlacklistReason_enum reason, float flDuration = 8.0f);

	bool IsAreaValidForStayNear(Vector vEntOrigin, CNavArea* pArea, bool bFixLocalZ = true);
	bool IsAreaValidForSnipe(Vector vEntOrigin, Vector vAreaOrigin, bool bFixSentryZ = true);
	int IsStayNearTargetValid(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, int iEntIndex);
	int IsSnipeTargetValid(CTFPlayer* pLocal, int iBuildingIdx);

	std::optional<std::pair<CNavArea*, int>> FindClosestHidingSpot(CNavArea* pArea, std::optional<Vector> vVischeckPoint, int iRecursionCount, int iRecursionIndex = 0);
	bool StayNearTarget(int iEntIndex);
	bool StayNear(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);

	bool RunSafeReload(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);
	bool RunReload(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);
	bool TryToSnipe(CBaseObject* pBuilding);
	bool SnipeSentries(CTFPlayer* pLocal);
	bool MeleeAttack(CUserCmd* pCmd, CTFPlayer* pLocal, int iSlot, ClosestEnemy_t tClosestEnemy);

	bool IsEngieMode(CTFPlayer* pLocal);
	bool BuildingNeedsToBeSmacked(CBaseObject* pBuilding);
	bool BlacklistedFromBuilding(CNavArea* pArea);
	bool NavToSentrySpot();
	bool BuildBuilding(CUserCmd* pCmd, CTFPlayer* pLocal, ClosestEnemy_t tClosestEnemy, int building);
	bool SmackBuilding(CUserCmd* pCmd, CTFPlayer* pLocal, CBaseObject* pBuilding);
	bool EngineerLogic(CUserCmd* pCmd, CTFPlayer* pLocal, ClosestEnemy_t tClosestEnemy);

	std::optional<Vector> GetCtfGoal(CTFPlayer* pLocal, int iOurTeam, int iEnemyTeam);
	std::optional<Vector> GetPayloadGoal(Vector vLocalOrigin, int iOurTeam);
	std::optional<Vector> GetControlPointGoal(Vector vLocalOrigin, int iOurTeam);
	std::optional<Vector> GetDoomsdayGoal(CTFPlayer* pLocal, int iOurTeam, int iEnemyTeam);
	bool CaptureObjectives(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);

	void UpdateLocalBotPositions(CTFPlayer* pLocal);
	bool MoveInFormation(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);
	std::optional<Vector> GetFormationOffset(CTFPlayer* pLocal, int positionIndex);
	bool Roam(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);

	bool EscapeDanger(CTFPlayer* pLocal);
	bool EscapeProjectiles(CTFPlayer* pLocal);
	bool EscapeSpawn(CTFPlayer* pLocal);

	slots GetReloadWeaponSlot(CTFPlayer* pLocal, ClosestEnemy_t tClosestEnemy);
	slots GetBestSlot(CTFPlayer* pLocal, slots eActiveSlot, ClosestEnemy_t tClosestEnemy);
	void UpdateSlot(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, ClosestEnemy_t tClosestEnemy);

	void UpdateClassConfig(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);
	void HandleMinigunSpinup(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, const ClosestEnemy_t& tClosestEnemy);
	void HandleDoubletapRecharge(CTFWeaponBase* pWeapon);

	bool IsFailedBuildingSpot(const Vector& spot) const;
	void AddFailedBuildingSpot(const Vector& spot);

public:
	void Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd);
	void Reset();
};

ADD_FEATURE(CNavBot, NavBot);