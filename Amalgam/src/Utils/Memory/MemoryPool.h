#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cstdlib>
#include <map>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace MemoryPool
{
    // Cache-line aligned memory block for better performance
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t DEFAULT_BLOCK_SIZE = 4096;
    
    // Fast pool allocator for fixed-size objects
    template<typename T, size_t BlockSize = DEFAULT_BLOCK_SIZE>
    class ObjectPool
    {
    private:
        struct alignas(CACHE_LINE_SIZE) Block
        {
            uint8_t data[BlockSize];
            Block* next;
            size_t used;
            
            Block() : next(nullptr), used(0) {}
        };
        
        struct FreeNode
        {
            FreeNode* next;
        };
        
        Block* m_currentBlock;
        Block* m_firstBlock;
        FreeNode* m_freeList;
        size_t m_objectsPerBlock;
        size_t m_totalAllocations;
        size_t m_totalDeallocations;
        
        static constexpr size_t AlignedSize = ((sizeof(T) + alignof(T) - 1) / alignof(T)) * alignof(T);
        
        void AllocateNewBlock()
        {
            Block* newBlock = new Block();
            newBlock->next = m_firstBlock;
            m_firstBlock = newBlock;
            m_currentBlock = newBlock;
            
            // Initialize free list for this block
            uint8_t* ptr = newBlock->data;
            size_t availableSpace = BlockSize;
            size_t objectCount = 0;
            
            while (availableSpace >= AlignedSize && objectCount < m_objectsPerBlock)
            {
                FreeNode* node = reinterpret_cast<FreeNode*>(ptr);
                node->next = m_freeList;
                m_freeList = node;
                
                ptr += AlignedSize;
                availableSpace -= AlignedSize;
                objectCount++;
            }
        }
        
    public:
        ObjectPool() 
            : m_currentBlock(nullptr)
            , m_firstBlock(nullptr)
            , m_freeList(nullptr)
            , m_objectsPerBlock(BlockSize / AlignedSize)
            , m_totalAllocations(0)
            , m_totalDeallocations(0)
        {
            static_assert(sizeof(T) >= sizeof(FreeNode), "Object size too small for free list");
            AllocateNewBlock();
        }
        
        ~ObjectPool()
        {
            Clear();
        }
        
        // Fast allocation - O(1)
        T* Allocate()
        {
            if (!m_freeList) [[unlikely]]
            {
                AllocateNewBlock();
            }
            
            FreeNode* node = m_freeList;
            m_freeList = node->next;
            m_totalAllocations++;
            
            return reinterpret_cast<T*>(node);
        }
        
        // Fast deallocation - O(1) 
        void Deallocate(T* ptr)
        {
            if (!ptr) return;
            
            FreeNode* node = reinterpret_cast<FreeNode*>(ptr);
            node->next = m_freeList;
            m_freeList = node;
            m_totalDeallocations++;
        }
        
        // Construct object in-place
        template<typename... Args>
        T* Construct(Args&&... args)
        {
            T* ptr = Allocate();
            new(ptr) T(std::forward<Args>(args)...);
            return ptr;
        }
        
        // Destruct and deallocate
        void Destroy(T* ptr)
        {
            if (ptr)
            {
                ptr->~T();
                Deallocate(ptr);
            }
        }
        
        void Clear()
        {
            Block* current = m_firstBlock;
            while (current)
            {
                Block* next = current->next;
                delete current;
                current = next;
            }
            
            m_firstBlock = nullptr;
            m_currentBlock = nullptr;
            m_freeList = nullptr;
            m_totalAllocations = 0;
            m_totalDeallocations = 0;
        }
        
        // Statistics
        size_t GetTotalAllocations() const { return m_totalAllocations; }
        size_t GetTotalDeallocations() const { return m_totalDeallocations; }
        size_t GetActiveAllocations() const { return m_totalAllocations - m_totalDeallocations; }
    };
    
    // Stack allocator for temporary allocations
    class StackAllocator
    {
    private:
        uint8_t* m_memory;
        size_t m_size;
        size_t m_used;
        
    public:
        explicit StackAllocator(size_t size) 
            : m_size(size), m_used(0)
        {
#ifdef _WIN32
            m_memory = static_cast<uint8_t*>(_aligned_malloc(size, CACHE_LINE_SIZE));
#else
            m_memory = static_cast<uint8_t*>(std::aligned_alloc(CACHE_LINE_SIZE, size));
#endif
        }
        
        ~StackAllocator()
        {
#ifdef _WIN32
            _aligned_free(m_memory);
#else
            std::free(m_memory);
#endif
        }
        
        template<typename T>
        T* Allocate(size_t count = 1)
        {
            size_t totalSize = sizeof(T) * count;
            size_t alignedSize = (totalSize + alignof(T) - 1) & ~(alignof(T) - 1);
            
            if (m_used + alignedSize > m_size) [[unlikely]]
            {
                return nullptr; // Out of memory
            }
            
            T* result = reinterpret_cast<T*>(m_memory + m_used);
            m_used += alignedSize;
            return result;
        }
        
        void Reset()
        {
            m_used = 0;
        }
        
        size_t GetUsed() const { return m_used; }
        size_t GetRemaining() const { return m_size - m_used; }
    };
    
    // Forward declarations for pooled types
    struct EntityInfo
    {
        int entityIndex = -1;
        int classId = -1;
        float lastUpdateTime = 0.0f;
        uint32_t flags = 0;
        
        EntityInfo() = default;
        EntityInfo(int index, int cls, float time, uint32_t f = 0) 
            : entityIndex(index), classId(cls), lastUpdateTime(time), flags(f) {}
    };
    
    struct TargetInfo
    {
        int entityIndex = -1;
        float priority = 0.0f;
        float distance = 0.0f;
        float fov = 0.0f;
        bool isValid = false;
        
        TargetInfo() = default;
        TargetInfo(int index, float prio, float dist, float f, bool valid = true)
            : entityIndex(index), priority(prio), distance(dist), fov(f), isValid(valid) {}
    };
    
    struct HitboxInfo
    {
        int m_iBone = -1;
        int m_nHitbox = -1;
        uint32_t entityIndex = 0;
        float radius = 0.0f;
        
        HitboxInfo() = default;
        HitboxInfo(int bone, int hitbox, uint32_t index = 0, float r = 0.0f)
            : m_iBone(bone), m_nHitbox(hitbox), entityIndex(index), radius(r) {}
    };
    
    struct PathNode
    {
        uint32_t nodeId = 0;
        float gCost = 0.0f;
        float hCost = 0.0f;
        float fCost = 0.0f;
        uint32_t parentId = 0;
        bool isOpen = false;
        bool isClosed = false;
        
        PathNode() = default;
        PathNode(uint32_t id, float g = 0.0f, float h = 0.0f)
            : nodeId(id), gCost(g), hCost(h), fCost(g + h) {}
    };
    
    struct NavArea
    {
        uint32_t areaId = 0;
        float centerX = 0.0f;
        float centerY = 0.0f;
        float centerZ = 0.0f;
        uint32_t flags = 0;
        
        NavArea() = default;
        NavArea(uint32_t id, float x, float y, float z, uint32_t f = 0)
            : areaId(id), centerX(x), centerY(y), centerZ(z), flags(f) {}
    };

    // Global memory pools for common types
    namespace Global
    {
        // Entity-related pools
        extern ObjectPool<EntityInfo, 8192> EntityPool;
        extern ObjectPool<TargetInfo, 4096> TargetPool;
        extern ObjectPool<HitboxInfo, 2048> HitboxPool;
        
        // Pathfinding pools  
        extern ObjectPool<PathNode, 16384> PathNodePool;
        extern ObjectPool<NavArea, 4096> NavAreaPool;
        
        // Temporary allocations
        extern StackAllocator TempAllocator;
        
        void Initialize();
        void Shutdown();
        void Reset(); // Reset temporary allocators
    }
    
    // RAII wrapper for automatic cleanup
    template<typename T>
    class PoolPtr
    {
    private:
        T* m_ptr;
        ObjectPool<T>* m_pool;
        
    public:
        PoolPtr(ObjectPool<T>* pool) : m_pool(pool), m_ptr(pool->Allocate()) {}
        
        template<typename... Args>
        PoolPtr(ObjectPool<T>* pool, Args&&... args) 
            : m_pool(pool), m_ptr(pool->Construct(std::forward<Args>(args)...)) {}
        
        ~PoolPtr()
        {
            if (m_ptr)
            {
                m_pool->Destroy(m_ptr);
            }
        }
        
        // Move semantics
        PoolPtr(PoolPtr&& other) noexcept : m_ptr(other.m_ptr), m_pool(other.m_pool)
        {
            other.m_ptr = nullptr;
        }
        
        PoolPtr& operator=(PoolPtr&& other) noexcept
        {
            if (this != &other)
            {
                if (m_ptr)
                {
                    m_pool->Destroy(m_ptr);
                }
                m_ptr = other.m_ptr;
                m_pool = other.m_pool;
                other.m_ptr = nullptr;
            }
            return *this;
        }
        
        // Disable copy
        PoolPtr(const PoolPtr&) = delete;
        PoolPtr& operator=(const PoolPtr&) = delete;
        
        T* get() const { return m_ptr; }
        T* operator->() const { return m_ptr; }
        T& operator*() const { return *m_ptr; }
        explicit operator bool() const { return m_ptr != nullptr; }
        
        T* release()
        {
            T* result = m_ptr;
            m_ptr = nullptr;
            return result;
        }
    };
    
    // Convenience macros
    #define POOL_ALLOC(pool, type) (pool).Allocate()
    #define POOL_FREE(pool, ptr) (pool).Deallocate(ptr)
    #define POOL_NEW(pool, type, ...) (pool).Construct(__VA_ARGS__)
    #define POOL_DELETE(pool, ptr) (pool).Destroy(ptr)
    
    // Stack allocation macros
    #define STACK_ALLOC(allocator, type, count) (allocator).Allocate<type>(count)
    #define STACK_RESET(allocator) (allocator).Reset()
}

// Custom allocators for STL containers
template<typename T>
class PoolAllocator
{
private:
    MemoryPool::ObjectPool<T>* m_pool;
    
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    template<typename U>
    struct rebind { using other = PoolAllocator<U>; };
    
    explicit PoolAllocator(MemoryPool::ObjectPool<T>* pool) : m_pool(pool) {}
    
    template<typename U>
    PoolAllocator(const PoolAllocator<U>& other) : m_pool(nullptr) {}
    
    pointer allocate(size_type n)
    {
        if (n == 1)
        {
            return m_pool->Allocate();
        }
        else
        {
            // Fall back to standard allocation for arrays
            return static_cast<pointer>(std::malloc(n * sizeof(T)));
        }
    }
    
    void deallocate(pointer p, size_type n)
    {
        if (n == 1)
        {
            m_pool->Deallocate(p);
        }
        else
        {
            std::free(p);
        }
    }
    
    template<typename U, typename... Args>
    void construct(U* p, Args&&... args)
    {
        new(p) U(std::forward<Args>(args)...);
    }
    
    template<typename U>
    void destroy(U* p)
    {
        p->~U();
    }
    
    bool operator==(const PoolAllocator& other) const
    {
        return m_pool == other.m_pool;
    }
    
    bool operator!=(const PoolAllocator& other) const
    {
        return !(*this == other);
    }
};

// Type aliases for pool-allocated containers
template<typename T>
using PoolVector = std::vector<T, PoolAllocator<T>>;

template<typename Key, typename Value>
using PoolMap = std::map<Key, Value, std::less<Key>, PoolAllocator<std::pair<const Key, Value>>>; 