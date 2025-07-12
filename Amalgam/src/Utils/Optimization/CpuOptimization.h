#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <functional>
#include "../Timer/Timer.h"
#include "../../SDK/SDK.h"

namespace CpuOptimization
{
    // Cache for expensive string operations
    class StringCache
    {
    private:
        static constexpr size_t MAX_CACHE_SIZE = 256;
        std::unordered_map<std::string, std::string> m_cache;
        
    public:
        const std::string& GetCached(const std::string& key, std::function<std::string()> generator)
        {
            auto it = m_cache.find(key);
            if (it != m_cache.end())
                return it->second;
                
            if (m_cache.size() >= MAX_CACHE_SIZE)
                m_cache.clear();
                
            return m_cache[key] = generator();
        }
        
        void Clear() { m_cache.clear(); }
    };
    
    // Frequency-based execution controller
    class FrequencyController
    {
    private:
        struct Task
        {
            std::function<void()> func;
            float interval;
            float lastRun;
            int priority;
        };
        
        std::vector<Task> m_tasks;
        int m_currentFrame = 0;
        
    public:
        void AddTask(std::function<void()> func, float interval, int priority = 0)
        {
            m_tasks.push_back({func, interval, 0.f, priority});
        }
        
        void ProcessFrame()
        {
            float currentTime = SDK::PlatFloatTime();
            m_currentFrame++;
            
            // Process high priority tasks every frame
            for (auto& task : m_tasks)
            {
                if (task.priority > 10)
                {
                    if (currentTime - task.lastRun >= task.interval)
                    {
                        task.func();
                        task.lastRun = currentTime;
                    }
                }
            }
            
            // Process normal tasks with frame-based distribution
            if (m_currentFrame % 2 == 0)
            {
                for (auto& task : m_tasks)
                {
                    if (task.priority <= 10 && currentTime - task.lastRun >= task.interval)
                    {
                        task.func();
                        task.lastRun = currentTime;
                    }
                }
            }
        }
    };
    
    // Adaptive performance controller
    class AdaptivePerformance
    {
    private:
        float m_lastFPS = 60.0f;
        float m_targetFPS = 60.0f;
        int m_frameCounter = 0;
        FastTimer m_fpsTimer;
        
        struct PerformanceLevel
        {
            float minFPS;
            int skipEveryN;
            bool enableExpensiveFeatures;
            float updateFrequencyMultiplier;
        };
        
        std::vector<PerformanceLevel> m_levels = {
            {60.0f, 1, true, 1.0f},    // High performance
            {45.0f, 2, true, 0.8f},    // Medium performance  
            {30.0f, 3, false, 0.6f},   // Low performance
            {15.0f, 4, false, 0.4f}    // Very low performance
        };
        
        int m_currentLevel = 0;
        
    public:
        void UpdateFPS()
        {
            if (m_fpsTimer.Run(0.5f))
            {
                m_lastFPS = I::EngineClient ? (1.0f / I::GlobalVars->frametime) : 60.0f;
                UpdatePerformanceLevel();
            }
        }
        
        void UpdatePerformanceLevel()
        {
            for (size_t i = 0; i < m_levels.size(); ++i)
            {
                if (m_lastFPS >= m_levels[i].minFPS)
                {
                    m_currentLevel = static_cast<int>(i);
                    break;
                }
            }
        }
        
        bool ShouldSkipFrame()
        {
            m_frameCounter++;
            return m_frameCounter % m_levels[m_currentLevel].skipEveryN != 0;
        }
        
        bool CanUseExpensiveFeatures() const
        {
            return m_levels[m_currentLevel].enableExpensiveFeatures;
        }
        
        float GetUpdateFrequencyMultiplier() const
        {
            return m_levels[m_currentLevel].updateFrequencyMultiplier;
        }
        
        int GetPerformanceLevel() const { return m_currentLevel; }
        float GetLastFPS() const { return m_lastFPS; }
    };
    
    // Cached entity operations
    class EntityCache
    {
    private:
        struct CachedEntity
        {
            CBaseEntity* entity;
            float lastUpdate;
            Vec3 position;
            bool alive;
            int team;
        };
        
        std::unordered_map<int, CachedEntity> m_cache;
        FastTimer m_updateTimer;
        
    public:
        void UpdateCache()
        {
            if (!m_updateTimer.Run(0.1f)) // Update every 100ms
                return;
                
            float currentTime = SDK::PlatFloatTime();
            
            for (auto it = m_cache.begin(); it != m_cache.end();)
            {
                auto clientEntity = I::ClientEntityList->GetClientEntity(it->first);
                if (!clientEntity)
                {
                    it = m_cache.erase(it);
                    continue;
                }
                
                CBaseEntity* entity = clientEntity->As<CBaseEntity>();
                it->second.entity = entity;
                it->second.position = entity->GetAbsOrigin();
                it->second.lastUpdate = currentTime;
                
                if (entity->IsPlayer())
                {
                    auto player = entity->As<CTFPlayer>();
                    it->second.alive = player->IsAlive();
                    it->second.team = player->m_iTeamNum();
                }
                
                ++it;
            }
        }
        
        const CachedEntity* GetCachedEntity(int index)
        {
            auto it = m_cache.find(index);
            if (it != m_cache.end())
                return &it->second;
                
            auto clientEntity = I::ClientEntityList->GetClientEntity(index);
            if (!clientEntity)
                return nullptr;
                
            CBaseEntity* entity = clientEntity->As<CBaseEntity>();
            
            CachedEntity& cached = m_cache[index];
            cached.entity = entity;
            cached.position = entity->GetAbsOrigin();
            cached.lastUpdate = SDK::PlatFloatTime();
            
            if (entity->IsPlayer())
            {
                auto player = entity->As<CTFPlayer>();
                cached.alive = player->IsAlive();
                cached.team = player->m_iTeamNum();
            }
            
            return &cached;
        }
        
        void Clear() { m_cache.clear(); }
    };
    
    // Memory pool for frequent allocations
    template<typename T, size_t PoolSize = 1024>
    class MemoryPool
    {
    private:
        alignas(T) char m_pool[PoolSize * sizeof(T)];
        bool m_used[PoolSize] = {false};
        size_t m_nextFree = 0;
        
    public:
        T* Allocate()
        {
            for (size_t i = 0; i < PoolSize; ++i)
            {
                size_t index = (m_nextFree + i) % PoolSize;
                if (!m_used[index])
                {
                    m_used[index] = true;
                    m_nextFree = (index + 1) % PoolSize;
                    return reinterpret_cast<T*>(&m_pool[index * sizeof(T)]);
                }
            }
            return nullptr; // Pool exhausted
        }
        
        void Deallocate(T* ptr)
        {
            if (!ptr) return;
            
            char* charPtr = reinterpret_cast<char*>(ptr);
            if (charPtr >= m_pool && charPtr < m_pool + PoolSize * sizeof(T))
            {
                size_t index = (charPtr - m_pool) / sizeof(T);
                if (index < PoolSize)
                    m_used[index] = false;
            }
        }
        
        void Clear()
        {
            std::fill(m_used, m_used + PoolSize, false);
            m_nextFree = 0;
        }
    };
    
    // Global optimization manager
    class OptimizationManager
    {
    private:
        AdaptivePerformance m_adaptivePerf;
        EntityCache m_entityCache;
        StringCache m_stringCache;
        FrequencyController m_frequencyController;
        
        FastTimer m_updateTimer;
        
    public:
        void Initialize()
        {
            // Setup frequency-controlled tasks
            std::function<void()> entityCacheUpdate = [this]() { m_entityCache.UpdateCache(); };
            m_frequencyController.AddTask(entityCacheUpdate, 0.1f, 5);
            
            std::function<void()> fpsUpdate = [this]() { m_adaptivePerf.UpdateFPS(); };
            m_frequencyController.AddTask(fpsUpdate, 0.5f, 10);
        }
        
        void Update()
        {
            m_adaptivePerf.UpdateFPS();
            m_frequencyController.ProcessFrame();
            
            if (m_updateTimer.Run(5.0f)) // Clean caches every 5 seconds
            {
                m_stringCache.Clear();
            }
        }
        
        AdaptivePerformance& GetAdaptivePerf() { return m_adaptivePerf; }
        EntityCache& GetEntityCache() { return m_entityCache; }
        StringCache& GetStringCache() { return m_stringCache; }
        FrequencyController& GetFrequencyController() { return m_frequencyController; }
    };
    
    // Global instance
    extern OptimizationManager g_OptimizationManager;
    
    // Utility macros for hot paths
    #define CPU_OPT_SKIP_IF_LOW_PERF() \
        if (CpuOptimization::g_OptimizationManager.GetAdaptivePerf().ShouldSkipFrame()) return;
        
    #define CPU_OPT_SKIP_IF_VERY_LOW_PERF(level) \
        if (CpuOptimization::g_OptimizationManager.GetAdaptivePerf().GetPerformanceLevel() > level) return;
        
    #define CPU_OPT_EXPENSIVE_FEATURE_CHECK() \
        (CpuOptimization::g_OptimizationManager.GetAdaptivePerf().CanUseExpensiveFeatures())
        
    #define CPU_OPT_GET_FREQ_MULTIPLIER() \
        (CpuOptimization::g_OptimizationManager.GetAdaptivePerf().GetUpdateFrequencyMultiplier())
} 