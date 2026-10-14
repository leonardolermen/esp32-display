#pragma once

#include <cstdint>
#include <vector>

namespace Sparkline {

std::vector<int32_t> toChart(const std::vector<float>& series, size_t maxPoints);

}
