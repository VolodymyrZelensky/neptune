#pragma once
#include "../../../SDK/SDK.h"

#include "../AimbotGlobal/AimbotGlobal.h"
#include "../../../Utils/Math/SIMDMath.h"

class CAimbotHitscan
{
private:
	float m_flLastShotTime = 0.0f;

	// Performance optimization members
	SIMDMath::AlignedVector<SIMDMath::Optimized::TargetDistance> m_SIMDTargetCache;
	SIMDMath::AlignedVector<SIMDMath::Vec3SIMD> m_SIMDPositionCache;
	SIMDMath::AlignedVector<float> m_FOVCache;
	
	// Branch prediction hints for hot paths
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
	
	// Cached calculations to avoid repeated work
	struct TargetCache {
		Vec3 lastEyePos;
		float lastMaxRange;
		int lastFrameCount;
		std::vector<Target_t> cachedTargets;
	} m_targetCache;
	
	// Memory pool for frequent allocations
	static constexpr size_t MAX_CACHED_TARGETS = 64;
	Target_t m_targetPool[MAX_CACHED_TARGETS];
	size_t m_poolIndex = 0;

	std::vector<Target_t> GetTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);
	std::vector<Target_t> SortTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);

	int GetHitboxPriority(int nHitbox, CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CBaseEntity* pTarget);
	int CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon);

	bool Aim(Vec3 vCurAngle, Vec3 vToAngle, Vec3& vOut, int iMethod = Vars::Aimbot::General::AimType.Value);
	void Aim(CUserCmd* pCmd, Vec3& vAngle, int iMethod = Vars::Aimbot::General::AimType.Value);
	bool ShouldFire(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, const Target_t& tTarget);

	// Optimized versions of existing functions
	std::vector<Target_t> SortTargetsOptimized(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);
	int CanHitOptimized(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon);
	
	// Fast path for common cases
	inline bool QuickVisibilityCheck(const Vec3& from, const Vec3& to, CTFPlayer* pLocal, CBaseEntity* pTarget) const;
	
	// SIMD batch operations
	void BatchCalculateDistances(const Vec3& eyePos, const std::vector<Target_t>& targets, float* distances) const;
	void BatchCalculateFOV(const Vec3& viewAngles, const std::vector<Target_t>& targets, float* fovs) const;

public:
	void Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd);
};

ADD_FEATURE(CAimbotHitscan, AimbotHitscan);