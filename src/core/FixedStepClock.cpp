#include <twodog/core/FixedStepClock.hpp>
#include <cmath>
#include <stdexcept>

namespace twodog {
FixedStepClock::FixedStepClock(ClockConfig config) : config_(config) {
    if (!std::isfinite(config.stepSeconds) || config.stepSeconds < 0.000001 ||
        config.stepSeconds > 1.0 || !std::isfinite(config.maxFrameSeconds) ||
        config.maxFrameSeconds < config.stepSeconds || config.maxFrameSeconds > 10.0 ||
        config.maxStepsPerFrame == 0 || config.maxStepsPerFrame > 10000) {
        throw std::invalid_argument("Invalid fixed-step configuration");
    }
}
FramePlan FixedStepClock::advance(double elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0)
        throw std::invalid_argument("Frame duration must be finite and non-negative");
    FramePlan frame;
    frame.acceptedSeconds = std::fmin(elapsedSeconds, config_.maxFrameSeconds);
    frame.droppedSeconds = elapsedSeconds - frame.acceptedSeconds;
    accumulator_ += frame.acceptedSeconds;
    // Tolerance is relative to a tick, below 1 ps at the default frequency.
    const double epsilon = config_.stepSeconds * 1e-10;
    while (accumulator_ + epsilon >= config_.stepSeconds && frame.fixedSteps < config_.maxStepsPerFrame) {
        accumulator_ = std::fmax(0.0, accumulator_ - config_.stepSeconds);
        ++frame.fixedSteps;
    }
    if (accumulator_ + epsilon >= config_.stepSeconds) {
        const double skipped = std::floor((accumulator_ + epsilon) / config_.stepSeconds);
        const double discarded = skipped * config_.stepSeconds;
        frame.droppedSeconds += discarded;
        accumulator_ = std::fmax(0.0, accumulator_ - discarded);
    }
    frame.alpha = accumulator_ / config_.stepSeconds;
    return frame;
}
void FixedStepClock::reset() noexcept { accumulator_ = 0.0; }
}
