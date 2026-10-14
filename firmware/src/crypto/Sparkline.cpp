#include "crypto/Sparkline.h"

#include <cmath>

namespace Sparkline {

// O lv_chart só aceita inteiros; centavos preservam a forma da linha mesmo no dólar, que varia nos centavos.
std::vector<int32_t> toChart(const std::vector<float>& series, size_t maxPoints)
{
    std::vector<int32_t> points;
    size_t skip = series.size() > maxPoints ? series.size() - maxPoints : 0;

    for (size_t index = skip; index < series.size(); index++) {
        points.push_back(static_cast<int32_t>(std::lround(series[index] * 100.0f)));
    }

    return points;
}

}
