#pragma once
#include <cstddef>

namespace twodog {
struct ClockConfig {
    double stepSeconds = 1.0 / 60.0;
    double maxFrameSeconds = 0.25;
    std::size_t maxStepsPerFrame = 8;
};
struct FramePlan {
    std::size_t fixedSteps = 0;
    double alpha = 0.0;
    double acceptedSeconds = 0.0;
    double droppedSeconds = 0.0;
};

// Pure scheduling: does not read wall time, allocate, sleep, or call game code.
// Excess complete steps are discarded to bound catch-up work; the fraction stays.
class FixedStepClock final {
public:
    explicit FixedStepClock(ClockConfig config = {});
    [[nodiscard]] FramePlan advance(double elapsedSeconds);
    void reset() noexcept;
    [[nodiscard]] double stepSeconds() const noexcept { return config_.stepSeconds; }
private:
    ClockConfig config_;
    double accumulator_ = 0.0;
};
}
