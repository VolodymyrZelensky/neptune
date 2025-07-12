#pragma once

// Branch prediction hints for better performance
#ifdef __has_cpp_attribute
    #if __has_cpp_attribute(likely)
        #define LIKELY [[likely]]
        #define UNLIKELY [[unlikely]]
    #else
        #define LIKELY
        #define UNLIKELY
    #endif
#else
    #define LIKELY
    #define UNLIKELY
#endif

// Compiler-specific branch prediction hints
#ifdef __GNUC__
    #define PREDICT_TRUE(x) __builtin_expect(!!(x), 1)
    #define PREDICT_FALSE(x) __builtin_expect(!!(x), 0)
#else
    #define PREDICT_TRUE(x) (x)
    #define PREDICT_FALSE(x) (x)
#endif

// Force inline for critical hot paths
#ifdef _MSC_VER
    #define FORCE_INLINE __forceinline
#elif defined(__GNUC__)
    #define FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define FORCE_INLINE inline
#endif

// Hot/cold code sections for better instruction cache usage
#ifdef __GNUC__
    #define HOT_FUNCTION __attribute__((hot))
    #define COLD_FUNCTION __attribute__((cold))
    #define HOT_SECTION __attribute__((section(".text.hot")))
    #define COLD_SECTION __attribute__((section(".text.cold")))
#else
    #define HOT_FUNCTION
    #define COLD_FUNCTION
    #define HOT_SECTION
    #define COLD_SECTION
#endif

// No-inline for rarely used functions
#ifdef _MSC_VER
    #define NO_INLINE __declspec(noinline)
#elif defined(__GNUC__)
    #define NO_INLINE __attribute__((noinline))
#else
    #define NO_INLINE
#endif

// Assume hints for better optimization
#ifdef _MSC_VER
    #define ASSUME(x) __assume(x)
#elif defined(__clang__)
    #define ASSUME(x) __builtin_assume(x)
#else
    #define ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while(0)
#endif

// Prefetch hints for memory access patterns
#ifdef __GNUC__
    #define PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
    #define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
    #define PREFETCH_READ_LOW(addr) __builtin_prefetch((addr), 0, 1)
    #define PREFETCH_WRITE_LOW(addr) __builtin_prefetch((addr), 1, 1)
#else
    #define PREFETCH_READ(addr)
    #define PREFETCH_WRITE(addr)
    #define PREFETCH_READ_LOW(addr)
    #define PREFETCH_WRITE_LOW(addr)
#endif

namespace BranchOptimization
{
    // Statistics tracking for branch prediction analysis
    struct BranchStats
    {
        uint64_t totalBranches = 0;
        uint64_t predictedCorrectly = 0;
        
        void RecordBranch(bool predicted, bool actual)
        {
            totalBranches++;
            if (predicted == actual)
                predictedCorrectly++;
        }
        
        double GetAccuracy() const
        {
            return totalBranches > 0 ? (double)predictedCorrectly / totalBranches : 0.0;
        }
        
        void Reset()
        {
            totalBranches = 0;
            predictedCorrectly = 0;
        }
    };
    
    // Template for branchless conditional operations
    template<typename T>
    FORCE_INLINE T SelectBranchless(bool condition, T ifTrue, T ifFalse)
    {
        // Use conditional move instead of branching
        return condition ? ifTrue : ifFalse;
    }
    
    // Branchless min/max operations
    template<typename T>
    FORCE_INLINE T MinBranchless(T a, T b)
    {
        return SelectBranchless(a < b, a, b);
    }
    
    template<typename T>
    FORCE_INLINE T MaxBranchless(T a, T b)
    {
        return SelectBranchless(a > b, a, b);
    }
    
    // Branchless clamp operation
    template<typename T>
    FORCE_INLINE T ClampBranchless(T value, T minVal, T maxVal)
    {
        return MaxBranchless(minVal, MinBranchless(value, maxVal));
    }
    
    // Branchless absolute value
    FORCE_INLINE int AbsBranchless(int x)
    {
        int mask = x >> 31;
        return (x + mask) ^ mask;
    }
    
    FORCE_INLINE float AbsBranchless(float x)
    {
        union { float f; int i; } u = { x };
        u.i &= 0x7FFFFFFF;
        return u.f;
    }
    
    // Fast branchless sign function
    template<typename T>
    FORCE_INLINE int SignBranchless(T x)
    {
        return (x > 0) - (x < 0);
    }
    
    // Branch elimination for common patterns
    namespace Patterns
    {
        // Replace if-else chains with lookup tables where possible
        template<size_t N>
        struct LookupTable
        {
            static constexpr size_t SIZE = N;
            
            template<typename T>
            static FORCE_INLINE T Lookup(const T (&table)[N], size_t index)
            {
                ASSUME(index < N);
                return table[index];
            }
        };
        
        // Conditional execution without branching
        template<typename Func>
        FORCE_INLINE void ConditionalExecute(bool condition, Func&& func)
        {
            if (PREDICT_TRUE(condition)) LIKELY
            {
                func();
            }
        }
        
        // Loop unrolling helper
        template<int N>
        struct UnrollLoop
        {
            template<typename Func>
            static FORCE_INLINE void Execute(Func&& func)
            {
                func(N - 1);
                UnrollLoop<N - 1>::Execute(func);
            }
        };
        
        template<>
        struct UnrollLoop<0>
        {
            template<typename Func>
            static FORCE_INLINE void Execute(Func&&) {}
        };
        
        // Duff's device for loop optimization
        template<typename Func>
        FORCE_INLINE void DuffsDevice(int count, Func&& func)
        {
            int n = (count + 7) / 8;
            switch (count % 8)
            {
                case 0: do { func();
                case 7: func();
                case 6: func();
                case 5: func();
                case 4: func();
                case 3: func();
                case 2: func();
                case 1: func();
                } while (--n > 0);
            }
        }
    }
    
    // Profile-guided optimization helpers
    namespace ProfileGuided
    {
        // Hot path tracking
        struct HotPath
        {
            uint64_t executionCount = 0;
            uint64_t totalCycles = 0;
            
            FORCE_INLINE void RecordExecution(uint64_t cycles)
            {
                executionCount++;
                totalCycles += cycles;
            }
            
            double GetAverageCycles() const
            {
                return executionCount > 0 ? (double)totalCycles / executionCount : 0.0;
            }
        };
        
        // Branch frequency tracker
        class BranchFrequencyTracker
        {
        private:
            std::unordered_map<void*, uint64_t> branchCounts;
            std::unordered_map<void*, uint64_t> takenCounts;
            
        public:
            void RecordBranch(void* branchAddr, bool taken)
            {
                branchCounts[branchAddr]++;
                if (taken)
                    takenCounts[branchAddr]++;
            }
            
            double GetTakenRatio(void* branchAddr) const
            {
                auto it = branchCounts.find(branchAddr);
                if (it == branchCounts.end() || it->second == 0)
                    return 0.5; // Default to 50% if no data
                    
                auto takenIt = takenCounts.find(branchAddr);
                uint64_t taken = (takenIt != takenCounts.end()) ? takenIt->second : 0;
                
                return (double)taken / it->second;
            }
            
            void Clear()
            {
                branchCounts.clear();
                takenCounts.clear();
            }
        };
    }
    
    // Optimization macros for hot paths
    #define HOT_PATH_BEGIN() \
        do { \
            static ProfileGuided::HotPath hotPathStats; \
            uint64_t startCycles = __rdtsc();
    
    #define HOT_PATH_END() \
            uint64_t endCycles = __rdtsc(); \
            hotPathStats.RecordExecution(endCycles - startCycles); \
        } while(0)
    
    // Fast comparison chains
    #define FAST_COMPARE_CHAIN_2(val, a, b) \
        ((val) == (a) || (val) == (b))
    
    #define FAST_COMPARE_CHAIN_3(val, a, b, c) \
        ((val) == (a) || (val) == (b) || (val) == (c))
    
    #define FAST_COMPARE_CHAIN_4(val, a, b, c, d) \
        ((val) == (a) || (val) == (b) || (val) == (c) || (val) == (d))
    
    // Branchless boolean operations
    #define BRANCHLESS_AND(a, b) ((a) & (b))
    #define BRANCHLESS_OR(a, b) ((a) | (b))
    #define BRANCHLESS_XOR(a, b) ((a) ^ (b))
    #define BRANCHLESS_NOT(a) (1 - (a))
    
    // Conditional assignment without branching
    #define CONDITIONAL_ASSIGN(condition, target, value) \
        (target) = SelectBranchless((condition), (value), (target))
    
    // Range checks without branching
    #define IN_RANGE_BRANCHLESS(val, min, max) \
        (((val) >= (min)) & ((val) <= (max)))
        
    #define OUT_OF_RANGE_BRANCHLESS(val, min, max) \
        (((val) < (min)) | ((val) > (max)))
} 