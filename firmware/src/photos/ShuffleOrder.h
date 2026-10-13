#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

class ShuffleOrder {
public:
    ShuffleOrder(size_t count, uint32_t seed);

    std::optional<size_t> next();
    size_t count() const;

private:
    void reshuffle(std::optional<size_t> avoidFirst);

    std::vector<size_t> order;
    size_t position = 0;
    std::minstd_rand random;
};
