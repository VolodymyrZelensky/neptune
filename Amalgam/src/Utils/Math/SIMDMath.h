#pragma once

#include "../../SDK/Definitions/Types.h"

#ifdef _WIN32
#include <immintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#endif

// SIMD configuration based on build settings
#if defined(__AVX2__) || defined(_M_X64)
    #define SIMD_AVX2_ENABLED 1
    #define SIMD_SSE_ENABLED 1
#elif defined(__SSE2__) || defined(_M_IX86_FP)
    #define SIMD_SSE_ENABLED 1
    #define SIMD_AVX2_ENABLED 0
#else
    #define SIMD_SSE_ENABLED 0
    #define SIMD_AVX2_ENABLED 0
#endif

namespace SIMDMath
{
    // Alignment for SIMD operations
    #define SIMD_ALIGN alignas(16)
    
    // Fast vector operations using SIMD
    #if SIMD_SSE_ENABLED
    
    // SIMD-optimized Vec3 operations
    struct alignas(16) Vec3SIMD
    {
        union {
            __m128 m;
            struct { float x, y, z, w; };
            float data[4];
        };
        
        Vec3SIMD() : m(_mm_setzero_ps()) {}
        Vec3SIMD(float x, float y, float z) : m(_mm_set_ps(0.0f, z, y, x)) {}
        Vec3SIMD(const Vec3& v) : m(_mm_set_ps(0.0f, v.z, v.y, v.x)) {}
        Vec3SIMD(__m128 m) : m(m) {}
        
        // Fast operations
        inline Vec3SIMD operator+(const Vec3SIMD& other) const {
            return Vec3SIMD(_mm_add_ps(m, other.m));
        }
        
        inline Vec3SIMD operator-(const Vec3SIMD& other) const {
            return Vec3SIMD(_mm_sub_ps(m, other.m));
        }
        
        inline Vec3SIMD operator*(float scalar) const {
            return Vec3SIMD(_mm_mul_ps(m, _mm_set1_ps(scalar)));
        }
        
        inline Vec3SIMD operator*(const Vec3SIMD& other) const {
            return Vec3SIMD(_mm_mul_ps(m, other.m));
        }
        
        inline float Dot(const Vec3SIMD& other) const {
            __m128 mul = _mm_mul_ps(m, other.m);
            __m128 hadd1 = _mm_hadd_ps(mul, mul);
            __m128 hadd2 = _mm_hadd_ps(hadd1, hadd1);
            return _mm_cvtss_f32(hadd2);
        }
        
        inline float Length() const {
            return sqrtf(Dot(*this));
        }
        
        inline float LengthSqr() const {
            return Dot(*this);
        }
        
        inline Vec3SIMD Normalized() const {
            float len = Length();
            if (len > 0.0f) {
                return *this * (1.0f / len);
            }
            return Vec3SIMD();
        }
        
        inline float DistTo(const Vec3SIMD& other) const {
            return (*this - other).Length();
        }
        
        inline float DistToSqr(const Vec3SIMD& other) const {
            return (*this - other).LengthSqr();
        }
        
        // Convert back to regular Vec3
        inline Vec3 ToVec3() const {
            return Vec3(x, y, z);
        }
    };
    
    // Fast matrix operations
    struct alignas(16) Matrix3x4SIMD
    {
        __m128 rows[3];
        
        Matrix3x4SIMD() {
            rows[0] = _mm_setzero_ps();
            rows[1] = _mm_setzero_ps();
            rows[2] = _mm_setzero_ps();
        }
        
        Matrix3x4SIMD(const matrix3x4& m) {
            rows[0] = _mm_load_ps(m[0]);
            rows[1] = _mm_load_ps(m[1]);
            rows[2] = _mm_load_ps(m[2]);
        }
        
        inline Vec3SIMD TransformVector(const Vec3SIMD& v) const {
            __m128 result = _mm_add_ps(
                _mm_add_ps(
                    _mm_mul_ps(_mm_shuffle_ps(v.m, v.m, _MM_SHUFFLE(0,0,0,0)), rows[0]),
                    _mm_mul_ps(_mm_shuffle_ps(v.m, v.m, _MM_SHUFFLE(1,1,1,1)), rows[1])
                ),
                _mm_mul_ps(_mm_shuffle_ps(v.m, v.m, _MM_SHUFFLE(2,2,2,2)), rows[2])
            );
            return Vec3SIMD(result);
        }
    };
    
    // Batch operations for multiple vectors
    inline void BatchDistanceCalculation(const Vec3SIMD* positions, const Vec3SIMD& target, float* distances, int count) {
        for (int i = 0; i < count; ++i) {
            distances[i] = positions[i].DistToSqr(target);
        }
    }
    
    // Fast angle calculations
    inline Vec3SIMD CalcAngleSIMD(const Vec3SIMD& from, const Vec3SIMD& to) {
        Vec3SIMD delta = from - to;
        float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);
        
        Vec3SIMD angles;
        angles.x = atan2f(delta.z, hyp) * (180.0f / 3.14159265f);
        angles.y = atan2f(delta.y, delta.x) * (180.0f / 3.14159265f);
        angles.z = 0.0f;
        
        if (delta.x >= 0.0f) {
            angles.y += 180.0f;
        }
        
        return angles;
    }
    
    #endif // SIMD_SSE_ENABLED
    
    // Fallback implementations for non-SIMD systems
    #if !SIMD_SSE_ENABLED
    using Vec3SIMD = Vec3;
    using Matrix3x4SIMD = matrix3x4;
    #endif
    
    // High-level optimization functions
    namespace Optimized
    {
        // Fast FOV calculation for multiple targets
        inline void CalculateFOVBatch(const Vec3* viewAngles, const Vec3* targetAngles, float* fovResults, int count) {
            #if SIMD_SSE_ENABLED
            for (int i = 0; i < count; ++i) {
                Vec3SIMD from(viewAngles[i]);
                Vec3SIMD to(targetAngles[i]);
                
                // Fast FOV approximation using dot product
                Vec3SIMD fromNorm = from.Normalized();
                Vec3SIMD toNorm = to.Normalized();
                float dot = fromNorm.Dot(toNorm);
                
                // Clamp and convert to degrees
                dot = std::clamp(dot, -1.0f, 1.0f);
                fovResults[i] = acosf(dot) * (180.0f / 3.14159265f);
            }
            #else
            // Fallback implementation
            for (int i = 0; i < count; ++i) {
                // Standard FOV calculation
                fovResults[i] = Math::CalcFov(viewAngles[i], targetAngles[i]);
            }
            #endif
        }
        
        // Fast target sorting by distance
        struct TargetDistance {
            int index;
            float distanceSqr;
        };
        
        inline void SortTargetsByDistance(TargetDistance* targets, int count) {
            // Use a fast radix sort for floating-point distances
            std::sort(targets, targets + count, [](const TargetDistance& a, const TargetDistance& b) {
                return a.distanceSqr < b.distanceSqr;
            });
        }
    }
    
    // Compile-time optimizations
    template<int N>
    struct UnrolledLoop {
        template<typename Func>
        static inline void Execute(Func&& func) {
            func(N-1);
            UnrolledLoop<N-1>::Execute(func);
        }
    };
    
    template<>
    struct UnrolledLoop<0> {
        template<typename Func>
        static inline void Execute(Func&&) {}
    };
    
    // Cache-friendly memory layout helpers
    template<typename T, size_t Alignment = 16>
    class AlignedVector {
    private:
        T* m_data;
        size_t m_size;
        size_t m_capacity;
        
    public:
        AlignedVector() : m_data(nullptr), m_size(0), m_capacity(0) {}
        
        ~AlignedVector() {
            if (m_data) {
                #ifdef _WIN32
                _aligned_free(m_data);
                #else
                free(m_data);
                #endif
            }
        }
        
        void resize(size_t newSize) {
            if (newSize > m_capacity) {
                size_t newCapacity = std::max(newSize, m_capacity * 2);
                #ifdef _WIN32
                T* newData = static_cast<T*>(_aligned_malloc(newCapacity * sizeof(T), Alignment));
                #else
                T* newData = static_cast<T*>(aligned_alloc(Alignment, newCapacity * sizeof(T)));
                #endif
                
                if (m_data) {
                    memcpy(newData, m_data, m_size * sizeof(T));
                    #ifdef _WIN32
                    _aligned_free(m_data);
                    #else
                    free(m_data);
                    #endif
                }
                
                m_data = newData;
                m_capacity = newCapacity;
            }
            m_size = newSize;
        }
        
        T& operator[](size_t index) { return m_data[index]; }
        const T& operator[](size_t index) const { return m_data[index]; }
        T* data() { return m_data; }
        const T* data() const { return m_data; }
        size_t size() const { return m_size; }
    };
} 