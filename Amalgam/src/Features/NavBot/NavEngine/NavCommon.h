// Common navigation-related definitions shared between NavBot, NavEngine and helpers.
// i decided to split this shit out of navengine.h because it was annoying to read ~ melody

#pragma once
#include <cstdint>
#define PLAYER_WIDTH        49.0f
#define HALF_PLAYER_WIDTH   (PLAYER_WIDTH / 2.0f)
#define PLAYER_HEIGHT       83.0f
#define PLAYER_JUMP_HEIGHT  72.0f
#define TICKCOUNT_TIMESTAMP(seconds) (I::GlobalVars->tickcount + static_cast<int>((seconds) / I::GlobalVars->interval_per_tick))

// Task / priority system -----------------------------------------------------
//  Lower value  -> lower urgency.
//  Higher value -> higher urgency (handled first).
//  IMPORTANT: Do NOT change the numeric ordering unless you also update
//  the behavioural scheduler in NBcore.cpp.

enum Priority_list
{
    patrol = 5,
    lowprio_health,
    staynear,
    run_reload,
    run_safe_reload,
    snipe_sentry,
    capture,
    ammo,
    prio_melee,
    engineer,
    health,
    escape_spawn,
    danger
};

// Black-list categories used by NavEngine & NavBot to mark nav areas that
// should be avoided for a period of time.
// New reasons can be added BUT keep BLACKLIST_LENGTH last.

enum BlacklistReason_enum
{
    BR_SENTRY,
    BR_SENTRY_MEDIUM,
    BR_SENTRY_LOW,
    BR_STICKY,
    BR_ENEMY_NORMAL,
    BR_ENEMY_DORMANT,
    BR_ENEMY_INVULN,
    BR_BAD_BUILDING_SPOT,
    // Always keep this as the final entry
    BLACKLIST_LENGTH
};

// Wrapper storing the blacklist reason *and* optional expiry tick.
// Using a tiny struct (rather than raw enum) lets us attach metadata
// without additional lookup structures.
class BlacklistReason
{
public:
    BlacklistReason_enum value { static_cast<BlacklistReason_enum>(-1) };
    int                  time  { 0 };   // expiry tick-count (0 == permanent)

    BlacklistReason() = default;
    explicit BlacklistReason(BlacklistReason_enum reason) : value(reason) {}
    BlacklistReason(BlacklistReason_enum reason, int expiryTick) : value(reason), time(expiryTick) {}

    void operator=(const BlacklistReason_enum& reason) { value = reason; }
}; 