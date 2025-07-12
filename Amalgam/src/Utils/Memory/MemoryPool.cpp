#include "MemoryPool.h"

namespace MemoryPool::Global
{
    // Define global memory pool instances
    ObjectPool<EntityInfo, 8192> EntityPool;
    ObjectPool<TargetInfo, 4096> TargetPool;
    ObjectPool<HitboxInfo, 2048> HitboxPool;
    ObjectPool<PathNode, 16384> PathNodePool;
    ObjectPool<NavArea, 4096> NavAreaPool;
    
    // 16MB temporary allocator for transient data
    StackAllocator TempAllocator(16 * 1024 * 1024);
    
    void Initialize()
    {
        // Pools are initialized automatically in their constructors
        // This function can be used for any additional setup if needed
    }
    
    void Shutdown()
    {
        // Clean up all pools
        EntityPool.Clear();
        TargetPool.Clear();
        HitboxPool.Clear();
        PathNodePool.Clear();
        NavAreaPool.Clear();
        TempAllocator.Reset();
    }
    
    void Reset()
    {
        // Reset temporary allocators for frame-based allocations
        TempAllocator.Reset();
    }
} 