#pragma once
#include <string_view>

enum class Stat : int {
    // Core
    Attack,
    Defense,
    HP,
    MarchSize,

    // March
    MarchSpeedPct,
    MarchSpeedToMonsterPct,

    // Monster generic
    AttackVsMonsterPct,
    DefenseVsMonsterPct,
    HPVsMonsterPct,

    // Enemy generic (PvP)
    AttackVsEnemyPct,
    DefenseVsEnemyPct,
    HPVsEnemyPct,

    // Troop-type PvP buffs
    MountedAttackPct,
    MountedDefensePct,
    MountedHPPct,

    GroundAttackPct,
    GroundDefensePct,
    GroundHPPct,

    RangedAttackPct,
    RangedDefensePct,
    RangedHPPct,

    SiegeAttackPct,
    SiegeDefensePct,
    SiegeHPPct,

    // Troop-type vs Monster buffs (separate)
    MountedAttackVsMonsterPct,
    MountedDefenseVsMonsterPct,
    MountedHPVsMonsterPct,

    GroundAttackVsMonsterPct,
    GroundDefenseVsMonsterPct,
    GroundHPVsMonsterPct,

    RangedAttackVsMonsterPct,
    RangedDefenseVsMonsterPct,
    RangedHPVsMonsterPct,

    SiegeAttackVsMonsterPct,
    SiegeDefenseVsMonsterPct,
    SiegeHPVsMonsterPct,

    // Debuffs you said definitely exist
    ReduceMonsterAttackPct,
    ReduceMonsterDefensePct,
    ReduceMonsterHPPct,

    ReduceEnemyAttackPct,
    ReduceEnemyDefensePct,
    ReduceEnemyHPPct,

    // Damage modifiers (v1 add)
    IncreaseDamageToMonstersPct,
    ReduceDamageFromMonstersPct,

    IncreaseDamageToEnemyPct,
    ReduceDamageFromEnemyPct,

    ReduceDamageFromRangedTroopsPct,
    ReduceDamageFromMountedTroopsPct,
    ReduceDamageFromGroundTroopsPct,
    ReduceDamageFromSiegeTroopsPct,

    // Rally
    RallyCapacityFlat,

    // Sentinel
    COUNT
};

constexpr int StatCount() { return static_cast<int>(Stat::COUNT); }

std::string_view to_string(Stat s);
