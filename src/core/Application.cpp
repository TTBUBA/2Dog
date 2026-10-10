#include <twodog/core/Application.hpp>
#include <cmath>
#include <stdexcept>

namespace twodog {
namespace {
double readClock(FrameHost& host) {
    const double time = host.nowSeconds();
    if (!std::isfinite(time)) throw std::runtime_error("Host clock is not finite");
    return time;
}
}
RunStats runApplication(FrameHost& host, std::uint64_t maxFrames, ClockConfig config) {
    FixedStepClock clock(config);
    RunStats stats;
    double previous = readClock(host);
    while (!host.shouldClose() && (maxFrames == 0 || stats.frames < maxFrames)) {
        host.pollEvents();
        if (host.shouldClose()) break;
        if (host.suspended()) {
            host.waitEvents(0.1);
            previous = readClock(host);
            clock.reset();
            continue;
        }
        const double now = readClock(host);
        const FramePlan plan = clock.advance(now - previous);
        previous = now;
        stats.droppedSeconds += plan.droppedSeconds;
        for (std::size_t i = 0; i < plan.fixedSteps; ++i) {
            host.fixedUpdate(clock.stepSeconds());
            ++stats.fixedUpdates;
        }
        host.update(plan.acceptedSeconds);
        host.render(plan.alpha);
        host.present();
        ++stats.frames;
    }
    return stats;
}
}
