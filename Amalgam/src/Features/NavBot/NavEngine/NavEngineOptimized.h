#pragma once

#include "NavEngine.h"
#include "../../../Utils/Memory/MemoryPool.h"
#include "../../../Utils/Math/SIMDMath.h"
#include <unordered_set>
#include <functional>

namespace NavEngineOptimized
{
    // Spatial partitioning grid for fast area lookup
    struct SpatialGrid
    {
        static constexpr int GRID_SIZE = 512; // 512x512 unit cells
        static constexpr int MAX_AREAS_PER_CELL = 16;
        
        struct GridCell
        {
            std::vector<CNavArea*> areas;
            
            void AddArea(CNavArea* area)
            {
                if (areas.size() < MAX_AREAS_PER_CELL)
                {
                    areas.push_back(area);
                }
            }
            
            void Clear() { areas.clear(); }
        };
        
        GridCell** grid;
        int gridWidth, gridHeight;
        Vector minBounds, maxBounds;
        
        SpatialGrid() : grid(nullptr), gridWidth(0), gridHeight(0) {}
        
        ~SpatialGrid()
        {
            if (grid)
            {
                for (int i = 0; i < gridWidth * gridHeight; ++i)
                {
                    delete grid[i];
                }
                delete[] grid;
            }
        }
        
        void Initialize(const Vector& min, const Vector& max, const std::vector<CNavArea*>& areas)
        {
            minBounds = min;
            maxBounds = max;
            
            gridWidth = static_cast<int>((max.x - min.x) / GRID_SIZE) + 1;
            gridHeight = static_cast<int>((max.y - min.y) / GRID_SIZE) + 1;
            
            grid = new GridCell*[gridWidth * gridHeight];
            for (int i = 0; i < gridWidth * gridHeight; ++i)
            {
                grid[i] = new GridCell();
            }
            
            // Populate grid
            for (CNavArea* area : areas)
            {
                AddAreaToGrid(area);
            }
        }
        
        void AddAreaToGrid(CNavArea* area)
        {
            Vector center = area->m_center;
            int gridX = static_cast<int>((center.x - minBounds.x) / GRID_SIZE);
            int gridY = static_cast<int>((center.y - minBounds.y) / GRID_SIZE);
            
            gridX = std::clamp(gridX, 0, gridWidth - 1);
            gridY = std::clamp(gridY, 0, gridHeight - 1);
            
            int index = gridY * gridWidth + gridX;
            grid[index]->AddArea(area);
        }
        
        std::vector<CNavArea*> GetNearbyAreas(const Vector& position, float radius = GRID_SIZE * 2.0f)
        {
            std::vector<CNavArea*> nearbyAreas;
            
            int startX = std::max(0, static_cast<int>((position.x - radius - minBounds.x) / GRID_SIZE));
            int endX = std::min(gridWidth - 1, static_cast<int>((position.x + radius - minBounds.x) / GRID_SIZE));
            int startY = std::max(0, static_cast<int>((position.y - radius - minBounds.y) / GRID_SIZE));
            int endY = std::min(gridHeight - 1, static_cast<int>((position.y + radius - minBounds.y) / GRID_SIZE));
            
            for (int x = startX; x <= endX; ++x)
            {
                for (int y = startY; y <= endY; ++y)
                {
                    int index = y * gridWidth + x;
                    const auto& areas = grid[index]->areas;
                    nearbyAreas.insert(nearbyAreas.end(), areas.begin(), areas.end());
                }
            }
            
            return nearbyAreas;
        }
    };
    
    // Enhanced path cache with multiple strategies
    struct PathCache
    {
        struct CachedPath
        {
            std::vector<CNavParser::Crumb> path;
            float cost;
            int frame;
            int useCount;
            
            CachedPath() : cost(0.0f), frame(0), useCount(0) {}
        };
        
        std::unordered_map<uint64_t, CachedPath> cache;
        static constexpr int MAX_CACHE_SIZE = 512;
        static constexpr int CACHE_FRAME_LIFETIME = 300; // ~5 seconds at 60fps
        
        uint64_t MakeKey(CNavArea* start, CNavArea* end)
        {
            // Create unique key from area pointers
            uint64_t startPtr = reinterpret_cast<uint64_t>(start);
            uint64_t endPtr = reinterpret_cast<uint64_t>(end);
            return startPtr ^ (endPtr << 1);
        }
        
        bool GetCachedPath(CNavArea* start, CNavArea* end, std::vector<CNavParser::Crumb>& outPath)
        {
            uint64_t key = MakeKey(start, end);
            auto it = cache.find(key);
            
            if (it != cache.end())
            {
                CachedPath& cachedPath = it->second;
                int currentFrame = I::GlobalVars ? I::GlobalVars->framecount : 0;
                
                // Check if cache entry is still valid
                if (currentFrame - cachedPath.frame < CACHE_FRAME_LIFETIME)
                {
                    outPath = cachedPath.path;
                    cachedPath.useCount++;
                    return true;
                }
                else
                {
                    cache.erase(it);
                }
            }
            
            return false;
        }
        
        void CachePath(CNavArea* start, CNavArea* end, const std::vector<CNavParser::Crumb>& path, float cost)
        {
            // Prevent cache from growing too large
            if (cache.size() >= MAX_CACHE_SIZE)
            {
                CleanupOldEntries();
            }
            
            uint64_t key = MakeKey(start, end);
            CachedPath& cachedPath = cache[key];
            cachedPath.path = path;
            cachedPath.cost = cost;
            cachedPath.frame = I::GlobalVars ? I::GlobalVars->framecount : 0;
            cachedPath.useCount = 1;
        }
        
        void CleanupOldEntries()
        {
            int currentFrame = I::GlobalVars ? I::GlobalVars->framecount : 0;
            
            // Remove entries that are too old or least used
            std::vector<std::pair<uint64_t, int>> candidates;
            for (const auto& [key, cachedPath] : cache)
            {
                int age = currentFrame - cachedPath.frame;
                int priority = age * 100 + (10 - std::min(cachedPath.useCount, 10));
                candidates.emplace_back(key, priority);
            }
            
            // Sort by priority (higher = remove first)
            std::sort(candidates.begin(), candidates.end(), 
                [](const auto& a, const auto& b) { return a.second > b.second; });
            
            // Remove worst 25% of entries
            size_t removeCount = cache.size() / 4;
            for (size_t i = 0; i < removeCount && i < candidates.size(); ++i)
            {
                cache.erase(candidates[i].first);
            }
        }
        
        void Clear()
        {
            cache.clear();
        }
    };
    
    // Hierarchical pathfinding for long-distance navigation
    struct HierarchicalPathfinder
    {
        struct Cluster
        {
            std::vector<CNavArea*> areas;
            Vector center;
            float radius;
            std::vector<CNavArea*> entrances; // Connection points to other clusters
        };
        
        std::vector<Cluster> clusters;
        std::unordered_map<CNavArea*, int> areaToCluster;
        
        void BuildClusters(const std::vector<CNavArea*>& allAreas)
        {
            // Simple clustering based on proximity
            const float CLUSTER_RADIUS = 2048.0f;
            
            for (CNavArea* area : allAreas)
            {
                bool assignedToCluster = false;
                
                for (size_t i = 0; i < clusters.size(); ++i)
                {
                    if (clusters[i].center.DistTo(area->m_center) <= CLUSTER_RADIUS)
                    {
                        clusters[i].areas.push_back(area);
                        areaToCluster[area] = static_cast<int>(i);
                        assignedToCluster = true;
                        break;
                    }
                }
                
                if (!assignedToCluster)
                {
                    Cluster newCluster;
                    newCluster.areas.push_back(area);
                    newCluster.center = area->m_center;
                    newCluster.radius = CLUSTER_RADIUS;
                    
                    areaToCluster[area] = static_cast<int>(clusters.size());
                    clusters.push_back(newCluster);
                }
            }
            
            // Find cluster entrance points
            for (size_t i = 0; i < clusters.size(); ++i)
            {
                FindClusterEntrances(i);
            }
        }
        
        void FindClusterEntrances(int clusterIndex)
        {
            Cluster& cluster = clusters[clusterIndex];
            cluster.entrances.clear();
            
            for (CNavArea* area : cluster.areas)
            {
                // Check if this area connects to other clusters
                for (int dir = 0; dir < NUM_DIRECTIONS; ++dir)
                {
                    auto& connections = area->m_connections[dir];
                    for (CNavConnection& conn : connections)
                    {
                        if (areaToCluster.find(conn.area) != areaToCluster.end())
                        {
                            int otherCluster = areaToCluster[conn.area];
                            if (otherCluster != clusterIndex)
                            {
                                cluster.entrances.push_back(area);
                                break;
                            }
                        }
                    }
                    if (!cluster.entrances.empty() && cluster.entrances.back() == area)
                        break;
                }
            }
        }
        
        std::vector<CNavParser::Crumb> FindHierarchicalPath(CNavArea* start, CNavArea* end)
        {
            auto startClusterIt = areaToCluster.find(start);
            auto endClusterIt = areaToCluster.find(end);
            
            if (startClusterIt == areaToCluster.end() || endClusterIt == areaToCluster.end())
            {
                return {}; // Fallback to normal pathfinding
            }
            
            int startClusterIndex = startClusterIt->second;
            int endClusterIndex = endClusterIt->second;
            
            // Same cluster - use normal pathfinding
            if (startClusterIndex == endClusterIndex)
            {
                return {};
            }
            
            // Different clusters - find path through cluster entrances
            // This is a simplified implementation
            std::vector<CNavParser::Crumb> path;
            
            // Find best entrance in start cluster
            CNavArea* startEntrance = FindNearestEntrance(startClusterIndex, start);
            if (!startEntrance) return {};
            
            // Find best entrance in end cluster  
            CNavArea* endEntrance = FindNearestEntrance(endClusterIndex, end);
            if (!endEntrance) return {};
            
            // This would normally involve inter-cluster pathfinding
            // For now, just return waypoints
            path.push_back({startEntrance, startEntrance->m_center});
            path.push_back({endEntrance, endEntrance->m_center});
            path.push_back({end, end->m_center});
            
            return path;
        }
        
        CNavArea* FindNearestEntrance(int clusterIndex, CNavArea* target)
        {
            if (clusterIndex >= static_cast<int>(clusters.size()))
                return nullptr;
                
            const Cluster& cluster = clusters[clusterIndex];
            if (cluster.entrances.empty())
                return nullptr;
                
            CNavArea* nearest = cluster.entrances[0];
            float nearestDist = target->m_center.DistToSqr(nearest->m_center);
            
            for (size_t i = 1; i < cluster.entrances.size(); ++i)
            {
                float dist = target->m_center.DistToSqr(cluster.entrances[i]->m_center);
                if (dist < nearestDist)
                {
                    nearestDist = dist;
                    nearest = cluster.entrances[i];
                }
            }
            
            return nearest;
        }
    };
    
    // Main optimized navigation engine
    class OptimizedNavEngine
    {
    private:
        SpatialGrid m_spatialGrid;
        PathCache m_pathCache;
        HierarchicalPathfinder m_hierarchicalPathfinder;
        
        // Performance monitoring
        struct PerformanceStats
        {
            int totalPathRequests = 0;
            int cacheHits = 0;
            int cacheMisses = 0;
            float avgPathfindingTime = 0.0f;
            
            float GetCacheHitRate() const
            {
                return totalPathRequests > 0 ? static_cast<float>(cacheHits) / totalPathRequests : 0.0f;
            }
        } m_stats;
        
        // Frame-based optimization flags
        bool m_enableSpatialOptimization = true;
        bool m_enablePathCaching = true;
        bool m_enableHierarchicalPathfinding = true;
        
    public:
        void Initialize(CNavEngine* navEngine)
        {
            if (!navEngine || !navEngine->IsNavMeshLoaded())
                return;
                
            // Build spatial grid
            BuildSpatialGrid(navEngine);
            
            // Build hierarchical clusters
            if (m_enableHierarchicalPathfinding)
            {
                BuildHierarchicalStructure(navEngine);
            }
        }
        
        void BuildSpatialGrid(CNavEngine* navEngine)
        {
            auto* navFile = navEngine->getNavFile();
            if (!navFile || navFile->m_areas.empty())
                return;
                
            // Find bounds
            Vector minBounds = navFile->m_areas[0]->m_center;
            Vector maxBounds = navFile->m_areas[0]->m_center;
            
            std::vector<CNavArea*> areas;
            for (auto& area : navFile->m_areas)
            {
                areas.push_back(area.get());
                
                Vector center = area->m_center;
                minBounds.x = std::min(minBounds.x, center.x);
                minBounds.y = std::min(minBounds.y, center.y);
                maxBounds.x = std::max(maxBounds.x, center.x);
                maxBounds.y = std::max(maxBounds.y, center.y);
            }
            
            m_spatialGrid.Initialize(minBounds, maxBounds, areas);
        }
        
        void BuildHierarchicalStructure(CNavEngine* navEngine)
        {
            auto* navFile = navEngine->getNavFile();
            if (!navFile || navFile->m_areas.empty())
                return;
                
            std::vector<CNavArea*> areas;
            for (auto& area : navFile->m_areas)
            {
                areas.push_back(area.get());
            }
            
            m_hierarchicalPathfinder.BuildClusters(areas);
        }
        
        CNavArea* FindClosestNavSquareOptimized(const Vector& position, CNavEngine* navEngine)
        {
            if (!m_enableSpatialOptimization)
            {
                return navEngine->findClosestNavSquare(position);
            }
            
            // Use spatial grid for faster lookup
            auto nearbyAreas = m_spatialGrid.GetNearbyAreas(position);
            if (nearbyAreas.empty())
            {
                return navEngine->findClosestNavSquare(position); // Fallback
            }
            
            CNavArea* closest = nullptr;
            float closestDistSqr = FLT_MAX;
            
            for (CNavArea* area : nearbyAreas)
            {
                float distSqr = position.DistToSqr(area->m_center);
                if (distSqr < closestDistSqr)
                {
                    closestDistSqr = distSqr;
                    closest = area;
                }
            }
            
            return closest;
        }
        
        std::vector<CNavParser::Crumb> FindPathOptimized(CNavEngine* navEngine, CNavArea* start, CNavArea* end)
        {
            m_stats.totalPathRequests++;
            
            // Try cache first
            if (m_enablePathCaching)
            {
                std::vector<CNavParser::Crumb> cachedPath;
                if (m_pathCache.GetCachedPath(start, end, cachedPath))
                {
                    m_stats.cacheHits++;
                    return cachedPath;
                }
            }
            
            m_stats.cacheMisses++;
            
            // Try hierarchical pathfinding for long distances
            if (m_enableHierarchicalPathfinding)
            {
                float distance = start->m_center.DistTo(end->m_center);
                if (distance > 3000.0f) // Use hierarchical for long paths
                {
                    auto hierarchicalPath = m_hierarchicalPathfinder.FindHierarchicalPath(start, end);
                    if (!hierarchicalPath.empty())
                    {
                        if (m_enablePathCaching)
                        {
                            m_pathCache.CachePath(start, end, hierarchicalPath, distance);
                        }
                        return hierarchicalPath;
                    }
                }
            }
            
            // Use standard pathfinding
            auto pathVoids = navEngine->map->findPath(start, end);
            std::vector<CNavParser::Crumb> path;
            
            for (void* voidArea : pathVoids)
            {
                CNavArea* area = static_cast<CNavArea*>(voidArea);
                path.push_back({area, area->m_center});
            }
            
            // Cache the result
            if (m_enablePathCaching && !path.empty())
            {
                float distance = start->m_center.DistTo(end->m_center);
                m_pathCache.CachePath(start, end, path, distance);
            }
            
            return path;
        }
        
        void Reset()
        {
            m_pathCache.Clear();
            m_stats = PerformanceStats{};
        }
        
        const PerformanceStats& GetStats() const { return m_stats; }
        
        void SetOptimizationFlags(bool spatial, bool caching, bool hierarchical)
        {
            m_enableSpatialOptimization = spatial;
            m_enablePathCaching = caching;
            m_enableHierarchicalPathfinding = hierarchical;
        }
    };
} 