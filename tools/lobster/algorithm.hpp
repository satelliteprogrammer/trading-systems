#pragma once

#include <cstddef>

namespace ome::tools::lobster::algorithm {

/// Welford's online algorithm for computing mean and variance
///
/// https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm
class Welford {
  public:
    void update(double value);

    [[nodiscard]] auto mean() const -> double;
    [[nodiscard]] auto stddev() const -> double;

  private:
    double mean_ = 0.0;
    std::size_t count_ = 0;
    double m2_ = 0.0;
};

} // namespace ome::tools::lobster::algorithm
