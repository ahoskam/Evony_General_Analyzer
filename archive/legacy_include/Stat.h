// Stat.h
#pragma once
#include <string_view>

enum class Stat : int {
    // ====================
    // Core (generic)
    // ====================
    Attack,
    Defense,
    HP,

    // ====================
    // March size (both exist in Evony)
    // ====================
    MarchSizePct,      // +5%, +8%, etc.
    MarchSizeFlat,     // +50000, etc.

    // ====================
    // March / movement
    // ====================
    MarchSpeedPct,
    MarchSpeedToMonsterPct,

    // ====================
    // Vs Enemy (generic)
    // ====================
    AttackVsEnemyPct,
    DefenseVsEnemyPct,
    HPVsEnemyPct,

    // ====================
    // Troop-type base stats
    // ====================
    GroundAttackPct,
    GroundDefensePct,
    GroundHPPct,

    MountedAttackPct,
    MountedDefensePct,
    MountedHPPct,

    RangedAttackPct,
    RangedDefensePct,
    RangedHPPct,

    SiegeAttackPct,
    SiegeDefensePct,
    SiegeHPPct,

    // ====================
    // Reductions (enemy / monster)
    // ====================
    ReduceMonsterAttackPct,
    ReduceMonsterDefensePct,
    ReduceMonsterHPPct,

    ReduceEnemyAttackPct,
    ReduceEnemyDefensePct,
    ReduceEnemyHPPct,

    // ====================
    // Troop-type stats (attacking only)
    // ====================
    AttackingGroundAttackPct,
    AttackingGroundDefensePct,
    AttackingGroundHPPct,

    AttackingMountedAttackPct,
    AttackingMountedDefensePct,
    AttackingMountedHPPct,

    AttackingRangedAttackPct,
    AttackingRangedDefensePct,
    AttackingRangedHPPct,

    AttackingSiegeAttackPct,
    AttackingSiegeDefensePct,
    AttackingSiegeHPPct,

    // ====================
    // Against monsters
    // ====================
    AttackAgainstMonstersPct,
    DefenseAgainstMonstersPct,
    HPAgainstMonstersPct,

    GroundAttackAgainstMonstersPct,
    GroundDefenseAgainstMonstersPct,
    GroundHPAgainstMonstersPct,

    MountedAttackAgainstMonstersPct,
    MountedDefenseAgainstMonstersPct,
    MountedHPAgainstMonstersPct,

    RangedAttackAgainstMonstersPct,
    RangedDefenseAgainstMonstersPct,
    RangedHPAgainstMonstersPct,

    SiegeAttackAgainstMonstersPct,
    SiegeDefenseAgainstMonstersPct,
    SiegeHPAgainstMonstersPct,

    // ====================
    // City / Sub-city
    // ====================
    CityDefensePct,

    SubCityConstructionSpeedPct,
    SubCityTrainingSpeedPct,

    // ====================
    // Traps
    // ====================
    TrapAttackPct,
    TrapDefensePct,
    TrapHPPct,

    TrapAttackAgainstMonstersPct,
    TrapDefenseAgainstMonstersPct,
    TrapHPAgainstMonstersPct,

    // ====================
    // Gathering / production
    // ====================
    FoodProductionPct,
    LumberProductionPct,
    StoneProductionPct,
    OreProductionPct,

    FoodGatheringSpeedPct,
    LumberGatheringSpeedPct,
    StoneGatheringSpeedPct,
    OreGatheringSpeedPct,

    // ====================
    // Construction / research / training
    // ====================
    ConstructionSpeedPct,
    ResearchSpeedPct,

    // "Free research time" appears in the wild with multiple spellings/keys.
    // Keep BOTH to map cleanly from input text.
    FreeResearchTimeFlat,     // e.g. "FreeResearchTime" (flat)

    TrainingSpeedPct,

    // ====================
    // Reinforcing
    // ====================
    ReinforcingGroundAttackPct,
    ReinforcingGroundDefensePct,
    ReinforcingGroundHPPct,

    ReinforcingMountedAttackPct,
    ReinforcingMountedDefensePct,
    ReinforcingMountedHPPct,

    ReinforcingRangedAttackPct,
    ReinforcingRangedDefensePct,
    ReinforcingRangedHPPct,

    ReinforcingSiegeAttackPct,
    ReinforcingSiegeDefensePct,
    ReinforcingSiegeHPPct,

    // ====================
    // Debuffs (enemy)
    // ====================
    EnemyGroundAttackDebuffPct,
    EnemyGroundDefenseDebuffPct,
    EnemyGroundHPDebuffPct,

    EnemyMountedAttackDebuffPct,
    EnemyMountedDefenseDebuffPct,
    EnemyMountedHPDebuffPct,

    EnemyRangedAttackDebuffPct,
    EnemyRangedDefenseDebuffPct,
    EnemyRangedHPDebuffPct,

    EnemySiegeAttackDebuffPct,
    EnemySiegeDefenseDebuffPct,
    EnemySiegeHPDebuffPct,

    // ====================
    // Damage / reduction
    // ====================
    IncreaseDamageDealtPct,
    ReduceDamageTakenPct,

    IncreaseDamageToMonstersPct,
    ReduceDamageFromMonstersPct,

    IncreaseDamageToEnemyPct,
    ReduceDamageFromEnemyPct,

    ReduceDamageFromRangedTroopsPct,
    ReduceDamageFromMountedTroopsPct,
    ReduceDamageFromGroundTroopsPct,
    ReduceDamageFromSiegeTroopsPct,

    // ====================
    // Rates / training / conversions
    // ====================
    AttackingTroopDeathToWoundedRatePct,   // +8% (when attacking)
    GroundTrainingSpeedPct,                // +10%
    EnemyWoundedToDeathWhenAttackingPct,   // +10%

    // ====================
    // Rally
    // ====================
    RallySizePct,          // e.g. "RallySizepct"
    RallyCapacityFlat,

    // ====================
    // Set bonuses / placeholders
    // ====================
    COUNT
};

std::string_view to_string(Stat s);
