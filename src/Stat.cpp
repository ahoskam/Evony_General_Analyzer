#include "Stat.h"

std::string_view to_string(Stat s) {
    switch (s) {
        case Stat::Attack: return "Attack";
        case Stat::Defense: return "Defense";
        case Stat::HP: return "HP";
        case Stat::MarchSize: return "March Size";

        case Stat::MarchSpeedPct: return "March Speed %";
        case Stat::MarchSpeedToMonsterPct: return "March Speed to Monster %";

        case Stat::AttackVsMonsterPct: return "Attack vs Monster %";
        case Stat::DefenseVsMonsterPct: return "Defense vs Monster %";
        case Stat::HPVsMonsterPct: return "HP vs Monster %";

        case Stat::AttackVsEnemyPct: return "Attack vs Enemy %";
        case Stat::DefenseVsEnemyPct: return "Defense vs Enemy %";
        case Stat::HPVsEnemyPct: return "HP vs Enemy %";

        case Stat::MountedAttackPct: return "Mounted Attack %";
        case Stat::MountedDefensePct: return "Mounted Defense %";
        case Stat::MountedHPPct: return "Mounted HP %";

        case Stat::GroundAttackPct: return "Ground Attack %";
        case Stat::GroundDefensePct: return "Ground Defense %";
        case Stat::GroundHPPct: return "Ground HP %";

        case Stat::RangedAttackPct: return "Ranged Attack %";
        case Stat::RangedDefensePct: return "Ranged Defense %";
        case Stat::RangedHPPct: return "Ranged HP %";

        case Stat::SiegeAttackPct: return "Siege Attack %";
        case Stat::SiegeDefensePct: return "Siege Defense %";
        case Stat::SiegeHPPct: return "Siege HP %";

        case Stat::MountedAttackVsMonsterPct: return "Mounted Attack vs Monster %";
        case Stat::MountedDefenseVsMonsterPct: return "Mounted Defense vs Monster %";
        case Stat::MountedHPVsMonsterPct: return "Mounted HP vs Monster %";

        case Stat::GroundAttackVsMonsterPct: return "Ground Attack vs Monster %";
        case Stat::GroundDefenseVsMonsterPct: return "Ground Defense vs Monster %";
        case Stat::GroundHPVsMonsterPct: return "Ground HP vs Monster %";

        case Stat::RangedAttackVsMonsterPct: return "Ranged Attack vs Monster %";
        case Stat::RangedDefenseVsMonsterPct: return "Ranged Defense vs Monster %";
        case Stat::RangedHPVsMonsterPct: return "Ranged HP vs Monster %";

        case Stat::SiegeAttackVsMonsterPct: return "Siege Attack vs Monster %";
        case Stat::SiegeDefenseVsMonsterPct: return "Siege Defense vs Monster %";
        case Stat::SiegeHPVsMonsterPct: return "Siege HP vs Monster %";

        case Stat::ReduceMonsterAttackPct: return "Reduce Monster Attack %";
        case Stat::ReduceMonsterDefensePct: return "Reduce Monster Defense %";
        case Stat::ReduceMonsterHPPct: return "Reduce Monster HP %";

        case Stat::ReduceEnemyAttackPct: return "Reduce Enemy Attack %";
        case Stat::ReduceEnemyDefensePct: return "Reduce Enemy Defense %";
        case Stat::ReduceEnemyHPPct: return "Reduce Enemy HP %";

        case Stat::IncreaseDamageToMonstersPct: return "Increase Damage to Monsters %";
        case Stat::ReduceDamageFromMonstersPct: return "Reduce Damage from Monsters %";
        case Stat::IncreaseDamageToEnemyPct: return "Increase Damage to Enemy %";
        case Stat::ReduceDamageFromEnemyPct: return "Reduce Damage from Enemy %";

        case Stat::ReduceDamageFromRangedTroopsPct: return "Reduce Damage from Ranged Troops %";
        case Stat::ReduceDamageFromMountedTroopsPct: return "Reduce Damage from Mounted Troops %";
        case Stat::ReduceDamageFromGroundTroopsPct: return "Reduce Damage from Ground Troops %";
        case Stat::ReduceDamageFromSiegeTroopsPct: return "Reduce Damage from Siege Troops %";

        case Stat::RallyCapacityFlat: return "Rally Capacity (Flat)";

        case Stat::COUNT: return "COUNT";
    }
    return "Unknown Stat";
}
