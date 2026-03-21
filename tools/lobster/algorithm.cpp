#include "algorithm.hpp"

#include <cmath>

namespace ome::tools::lobster::algorithm {

void Welford::update(double value) {
    count_ += 1;
    double delta = value - mean_;
    mean_ += delta / static_cast<double>(count_);
    m2_ += delta * (value - mean_);
}

auto Welford::mean() const -> double { return mean_; }

auto Welford::stddev() const -> double {
    if (count_ == 0) {
        return 0.0;
    }
    return std::sqrt(m2_ / static_cast<double>(count_));
}

} // namespace ome::tools::lobster::algorithm
