#pragma once
#include <twodog/core/FixedStepClock.hpp>
#include <cstdint>

namespace twodog {
// Host owns platform operations and the monotonic time source.
class FrameHost {
public:
    virtual ~FrameHost() = default;
    virtual void pollEvents() = 0;
    virtual bool shouldClose() const = 0;
    virtual bool suspended() const = 0;
    virtual void waitEvents(double timeoutSeconds) = 0;
    virtual double nowSeconds() const = 0;
    virtual void fixedUpdate(double dt) = 0;
    virtual void update(double dt) { static_cast<void>(dt); }
    virtual void render(double alpha) = 0;
    virtual void present() = 0;
};
struct RunStats {
    std::uint64_t frames = 0;
    std::uint64_t fixedUpdates = 0;
    double droppedSeconds = 0.0;
};
// maxFrames == 0 runs until the host requests close. Exceptions propagate to main.
[[nodiscard]] RunStats runApplication(FrameHost& host, std::uint64_t maxFrames = 0,
                                      ClockConfig config = {});
}
