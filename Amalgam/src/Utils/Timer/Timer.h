#pragma once
#include <chrono>

/*
 *	Credits to cathook (nullifiedcat)
 */

// Fast timer for high-frequency checks
class FastTimer
{
private:
	float m_flLast = 0.f;
	float m_flInterval = 0.f;
	bool m_bCached = false;

public:
	FastTimer() = default;
	
	bool CheckCached(float flS);
	bool Check(float flS) const;
	void Update();
	bool Run(float flS);
};

class Timer
{
private:
	float m_flLast = 0.f;

public:
	Timer();
	bool Check(float flS) const;
	bool Run(float flS);
	inline void Update();

	inline void operator-=(float flS)
    {
		m_flLast -= flS;
    }

	inline void operator+=(float flS)
    {
        m_flLast += flS;
    }
};

// Optimized timer manager for multiple timers
class TimerManager
{
private:
	static constexpr size_t MAX_TIMERS = 32;
	struct TimerEntry
	{
		float lastTime = 0.f;
		float interval = 0.f;
		bool active = false;
	};
	
	TimerEntry m_timers[MAX_TIMERS];
	size_t m_nextId = 0;
	
public:
	size_t CreateTimer(float interval);
	bool CheckTimer(size_t id);
	void ResetTimer(size_t id);
	void SetInterval(size_t id, float interval);
};