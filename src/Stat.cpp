// Stat.cpp
#include "Stat.h"
#include "StatParse.h"

std::string_view to_string(Stat s) {
    switch (s) {
        case Stat::Attack: return "Attack";
        case Stat::Defense: return "Defense";
        case Stat::HP: return "HP";

        case Stat::MarchSizePct:  return "March Size %";
        case Stat::MarchSizeFlat: return "March Size (Flat)";

        case Stat::MarchSpeedPct: return "March Speed %";
        case Stat::MarchSpeedToMonsterPct: return "March Speed to Monsters %";

        case Stat::AttackVsEnemyPct:  return "Attack vs Enemy %";
        case Stat::DefenseVsEnemyPct: return "Defense vs Enemy %";
        case Stat::HPVsEnemyPct:      return "HP vs Enemy %";

        case Stat::GroundAttackPct: return "Ground Attack %";
        case Stat::GroundDefensePct: return "Ground Defense %";
        case Stat::GroundHPPct: return "Ground HP %";

        case Stat::MountedAttackPct: return "Mounted Attack %";
        case Stat::MountedDefensePct: return "Mounted Defense %";
        case Stat::MountedHPPct: return "Mounted HP %";

        case Stat::RangedAttackPct: return "Ranged Attack %";
        case Stat::RangedDefensePct: return "Ranged Defense %";
        case Stat::RangedHPPct: return "Ranged HP %";

        case Stat::SiegeAttackPct: return "Siege Attack %";
        case Stat::SiegeDefensePct: return "Siege Defense %";
        case Stat::SiegeHPPct: return "Siege HP %";

        case Stat::ReduceMonsterAttackPct:  return "Reduce Monster Attack %";
        case Stat::ReduceMonsterDefensePct: return "Reduce Monster Defense %";
        case Stat::ReduceMonsterHPPct:      return "Reduce Monster HP %";

        case Stat::ReduceEnemyAttackPct:  return "Reduce Enemy Attack %";
        case Stat::ReduceEnemyDefensePct: return "Reduce Enemy Defense %";
        case Stat::ReduceEnemyHPPct:      return "Reduce Enemy HP %";

        case Stat::AttackAgainstMonstersPct: return "Attack against Monsters %";
        case Stat::DefenseAgainstMonstersPct: return "Defense against Monsters %";
        case Stat::HPAgainstMonstersPct: return "HP against Monsters %";

        case Stat::GroundAttackAgainstMonstersPct: return "Ground Attack against Monsters %";
        case Stat::GroundDefenseAgainstMonstersPct: return "Ground Defense against Monsters %";
        case Stat::GroundHPAgainstMonstersPct: return "Ground HP against Monsters %";

        case Stat::MountedAttackAgainstMonstersPct: return "Mounted Attack against Monsters %";
        case Stat::MountedDefenseAgainstMonstersPct: return "Mounted Defense against Monsters %";
        case Stat::MountedHPAgainstMonstersPct: return "Mounted HP against Monsters %";

        case Stat::RangedAttackAgainstMonstersPct: return "Ranged Attack against Monsters %";
        case Stat::RangedDefenseAgainstMonstersPct: return "Ranged Defense against Monsters %";
        case Stat::RangedHPAgainstMonstersPct: return "Ranged HP against Monsters %";

        case Stat::SiegeAttackAgainstMonstersPct: return "Siege Attack against Monsters %";
        case Stat::SiegeDefenseAgainstMonstersPct: return "Siege Defense against Monsters %";
        case Stat::SiegeHPAgainstMonstersPct: return "Siege HP against Monsters %";

        case Stat::CityDefensePct: return "City Defense %";

        case Stat::SubCityConstructionSpeedPct: return "Sub-city Construction Speed %";
        case Stat::SubCityTrainingSpeedPct: return "Sub-city Training Speed %";

        case Stat::TrapAttackPct: return "Trap Attack %";
        case Stat::TrapDefensePct: return "Trap Defense %";
        case Stat::TrapHPPct: return "Trap HP %";

        case Stat::TrapAttackAgainstMonstersPct: return "Trap Attack against Monsters %";
        case Stat::TrapDefenseAgainstMonstersPct: return "Trap Defense against Monsters %";
        case Stat::TrapHPAgainstMonstersPct: return "Trap HP against Monsters %";

        case Stat::FoodProductionPct: return "Food Production %";
        case Stat::LumberProductionPct: return "Lumber Production %";
        case Stat::StoneProductionPct: return "Stone Production %";
        case Stat::OreProductionPct: return "Ore Production %";

        case Stat::FoodGatheringSpeedPct: return "Food Gathering Speed %";
        case Stat::LumberGatheringSpeedPct: return "Lumber Gathering Speed %";
        case Stat::StoneGatheringSpeedPct: return "Stone Gathering Speed %";
        case Stat::OreGatheringSpeedPct: return "Ore Gathering Speed %";

        case Stat::ConstructionSpeedPct: return "Construction Speed %";
        case Stat::ResearchSpeedPct: return "Research Speed %";
        case Stat::FreeResearchTimeFlat: return "Free Research Time (Flat)";
        case Stat::TrainingSpeedPct: return "Training Speed %";

        case Stat::ReinforcingGroundAttackPct: return "Reinforcing Ground Attack %";
        case Stat::ReinforcingGroundDefensePct: return "Reinforcing Ground Defense %";
        case Stat::ReinforcingGroundHPPct: return "Reinforcing Ground HP %";

        case Stat::ReinforcingMountedAttackPct: return "Reinforcing Mounted Attack %";
        case Stat::ReinforcingMountedDefensePct: return "Reinforcing Mounted Defense %";
        case Stat::ReinforcingMountedHPPct: return "Reinforcing Mounted HP %";

        case Stat::ReinforcingRangedAttackPct: return "Reinforcing Ranged Attack %";
        case Stat::ReinforcingRangedDefensePct: return "Reinforcing Ranged Defense %";
        case Stat::ReinforcingRangedHPPct: return "Reinforcing Ranged HP %";

        case Stat::ReinforcingSiegeAttackPct: return "Reinforcing Siege Attack %";
        case Stat::ReinforcingSiegeDefensePct: return "Reinforcing Siege Defense %";
        case Stat::ReinforcingSiegeHPPct: return "Reinforcing Siege HP %";

        case Stat::EnemyGroundAttackDebuffPct: return "Enemy Ground Attack Debuff %";
        case Stat::EnemyGroundDefenseDebuffPct: return "Enemy Ground Defense Debuff %";
        case Stat::EnemyGroundHPDebuffPct: return "Enemy Ground HP Debuff %";

        case Stat::EnemyMountedAttackDebuffPct: return "Enemy Mounted Attack Debuff %";
        case Stat::EnemyMountedDefenseDebuffPct: return "Enemy Mounted Defense Debuff %";
        case Stat::EnemyMountedHPDebuffPct: return "Enemy Mounted HP Debuff %";

        case Stat::EnemyRangedAttackDebuffPct: return "Enemy Ranged Attack Debuff %";
        case Stat::EnemyRangedDefenseDebuffPct: return "Enemy Ranged Defense Debuff %";
        case Stat::EnemyRangedHPDebuffPct: return "Enemy Ranged HP Debuff %";

        case Stat::EnemySiegeAttackDebuffPct: return "Enemy Siege Attack Debuff %";
        case Stat::EnemySiegeDefenseDebuffPct: return "Enemy Siege Defense Debuff %";
        case Stat::EnemySiegeHPDebuffPct: return "Enemy Siege HP Debuff %";

        case Stat::IncreaseDamageDealtPct: return "Increase Damage Dealt %";
        case Stat::ReduceDamageTakenPct: return "Reduce Damage Taken %";

        case Stat::IncreaseDamageToMonstersPct: return "Increase Damage to Monsters %";
        case Stat::ReduceDamageFromMonstersPct: return "Reduce Damage from Monsters %";

        case Stat::IncreaseDamageToEnemyPct: return "Increase Damage to Enemy %";
        case Stat::ReduceDamageFromEnemyPct: return "Reduce Damage from Enemy %";

        case Stat::ReduceDamageFromRangedTroopsPct: return "Reduce Damage from Ranged Troops %";
        case Stat::ReduceDamageFromMountedTroopsPct: return "Reduce Damage from Mounted Troops %";
        case Stat::ReduceDamageFromGroundTroopsPct: return "Reduce Damage from Ground Troops %";
        case Stat::ReduceDamageFromSiegeTroopsPct: return "Reduce Damage from Siege Troops %";

        case Stat::AttackingTroopDeathToWoundedRatePct: return "Attacking Troop Death to Wounded Rate %";
        case Stat::GroundTrainingSpeedPct: return "Ground Training Speed %";
        case Stat::EnemyWoundedToDeathWhenAttackingPct: return "Enemy Wounded to Death when Attacking %";

        case Stat::RallySizePct: return "Rally Size %";
        case Stat::RallyCapacityFlat: return "Rally Capacity (Flat)";

        case Stat::COUNT: return "COUNT";
    }

    // Fallback: if we forgot a pretty label, return the canonical DB key
    const auto k = key_from_stat(s);
    if (!k.empty()) return k;
    return "Unknown";
}
