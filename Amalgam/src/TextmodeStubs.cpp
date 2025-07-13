#ifdef TEXTMODE

#include "Features/Visuals/Visuals.h"
#include "Features/Aimbot/AutoRocketJump/AutoRocketJump.h"
#include "Features/Simulation/MovementSimulation/MovementSimulation.h"
#include "Features/Simulation/ProjectileSimulation/ProjectileSimulation.h"
#include "Features/Visuals/ESP/ESP.h"
#include "Features/Spectate/Spectate.h"
#include "Features/Killstreak/Killstreak.h"
#include "Features/ImGui/Render.h"
#include "Features/Aimbot/AimbotProjectile/AimbotProjectile.h"
#include "SDK/Definitions/Misc/VGUI.h"
#include "SDK/Definitions/Interfaces/IVModelRender.h"
#include <d3d9.h>

void CVisuals::RestoreWorldModulation()
{

}

std::vector<DrawBox_t> CVisuals::GetHitboxes(matrix3x4* /*aBones*/, CBaseAnimating* /*pEntity*/, std::vector<int> /*vHitboxes*/, int /*iTarget*/)
{
    return {};
}

void CAutoRocketJump::Run(CTFPlayer* /*pLocal*/, CTFWeaponBase* /*pWeapon*/, CUserCmd* /*pCmd*/)
{

}

void CMovementSimulation::Store()
{

}

bool CMovementSimulation::Initialize(CBaseEntity* /*pEntity*/, PlayerStorage& /*tStorage*/, bool /*bHitchance*/, bool /*bStrafe*/)
{
    return false;
}

bool CMovementSimulation::SetDuck(PlayerStorage& /*tStorage*/, bool /*bDuck*/)
{
    return false;
}

void CMovementSimulation::RunTick(PlayerStorage& /*tStorage*/, bool /*bPath*/, std::function<void(CMoveData&)>* /*pCallback*/)
{
    // No-op.
}

void CMovementSimulation::RunTick(PlayerStorage& /*tStorage*/, bool /*bPath*/, std::function<void(CMoveData&)> /*fCallback*/)
{
    // No-op.
}

void CMovementSimulation::Restore(PlayerStorage& /*tStorage*/)
{
    // No-op.
}

float CMovementSimulation::GetPredictedDelta(CBaseEntity* /*pEntity*/)
{
    return 0.0f;
}

void CRender::Render(IDirect3DDevice9* /*pDevice*/)
{
    // No rendering in textmode.
}

void CRender::Initialize(IDirect3DDevice9* /*pDevice*/)
{
    // Nothing to initialize in textmode.
}

void CRender::LoadColors()
{
}

void CRender::LoadFonts()
{
}

void CRender::LoadStyle()
{
}

bool CAimbotProjectile::AutoAirblast(CTFPlayer* /*pLocal*/, CTFWeaponBase* /*pWeapon*/, CUserCmd* /*pCmd*/, CBaseEntity* /*pProjectile*/)
{
    return false;
}

float CAimbotProjectile::GetSplashRadius(CTFWeaponBase* /*pWeapon*/, CTFPlayer* /*pPlayer*/)
{
    return 0.0f;
}

float CAimbotProjectile::GetSplashRadius(CBaseEntity* /*pProjectile*/, CTFWeaponBase* /*pWeapon*/, CTFPlayer* /*pPlayer*/, float /*flScale*/, CTFWeaponBase* /*pAirblast*/)
{
    return 0.0f;
}

// ESP stubs
void CESP::Store(CTFPlayer* /*pLocal*/)
{
    // No-op.
}

void CESP::Draw()
{
    // No-op.
}

// Spectate stubs
void CSpectate::NetUpdateEnd(CTFPlayer* /*pLocal*/)
{
    // No-op.
}

void CSpectate::NetUpdateStart(CTFPlayer* /*pLocal*/)
{
    // No-op.
}

void CSpectate::CreateMove(CUserCmd* /*pCmd*/)
{
    // No-op.
}

void CSpectate::SetTarget(int /*iTarget*/)
{
    // No-op.
}

// Killstreak stubs
void CKillstreak::PlayerDeath(IGameEvent* /*pEvent*/)
{
    // No-op.
}

void CKillstreak::PlayerSpawn(IGameEvent* /*pEvent*/)
{
    // No-op.
}

void CKillstreak::Reset()
{
    // No-op.
}

// ProjectileSimulation stubs
bool CProjectileSimulation::GetInfo(CTFPlayer* /*pPlayer*/, CTFWeaponBase* /*pWeapon*/, Vec3 /*vAngles*/, ProjectileInfo& /*tProjInfo*/, int /*iFlags*/, float /*flAutoCharge*/)
{
    return false;
}

void CProjectileSimulation::SetupTrace(CTraceFilterCollideable& /*filter*/, int& /*nMask*/, CTFWeaponBase* /*pWeapon*/, int /*nTick*/, bool /*bQuick*/)
{
    // No-op.
}

void CProjectileSimulation::GetInfo(CBaseEntity* /*pProjectile*/, ProjectileInfo& /*tProjInfo*/)
{
    // No-op.
}

void CProjectileSimulation::SetupTrace(CTraceFilterCollideable& /*filter*/, int& /*nMask*/, CBaseEntity* /*pProjectile*/)
{
    // No-op.
}

bool CProjectileSimulation::Initialize(ProjectileInfo& /*tProjInfo*/, bool /*bSimulate*/, bool /*bWorld*/)
{
    return false;
}

void CProjectileSimulation::RunTick(ProjectileInfo& /*tProjInfo*/, bool /*bPath*/)
{
    // No-op.
}

Vec3 CProjectileSimulation::GetOrigin()
{
    return Vec3();
}

Vec3 CProjectileSimulation::GetVelocity()
{
    return Vec3();
}

void IPanel_PaintTraverse(void* /*rcx*/, VPANEL /*vguiPanel*/, bool /*forceRepaint*/, bool /*allowForce*/)
{
    // No-op.
}

void IVModelRender_DrawModelExecute(void* /*rcx*/, const DrawModelState_t& /*pState*/, const ModelRenderInfo_t& /*pInfo*/, matrix3x4* /*pBoneToWorld*/)
{
    // No-op.
}

#endif // TEXTMODE 