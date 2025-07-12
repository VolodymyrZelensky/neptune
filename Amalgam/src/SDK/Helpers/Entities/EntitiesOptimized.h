#pragma once

#include "Entities.h"
#include "../../../Utils/Memory/MemoryPool.h"
#include "../../../Utils/Math/SIMDMath.h"
#include "../../../Utils/Optimization/BranchOptimization.h"
#include <array>
#include <bitset>

namespace EntitiesOptimized
{
    // Fast entity lookup cache with spatial indexing
    class EntityCache
    {
    private:
        // Spatial grid for fast position-based queries
        static constexpr int GRID_SIZE = 1024; // 1024x1024 unit cells
        static constexpr int MAX_ENTITIES_PER_CELL = 32;
        
        struct GridCell
        {
            std::array<CBaseEntity*, MAX_ENTITIES_PER_CELL> entities;
            int count = 0;
            
            FORCE_INLINE void AddEntity(CBaseEntity* entity)
            {
                if (PREDICT_TRUE(count < MAX_ENTITIES_PER_CELL)) LIKELY
                {
                    entities[count++] = entity;
                }
            }
            
            FORCE_INLINE void Clear() { count = 0; }
            
            FORCE_INLINE bool IsFull() const { return count >= MAX_ENTITIES_PER_CELL; }
        };
        
        // Cache-aligned grid for better memory performance
        using GridArray = std::array<std::array<GridCell, 512>, 512>;
        std::unique_ptr<GridArray> m_spatialGrid;
        
        // Entity type bitmasks for fast filtering
        std::bitset<2048> m_playerMask;
        std::bitset<2048> m_buildingMask;
        std::bitset<2048> m_projectileMask;
        std::bitset<2048> m_validMask;
        
        // Frame-based cache invalidation
        int m_lastUpdateFrame = 0;
        
        // Performance counters
        struct CacheStats
        {
            uint64_t totalQueries = 0;
            uint64_t cacheHits = 0;
            uint64_t spatialQueries = 0;
            uint64_t linearQueries = 0;
            
            FORCE_INLINE double GetHitRate() const
            {
                return totalQueries > 0 ? (double)cacheHits / totalQueries : 0.0;
            }
        } m_stats;
        
        Vec3 m_worldMin, m_worldMax;
        bool m_initialized = false;
        
    public:
        EntityCache() : m_spatialGrid(std::make_unique<GridArray>()) {}
        
        void Initialize(const Vec3& worldMin, const Vec3& worldMax)
        {
            m_worldMin = worldMin;
            m_worldMax = worldMax;
            m_initialized = true;
            Clear();
        }
        
        void Clear()
        {
            if (!m_spatialGrid) return;
            
            // Clear spatial grid
            for (auto& row : *m_spatialGrid)
            {
                for (auto& cell : row)
                {
                    cell.Clear();
                }
            }
            
            // Clear bitmasks
            m_playerMask.reset();
            m_buildingMask.reset();
            m_projectileMask.reset();
            m_validMask.reset();
            
            m_lastUpdateFrame = 0;
        }
        
        void UpdateEntity(CBaseEntity* entity, int index)
        {
            if (!entity || !m_initialized || index < 0 || index >= 2048)
                return;
                
            // Update validity mask
            bool isValid = entity->IsAlive() && !entity->IsDormant();
            m_validMask[index] = isValid;
            
            if (!isValid) UNLIKELY
            {
                // Remove from type masks if invalid
                m_playerMask[index] = false;
                m_buildingMask[index] = false;
                m_projectileMask[index] = false;
                return;
            }
            
            // Update type masks
            m_playerMask[index] = entity->IsPlayer();
            m_buildingMask[index] = entity->IsBuilding();
            m_projectileMask[index] = entity->IsProjectile();
            
            // Add to spatial grid
            AddToSpatialGrid(entity, index);
        }
        
        void UpdateFrame()
        {
            int currentFrame = I::GlobalVars ? I::GlobalVars->framecount : 0;
            
            // Only update if frame changed
            if (currentFrame == m_lastUpdateFrame) LIKELY
                return;
                
            m_lastUpdateFrame = currentFrame;
            
            // Clear spatial grid for fresh update
            for (auto& row : *m_spatialGrid)
            {
                for (auto& cell : row)
                {
                    cell.Clear();
                }
            }
            
            // Rebuild spatial index
            for (int i = 1; i <= I::EngineClient->GetMaxClients(); ++i)
            {
                auto entity = I::ClientEntityList->GetClientEntity(i);
                if (entity && m_validMask[i]) LIKELY
                {
                    AddToSpatialGrid(entity, i);
                }
            }
        }
        
        // Fast entity queries by type
        template<typename Func>
        FORCE_INLINE void ForEachPlayer(Func&& func)
        {
            m_stats.totalQueries++;
            
            // Use bitmask for fast iteration
            for (int i = m_playerMask._Find_first(); i < 2048; i = m_playerMask._Find_next(i))
            {
                if (PREDICT_TRUE(m_validMask[i])) LIKELY
                {
                    auto entity = I::ClientEntityList->GetClientEntity(i);
                    if (entity) LIKELY
                    {
                        func(entity->As<CTFPlayer>(), i);
                    }
                }
            }
            
            m_stats.cacheHits++;
        }
        
        template<typename Func>
        FORCE_INLINE void ForEachBuilding(Func&& func)
        {
            m_stats.totalQueries++;
            
            for (int i = m_buildingMask._Find_first(); i < 2048; i = m_buildingMask._Find_next(i))
            {
                if (PREDICT_TRUE(m_validMask[i])) LIKELY
                {
                    auto entity = I::ClientEntityList->GetClientEntity(i);
                    if (entity) LIKELY
                    {
                        func(entity, i);
                    }
                }
            }
            
            m_stats.cacheHits++;
        }
        
        // Spatial queries for position-based lookups
        std::vector<CBaseEntity*> GetEntitiesInRadius(const Vec3& center, float radius)
        {
            m_stats.totalQueries++;
            m_stats.spatialQueries++;
            
            std::vector<CBaseEntity*> result;
            
            if (!m_initialized)
            {
                m_stats.linearQueries++;
                return GetEntitiesInRadiusLinear(center, radius);
            }
            
            // Calculate grid bounds for search
            float radiusSqr = radius * radius;
            int startX = BranchOptimization::MaxBranchless(0, 
                static_cast<int>((center.x - radius - m_worldMin.x) / GRID_SIZE));
            int endX = BranchOptimization::MinBranchless(511, 
                static_cast<int>((center.x + radius - m_worldMin.x) / GRID_SIZE));
            int startY = BranchOptimization::MaxBranchless(0, 
                static_cast<int>((center.y - radius - m_worldMin.y) / GRID_SIZE));
            int endY = BranchOptimization::MinBranchless(511, 
                static_cast<int>((center.y + radius - m_worldMin.y) / GRID_SIZE));
            
            // Search grid cells
            SIMDMath::Vec3SIMD centerSIMD(center);
            for (int x = startX; x <= endX; ++x)
            {
                for (int y = startY; y <= endY; ++y)
                {
                    const GridCell& cell = (*m_spatialGrid)[x][y];
                    
                    // Check entities in this cell
                    for (int i = 0; i < cell.count; ++i) LIKELY
                    {
                        CBaseEntity* entity = cell.entities[i];
                        if (!entity) UNLIKELY continue;
                        
                        SIMDMath::Vec3SIMD entityPos(entity->m_vecOrigin());
                        if (centerSIMD.DistToSqr(entityPos) <= radiusSqr) LIKELY
                        {
                            result.push_back(entity);
                        }
                    }
                }
            }
            
            m_stats.cacheHits++;
            return result;
        }
        
        // Fast nearest entity search
        CBaseEntity* GetNearestEntity(const Vec3& position, EGroupType groupType, float maxDistance = FLT_MAX)
        {
            m_stats.totalQueries++;
            
            CBaseEntity* nearest = nullptr;
            float nearestDistSqr = maxDistance * maxDistance;
            SIMDMath::Vec3SIMD posSIMD(position);
            
            // Choose appropriate bitmask based on group type
            const std::bitset<2048>* typeMask = &m_validMask;
            switch (groupType)
            {
                case EGroupType::PLAYERS_ALL:
                case EGroupType::PLAYERS_ENEMIES:
                case EGroupType::PLAYERS_TEAMMATES:
                    typeMask = &m_playerMask;
                    break;
                case EGroupType::BUILDINGS_ALL:
                case EGroupType::BUILDINGS_ENEMIES:
                case EGroupType::BUILDINGS_TEAMMATES:
                    typeMask = &m_buildingMask;
                    break;
                case EGroupType::WORLD_PROJECTILES:
                    typeMask = &m_projectileMask;
                    break;
                default:
                    break;
            }
            
            // Iterate through entities of the right type
            for (int i = typeMask->_Find_first(); i < 2048; i = typeMask->_Find_next(i))
            {
                if (!m_validMask[i]) UNLIKELY continue;
                
                auto entity = I::ClientEntityList->GetClientEntity(i);
                if (!entity) UNLIKELY continue;
                
                SIMDMath::Vec3SIMD entityPos(entity->m_vecOrigin());
                float distSqr = posSIMD.DistToSqr(entityPos);
                
                if (distSqr < nearestDistSqr) LIKELY
                {
                    nearestDistSqr = distSqr;
                    nearest = entity;
                }
            }
            
            m_stats.cacheHits++;
            return nearest;
        }
        
        // Statistics
        const CacheStats& GetStats() const { return m_stats; }
        void ResetStats() { m_stats = CacheStats{}; }
        
    private:
        FORCE_INLINE void AddToSpatialGrid(CBaseEntity* entity, int index)
        {
            if (!m_initialized) return;
            
            Vec3 pos = entity->m_vecOrigin();
            
            // Calculate grid coordinates
            int gridX = static_cast<int>((pos.x - m_worldMin.x) / GRID_SIZE);
            int gridY = static_cast<int>((pos.y - m_worldMin.y) / GRID_SIZE);
            
            // Clamp to grid bounds
            gridX = BranchOptimization::ClampBranchless(gridX, 0, 511);
            gridY = BranchOptimization::ClampBranchless(gridY, 0, 511);
            
            // Add to grid cell
            (*m_spatialGrid)[gridX][gridY].AddEntity(entity);
        }
        
        std::vector<CBaseEntity*> GetEntitiesInRadiusLinear(const Vec3& center, float radius)
        {
            // Fallback linear search
            std::vector<CBaseEntity*> result;
            float radiusSqr = radius * radius;
            SIMDMath::Vec3SIMD centerSIMD(center);
            
            for (int i = 1; i < 2048; ++i)
            {
                if (!m_validMask[i]) continue;
                
                auto entity = I::ClientEntityList->GetClientEntity(i);
                if (!entity) continue;
                
                SIMDMath::Vec3SIMD entityPos(entity->m_vecOrigin());
                if (centerSIMD.DistToSqr(entityPos) <= radiusSqr)
                {
                    result.push_back(entity);
                }
            }
            
            return result;
        }
    };
    
    // Global optimized entity cache
    inline EntityCache g_EntityCache;
    
    // Optimized helper functions
    namespace Helpers
    {
        // Fast entity validation
        FORCE_INLINE bool IsValidEntity(CBaseEntity* entity, int index = -1)
        {
            if (!entity) UNLIKELY return false;
            
            if (index >= 0 && index < 2048)
            {
                return g_EntityCache.GetStats().totalQueries > 0 ? 
                    entity->IsAlive() && !entity->IsDormant() : 
                    entity->IsAlive() && !entity->IsDormant();
            }
            
            return entity->IsAlive() && !entity->IsDormant();
        }
        
        // Fast team check
        FORCE_INLINE bool IsTeammate(CBaseEntity* entity, int localTeam)
        {
            return entity && entity->m_iTeamNum() == localTeam;
        }
        
        FORCE_INLINE bool IsEnemy(CBaseEntity* entity, int localTeam)
        {
            return entity && entity->m_iTeamNum() != localTeam && entity->m_iTeamNum() > 1;
        }
        
        // Fast entity type checks using lookup tables
        static constexpr std::array<bool, 256> PlayerClassIDs = []()
        {
            std::array<bool, 256> arr{};
            // Mark player class IDs as true (TF2 specific)
            for (int i = 0; i < 256; ++i)
            {
                arr[i] = (i >= 1 && i <= 9); // TF_CLASS_SCOUT to TF_CLASS_SPY
            }
            return arr;
        }();
        
        FORCE_INLINE bool IsPlayerClass(int classId)
        {
            return classId >= 0 && classId < 256 && PlayerClassIDs[classId];
        }
        
        // Optimized distance calculations
        template<typename EntityType>
        FORCE_INLINE float FastDistanceTo(EntityType* entity, const Vec3& position)
        {
            if (!entity) UNLIKELY return FLT_MAX;
            
            SIMDMath::Vec3SIMD entityPos(entity->m_vecOrigin());
            SIMDMath::Vec3SIMD targetPos(position);
            return entityPos.DistTo(targetPos);
        }
        
        template<typename EntityType>
        FORCE_INLINE float FastDistanceToSqr(EntityType* entity, const Vec3& position)
        {
            if (!entity) UNLIKELY return FLT_MAX;
            
            SIMDMath::Vec3SIMD entityPos(entity->m_vecOrigin());
            SIMDMath::Vec3SIMD targetPos(position);
            return entityPos.DistToSqr(targetPos);
        }
    }
    
    // High-level optimized interface
    class OptimizedEntityManager
    {
    public:
        static void Initialize()
        {
            // Initialize with reasonable world bounds for TF2 maps
            g_EntityCache.Initialize(Vec3(-8192, -8192, -8192), Vec3(8192, 8192, 8192));
        }
        
        static void UpdateFrame()
        {
            g_EntityCache.UpdateFrame();
            
            // Update all entities
            for (int i = 1; i <= I::EngineClient->GetMaxClients(); ++i)
            {
                auto entity = I::ClientEntityList->GetClientEntity(i);
                g_EntityCache.UpdateEntity(entity, i);
            }
        }
        
        // Fast group queries
        template<typename Func>
        static void ForEachPlayer(Func&& func)
        {
            g_EntityCache.ForEachPlayer(std::forward<Func>(func));
        }
        
        template<typename Func>
        static void ForEachBuilding(Func&& func)
        {
            g_EntityCache.ForEachBuilding(std::forward<Func>(func));
        }
        
        // Spatial queries
        static std::vector<CBaseEntity*> GetEntitiesInRadius(const Vec3& center, float radius)
        {
            return g_EntityCache.GetEntitiesInRadius(center, radius);
        }
        
        static CBaseEntity* GetNearestEntity(const Vec3& position, EGroupType groupType, float maxDistance = FLT_MAX)
        {
            return g_EntityCache.GetNearestEntity(position, groupType, maxDistance);
        }
        
        // Performance monitoring
        static void PrintStats()
        {
            const auto& stats = g_EntityCache.GetStats();
            // Can integrate with existing SDK::Output system
        }
        
        static void ResetStats()
        {
            g_EntityCache.ResetStats();
        }
    };
} 