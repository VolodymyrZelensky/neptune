#include "Timer.h"

#include "../../SDK/SDK.h"

Timer::Timer()
{
	m_flLast = SDK::PlatFloatTime();
}

bool Timer::Check(float flS) const
{
	float flCurrentTime = SDK::PlatFloatTime();
	return flCurrentTime - m_flLast >= flS;
}

bool Timer::Run(float flS)
{
	if (Check(flS))
	{
		Update();
		return true;
	}
	return false;
}

inline void Timer::Update()
{
	m_flLast = SDK::PlatFloatTime();
}

bool FastTimer::CheckCached(float flS)
{
	if (m_bCached && m_flInterval == flS)
	{
		float flCurrent = SDK::PlatFloatTime();
		return flCurrent - m_flLast >= flS;
	}
	
	m_flInterval = flS;
	m_bCached = true;
	return Check(flS);
}

bool FastTimer::Check(float flS) const
{
	float flCurrentTime = SDK::PlatFloatTime();
	return flCurrentTime - m_flLast >= flS;
}

void FastTimer::Update()
{
	m_flLast = SDK::PlatFloatTime();
}

bool FastTimer::Run(float flS)
{
	if (CheckCached(flS))
	{
		Update();
		return true;
	}
	return false;
}

size_t TimerManager::CreateTimer(float interval)
{
	size_t id = m_nextId++;
	if (id < MAX_TIMERS)
	{
		m_timers[id].interval = interval;
		m_timers[id].lastTime = SDK::PlatFloatTime();
		m_timers[id].active = true;
	}
	return id;
}

bool TimerManager::CheckTimer(size_t id)
{
	if (id >= MAX_TIMERS || !m_timers[id].active)
		return false;
		
	float current = SDK::PlatFloatTime();
	if (current - m_timers[id].lastTime >= m_timers[id].interval)
	{
		m_timers[id].lastTime = current;
		return true;
	}
	return false;
}

void TimerManager::ResetTimer(size_t id)
{
	if (id < MAX_TIMERS)
		m_timers[id].lastTime = SDK::PlatFloatTime();
}

void TimerManager::SetInterval(size_t id, float interval)
{
	if (id < MAX_TIMERS)
		m_timers[id].interval = interval;
}