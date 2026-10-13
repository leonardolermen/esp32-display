#include "photos/ShuffleOrder.h"

#include <algorithm>
#include <numeric>

ShuffleOrder::ShuffleOrder(size_t count, uint32_t seed)
    : order(count)
    , random(seed)
{
    std::iota(order.begin(), order.end(), 0);
    reshuffle(std::nullopt);
}

std::optional<size_t> ShuffleOrder::next()
{
    if (order.empty()) {
        return std::nullopt;
    }

    if (position == order.size()) {
        reshuffle(order.back());
    }

    return order[position++];
}

size_t ShuffleOrder::count() const
{
    return order.size();
}

void ShuffleOrder::reshuffle(std::optional<size_t> avoidFirst)
{
    std::shuffle(order.begin(), order.end(), random);

    // Ao dar a volta, a mesma foto não pode ficar duas vezes seguidas na tela.
    if (avoidFirst && order.size() > 1 && order.front() == *avoidFirst) {
        std::swap(order.front(), order.back());
    }

    position = 0;
}
