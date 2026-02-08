#pragma once
#include <string>
#include "Stats.h"

class General {
public:
    explicit General(std::string name) : name_(std::move(name)) {}

    const std::string& name() const { return name_; }
    Stats& stats() { return stats_; }
    const Stats& stats() const { return stats_; }

private:
    std::string name_;
    Stats stats_;
};
