// StatParse.cpp
#include "StatParse.h"
#include <unordered_map>

static const std::unordered_map<std::string, Stat>& forward_map()
{
    static const std::unordered_map<std::string, Stat> map = {
        {"Attack", Stat::Attack},
        {"Defense", Stat::Defense},
        {"HP", Stat::HP},
        {"MarchSizePct", Stat::MarchSizePct},
        {"MarchSizeFlat", Stat::MarchSizeFlat},
        {"MarchSpeedPct", Stat::MarchSpeedPct},
        {"MarchSpeedToMonsterPct", Stat::MarchSpeedToMonsterPct},
        {"AttackVsEnemyPct", Stat::AttackVsEnemyPct},
        {"DefenseVsEnemyPct", Stat::DefenseVsEnemyPct},
        {"HPVsEnemyPct", Stat::HPVsEnemyPct},
        {"MountedAttackPct", Stat::MountedAttackPct},
        {"MountedDefensePct", Stat::MountedDefensePct},
        {"MountedHPPct", Stat::MountedHPPct},
        {"GroundAttackPct", Stat::GroundAttackPct},
        {"GroundDefensePct", Stat::GroundDefensePct},
        {"GroundHPPct", Stat::GroundHPPct},
        {"RangedAttackPct", Stat::RangedAttackPct},
        {"RangedDefensePct", Stat::RangedDefensePct},
        {"RangedHPPct", Stat::RangedHPPct},
        {"SiegeAttackPct", Stat::SiegeAttackPct},
        {"SiegeDefensePct", Stat::SiegeDefensePct},
        {"SiegeHPPct", Stat::SiegeHPPct},
        {"ReduceMonsterAttackPct", Stat::ReduceMonsterAttackPct},
        {"ReduceMonsterDefensePct", Stat::ReduceMonsterDefensePct},
        {"ReduceMonsterHPPct", Stat::ReduceMonsterHPPct},
        {"ReduceEnemyAttackPct", Stat::ReduceEnemyAttackPct},
        {"ReduceEnemyDefensePct", Stat::ReduceEnemyDefensePct},
        {"ReduceEnemyHPPct", Stat::ReduceEnemyHPPct},
        {"IncreaseDamageToMonstersPct", Stat::IncreaseDamageToMonstersPct},
        {"ReduceDamageFromMonstersPct", Stat::ReduceDamageFromMonstersPct},
        {"IncreaseDamageToEnemyPct", Stat::IncreaseDamageToEnemyPct},
        {"ReduceDamageFromEnemyPct", Stat::ReduceDamageFromEnemyPct},
        {"IncreaseDamageDealtPct", Stat::IncreaseDamageDealtPct},
        {"ReduceDamageTakenPct", Stat::ReduceDamageTakenPct},
        {"TrapAttackPct", Stat::TrapAttackPct},
        {"TrapDefensePct", Stat::TrapDefensePct},
        {"TrapHPPct", Stat::TrapHPPct},
        {"TrapAttackAgainstMonstersPct", Stat::TrapAttackAgainstMonstersPct},
        {"TrapDefenseAgainstMonstersPct", Stat::TrapDefenseAgainstMonstersPct},
        {"TrapHPAgainstMonstersPct", Stat::TrapHPAgainstMonstersPct},
        {"AttackAgainstMonstersPct", Stat::AttackAgainstMonstersPct},
        {"DefenseAgainstMonstersPct", Stat::DefenseAgainstMonstersPct},
        {"HPAgainstMonstersPct", Stat::HPAgainstMonstersPct},
        {"GroundAttackAgainstMonstersPct", Stat::GroundAttackAgainstMonstersPct},
        {"GroundDefenseAgainstMonstersPct", Stat::GroundDefenseAgainstMonstersPct},
        {"GroundHPAgainstMonstersPct", Stat::GroundHPAgainstMonstersPct},
        {"MountedAttackAgainstMonstersPct", Stat::MountedAttackAgainstMonstersPct},
        {"MountedDefenseAgainstMonstersPct", Stat::MountedDefenseAgainstMonstersPct},
        {"MountedHPAgainstMonstersPct", Stat::MountedHPAgainstMonstersPct},
        {"RangedAttackAgainstMonstersPct", Stat::RangedAttackAgainstMonstersPct},
        {"RangedDefenseAgainstMonstersPct", Stat::RangedDefenseAgainstMonstersPct},
        {"RangedHPAgainstMonstersPct", Stat::RangedHPAgainstMonstersPct},
        {"SiegeAttackAgainstMonstersPct", Stat::SiegeAttackAgainstMonstersPct},
        {"SiegeDefenseAgainstMonstersPct", Stat::SiegeDefenseAgainstMonstersPct},
        {"SiegeHPAgainstMonstersPct", Stat::SiegeHPAgainstMonstersPct},
        {"FoodGatheringSpeedPct", Stat::FoodGatheringSpeedPct},
        {"LumberGatheringSpeedPct", Stat::LumberGatheringSpeedPct},
        {"StoneGatheringSpeedPct", Stat::StoneGatheringSpeedPct},
        {"OreGatheringSpeedPct", Stat::OreGatheringSpeedPct},
        {"FoodProductionPct", Stat::FoodProductionPct},
        {"LumberProductionPct", Stat::LumberProductionPct},
        {"StoneProductionPct", Stat::StoneProductionPct},
        {"OreProductionPct", Stat::OreProductionPct},
        {"ConstructionSpeedPct", Stat::ConstructionSpeedPct},
        {"ResearchSpeedPct", Stat::ResearchSpeedPct},
        {"TrainingSpeedPct", Stat::TrainingSpeedPct},
        {"CityDefensePct", Stat::CityDefensePct},
        {"SubCityConstructionSpeedPct", Stat::SubCityConstructionSpeedPct},
        {"SubCityTrainingSpeedPct", Stat::SubCityTrainingSpeedPct},
        {"RallyCapacityFlat", Stat::RallyCapacityFlat},
        {"RallySizePct", Stat::RallySizePct},
        {"FreeResearchTimeFlat", Stat::FreeResearchTimeFlat},
        {"AttackingTroopDeathToWoundedRatePct", Stat::AttackingTroopDeathToWoundedRatePct},
        {"GroundTrainingSpeedPct", Stat::GroundTrainingSpeedPct},
        {"EnemyWoundedToDeathWhenAttackingPct", Stat::EnemyWoundedToDeathWhenAttackingPct},
        {"AttackingSiegeAttackPct", Stat::AttackingSiegeAttackPct},
        {"AttackingSiegeDefensePct", Stat::AttackingSiegeDefensePct},
        {"AttackingSiegeHPPct", Stat::AttackingSiegeHPPct},
        {"AttackingMountedAttackPct", Stat::AttackingMountedAttackPct},
        {"AttackingMountedDefensePct", Stat::AttackingMountedDefensePct},
        {"AttackingMountedHPPct", Stat::AttackingMountedHPPct},
        {"AttackingGroundAttackPct", Stat::AttackingGroundAttackPct},
        {"AttackingRangedDefensePct", Stat::AttackingRangedDefensePct},
        {"AttackingGroundDefensePct", Stat::AttackingGroundDefensePct},
        {"AttackingGroundHPPct", Stat::AttackingGroundHPPct},
        {"AttackingRangedAttackPct", Stat::AttackingRangedAttackPct},
        {"AttackingRangedHPPct", Stat::AttackingRangedHPPct},
        {"ReinforcingSiegeAttackPct", Stat::ReinforcingSiegeAttackPct},
        {"ReinforcingSiegeDefensePct", Stat::ReinforcingSiegeDefensePct},
        {"ReinforcingSiegeHPPct", Stat::ReinforcingSiegeHPPct},
        {"ReinforcingMountedAttackPct", Stat::ReinforcingMountedAttackPct},
        {"ReinforcingMountedDefensePct", Stat::ReinforcingMountedDefensePct},
        {"ReinforcingMountedHPPct", Stat::ReinforcingMountedHPPct},
        {"ReinforcingGroundAttackPct", Stat::ReinforcingGroundAttackPct},
        {"ReinforcingRangedDefensePct", Stat::ReinforcingRangedDefensePct},
        {"ReinforcingGroundDefensePct", Stat::ReinforcingGroundDefensePct},
        {"ReinforcingGroundHPPct", Stat::ReinforcingGroundHPPct},
        {"ReinforcingRangedAttackPct", Stat::ReinforcingRangedAttackPct},
        {"ReinforcingRangedHPPct", Stat::ReinforcingRangedHPPct},
        {"EnemySiegeAttackDebuffPct", Stat::EnemySiegeAttackDebuffPct},
        {"EnemySiegeDefenseDebuffPct", Stat::EnemySiegeDefenseDebuffPct},
        {"EnemySiegeHPDebuffPct", Stat::EnemySiegeHPDebuffPct},
        {"EnemyMountedAttackDebuffPct", Stat::EnemyMountedAttackDebuffPct},
        {"EnemyMountedDefenseDebuffPct", Stat::EnemyMountedDefenseDebuffPct},
        {"EnemyMountedHPDebuffPct", Stat::EnemyMountedHPDebuffPct},
        {"EnemyGroundAttackDebuffPct", Stat::EnemyGroundAttackDebuffPct},
        {"EnemyRangedDefenseDebuffPct", Stat::EnemyRangedDefenseDebuffPct},
        {"EnemyGroundDefenseDebuffPct", Stat::EnemyGroundDefenseDebuffPct},
        {"EnemyGroundHPDebuffPct", Stat::EnemyGroundHPDebuffPct},
        {"EnemyRangedAttackDebuffPct", Stat::EnemyRangedAttackDebuffPct},
        {"ReduceDamageFromRangedTroopsPct", Stat::ReduceDamageFromRangedTroopsPct},
        {"ReduceDamageFromMountedTroopsPct", Stat::ReduceDamageFromMountedTroopsPct},
        {"ReduceDamageFromGroundTroopsPct", Stat::ReduceDamageFromGroundTroopsPct},
        {"ReduceDamageFromSiegeTroopsPct", Stat::ReduceDamageFromSiegeTroopsPct},
    };
    return map;
}

std::optional<Stat> stat_from_key(const std::string& key) {
    const auto& map = forward_map();
    auto it = map.find(key);
    if (it == map.end()) return std::nullopt;
    return it->second;
}

std::string_view key_from_stat(Stat s)
{
    // Build a reverse map once
    static std::unordered_map<int, std::string_view> rev;
    static bool init = false;
    if (!init) {
        init = true;
        for (const auto& [k, v] : forward_map()) {
            if (rev.find((int)v) == rev.end()) {
                rev[(int)v] = k;
            }
        }
    }
    auto it = rev.find((int)s);
    if (it == rev.end()) return {};
    return it->second;
}

ModifierKind kind_for_stat(Stat s) {
    switch (s) {
        case Stat::MarchSizeFlat:
        case Stat::RallyCapacityFlat:
        case Stat::FreeResearchTimeFlat:
            return ModifierKind::Flat;
        default:
            return ModifierKind::Percent;
    }
}

const std::vector<std::string_view>& supported_stat_keys()
{
    static std::vector<std::string_view> keys;
    static bool init = false;

    if (!init) {
        init = true;

        std::unordered_map<int, std::string_view> seen;
        for (const auto& [k, v] : forward_map()) {
            int stat_id = (int)v;
            if (seen.find(stat_id) == seen.end()) {
                seen[stat_id] = k;
            }
        }

        keys.reserve(seen.size());
        for (const auto& [_, key] : seen) {
            keys.push_back(key);
        }
    }

    return keys;
}
