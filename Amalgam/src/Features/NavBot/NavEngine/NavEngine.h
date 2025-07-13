#pragma once
#include "FileReader/CNavFile.h"
#include "MicroPather/micropather.h"
#include <boost/container_hash/hash.hpp>
#include "NavCommon.h"

class NavPoints
{
public:
    Vector current;
    Vector center;
    // The above but on the "next" vector, used for height checks.
    Vector center_next;
    Vector next;
    NavPoints(Vector A, Vector B, Vector C, Vector D) : current(A), center(B), center_next(C), next(D){};
};

class CNavParser
{
public:
	std::optional<Vector> GetDormantOrigin(int iIndex);
	bool IsVectorVisibleNavigation(Vector from, Vector to, unsigned int mask = MASK_SHOT_HULL);
	static bool IsSetupTime();
	void DoSlowAim(Vector& input_angle, float speed, Vector viewangles);
	Vector GetForwardVector(Vector origin, Vector viewangles, float distance);

	struct Crumb
	{
		CNavArea* navarea;
		Vector vec;
	};

	enum class NavState
	{
		Unavailable = 0,
		Active
	};

	struct CachedConnection
	{
		int expire_tick;
		int vischeck_state;
	};

	struct CachedStucktime
	{
		int expire_tick;
		int time_stuck;
	};

	struct ConnectionInfo
	{
		enum State
		{
			// Tried using this connection, failed for some reason
			STUCK
		};
		int expire_tick;
		State state;
	};

	bool IsPlayerPassableNavigation(Vector origin, Vector target, unsigned int mask = MASK_PLAYERSOLID);

	Vector handleDropdown(Vector current_pos, Vector next_pos);
	NavPoints determinePoints(CNavArea* current, CNavArea* next);
	class Map : public micropather::Graph
	{
	public:
		CNavFile navfile;
		NavState state;
		micropather::MicroPather pather{ this, 3000, 6, true };
		std::string mapname;
		std::unordered_map<std::pair<CNavArea*, CNavArea*>, CachedConnection, boost::hash<std::pair<CNavArea*, CNavArea*>>> vischeck_cache;
		std::unordered_map<std::pair<CNavArea*, CNavArea*>, CachedStucktime, boost::hash<std::pair<CNavArea*, CNavArea*>>> connection_stuck_time;
		// This is a pure blacklist that does not get cleared and is for free usage internally and externally, e.g. blacklisting where enemies are standing
		// This blacklist only gets cleared on map change, and can be used time independently.
		// the enum is the Blacklist reason, so you can easily edit it
		std::unordered_map<CNavArea*, BlacklistReason> free_blacklist;
		// When the local player stands on one of the nav squares the free blacklist should NOT run
		bool free_blacklist_blocked = false;

		explicit Map(const char* mapname) : navfile(mapname), mapname(mapname) { state = navfile.m_isOK ? NavState::Active : NavState::Unavailable; }

		float LeastCostEstimate(void* start, void* end) override { return reinterpret_cast<CNavArea*>(start)->m_center.DistTo(reinterpret_cast<CNavArea*>(end)->m_center); }

		void AdjacentCost(void* main, std::vector<micropather::StateCost>* adjacent) override;

		// Function for getting the closest area to the player, aka "LocalNav"
		CNavArea* findClosestNavSquare(const Vector& vec);

		std::vector<void*> findPath(CNavArea* local, CNavArea* dest)
		{
			if (state != NavState::Active)
				return {};

			std::vector<void*> path;
			float cost;

			//auto begin_pathing = std::chrono::high_resolution_clock::now( );
			int result = pather.Solve(reinterpret_cast<void*>(local), reinterpret_cast<void*>(dest), &path, &cost);
			//auto timetaken = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now( ) - begin_pathing ).count( );

			if (result == micropather::MicroPather::START_END_SAME)
				return { reinterpret_cast<void*>(local) };

			return path;
		}

		void updateIgnores();

		void UpdateRespawnRooms();

		void Reset()
		{
			vischeck_cache.clear();
			connection_stuck_time.clear();
			free_blacklist.clear();
			pather.Reset();
		}

		// Unnecessary thing that is sadly necessary
		void PrintStateInfo(void*) {}
	};
};

ADD_FEATURE(CNavParser, NavParser);

class CNavEngine
{
public:
	std::unique_ptr<CNavParser::Map> map;
	CNavParser::Crumb last_crumb;
	std::vector<CNavParser::Crumb> crumbs;
	int current_priority = 0;
	bool current_navtolocal = false;
	bool repath_on_fail = false;
	Vector last_destination;
	bool m_bPathing = false;

	// Is the Nav engine ready to run?
	bool isReady(bool bRoundCheck = false);
	bool IsNavMeshLoaded() { return map->state == CNavParser::NavState::Active; }

	// Are we currently pathing?
	bool isPathing() { return !crumbs.empty(); }

	CNavFile* getNavFile() { return &map->navfile; }

	// Get closest nav square to target vector
	CNavArea* findClosestNavSquare(const Vector origin) { return map->findClosestNavSquare(origin); }

	// Get the path nodes
	std::vector<CNavParser::Crumb>* getCrumbs() { return &crumbs; }

	bool navTo(const Vector& destination, int priority = patrol, bool should_repath = true, bool nav_to_local = true, bool is_repath = true);
	// Use when something unexpected happens, e.g. vischeck fails
	void abandonPath();

	// Use to cancel pathing completely
	void cancelPath();

	// Return the whole thing
	std::unordered_map<CNavArea*, BlacklistReason>* getFreeBlacklist() { return &map->free_blacklist; }

	// Return a specific category, we keep the same indexes to provide single element erasing
	std::unordered_map<CNavArea*, BlacklistReason> getFreeBlacklist(BlacklistReason reason)
	{
		std::unordered_map<CNavArea*, BlacklistReason> return_map;
		for (const auto& entry : map->free_blacklist)
		{
			// Category matches
			if (entry.second.value == reason.value)
				return_map[entry.first] = entry.second;
		}
		return return_map;
	}

	// Clear whole blacklist
	void clearFreeBlacklist() const { map->free_blacklist.clear(); }
	// Clear by category
	void clearFreeBlacklist(BlacklistReason reason) { std::erase_if(map->free_blacklist, [&reason](const auto& entry) { return entry.second.value == reason.value; }); }

	bool isInactive(float flTime);

	CNavParser::Crumb current_crumb;
	void followCrumbs(CTFPlayer* pLocal, CUserCmd* pCmd);
	bool findNearestNavNode(CTFPlayer* pLocal);

	void vischeckPath();
	void checkBlacklist();
	void updateStuckTime();
	void Run(CUserCmd* pCmd);
	void Reset(bool bForced = false);
};

ADD_FEATURE(CNavEngine, NavEngine);